#include "KContentFilterMarks.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstring>
#include <string>

#include "KFilterContext.h"
#include "KHttpLib.h"
#include "KHttpObject.h"
#include "KHttpRequest.h"
#include "KRewriteMarkEx.h"
#include "log.h"
#include "utils.h"

namespace {

constexpr size_t kDefaultFilterBuffer = 1024 * 1024;
constexpr size_t kMaxFilterBuffer = 16 * 1024 * 1024;
constexpr size_t kMaxHtmlTag = 64 * 1024;

size_t parse_filter_buffer(const char* value) {
	INT64 parsed = get_size(value);
	if (parsed <= 0) return kDefaultFilterBuffer;
	return (size_t)std::min<INT64>(parsed, (INT64)kMaxFilterBuffer);
}

class MatchData {
public:
#ifdef ENABLE_PCRE2
	MatchData() : data(pcre2_match_data_create(100, nullptr)) {}
	~MatchData() { pcre2_match_data_free(data); }
	kgl_pcre_match_data* get() { return data; }
	KGL_OVECTOR_SIZE* vector() { return pcre2_get_ovector_pointer(data); }
	int count() const { return (int)pcre2_get_ovector_count(data); }
private:
	kgl_pcre_match_data* data;
#else
	MatchData() : data{ vector_data, (int)(sizeof(vector_data) / sizeof(vector_data[0])) } {}
	kgl_pcre_match_data* get() { return &data; }
	KGL_OVECTOR_SIZE* vector() { return vector_data; }
	int count() const { return data.ovector_size / 2; }
private:
	KGL_OVECTOR_SIZE vector_data[300];
	kgl_pcre_match_data data;
#endif
};

int partial_flag() {
#ifdef ENABLE_PCRE2
	return PCRE2_PARTIAL_SOFT;
#else
	return PCRE_PARTIAL;
#endif
}

bool is_partial_result(int result) {
#ifdef ENABLE_PCRE2
	return result == PCRE2_ERROR_PARTIAL;
#else
	return result == PCRE_ERROR_PARTIAL;
#endif
}

KGL_RESULT write_down(kgl_response_body& down, const char* data, size_t length) {
	if (length == 0) {
		return KGL_OK;
	}
	while (length > 0) {
		int chunk = (int)std::min<size_t>(length, INT_MAX);
		KGL_RESULT result = down.f->write(down.ctx, data, chunk);
		if (result != KGL_OK) {
			return result;
		}
		data += chunk;
		length -= chunk;
	}
	return KGL_OK;
}

class BodyFilter {
public:
	virtual ~BodyFilter() = default;
	virtual KGL_RESULT write(const char* data, int length) = 0;
	virtual KGL_RESULT flush_pending() = 0;
	virtual void abort_pending() {}
	kgl_response_body down{};
};

KGL_RESULT filter_write(kgl_response_body_ctx* raw, const char* data, int length) {
	return reinterpret_cast<BodyFilter*>(raw)->write(data, length);
}

KGL_RESULT filter_writev(kgl_response_body_ctx* raw, const kbuf* bufs, int length) {
	auto* ctx = reinterpret_cast<BodyFilter*>(raw);
	while (length > 0 && bufs) {
		int got = (std::min)(length, bufs->used);
		KGL_RESULT result = ctx->write((const char*)bufs->data, got);
		if (result != KGL_OK) {
			return result;
		}
		length -= got;
		bufs = bufs->next;
	}
	return length == 0 ? KGL_OK : KGL_EDATA_FORMAT;
}

KGL_RESULT filter_flush(kgl_response_body_ctx* raw) {
	auto* ctx = reinterpret_cast<BodyFilter*>(raw);
	KGL_RESULT result = ctx->flush_pending();
	if (result != KGL_OK) {
		return result;
	}
	return ctx->down.f->flush(ctx->down.ctx);
}

bool filter_support_sendfile(kgl_response_body_ctx*) {
	return false;
}

KGL_RESULT filter_close(kgl_response_body_ctx* raw, KGL_RESULT result) {
	auto* ctx = reinterpret_cast<BodyFilter*>(raw);
	if (result >= KGL_OK) {
		KGL_RESULT pending_result = ctx->flush_pending();
		if (pending_result != KGL_OK) {
			result = pending_result;
		}
	} else {
		ctx->abort_pending();
	}
	kgl_response_body down = ctx->down;
	delete ctx;
	return down.f->close(down.ctx, result);
}

kgl_response_body_function body_filter_functions = {
	filter_writev,
	filter_write,
	filter_flush,
	filter_support_sendfile,
	nullptr,
	filter_close,
};

KGL_RESULT tee_body(kgl_response_body_ctx* raw, KREQUEST, kgl_response_body* body) {
	auto* ctx = reinterpret_cast<BodyFilter*>(raw);
	if (!body) {
		delete ctx;
		return KGL_OK;
	}
	ctx->down = *body;
	body->ctx = raw;
	body->f = &body_filter_functions;
	return KGL_OK;
}

kgl_out_filter body_filter = {
	sizeof(kgl_out_filter),
	KGL_FILTER_CACHE | KGL_FILTER_NOT_CACHE,
	tee_body,
};

void register_body_filter(KHttpRequest* rq, BodyFilter* ctx) {
	auto* body = new kgl_out_filter_body;
	body->filter = &body_filter;
	body->ctx = reinterpret_cast<kgl_response_body_ctx*>(ctx);
	rq->getOutputFilterContext()->add(body);
}

class ReplaceContentFilter final : public BodyFilter {
public:
	ReplaceContentFilter(KHttpRequest* rq, KSharedObj<KReplaceContentState> state)
		: rq(rq), state(std::move(state)) {}

	KGL_RESULT write(const char* data, int length) override {
		if (stopped) {
			return KGL_OK;
		}
		if (disabled) {
			return write_down(down, data, length);
		}
		pending.append(data, (size_t)length);
		return process(false);
	}

	KGL_RESULT flush_pending() override {
		if (stopped) {
			return KGL_OK;
		}
		return process(true);
	}

	void abort_pending() override { pending.clear(); }

private:
	/* Data before pos is done. Moving an offset instead of erasing the
	 * front of pending after each match, collecting the output and writing
	 * it down once, and one match buffer per call keep many matches cheap:
	 * 400k one-byte matches took over two minutes before. */
	KGL_RESULT process(bool final) {
		size_t pos = 0;
		std::string out;
		MatchData match_data;
		while (pos < pending.size()) {
			const char* data = pending.data() + pos;
			int left = (int)(pending.size() - pos);
			int matched = state->match(data, left, match_data.get());
			auto* offsets = match_data.vector();
			if (matched > 0) {
				if (match_data.count() < 1 || offsets[1] <= offsets[0]) {
					disabled = true;
					break;
				}
				auto* replacement = state->make_replacement(rq, data, match_data.get(), matched);
				if (!replacement) {
					return KGL_ENO_MEMORY;
				}
				state->update_mark(rq);
				if (state->replaced_stop) {
					pending.clear();
					stopped = true;
					out.append(replacement->c_str(), replacement->size());
					delete replacement;
					return write_down(down, out.data(), out.size());
				}
				out.append(data, (size_t)offsets[0]);
				out.append(replacement->c_str(), replacement->size());
				delete replacement;
				pos += (size_t)offsets[1];
				continue;
			}
			if (!final && is_partial_result(matched)) {
				/* Only the tail from where the partial match starts can
				 * still change: send what is before it. Keeping everything
				 * until the buffer limit stopped all later replacements
				 * once the limit was hit. */
				size_t start = match_data.count() >= 1 ? (size_t)offsets[0] : 0;
				if (start > (size_t)left) {
					start = 0;
				}
				out.append(data, start);
				pos += start;
				if (pending.size() - pos <= state->buffer) {
					pending.erase(0, pos);
					return write_down(down, out.data(), out.size());
				}
				klog(KLOG_WARNING, "replace_content partial match exceeded buffer limit\n");
				disabled = true;
				break;
			}
			if (!final && state->replaced_stop) {
				if (pending.size() - pos <= state->buffer) {
					pending.erase(0, pos);
					return write_down(down, out.data(), out.size());
				}
				out.append(data, (size_t)left);
				pending.clear();
				return write_down(down, out.data(), out.size());
			}
			break;
		}
		if (pos < pending.size()) {
			out.append(pending.data() + pos, pending.size() - pos);
		}
		pending.clear();
		return write_down(down, out.data(), out.size());
	}

	KHttpRequest* rq;
	KSharedObj<KReplaceContentState> state;
	std::string pending;
	bool stopped{ false };
	bool disabled{ false };
};

class RegContentFilter final : public BodyFilter {
public:
	RegContentFilter(KHttpRequest* rq, KSharedObj<KRegContentState> state, KReg* reg,
		kgl_jump_type action)
		: rq(rq), state(std::move(state)), reg(reg), action(action) {}

	KGL_RESULT write(const char* data, int length) override {
		if (disabled) {
			return write_down(down, data, length);
		}
		pending.append(data, (size_t)length);
		return process(false);
	}

	KGL_RESULT flush_pending() override { return process(true); }
	void abort_pending() override { pending.clear(); }

private:
	KGL_RESULT process(bool final) {
		if (pending.empty()) {
			return KGL_OK;
		}
		MatchData match_data;
		int matched = reg->match(pending.data(), (int)pending.size(),
			final ? 0 : partial_flag(), match_data.get());
		if (matched > 0) {
			auto* offsets = match_data.vector();
			int key_length = match_data.count() > 0 ? (int)(offsets[1] - offsets[0]) : 0;
			klog(KLOG_WARNING, "http://%s%s content filter matched key [%.*s]\n",
				rq->sink->data.url->host, rq->sink->data.url->path,
				key_length, key_length > 0 ? pending.data() + offsets[0] : "");
			if (action != JUMP_ALLOW && action != JUMP_CONTINUE) {
				pending.clear();
				return KGL_EDENIED;
			}
			if (action == JUMP_ALLOW) {
				disabled = true;
			}
			KGL_RESULT result = write_down(down, pending.data(), pending.size());
			pending.clear();
			return result;
		}
		if (!final && is_partial_result(matched)) {
			/* send what is before the partial match, keep only its tail */
			size_t start = match_data.count() >= 1 ? (size_t)match_data.vector()[0] : 0;
			if (start > pending.size()) {
				start = 0;
			}
			if (start > 0) {
				KGL_RESULT result = write_down(down, pending.data(), start);
				if (result != KGL_OK) {
					return result;
				}
				pending.erase(0, start);
			}
			if (pending.size() <= state->buffer) {
				return KGL_OK;
			}
			klog(KLOG_WARNING, "content partial match exceeded buffer limit\n");
		}
		KGL_RESULT result = write_down(down, pending.data(), pending.size());
		pending.clear();
		return result;
	}

	KHttpRequest* rq;
	/* keeps reg alive */
	KSharedObj<KRegContentState> state;
	KReg* reg;
	kgl_jump_type action;
	std::string pending;
	bool disabled{ false };
};

bool ascii_equal(const std::string& value, size_t offset, size_t length,
	const char* expected) {
	size_t expected_length = strlen(expected);
	if (length != expected_length) {
		return false;
	}
	for (size_t i = 0; i < length; ++i) {
		if (tolower((unsigned char)value[offset + i]) !=
			tolower((unsigned char)expected[i])) {
			return false;
		}
	}
	return true;
}

const char* url_attribute(const std::string& tag, size_t offset, size_t length) {
	struct TagAttribute { const char* tag; const char* attribute; };
	static const TagAttribute attributes[] = {
		{ "a", "href" }, { "form", "action" }, { "area", "href" },
		{ "frame", "src" }, { "input", "src" }, { "img", "src" },
		{ "javascript", "src" }, { "base", "href" }, { "script", "src" },
		{ "link", "href" }, { "iframe", "src" },
	};
	for (const auto& item : attributes) {
		if (ascii_equal(tag, offset, length, item.tag)) {
			return item.attribute;
		}
	}
	return nullptr;
}

std::string rewrite_html_tag(KHttpRequest* rq, KReplaceUrlState* mark,
	const std::string& tag) {
	if (tag.size() < 3 || tag.front() != '<' || tag.back() != '>') {
		return tag;
	}
	size_t pos = 1;
	while (pos < tag.size() && isspace((unsigned char)tag[pos])) ++pos;
	if (pos >= tag.size() || tag[pos] == '/' || tag[pos] == '!' || tag[pos] == '?') {
		return tag;
	}
	size_t name_start = pos;
	while (pos < tag.size() && !isspace((unsigned char)tag[pos]) &&
		tag[pos] != '/' && tag[pos] != '>') ++pos;
	const char* wanted = url_attribute(tag, name_start, pos - name_start);
	if (!wanted) {
		return tag;
	}
	while (pos < tag.size() - 1) {
		while (pos < tag.size() - 1 && isspace((unsigned char)tag[pos])) ++pos;
		if (tag[pos] == '/' || tag[pos] == '>') break;
		size_t attr_start = pos;
		while (pos < tag.size() - 1 && !isspace((unsigned char)tag[pos]) &&
			tag[pos] != '=' && tag[pos] != '>' && tag[pos] != '/') ++pos;
		size_t attr_length = pos - attr_start;
		while (pos < tag.size() - 1 && isspace((unsigned char)tag[pos])) ++pos;
		if (pos >= tag.size() - 1 || tag[pos] != '=') {
			continue;
		}
		++pos;
		while (pos < tag.size() - 1 && isspace((unsigned char)tag[pos])) ++pos;
		char quote = 0;
		if (tag[pos] == '\'' || tag[pos] == '"') {
			quote = tag[pos++];
		}
		size_t value_start = pos;
		if (quote) {
			while (pos < tag.size() - 1 && tag[pos] != quote) ++pos;
		} else {
			while (pos < tag.size() - 1 && !isspace((unsigned char)tag[pos]) &&
				tag[pos] != '>') ++pos;
		}
		size_t value_length = pos - value_start;
		if (!ascii_equal(tag, attr_start, attr_length, wanted)) {
			if (quote && pos < tag.size() - 1) ++pos;
			continue;
		}
		auto* replaced = mark->replace(rq, tag.data() + value_start, (int)value_length);
		if (!replaced) {
			return tag;
		}
		std::string result;
		result.reserve(tag.size() - value_length + replaced->size());
		result.append(tag.data(), value_start);
		result.append(replaced->c_str(), replaced->size());
		result.append(tag.data() + value_start + value_length,
			tag.size() - value_start - value_length);
		delete replaced;
		return result;
	}
	return tag;
}

class ReplaceUrlFilter final : public BodyFilter {
public:
	ReplaceUrlFilter(KHttpRequest* rq, KSharedObj<KReplaceUrlState> state)
		: rq(rq), state(std::move(state)) {}

	KGL_RESULT write(const char* data, int length) override {
		pending.append(data, (size_t)length);
		return process(false);
	}
	KGL_RESULT flush_pending() override { return process(true); }
	void abort_pending() override { pending.clear(); }

private:
	KGL_RESULT process(bool final) {
		size_t consumed = 0;
		while (consumed < pending.size()) {
			size_t open = pending.find('<', consumed);
			if (open == std::string::npos) {
				if (!final) {
					KGL_RESULT result = write_down(down, pending.data() + consumed,
						pending.size() - consumed);
					pending.clear();
					return result;
				}
				break;
			}
			KGL_RESULT result = write_down(down, pending.data() + consumed, open - consumed);
			if (result != KGL_OK) return result;
			size_t close = pending.find('>', open + 1);
			if (close == std::string::npos) {
				if (!final && pending.size() - open <= kMaxHtmlTag) {
					pending.erase(0, open);
					return KGL_OK;
				}
				result = write_down(down, pending.data() + open, pending.size() - open);
				pending.clear();
				return result;
			}
			std::string tag = pending.substr(open, close - open + 1);
			std::string rewritten = rewrite_html_tag(rq, state.get(), tag);
			result = write_down(down, rewritten.data(), rewritten.size());
			if (result != KGL_OK) return result;
			consumed = close + 1;
		}
		KGL_RESULT result = write_down(down, pending.data() + consumed,
			pending.size() - consumed);
		pending.clear();
		return result;
	}

	KHttpRequest* rq;
	KSharedObj<KReplaceUrlState> state;
	std::string pending;
};

class FixHeaderFilter final : public BodyFilter {
public:
	explicit FixHeaderFilter(const KString& header) : header(header) {}
	KGL_RESULT write(const char* data, int length) override {
		KGL_RESULT result = write_header();
		return result == KGL_OK ? write_down(down, data, length) : result;
	}
	KGL_RESULT flush_pending() override { return write_header(); }
private:
	KGL_RESULT write_header() {
		if (written) return KGL_OK;
		written = true;
		return write_down(down, header.c_str(), header.size());
	}
	KString header;
	bool written{ false };
};

void replace_header_value(KHttpHeader* header, const KStringStream& value) {
	size_t new_length = (size_t)header->val_offset + value.size();
	if (new_length > UINT16_MAX) {
		return;
	}
	char* buffer = (char*)xmalloc(new_length + 1);
	if (header->val_offset > 0) {
		memcpy(buffer, header->buf, header->val_offset);
	}
	memcpy(buffer + header->val_offset, value.buf(), value.size());
	buffer[new_length] = '\0';
	xfree_header_buffer(header);
	header->buf = buffer;
	header->buf_in_pool = 0;
	header->val_len = (uint16_t)value.size();
}

} // namespace

KReplaceContentMark::KReplaceContentMark()
	: mark_acl(0), mark_mark(0), nc(true), replaced_stop(false),
	  buffer(kDefaultFilterBuffer) {}

KMark* KReplaceContentMark::new_instance() { return new KReplaceContentMark; }
const char* KReplaceContentMark::get_module() const { return "replace_content"; }

uint32_t KReplaceContentMark::process(KHttpRequest* rq, KHttpObject*, KSafeSource&) {
	KSharedObj<KReplaceContentState> st = state;
	if (!st || !st->valid || !st->should_process(rq)) return KF_STATUS_REQ_FALSE;
	register_body_filter(rq, new ReplaceContentFilter(rq, std::move(st)));
	return KF_STATUS_REQ_TRUE;
}

void KReplaceContentMark::get_display(KWStream& s) {
	s << content_text << "==>" << replace_text;
}

void KReplaceContentMark::get_html(KWStream& s) {
	s << "content(regex):<textarea name='content' rows=1>" << content_text
	  << "</textarea><input type=checkbox name='nc' value='1' "
	  << (nc ? "checked" : "") << ">nc replace:<textarea name='replace' rows=1>"
	  << replace_text << "</textarea><br>charset:<input name='charset' value='"
	  << charset << "'> buffer:<input name='buffer' size=6 value='" << buffer
	  << "'> acl:<input name='mark_acl' size=4 value='" << mark_acl
	  << "'> mark:<input name='mark_mark' size=4 value='" << mark_mark
	  << "'><input type=checkbox name='replaced_stop' value='1' "
	  << (replaced_stop ? "checked" : "") << ">replaced_stop";
}

void KReplaceContentMark::parse_config(const khttpd::KXmlNodeBody* xml) {
	auto attr = xml->attr();
	content_text = attr["content"];
	replace_text = attr["replace"];
	charset = attr["charset"];
	mark_acl = attr.get_int("mark_acl", 0);
	mark_mark = attr.get_int("mark_mark", 0);
	nc = attr["nc"].empty() || attr["nc"] == "1";
	replaced_stop = attr["replaced_stop"] == "1";
	buffer = parse_filter_buffer(attr["buffer"].c_str());
	KSharedObj<KReplaceContentState> st(new KReplaceContentState);
	st->mark_acl = mark_acl;
	st->mark_mark = mark_mark;
	st->replaced_stop = replaced_stop;
	st->buffer = buffer;
	KString pattern = content_text;
	st->replace = replace_text;
	if (!charset.empty() && strcasecmp(charset.c_str(), "utf-8") != 0) {
		char* converted = utf82charset(pattern.c_str(), pattern.size(), charset.c_str());
		if (converted) {
			pattern = converted;
			free(converted);
		}
		converted = utf82charset(replace_text.c_str(), replace_text.size(), charset.c_str());
		if (converted) {
			st->replace = converted;
			free(converted);
		}
	}
	st->valid = st->content.setModel(pattern.c_str(), nc ? KGL_PCRE_CASELESS : 0);
	state = std::move(st);
}

int KReplaceContentState::match(const char* data, int length,
	kgl_pcre_match_data* match_data) {
	return content.match(data, length, partial_flag(), match_data);
}

KStringStream* KReplaceContentState::make_replacement(KHttpRequest* rq,
	const char* data, kgl_pcre_match_data* match_data, int matched) const {
	KRegSubString* substring = KReg::makeSubString(data, match_data, matched);
	auto* result = KRewriteMarkEx::getString(nullptr, replace.c_str(),
		rq, substring, substring);
	delete substring;
	return result;
}

bool KReplaceContentState::should_process(KHttpRequest* rq) const {
	if (mark_acl > 0) return (int)rq->sink->data.mark == mark_acl;
	if (mark_acl < 0) return KBIT_TEST(rq->sink->data.mark, -mark_acl);
	return true;
}

void KReplaceContentState::update_mark(KHttpRequest* rq) const {
	if (mark_mark > 0) rq->sink->data.mark = (uint32_t)mark_mark;
	else if (mark_mark < 0) KBIT_SET(rq->sink->data.mark, -mark_mark);
}

KRegContentMark::KRegContentMark()
	: buffer(kDefaultFilterBuffer) {}
KMark* KRegContentMark::new_instance() { return new KRegContentMark; }
const char* KRegContentMark::get_module() const { return "content"; }
uint32_t KRegContentMark::process(KHttpRequest*, KHttpObject*, KSafeSource&) {
	return KF_STATUS_REQ_FALSE;
}

uint32_t KRegContentMark::process_with_chain(KHttpRequest* rq, KHttpObject* obj,
	KSafeSource&, kgl_jump_type action) {
	KSharedObj<KRegContentState> st = state;
	if (!st) return KF_STATUS_REQ_FALSE;
	char* response_charset = obj ? obj->getCharset() : nullptr;
	KReg* reg = st->select_reg(response_charset);
	if (response_charset) xfree(response_charset);
	if (!reg) return KF_STATUS_REQ_FALSE;
	register_body_filter(rq, new RegContentFilter(rq, std::move(st), reg, action));
	return KF_STATUS_REQ_TRUE | KF_STATUS_REQ_DEFERRED;
}

void KRegContentMark::get_display(KWStream& s) {
	s << expression << " charset:" << charset;
}

void KRegContentMark::get_html(KWStream& s) {
	s << "content:<input type=text size=80 name='content' value='" << expression
	  << "'><br>charset:<input type=text size=10 name='charset' value='" << charset
	  << "'> buffer:<input type=text size=8 name='buffer' value='" << buffer << "'>";
}

void KRegContentMark::parse_config(const khttpd::KXmlNodeBody* xml) {
	auto attr = xml->attr();
	expression = attr["content"];
	if (expression.empty()) expression = xml->get_text();
	charset = attr["charset"];
	buffer = parse_filter_buffer(attr["buffer"].c_str());
	KSharedObj<KRegContentState> st(new KRegContentState);
	st->buffer = buffer;
	st->utf8_valid = st->utf8_reg.setModel(expression.c_str(), 0);
	if (!charset.empty() && strcasecmp(charset.c_str(), "utf-8") != 0) {
		char* converted = utf82charset(expression.c_str(), expression.size(), charset.c_str());
		if (converted) {
			st->local_valid = st->local_reg.setModel(converted, 0);
			free(converted);
		}
	}
	state = std::move(st);
}

KReg* KRegContentState::select_reg(const char* response_charset) {
	if (local_valid && (!response_charset || strcasecmp(response_charset, "utf-8") != 0)) {
		return &local_reg;
	}
	return utf8_valid ? &utf8_reg : nullptr;
}

KReplaceUrlMark::KReplaceUrlMark()
	: nc(true), location(true) {}
KMark* KReplaceUrlMark::new_instance() { return new KReplaceUrlMark; }
const char* KReplaceUrlMark::get_module() const { return "replace_url"; }

uint32_t KReplaceUrlMark::process(KHttpRequest* rq, KHttpObject* obj, KSafeSource&) {
	KSharedObj<KReplaceUrlState> st = state;
	if (!st || !st->valid) return KF_STATUS_REQ_FALSE;
	if (location && obj && obj->data->i.status_code >= 300 && obj->data->i.status_code < 400) {
		for (KHttpHeader* header = obj->data->headers; header; header = header->next) {
			if (!kgl_is_attr(header, _KS("Location"))) continue;
			auto* value = st->replace(rq, header->buf + header->val_offset, header->val_len);
			if (value) {
				replace_header_value(header, *value);
				delete value;
			}
			break;
		}
	}
	register_body_filter(rq, new ReplaceUrlFilter(rq, std::move(st)));
	return KF_STATUS_REQ_TRUE;
}

void KReplaceUrlMark::get_display(KWStream& s) {
	s << src_text << "==>" << dst;
	if (location) s << " [location]";
}

void KReplaceUrlMark::get_html(KWStream& s) {
	s << "src:<input name='src' value='" << src_text
	  << "'><input type=checkbox name='nc' value='1' " << (nc ? "checked" : "")
	  << ">no case<br>dst:<input name='dst' value='" << dst
	  << "'><input type=checkbox name='location' value='1' "
	  << (location ? "checked" : "") << ">location";
}

void KReplaceUrlMark::parse_config(const khttpd::KXmlNodeBody* xml) {
	auto attr = xml->attr();
	src_text = attr["src"];
	dst = attr["dst"];
	nc = attr["nc"].empty() || attr["nc"] == "1";
	location = attr["location"].empty() || attr["location"] == "1";
	KSharedObj<KReplaceUrlState> st(new KReplaceUrlState);
	st->dst = dst;
	st->valid = st->src.setModel(src_text.c_str(), nc ? KGL_PCRE_CASELESS : 0);
	state = std::move(st);
}

KStringStream* KReplaceUrlState::replace(KHttpRequest* rq, const char* url, int length) {
	KRegSubString* substring = src.matchSubString(url, length, 0);
	if (!substring) return nullptr;
	auto* result = KRewriteMarkEx::getString(nullptr, dst.c_str(), rq, nullptr, substring);
	delete substring;
	return result;
}

KFixHeaderMark::KFixHeaderMark() = default;
KMark* KFixHeaderMark::new_instance() { return new KFixHeaderMark; }
const char* KFixHeaderMark::get_module() const { return "fix_header"; }

uint32_t KFixHeaderMark::process(KHttpRequest* rq, KHttpObject*, KSafeSource&) {
	kgl_request_range* range = rq->get_range();
	if (!range || range->from <= 0 || header.empty()) return KF_STATUS_REQ_FALSE;
	register_body_filter(rq, new FixHeaderFilter(header));
	return KF_STATUS_REQ_TRUE;
}

void KFixHeaderMark::get_display(KWStream& s) {
	for (size_t i = 0; i < header.size(); ++i) {
		unsigned char c = (unsigned char)header[i];
		char tmp[4];
		snprintf(tmp, sizeof(tmp), "%%%02X", c);
		s << tmp;
	}
}

void KFixHeaderMark::get_html(KWStream& s) {
	s << "header(URL encoded):<textarea name='header' rows=1>";
	get_display(s);
	s << "</textarea>";
}

void KFixHeaderMark::parse_config(const khttpd::KXmlNodeBody* xml) {
	std::string decoded = xml->attr()["header"].c_str();
	if (!decoded.empty()) {
		int length = url_decode(&decoded[0], (int)decoded.size(), nullptr, false);
		if (length >= 0) decoded.resize((size_t)length);
	}
	header = KString(decoded.data(), decoded.size());
}
