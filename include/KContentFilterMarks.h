#ifndef KCONTENTFILTERMARKS_H
#define KCONTENTFILTERMARKS_H

#include "KMark.h"
#include "KReg.h"

/* what a replace_content body filter uses, fixed once built */
class KReplaceContentState final : public KMarkState {
public:
	int match(const char* data, int length, kgl_pcre_match_data* match_data);
	KStringStream* make_replacement(KHttpRequest* rq, const char* data,
		kgl_pcre_match_data* match_data, int matched) const;
	bool should_process(KHttpRequest* rq) const;
	void update_mark(KHttpRequest* rq) const;

	KReg content;
	KString replace;
	int mark_acl = 0;
	int mark_mark = 0;
	bool replaced_stop = false;
	size_t buffer = 0;
	bool valid = false;
};

class KReplaceContentMark final : public KMark {
public:
	KReplaceContentMark();
	KMark* new_instance() override;
	const char* get_module() const override;
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	void get_display(KWStream& s) override;
	void get_html(KWStream& s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;

private:
	KSharedObj<KReplaceContentState> state;
	KString content_text;
	KString replace_text;
	KString charset;
	int mark_acl;
	int mark_mark;
	bool nc;
	bool replaced_stop;
	size_t buffer;
};

/* what a content body filter uses, fixed once built */
class KRegContentState final : public KMarkState {
public:
	KReg* select_reg(const char* response_charset);

	KReg utf8_reg;
	KReg local_reg;
	size_t buffer = 0;
	bool utf8_valid = false;
	bool local_valid = false;
};

class KRegContentMark final : public KMark {
public:
	KRegContentMark();
	KMark* new_instance() override;
	const char* get_module() const override;
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	uint32_t process_with_chain(KHttpRequest* rq, KHttpObject* obj,
		KSafeSource& fo, kgl_jump_type chain_jump_type) override;
	void get_display(KWStream& s) override;
	void get_html(KWStream& s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;

private:
	KSharedObj<KRegContentState> state;
	KString expression;
	KString charset;
	size_t buffer;
};

/* what a replace_url body filter uses, fixed once built */
class KReplaceUrlState final : public KMarkState {
public:
	KStringStream* replace(KHttpRequest* rq, const char* url, int length);

	KReg src;
	KString dst;
	bool valid = false;
};

class KReplaceUrlMark final : public KMark {
public:
	KReplaceUrlMark();
	KMark* new_instance() override;
	const char* get_module() const override;
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	void get_display(KWStream& s) override;
	void get_html(KWStream& s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;

private:
	KSharedObj<KReplaceUrlState> state;
	KString src_text;
	KString dst;
	bool nc;
	bool location;
};

class KFixHeaderMark final : public KMark {
public:
	KFixHeaderMark();
	KMark* new_instance() override;
	const char* get_module() const override;
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	void get_display(KWStream& s) override;
	void get_html(KWStream& s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;

private:
	KString header;
};

#endif
