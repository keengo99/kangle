#include <string.h>
#include "KConfigTree.h"
#include "kmalloc.h"
#include "KConfig.h"
#include "kfiber.h"
#include "KDefer.h"
#include "KXmlDocument.h"
#include "klog.h"
#include "KHttpLib.h"
#include "directory.h"
#include "KFileName.h"
#include "KFileStream.h"
#include "KConfig.h"
#include "KHttpLib.h"
#include "KFiberLocker.h"
#include "KXmlException.h"
#include <errno.h>
#define ASSERT_CONFIG_IS_LOCKED() kassert(kfiber_mutex_get_worker(locker)>0)
namespace kconfig {
	bool is_first_config = true;
	bool need_reboot_flag = false;

	kgl_ref_str_t default_file_name{ _KS("default"),1 };
	static KConfigTree events(nullptr, _KS("config"));
	static uint32_t cur_know_qname_id = 100;
	static kfiber_mutex* locker = nullptr;
	kgl_refs_string KConfigTree::skip_vary{ (char*)"",0,1 };
	KMap<kgl_ref_str_t, khttpd::KXmlKey> qname_config;
	KMap<kgl_ref_str_t, KConfigFile> config_files;
	static on_begin_parse_f begin_parse_cb = nullptr;
	static KConfigFileSourceDriver* sources[static_cast<int>(KConfigFileSource::Size)] = { 0 };
	KConfigFile* find_file(const kgl_ref_str_t& name);
	khttpd::KXmlNode* insert_child(khttpd::KXmlNodeBody* node, const char* name, size_t len);
	KConfigFileSourceDriver* find_source_driver(KConfigFileSource s) {
		return sources[static_cast<int>(s)];
	}
	void register_source_driver(KConfigFileSource s, KConfigFileSourceDriver* dr) {
		sources[static_cast<int>(s)] = dr;
	}
	static KString get_default_config_filename() {
#ifdef KANGLE_ETC_DIR
		KString config_dir = KANGLE_ETC_DIR;
#else
		KString config_dir = conf.path + "/etc";
#endif
		return config_dir + CONFIG_FILE;
	}
	/* A vhost access file is written by the site user: it must not read
	 * kangle's environment through %{env:NAME}. */
	class KVhAccessDynamicString : public KExtConfigDynamicString
	{
	public:
		KVhAccessDynamicString(const char* file) : KExtConfigDynamicString(file) {
		}
		bool buildValue(const char* name, KWStream* s) override {
			return strncasecmp(name, "env:", 4) == 0;
		}
	};
	class KDefaultConfigFileDriver : public KConfigFileSourceDriver
	{
	public:
		khttpd::KSafeXmlNode load(KConfigFile* file) override {
			return load_file(file, false);
		}
	protected:
		khttpd::KSafeXmlNode load_file(KConfigFile* file, bool site_file) {
			auto fp = kfiber_file_open(file->get_filename()->data, fileRead, KFILE_ASYNC);
			if (!fp) {
				klog(KLOG_ERR, "ERROR! cann't open config file [%s] for read\n", file->get_filename()->data);
				return nullptr;
			}
			defer(kfiber_file_close(fp));
			int64_t file_size = kfiber_file_size(fp);
			if (file_size < 0 || file_size > max_file_size) {
				klog(KLOG_ERR, "config file [%s %s] has invalid size=[" INT64_FORMAT "]\n",
					file->get_name()->data, file->get_filename()->data, file_size);
				return nullptr;
			}
			int size = (int)file_size;
			kgl_auto_cstr buf((char*)malloc(size + 1));
			if (!buf) {
				return nullptr;
			}
			buf.get()[size] = '\0';
			if (!kfiber_file_read_full(fp, buf.get(), &size)) {
				return nullptr;
			}
			file->set_remove_flag(false);
			char* hot = buf.get();
			while (*hot && isspace((unsigned char)*hot)) {
				hot++;
			}
			uint32_t id = default_file_index;
			if (strncmp(hot, "<!--#", 5) == 0) {
				hot += 5;
				if (strncmp(hot, "stop", 4) == 0) {
					/* This extension has no configuration. */
					id = 0;
					file->set_index(0);
					return nullptr;
				} else if (strncmp(hot, "start", 5) == 0) {
					hot += 5;
					while (*hot && isspace((unsigned char)*hot)) {
						hot++;
					}
					if (*hot) {
						id = atoi(hot);
					}
				}
			}
			if (file->get_index() == 0) {
				/* only first time can set_index */
				file->set_index(id);
			}
			std::unique_ptr<KExtConfigDynamicString> ds(site_file ?
				new KVhAccessDynamicString(file->get_filename()->data) :
				new KExtConfigDynamicString(file->get_filename()->data));
			ds->dimModel = false;
			ds->blockModel = false;
			ds->envChar = '%';
			auto content = ds->parseDirect(buf.get());
			return parse_xml(content.get());
		}
	public:
		KFileModified get_last_modified(KConfigFile* file) override {
			return file->get_filename()->data;
		}
		bool save(KConfigFile* f, khttpd::KXmlNode* nodes) override {
			KString tmpfile = f->get_filename()->data;
			tmpfile += KString(_KS(".tmp"));
			/* A vhost access file sits in the site's own directory. Never
			 * write through whatever the site user left at the temp name. */
			unlink(tmpfile.c_str());
			auto fp = kfiber_file_open(tmpfile.c_str(), fileWrite, KFILE_NOFOLLOW);
			if (!fp) {
				return false;
			}
			KAsyncFileStream file(fp);
			if (!f->is_default()) {
				file << "<!--#start " << f->get_index() << " -->\n";
			}
			KGL_RESULT result = KGL_OK;
			if (nodes) {
				result = nodes->write(&file);
			}
			if (result == KGL_OK) {
				file.write_all(_KS("\n"));
				result = file.write_all(CONFIG_FILE_SIGN);
			}
			result = file.write_end(result);
			if (result != KGL_OK) {
				unlink(tmpfile.c_str());
				return false;
			}
			auto filename = f->get_filename();
			KString default_file;
			if (f->is_default()) {
				default_file = get_default_config_filename();
				filename = default_file.data();
			}
			if (0 != unlink(filename->data) && errno != ENOENT) {
				unlink(tmpfile.c_str());
				return false;
			}
			if (0 != rename(tmpfile.c_str(), filename->data)) {
				return false;
			}
			//update last_modified
			f->set_last_modified(filename->data);
			return true;
		}
	};
	struct kgl_ext_config_context
	{
		KString dir;
		KString prefix;
		KConfigFileScanInfo* info;
	};

	void load_config_file(const KString& name, KFileName* file, KConfigFileScanInfo* provider, bool is_default = false) {
		KString filename(file->getName());
		provider->new_file(name.data(), filename.data(), KFileModified(file->getLastModified(), file->get_file_size()), is_default);
	}
	bool load_config_from_name(const KString& name, KFileName& filename) {
		KStringBuf path;
		KStringBuf s;
		if (strstr(name.c_str(), "..") != NULL) {
			return false;
		}
		path << conf.path;
		size_t pos = 0;
		if (strncmp(name.c_str(), _KS("ext|")) == 0) {
			path << "ext" << PATH_SPLIT_CHAR;
			pos = 4;

		} else if (strncmp(name.c_str(), _KS("vh_d|")) == 0) {
			path << "etc" << PATH_SPLIT_CHAR << "vh.d" << PATH_SPLIT_CHAR;
			pos = 5;
		} else {
			return false;
		}
		if (name[name.size() - 1] == '_') {
			s << name.substr(pos, name.size() - pos - 1);
		} else if (name[name.size() - 1] == '|') {
			s << name.substr(pos, name.size() - pos - 1) << PATH_SPLIT_CHAR << "config.xml"_CS;
		} else {
			return false;
		}
		return filename.setName(path.c_str(), s.c_str(), FOLLOW_LINK_ALL);
	}
	int handle_ext_config_dir(const char* file, void* param) {
		kgl_ext_config_context* ctx = (kgl_ext_config_context*)param;
		KFileName configFile;
		KStringBuf name;
		if (!configFile.setName(ctx->dir.c_str(), file, FOLLOW_LINK_ALL)) {
			return 0;
		}
		if (!ctx->prefix.empty()) {
			name << ctx->prefix << "|"_CS;
		}
		name << file;
		if (configFile.isDirectory()) {
			name << "|"_CS;
			KFileName dirConfigFile;
			if (!dirConfigFile.setName(configFile.getName(), "config.xml", FOLLOW_LINK_ALL)) {
				return 0;
			}
			load_config_file(name.str(), &dirConfigFile, ctx->info);
			return 0;
		}
		name << "_"_CS;
		load_config_file(name.str(), &configFile, ctx->info);
		return 0;
	}
	class KSystemConfigFileDriver : public KDefaultConfigFileDriver
	{
	public:
		bool enable_scan() override {
			return true;
		};
		void scan(KConfigFileScanInfo* info) override {
			kgl_ext_config_context ctx;
			ctx.info = info;
#ifdef KANGLE_ETC_DIR
			KString config_dir = KANGLE_ETC_DIR;
#else
			KString config_dir = conf.path + "/etc";
#endif
			KFileName file;
			if (!file.setName(config_dir.c_str(), CONFIG_FILE, FOLLOW_LINK_ALL)) {
				file.setName(config_dir.c_str(), CONFIG_DEFAULT_FILE, FOLLOW_LINK_ALL);
			}
			KStringBuf default_name;
			default_name << default_file_name;
			load_config_file(default_name.str(), &file, info, true);
			ctx.prefix = "ext";
#ifdef KANGLE_EXT_DIR
			ctx.dir = KANGLE_EXT_DIR;
#else
			ctx.dir = conf.path + "/ext";
#endif		
			list_dir(ctx.dir.c_str(), handle_ext_config_dir, (void*)&ctx);
			ctx.dir = conf.path + "etc/vh.d/";
			ctx.prefix = "vh_d";
			list_dir(ctx.dir.c_str(), handle_ext_config_dir, (void*)&ctx);
		}
	};
	class KVhAccessConfigFileDriver : public KDefaultConfigFileDriver
	{
	public:
		khttpd::KSafeXmlNode load(KConfigFile* file) override {
			auto xml = load_file(file, true);
			if (xml) {
				/* The file is mounted under the vhost, so an <alias>, <map>,
				 * <error> or <env> in it would change the vhost itself, e.g.
				 * alias / to read any file as root. The site may only add its
				 * own access rules. */
				auto root = xml->get_first();
				std::vector<khttpd::KXmlNode*> drop;
				for (auto child : root->childs) {
					if (!child->is_tag(_KS("request")) && !child->is_tag(_KS("response"))) {
						drop.push_back(child);
					}
				}
				for (auto child : drop) {
					klog(KLOG_ERR, "ignore <%s> in vhost access file [%s], only <request> and <response> are allowed\n",
						child->get_tag().c_str(), file->get_filename()->data);
					root->remove_child(&child->key);
				}
			}
			if (!xml) {
				// A vhost may enable access.xml before the file exists. Treat that
				// state as an empty config so the first WHM rule can create the file.
				// Invalid existing XML must still fail instead of being overwritten.
				KFileName filename;
				if (filename.setName(file->get_filename()->data)) {
					return nullptr;
				}
				xml = new_xml("config"_CS);
			}

			// The legacy access loader always supplied an empty BEGIN table for
			// both directions. EasyPanel relies on that invariant when toggling
			// bandwidth, anti-CC and similar features: it inserts a jump into
			// BEGIN without creating the table itself.
			for (const char* direction : { "request", "response" }) {
				auto direction_xml = find_child(xml->get_first(), direction, strlen(direction));
				khttpd::KSafeXmlNode new_direction;
				if (!direction_xml) {
					new_direction = new_xml(direction, strlen(direction));
					xml->append(new_direction.get());
					direction_xml = new_direction.get();
				}
				bool found = false;
				for (auto child : direction_xml->get_first()->childs) {
					if (!child->is_tag(_KS("table"))) {
						continue;
					}
					for (uint32_t index = 0;; ++index) {
						auto body = child->get_body(index);
						if (!body) {
							break;
						}
						if (body->attributes["name"] == "BEGIN") {
							found = true;
							break;
						}
					}
					if (found) {
						break;
					}
				}
				if (!found) {
					auto table = new_xml(_KS("table"), _KS("BEGIN"));
					direction_xml->append(table.get());
				}
			}
			return xml;
		}
		bool enable_scan() override {
			return false;
		}
	};
	KConfigTree::~KConfigTree() noexcept {
		clean();
		if (node) {
			delete node;
		}
	}

	bool update_xml_node(khttpd::KXmlNodeBody* nodes, const char* name, size_t size, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type) {
		size_t name_len;
		while (size > 1 && *name == '/') {
			name++;
			size--;
		}
		if (size <= 0 || *name == '/') {
			switch (ev_type) {
			case kconfig::EvNew:
				nodes->add(xml, index);
				return true;
			case kconfig::EvRemove:
				return nodes->update(&xml->key, index, nullptr);
			default:
				if (!KBIT_TEST(ev_type, kconfig::EvUpdate)) {
					return false;
				}
				return nodes->update(&xml->key, index, xml, KBIT_TEST(ev_type, FlagCopyChilds), KBIT_TEST(ev_type, FlagCreate));
			}
		}
		auto path = (const char*)memchr(name, '/', size);
		if (path) {
			name_len = path - name;
		} else {
			name_len = size;
		}
		uint32_t nodes_index = 0;
		auto index_str = (const char*)memchr(name, '#', name_len);
		size_t name_len2 = name_len;
		if (index_str) {
			name_len2 = index_str - name;
			nodes_index = atoi(index_str + 1);
		}
		auto child_nodes = KBIT_TEST(ev_type, FlagCreateParent) ? insert_child(nodes, name, name_len2) : find_child(nodes, name, name_len2);
		if (!child_nodes) {
			return false;
		}
		nodes = child_nodes->get_body(nodes_index);
		if (!nodes) {
			return false;
		}
		name += name_len;
		size -= name_len;
		return update_xml_node(nodes, name, size, index, xml, ev_type);
	}
	static bool move_xml_node_down(khttpd::KXmlNodeBody* nodes, const char* path, size_t size, uint32_t index) {
		while (size && *path == '/') {
			++path;
			--size;
		}
		if (!size) {
			return false;
		}
		const char* slash = (const char*)memchr(path, '/', size);
		size_t segment_len = slash ? static_cast<size_t>(slash - path) : size;
		const char* hash = (const char*)memchr(path, '#', segment_len);
		size_t key_len = hash ? static_cast<size_t>(hash - path) : segment_len;
		auto child = find_child(nodes, path, key_len);
		if (!child) {
			return false;
		}
		if (slash) {
			uint32_t body_index = hash ? static_cast<uint32_t>(atoi(hash + 1)) : 0;
			auto body = child->get_body(body_index);
			return body && move_xml_node_down(body, slash + 1, size - segment_len - 1, index);
		}
		auto first = child->get_body_address(index);
		auto second = index == UINT32_MAX ? nullptr : child->get_body_address(index + 1);
		if (!first || !second) {
			return false;
		}
		auto old_first = *first;
		auto old_second = *second;
		for (auto& entry : nodes->child_order) {
			if (entry.node == child) {
				if (entry.body == old_first) {
					entry.body = old_second;
				} else if (entry.body == old_second) {
					entry.body = old_first;
				}
			}
		}
		std::swap(*first, *second);
		return true;
	}
	static bool rename_xml_table(khttpd::KXmlNodeBody* nodes, const char* path, size_t size, const KString& new_name) {
		while (size && *path == '/') {
			++path;
			--size;
		}
		if (!size) {
			return false;
		}
		const char* slash = (const char*)memchr(path, '/', size);
		size_t segment_len = slash ? static_cast<size_t>(slash - path) : size;
		const char* hash = (const char*)memchr(path, '#', segment_len);
		size_t key_len = hash ? static_cast<size_t>(hash - path) : segment_len;
		auto child = find_child(nodes, path, key_len);
		if (!child) {
			return false;
		}
		if (slash) {
			uint32_t body_index = hash ? static_cast<uint32_t>(atoi(hash + 1)) : 0;
			auto body = child->get_body(body_index);
			return body && rename_xml_table(body, slash + 1, size - segment_len - 1, new_name);
		}
		if (!child->is_tag(_KS("table")) || !child->key.vary) {
			return false;
		}
		auto replacement = child->clone();
		kstring_release(replacement->key.vary);
		replacement->key.vary = kstring_from2(new_name.c_str(), new_name.size());
		if (nodes->childs.find(&replacement->key)) {
			return false;
		}
		for (uint32_t i = 0; i < replacement->get_body_count(); ++i) {
			auto& attrs = replacement->get_body(i)->attributes;
			attrs.erase("name"_CS);
			attrs.emplace("name"_CS, new_name);
		}
		auto old_entry = nodes->childs.find(&child->key);
		if (!old_entry) {
			return false;
		}
		nodes->childs.erase(old_entry);
		int inserted = 0;
		auto new_entry = nodes->childs.insert(&replacement->key, &inserted);
		if (!inserted) {
			return false;
		}
		new_entry->value(replacement->add_ref());
		for (auto& entry : nodes->child_order) {
			if (entry.node != child) {
				continue;
			}
			for (uint32_t i = 0; i < child->get_body_count(); ++i) {
				if (entry.body == child->get_body(i)) {
					entry.node = replacement.get();
					entry.body = replacement->get_body(i);
					break;
				}
			}
		}
		child->release();
		return true;
	}
	static bool rewrite_table_action(KXmlAttribute& attributes,
		const KString& old_name, const KString& new_name) {
		KStringBuf old_action;
		old_action << "table:" << old_name;
		if (attributes["action"] != old_action.str()) {
			return false;
		}
		KStringBuf new_action;
		new_action << "table:" << new_name;
		attributes.erase("action"_CS);
		attributes.emplace("action"_CS, new_action.str());
		return true;
	}
	static bool rewrite_access_table(khttpd::KXmlNodeBody* access,
		const KString& old_name, const KString& new_name,
		bool rename_definition, bool& found_definition) {
		bool changed = rewrite_table_action(access->attributes, old_name, new_name);
		for (auto table : access->childs) {
			if (!table->is_tag(_KS("table"))) {
				continue;
			}
			for (uint32_t table_index = 0;; ++table_index) {
				auto table_body = table->get_body(table_index);
				if (!table_body) {
					break;
				}
				auto chains = find_child(table_body, _KS("chain"));
				if (!chains) {
					continue;
				}
				for (uint32_t chain_index = 0;; ++chain_index) {
					auto chain = chains->get_body(chain_index);
					if (!chain) {
						break;
					}
					changed |= rewrite_table_action(chain->attributes, old_name, new_name);
				}
			}
		}
		if (!rename_definition) {
			return changed;
		}
		KStringBuf table_path;
		table_path << "table@" << old_name;
		if (rename_xml_table(access, table_path.c_str(), table_path.size(), new_name)) {
			found_definition = true;
			changed = true;
		}
		return changed;
	}
	static khttpd::KXmlNode* find_xml_path(khttpd::KXmlNodeBody* nodes,
		const char* path, size_t size) {
		while (size && *path == '/') {
			++path;
			--size;
		}
		if (!size) {
			return nullptr;
		}
		const char* slash = (const char*)memchr(path, '/', size);
		size_t segment_len = slash ? static_cast<size_t>(slash - path) : size;
		const char* hash = (const char*)memchr(path, '#', segment_len);
		size_t key_len = hash ? static_cast<size_t>(hash - path) : segment_len;
		auto child = find_child(nodes, path, key_len);
		if (!child || !slash) {
			return child;
		}
		uint32_t body_index = hash ? static_cast<uint32_t>(atoi(hash + 1)) : 0;
		auto body = child->get_body(body_index);
		return body ? find_xml_path(body, slash + 1, size - segment_len - 1) : nullptr;
	}
	static bool build_relative_config_path(KConfigFile* file, KConfigTree* tree, KString& path) {
		std::vector<KConfigTree*> parts;
		auto root = file->get_ev();
		for (auto current = tree; current && current != root; current = current->parent) {
			parts.push_back(current);
		}
		auto current = tree;
		while (current && current != root) {
			current = current->parent;
		}
		if (current != root) {
			return false;
		}
		KStringBuf result;
		for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
			auto key = &(*it)->key;
			if (!key->tag || key->tag->len == 0) {
				continue;
			}
			if (!result.empty()) {
				result << "/";
			}
			result.write_all(key->tag->data, key->tag->len);
			if (key->vary && key->vary->len > 0) {
				result << "@";
				result.write_all(key->vary->data, key->vary->len);
			}
		}
		path = result.str();
		return !path.empty();
	}
	iterator_ret config_tree_clean_iterator(void* data, void* argv) {
		KConfigTree* rn = (KConfigTree*)data;
		delete rn;
		return iterator_remove_continue;
	}
	void KConfigTree::check_at_once() {
		if (is_at_once() && !node) {
			ls->on_config_event(this, nullptr);
		}

		child.iterator([](void* data, void* arg) {
			KConfigTree* rn = (KConfigTree*)data;
			rn->check_at_once();
			return iterator_continue;
			}, nullptr);

	}
	void KConfigTree::clean() {
		child.iterator(config_tree_clean_iterator, NULL);
	}
	KConfigTree* KConfigTree::find(const char** name, size_t* size) {
		size_t name_len;
		while (*size > 1 && (**name == '/')) {
			(*name)++;
			(*size)--;
		}
		if (*size <= 0 || **name == '/') {
			return this;
		}
		auto path = (char*)memchr(*name, '/', *size);
		//auto path = kgl_mempbrk(*name, (int)*size, _KS("/@"));
		if (path) {
			name_len = path - (*name);
		} else {
			name_len = *size;
		}
		khttpd::KXmlKey key(*name, name_len);
		auto node = child.find(&key);
		if (!node) {
			return this;
		}
		if (key.vary && node->value()->is_skip_vary()) {
			node = node->value()->child.find(&key);
		}
		if (!node) {
			return this;
		}
		*name += name_len;
		*size -= name_len;
		return node->value()->find(name, size);
	}
	KConfigListen* KConfigTree::unbind() {
		auto ret = this->ls;
		this->ls = nullptr;
		return ret;
	}
	bool KConfigTree::bind(KConfigListen* ls) {
		if (this->ls) {
			return false;
		}
		this->ls = ls;
		if (ls->config_flag() & ev_skip_vary) {
			if (key.vary) {
				kstring_release(key.vary);
			}
			key.vary = kstring_refs(&skip_vary);
		}
		return true;
	}
	KConfigTree* KConfigTree::add(const char* name, size_t size, KConfigListen* ls) {
		size_t name_len;
		while (size > 1 && *name == '/') {
			name++;
			size--;
		}
		if (size <= 0 || *name == '/') {
			if (bind(ls)) {
				return this;
			}
			return nullptr;
		}
		auto path = (const char*)memchr(name, '/', size);
		if (path) {
			name_len = path - name;
		} else {
			name_len = size;
		}
		khttpd::KXmlKey key(name, name_len);

		int new_flag = 0;
		auto node = child.insert(&key, &new_flag);
		KConfigTree* rn;
		if (new_flag) {
			rn = new KConfigTree(this, name, name_len);
			node->value(rn);
		} else {
			rn = node->value();
		}
		return rn->add(name + name_len, size - name_len, ls);
	}
	KConfigListen* KConfigTree::remove(const char* name, size_t size) {
		size_t name_len;
		while (size > 1 && *name == '/') {
			name++;
			size--;
		}
		if (size <= 0 || *name == '/') {
			return unbind();
		}
		auto path = (const char*)memchr(name, '/', size);
		if (path) {
			name_len = path - name;
		} else {
			name_len = size;
		}
		khttpd::KXmlKey key(name, name_len);
		auto node = child.find(&key);
		if (!node) {
			return nullptr;
		}
		KConfigTree* rn = node->value();
		auto ret = rn->remove(name + name_len, size - name_len);
		if (!ret) {
			return nullptr;
		}
		if (rn->empty()) {
			delete rn;
			child.erase(node);
		}
		return ret;
	}

	bool KConfigTree::notice(KConfigTree* ev_tree, KConfigFile* file, khttpd::KXmlNode* xml, KConfigEventType ev_type, kgl_config_diff* diff) {
		assert(xml);
		KConfigEventNode* file_node;
		KConfigEventNode* last_node;
		KConfigEvent ev;
		memset(&ev, 0, sizeof(ev));
		ev.diff = diff;
		ev.file = file;
		ev.type = ev_type;

		if (ev_type != EvNone) {
			klog(KLOG_DEBUG, "config event %s%s%s [%d] ev_tree=[%p] xml=[%p]\n",
				xml->key.tag->data,
				xml->key.vary ? "@" : "",
				xml->key.vary ? xml->key.vary->data : "",
				ev_type,
				ev_tree,
				xml);
		}

		if (ev_type != EvNew) {
			file_node = node;
			last_node = nullptr;
			while (file_node && file_node->file != file) {
				last_node = file_node;
				file_node = file_node->next;
				continue;
			}
			assert(file_node);
			if (!file_node) {
				return true;
			}
			if (ev_type == EvRemove) {
				//remove
				ev.old_xml = xml;
				defer(delete file_node);
				if (!last_node) {
					node = file_node->next;
					if (node && !ev_tree->is_merge()) {
						ev.file = node->file;
						ev.new_xml = node->xml;
						ev.diff->new_to = ev.new_xml->get_body_count();
						ev.type = EvUpdate;
					}
				} else {
					last_node->next = file_node->next;
				}
				if (!last_node || ev_tree->is_merge()) {
					if (this != ev_tree) {
						ev.type |= EvSubDir;
					}
					return ev_tree->ls->on_config_event(this, &ev);
				}
				return true;
			}
			//update
			auto old_xml = file_node->update(xml);
			ev.old_xml = old_xml.get();
			if (ev_type == EvNone) {
				return true;
			}
			ev.new_xml = xml;
			assert(ev_type == EvUpdate);
			if (!last_node || ev_tree->is_merge()) {
				if (this != ev_tree) {
					ev.type |= EvSubDir;
				}
				return ev_tree->ls->on_config_event(this, &ev);
			}
			return true;
		}
		last_node = node;
		//new
		ev.new_xml = xml;
		assert(ev_type == EvNew);
		KConfigEventNode* prev_node = nullptr;
		while (last_node && file->get_index() > last_node->file->get_index()) {
			prev_node = last_node;
			last_node = last_node->next;
			continue;
		}
		file_node = new KConfigEventNode(file, xml);
		if (!prev_node) {
			file_node->next = node;
			if (node && !ev_tree->is_merge()) {
				ev.type = EvUpdate;
				ev.old_xml = node->xml;
				ev.diff->old_to = ev.old_xml->get_body_count();
			}
			node = file_node;
		} else {
			file_node->next = prev_node->next;
			prev_node->next = file_node;
		}
		if (!prev_node || ev_tree->is_merge()) {
			if (this != ev_tree) {
				ev.type |= EvSubDir;
			}
			return ev_tree->ls->on_config_event(this, &ev);
		}
		return true;
	}
	bool KConfigTree::notice(KConfigFile* file, khttpd::KXmlNode* xml, KConfigEventType ev_type, kgl_config_diff* diff) {
		try {
			if (is_self()) {
				return notice(this, file, xml, ev_type, diff);
			}
			if (parent->is_subdir()) {
				return notice(parent, file, xml, ev_type, diff);
			}
		} catch (std::exception& a) {
			klog(KLOG_ERR, "config event exception happend [%s]\n", a.what());
		} catch (...) {
			klog(KLOG_ERR, "config event unknow exception happend\n");
		}
		return false;
	}
	void KConfigTree::remove_node(KMapNode<KConfigTree>* node) {
		delete node->value();
		child.erase(node);
	}
	KMapNode<KConfigTree>* KConfigTree::find_child(khttpd::KXmlKey* name) {
		static kgl_ref_str_t empty_tag{ nullptr ,0,1 };
		decltype(child.find<kgl_ref_str_t>(nullptr)) it;
		if (is_subdir()) {
			int new_flag;
			it = child.insert(name, &new_flag);
			if (new_flag) {
				it->value(new KConfigTree(this, is_skip_vary() ? &empty_tag : name->tag, name->vary, name->tag_id));
			}
		} else {
			it = child.find(name);
		}
		if (it && it->value()->is_skip_vary()) {
			return it->value()->find_child(name);
		}
		return it;
	}
	khttpd::KSafeXmlNode KConfigFile::load() {
		auto fs = find_source_driver(source);
		auto xml = fs->load(this);
		if (begin_parse_cb && xml) {
			if (!begin_parse_cb(this, xml.get())) {
				return nullptr;
			}
		}
		return xml;
	}
	bool KConfigFile::diff_body(KConfigTree* name, khttpd::KXmlNodeBody* o, khttpd::KXmlNodeBody* n, int* notice_count) {
		bool result = false;
		if (!o) {
			assert(n != nullptr);
			assert(name);
			for (auto node : n->childs) {
				if (diff(name, nullptr, node, notice_count)) {
					result = true;
				}
			}
			return result;
		}
		if (!n) {
			assert(o != nullptr);
			for (auto it = o->childs.rbegin(); it != o->childs.rend(); ++it) {
				if (diff(name, *it, nullptr, notice_count)) {
					result = true;
				}
			}
			return result;
		}
		for (auto node : n->childs) {
			auto it = o->childs.find(&node->key);
			if (it) {
				auto o_node = it->value();
				if (diff(name, o_node, node, notice_count)) {
					if (!name) {
						return true;
					}
					result = true;
				}
				continue;
			}
			if (diff(name, nullptr, node, notice_count)) {
				if (!name) {
					return true;
				}
				result = true;
			}
		};
		for (auto it = o->childs.rbegin(); it != o->childs.rend(); ++it) {
			auto node = *it;
			if (n->childs.find(&node->key)) {
				continue;
			}
			if (diff(name, node, nullptr, notice_count)) {
				if (!name) {
					return true;
				}
				result = true;
			}
		}
		if (result) {
			return result;
		}
		return !o->is_same(n);
	}
	bool KConfigFile::diff(KConfigTree* ev_node, khttpd::KXmlNode* o, khttpd::KXmlNode* n, int* notice_count) {
		KMapNode<KConfigTree>* child_node = nullptr;
		kgl_config_diff diff = { 0 };
		if (ev_node) {
			khttpd::KXmlKey* xml = o ? &o->key : &n->key;
			child_node = ev_node->find_child(xml);
			ev_node = child_node ? child_node->value() : nullptr;
		}
		if (!o) {
			assert(n != nullptr);
			if (ev_node) {
				diff.new_to = n->get_body_count();
				bool notice_result = ev_node->notice(this, n, EvNew, &diff);
				if (notice_result) {
					(*notice_count)++;
				}
				diff_body(ev_node, nullptr, n->get_first(), notice_count);
				return !notice_result;
			}
			return true;
		}
		if (!n) {
			if (ev_node) {
				diff_body(ev_node, o->get_first(), nullptr, notice_count);
				diff.old_to = o->get_body_count();
				bool notice_result = ev_node->notice(this, o, EvRemove, &diff);
				if (notice_result) {
					(*notice_count)++;
				}
				if (child_node) {
					assert(ev_node);
					assert(ev_node->parent);
					if (ev_node->empty()) {
						//assert(ev_node->parent->is_subdir());
						ev_node->parent->remove_node(child_node);
					}
				}
				return !notice_result;
			}
			return true;
		}
		bool is_diff = false;
		//from start
		for (;; ++diff.from) {
			auto o_body = o->get_body(diff.from);
			if (!o_body) {
				break;
			}
			auto n_body = n->get_body(diff.from);
			if (!n_body) {
				break;
			}
			if (diff_body(diff.from == 0 ? ev_node : nullptr, o_body, n_body, notice_count)) {
				is_diff = true;
				break;
			}
		}
		//from last
		diff.new_to = n->get_body_count();
		diff.old_to = o->get_body_count();
		if (diff.new_to != diff.old_to) {
			is_diff = true;
		}
		for (; (diff.new_to > diff.from + 1 && diff.old_to > diff.from + 1); --diff.old_to, --diff.new_to) {
			if (diff.old_to == 0 || diff.new_to == 0) {
				break;
			}
			auto o_body = o->get_body(diff.old_to - 1);
			auto n_body = n->get_body(diff.new_to - 1);
			if (diff_body(nullptr, o_body, n_body, notice_count)) {
				is_diff = true;
				break;
			}
		}
		if (ev_node) {
			assert(ev_node);
			if (ev_node->notice(this, n, is_diff ? EvUpdate : EvNone, &diff)) {
				if (is_diff) {
					(*notice_count)++;
				}
				return false;
			}
		}
		return is_diff;
	}
	khttpd::KSafeXmlNode parse_xml(char* buf) {
		khttpd::KXmlDocument document;
		document.set_qname_config(&qname_config);
		return document.parse(buf);
	}
	bool KConfigFile::reload(const char* str, size_t size) {
		char* buf = (char*)malloc(size + 1);
		if (!buf) {
			return false;
		}
		defer(xfree(buf));
		buf[size] = '\0';
		memcpy(buf, str, size);
		auto tree_node = parse_xml(buf);
		if (!tree_node) {
			return false;
		}
		update(std::move(tree_node));
		return true;
	}
#if 0
	uint16_t KConfigFile::load_index() {
		auto fp = kfiber_file_open(get_filename()->data, fileRead, KFILE_ASYNC);
		if (!fp) {
			return default_file_index;
		}
		defer(kfiber_file_close(fp));
		char buffer[512];
		auto size = (int)kfiber_file_size(fp);
		size = KGL_MIN(size, sizeof(buffer) - 1);
		buffer[size] = '\0';
		if (!kfiber_file_read_full(fp, buffer, &size)) {
			return default_file_index;
		}
		char* hot = buffer;
		while (*hot && isspace((unsigned char)*hot)) {
			hot++;
		}
		// The default configuration order is 50.
		uint32_t id = default_file_index;
		if (strncmp(hot, "<!--#", 5) == 0) {
			hot += 5;
			if (strncmp(hot, "stop", 4) == 0) {
				/* This extension has no configuration. */
				return 0;
			} else if (strncmp(hot, "start", 5) == 0) {
				hot += 6;
				id = atoi(hot);
			}
		}
		set_index(id);
		return id;
	}
#endif
	KConfigFileSourceDriver* KConfigFile::get_source_driver() const {
		return sources[static_cast<int>(source)];
	}
	bool KConfigFile::update(const char* name, size_t size, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type) {
		if (!this->nodes) {
			return false;
		}
		auto nodes = this->nodes->clone();
		if (!update_xml_node(nodes->get_first(), name, size, index, xml, ev_type)) {
			return false;
		}
		update(std::move(nodes));
		return true;
	}
	bool KConfigFile::move_down(const char* path, size_t size, uint32_t index) {
		if (!nodes) {
			return false;
		}
		auto updated = nodes->clone();
		if (!move_xml_node_down(updated->get_first(), path, size, index)) {
			return false;
		}
		update(std::move(updated));
		return true;
	}
	bool KConfigFile::rename_table(const char* path, size_t size, const KString& new_name) {
		if (!nodes) {
			return false;
		}
		auto updated = nodes->clone();
		if (!rename_xml_table(updated->get_first(), path, size, new_name)) {
			return false;
		}
		update(std::move(updated));
		return true;
	}
	bool KConfigFile::save() {
		if (!need_save) {
			return true;
		}
		auto fs = find_source_driver(this->source);
		if (!fs) {
			return false;
		}
		if (!fs->enable_save()) {
			return false;
		}
		if (fs->save(this, nodes.get())) {
			this->last_modified = fs->get_last_modified(this);
			return true;
		}
		return false;
	}
	void KConfigFile::update(khttpd::KSafeXmlNode new_nodes) {
		if (new_nodes == nodes) {
			need_save = 0;
			return;
		}
		int notice_count = 0;
		diff_body(ev, nodes ? nodes->get_first() : nullptr, new_nodes ? new_nodes->get_first() : nullptr, &notice_count);
		nodes = new_nodes;
		need_save = !!notice_count;
	}
	bool KConfigFile::reload(bool force) {
		auto fs = find_source_driver(source);
		KFileModified last_modified = fs->get_last_modified(this);
		if (!force) {
			if (last_modified == this->last_modified) {
				return true;
			}
		}
		auto pre_last_modified = this->last_modified;
		this->last_modified = last_modified;
		auto xml = load();
		if (!force) {
			/* after load last_modified may be changed */
			if (pre_last_modified == this->last_modified) {
				return true;
			}
		}
		if (!xml && !last_modified.empty() && get_index() != 0) {
			/* the file is there but did not load: keep its running config */
			klog(KLOG_ERR, "cann't load config file [%s %s], keep its running config\n", name->data, filename->data);
			this->last_modified = pre_last_modified;
			return true;
		}
		update(std::move(xml));
		return !last_modified.empty();
	}
	KConfigTree* remove_config_file(const kgl_ref_str_t* name) {
		ASSERT_CONFIG_IS_LOCKED();
		auto it = config_files.find(name);
		if (!it) {
			return nullptr;
		}
		auto file = it->value();
		auto tree = file->get_ev();
		config_files.erase(it);
		file->clear();
		file->release();
		return tree;
	}
	bool add_config_file(const kgl_ref_str_t* name, const kgl_ref_str_t* filename, KConfigTree* tree, KConfigFileSource source) {
		ASSERT_CONFIG_IS_LOCKED();
		int new_flag;
		auto it = config_files.insert(name, &new_flag);
		if (!new_flag) {
			return it->value()->reload(false);
		}
		KConfigFile* file = new KConfigFile(tree, name, filename, source);
		it->value(file);
		return file->reload(true);
	}
	KConfigTree* listen(const char* name, size_t size, KConfigListen* ls, KConfigTree* ev_tree) {
		return ev_tree->add(name, size, ls);
	}
	KConfigTree* listen(const char* name, size_t size, KConfigListen* ls) {
		return events.add(name, size, ls);
	}
	KConfigTree* find(const char** name, size_t* size) {
		return events.find(name, size);
	}
	KConfigTree* find(const char* name, size_t size) {
		auto tree = find(&name, &size);
		if (size == 0) {
			return tree;
		}
		return nullptr;
	}
	KConfigListen* remove_listen(const char* name, size_t size, KConfigTree* ev_tree) {
		return ev_tree->remove(name, size);
	}
	KConfigListen* remove_listen(const char* name, size_t size) {
		return events.remove(name, size);
	}
	class config_prepare_parse {
	public:
		config_prepare_parse(khttpd::KSafeXmlNode&& body, KSafeConfigFile&& cfg) : body{ body }, cfg{ cfg } {
		}
		khttpd::KSafeXmlNode body;
		KSafeConfigFile cfg;
	};
	class KConfigScanInfoProvider : public KConfigFileScanInfo
	{
	public:
		void new_file(const kgl_ref_str_t* name, const kgl_ref_str_t* filename, const KFileModified& last_modified, bool is_default) {
			int new_flag;
			if ((current_source == KConfigFileSource::System || current_source == KConfigFileSource::Vh) &&
				(filename->len < 5 || kgl_memcmp(filename->data + filename->len - 4, _KS(".xml")) != 0)) {
				klog(KLOG_ERR, "config file [%s %s] ext must be .xml\n", name->data, filename->data);
				return;
			}
			auto it = config_files.insert(name, &new_flag);
			//printf("file [%s] new_flag=[%d]\n",name->data,new_flag);
			KConfigFile* cfg_file;
			if (new_flag) {
				cfg_file = new KConfigFile(&events, name, filename, current_source);
				cfg_file->set_remove_flag(!is_default);
				if (is_default) {
					cfg_file->set_default_config();
				}
				it->value(cfg_file);
			} else {
				cfg_file = it->value();
				if (cfg_file->get_source() != current_source) {
					klog(KLOG_ERR, "source is not match\n");
					return;
				}
				if (is_default) {
					cfg_file->update_filename(filename);
				}
			}
			if (cfg_file->last_modified != last_modified) {
				auto xml = cfg_file->load();
				if (!xml && cfg_file->get_index() != 0) {
					/* A typo in config.xml used to remove every listen and
					 * vhost, the admin port included. Keep what runs and
					 * try again on the next reload. */
					klog(KLOG_ERR, "cann't load config file [%s %s], keep its running config\n", name->data, filename->data);
					cfg_file->set_remove_flag(false);
					return;
				}
				cfg_file->set_last_modified(last_modified);
				prepare_files.emplace(std::piecewise_construct,
					std::forward_as_tuple(cfg_file->get_index()),
					std::forward_as_tuple(std::move(xml),
						KSafeConfigFile(cfg_file->add_ref())));
			} else {
				cfg_file->set_remove_flag(false);
			}
		}
		std::multimap<uint16_t, config_prepare_parse> prepare_files;
		KConfigFileSource current_source = KConfigFileSource::System;
	};

	bool reload_config(const kgl_ref_str_t* name, bool force) {
		auto locker = lock();
		auto it = config_files.find(name);
		if (!it) {
			KFileName filename;
			if (!load_config_from_name(name, filename)) {
				return false;
			}
			KConfigScanInfoProvider provider;
			load_config_file(name, &filename, &provider);
			for (auto&& info : provider.prepare_files) {
				klog(KLOG_INFO, "load config file [%s] index=[%d]\n", info.second.cfg->get_filename()->data, info.first);
				info.second.cfg->update(info.second.body);
			}
			return true;
		}
		auto file = it->value();
		if (!file->reload(force)) {
			if (file->get_source_driver()->enable_scan() && !file->is_default()) {
				//remove config
				file->clear();
				config_files.erase(it);
				file->release();
			}
			return false;
		}
		return true;
	}
	void reload() {
		auto locker = lock();
		kgl_ext_config_context ctx;
		KConfigScanInfoProvider provider;
		bool scanned[static_cast<int>(KConfigFileSource::Size)] = { false };
		auto scan_source = [&provider](int i) {
			provider.current_source = static_cast<KConfigFileSource>(i);
			provider.scan_failed = false;
			sources[i]->scan(&provider);
			if (!provider.scan_failed) {
				return;
			}
			/* e.g. the vhost database is down: removing all its vhosts
			 * would take every site offline. Keep them until it is back. */
			klog(KLOG_ERR, "cann't scan config source [%d], keep its running config\n", i);
			config_files.iterator([](void* data, void* arg) {
				KConfigFile* file = (KConfigFile*)data;
				if (file->get_source() == *(KConfigFileSource*)arg) {
					file->set_remove_flag(false);
				}
				return iterator_continue;
				}, &provider.current_source);
		};
		config_files.iterator([](void* data, void* arg) {
			KConfigFile* file = (KConfigFile*)data;
			//printf("set file [%s] remove_flag\n", file->filename->data);
			if (file->get_source_driver()->enable_scan()) {
				file->set_remove_flag(true);
			}
			return iterator_continue;
			}, NULL);
		for (int i = 0; i < static_cast<int>(KConfigFileSource::Size); ++i) {
			if (sources[i] != nullptr) {
				// Database-backed virtual hosts inherit settings from the system
				// templates. Apply the ordinary config sources first so a reload
				// cannot rebuild vhosts against the previous template generation.
				if (i != static_cast<int>(KConfigFileSource::Db) && sources[i]->enable_scan()) {
					scan_source(i);
					scanned[i] = true;
				}
			}
		}
		auto apply_prepare_files = [&provider]() {
			for (auto&& info : provider.prepare_files) {
				klog(KLOG_INFO, "load config file [%s] index=[%d]\n", info.second.cfg->get_filename()->data, info.first);
				info.second.cfg->update(info.second.body);
			}
			provider.prepare_files.clear();
		};
		apply_prepare_files();
		// A source driver can become available after the main configuration
		// has been applied (for example, the virtual-host database driver).
		for (int i = 0; i < static_cast<int>(KConfigFileSource::Size); ++i) {
			if (!scanned[i] && sources[i] != nullptr && sources[i]->enable_scan()) {
				scan_source(i);
				scanned[i] = true;
			}
		}
		apply_prepare_files();
		config_files.iterator([](void* data, void* arg) {
			KConfigFile* file = (KConfigFile*)data;
			if (file->is_removed()) {
				klog(KLOG_ERR, "remove config file [%s %s]\n", file->get_name()->data, file->get_filename()->data);
				file->clear();
				file->release();
				return iterator_remove_continue;
			}
			if (!file->get_source_driver()->enable_scan()) {
				file->reload(false);
			}
			return iterator_continue;
			}, NULL);
		if (is_first_config) {
			events.check_at_once();
			is_first_config = false;
		}
	}

	khttpd::KXmlNode* find_child(const khttpd::KXmlNode* node, uint32_t name_id, const char* vary, size_t len) {
		khttpd::KXmlKey key;
		kgl_ref_str_t tag = { 0 };
		tag.ref = 2;
		key.tag = &tag;
		key.tag_id = name_id;
		if (vary) {
			key.vary = kstring_from2(vary, len);
		}
		auto it = node->get_first()->childs.find(&key);
		if (!it) {
			return nullptr;
		}
		return it->value();
	}
	khttpd::KXmlNode* insert_child(khttpd::KXmlNodeBody* node, const char* name, size_t len) {
		khttpd::KXmlKey key(name, len);
		auto it2 = qname_config.find(key.tag);
		if (it2) {
			auto tag = it2->value();
			key.tag_id = tag->tag_id;
		}
		auto child_node = node->find_child(&key);
		if (!child_node) {
			khttpd::KSafeXmlNode new_node(new khttpd::KXmlNode(&key));
			node->add(new_node.get(), khttpd::last_pos);
			child_node = node->find_child(&key);
		}
		return child_node;
	}
	khttpd::KXmlNode* find_child(const khttpd::KXmlNodeBody* node, const char* name, size_t len) {
		khttpd::KXmlKey key(name, len);
		auto it2 = qname_config.find(key.tag);
		if (it2) {
			auto tag = it2->value();
			key.tag_id = tag->tag_id;
		}
		auto it = node->childs.find(&key);
		if (!it) {
			return nullptr;
		}
		return it->value();
	}
	KMapNode<khttpd::KXmlNode>* find_first_child(const khttpd::KXmlNodeBody* node, const kgl_ref_str_t& a) {
		khttpd::KXmlKeyTag tag(&a, 0);
		auto it = qname_config.find(&a);
		if (it) {
			tag.tag_id = it->value()->tag_id;
		}
		auto it2 = node->find_any_child(&tag);
		if (!it2) {
			return nullptr;
		}
		for (;;) {
			auto last = it2->prev();
			if (!last) {
				return it2;
			}
			auto data = last->value();
			if (data->cmp(&tag) != 0) {
				return it2;
			}
			it2 = last;
		}
	}
	khttpd::KSafeXmlNode new_xml(const char* name, size_t len, const char* vary, size_t vary_len) {
		auto key_tag = kstring_from2(name, len);
		auto it = qname_config.find(key_tag);
		uint32_t key_tag_id = 0;
		khttpd::KXmlKey* tag = nullptr;
		if (it) {
			tag = it->value();
			key_tag_id = tag->tag_id;
			kstring_release(key_tag);
			key_tag = kstring_refs(tag->tag);
		}
		kgl_ref_str_t* key_vary = nullptr;
		if (vary) {
			key_vary = kstring_from2(vary, vary_len);
		}
		auto xml = khttpd::KSafeXmlNode(new khttpd::KXmlNode(key_tag, key_vary, key_tag_id));
		if (tag && key_vary) {
			xml->attributes().emplace(tag->vary, key_vary);
		}
		if (key_tag) {
			kstring_release(key_tag);
		}
		if (key_vary) {
			kstring_release(key_vary);
		}
		return xml;
	}
	khttpd::KSafeXmlNode new_xml(const char* name, size_t len) {
		auto vary = (char*)memchr(name, '@', len);
		if (vary == nullptr) {
			return new_xml(name, len, nullptr, 0);
		}
		size_t name_len = vary - name;
		return new_xml(name, name_len, vary + 1, len - name_len - 1);
	}
	khttpd::KXmlNodeBody* new_child(khttpd::KXmlNodeBody* node, const char* name, size_t len, const char* vary, size_t vary_len) {
		auto xml = new_xml(name, len, vary, vary_len);
		return node->add(xml.get(), khttpd::last_pos);
	}
	khttpd::KXmlNodeBody* new_child(khttpd::KXmlNodeBody* node, const char* name, size_t len) {
		auto xml = new_xml(name, len);
		return node->add(xml.get(), khttpd::last_pos);
	}
	uint32_t register_qname(const char* name, size_t len, bool vary_icase) {
		auto key = new khttpd::KXmlKey(name, len);
		auto old_key = qname_config.add(key->tag, key);
		if (old_key) {
			key->tag_id = old_key->tag_id;
			delete old_key;
			return key->tag_id;
		}
		key->tag_id = (++cur_know_qname_id) << 1;
		if (vary_icase) {
			key->tag_id |= 1;
		}
		return key->tag_id;

	}
	void init(on_begin_parse_f cb) {
		locker = kfiber_mutex_init();
		register_source_driver(KConfigFileSource::System, new KSystemConfigFileDriver);
		register_source_driver(KConfigFileSource::Vh, new KVhAccessConfigFileDriver);
		register_qname(_KS("worker_thread"));
		register_qname(_KS("access_log"));
		register_qname(_KS("log"));
		register_qname(_KS("dso_extend@name"));
		register_qname(_KS("server@name"));
		register_qname(_KS("api@name"));
		register_qname(_KS("cmd@name"));
		register_qname(_KS("request"));
		register_qname(_KS("response"));
		register_qname(_KS("named_acl@name"));
		register_qname(_KS("named_mark@name"));
		register_qname(_KS("acl"));
		register_qname(_KS("mark"));
		register_qname(_KS("table@name"));
		register_qname(_KS("ssl@domain"), true);
		register_qname(_KS("vhs"));
		register_qname(_KS("vh@name"));
		register_qname(_KS("vh_templete@name"));
		register_qname(_KS("error@code"));
#ifdef _WIN32
		register_qname(_KS("mime_type@ext"), true);
		register_qname(_KS("map_file@ext"), true);
#else
		register_qname(_KS("mime_type@ext"), false);
		register_qname(_KS("map_file@ext"), false);
#endif
		assert(kconfig::test());
		begin_parse_cb = cb;
	}
	void shutdown() {
		config_files.iterator([](void* data, void* arg) {
			KConfigFile* file = (KConfigFile*)data;
			file->clear();
			file->release();
			return iterator_remove_continue;
			}, NULL);
		qname_config.iterator([](void* data, void* arg) {
			delete (khttpd::KXmlKey*)data;
			return iterator_remove_continue;
			}, NULL);
		events.clean();
		kfiber_mutex_destroy(locker);
	}
	KConfigResult remove(KConfigFile* file, const kgl_ref_str_t& path, uint32_t index) {
		const char* name = (const char*)kgl_memrchr(path.data, '/', path.len);
		size_t path_len;
		khttpd::KSafeXmlNode xml;
		if (!name) {
			path_len = 0;
			xml = new_xml(path.data, path.len);
		} else {
			path_len = name - path.data;
			xml = new_xml(name + 1, path.len - path_len - 1);
		}
		if (!file->get_source_driver()->enable_save()) {
			return KConfigResult::ErrSaveFile;
		}
		if (!file->update(path.data, path_len, index, xml.get(), kconfig::EvRemove)) {
			return KConfigResult::ErrNotFound;
		}
		if (!file->save()) {
			return KConfigResult::ErrSaveFile;
		}
		return KConfigResult::Success;
	}
	KConfigResult remove(const kgl_ref_str_t& file_name, const kgl_ref_str_t& path, uint32_t index) {
		KFiberLocker lock(locker);
		auto file = find_file(file_name);
		if (!file) {
			return KConfigResult::ErrNotFound;
		}
		return remove(file, path, index);
	}
	KConfigResult remove(const kgl_ref_str_t& path, uint32_t index) {
		KFiberLocker lock(locker);
		const char* tree_path = path.data;
		size_t tree_len = path.len;
		auto tree = kconfig::find(&tree_path, &tree_len);
		if (!tree || !tree->node) {
			return KConfigResult::ErrNotFound;
		}
		auto file = tree->node->file;
		if (!file) {
			return KConfigResult::ErrNotFound;
		}
		return remove(file, path, index);
	}
	KConfigResult add(const kgl_ref_str_t& path, uint32_t index, khttpd::KXmlNode* xml) {
		return update(path, index, xml, EvNew);
	}
	bool is_first() {
		return is_first_config;
	}
	bool need_reboot() {
		return need_reboot_flag;
	}
	void set_need_reboot() {
		need_reboot_flag = true;
	}
	KConfigFile* find_file(const kgl_ref_str_t& name) {
		auto it = config_files.find(&name);
		if (!it) {
			return nullptr;
		}
		return it->value();
	}
	KConfigResult update(KConfigFile* file, const kgl_ref_str_t& path, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type) {
		const char* name = (const char*)kgl_memrchr(path.data, '/', path.len);
		if (!name) {
			name = path.data;
		}
		size_t path_len = name - path.data;
		if (!file->get_source_driver()->enable_save()) {
			return KConfigResult::ErrSaveFile;
		}
		if (!file->update(path.data, path_len, index, xml, ev_type)) {
			return KConfigResult::ErrNotFound;
		}
		if (!file->save()) {
			return KConfigResult::ErrSaveFile;
		}
		return KConfigResult::Success;
	}
	KConfigResult update(const kgl_ref_str_t& path, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type) {
		KFiberLocker lock(locker);
		const char* tree_path = path.data;
		size_t tree_len = path.len;
		auto tree = kconfig::find(&tree_path, &tree_len);
		KConfigFile* file = nullptr;
		if (tree && tree->node) {
			file = tree->node->file;
		}
		if (!file) {
			if (KBIT_TEST(ev_type, FlagCreate | EvNew)) {
				file = find_file(default_file_name);
			}
			if (!file) {
				return KConfigResult::ErrNotFound;
			}
		}
		return update(file, path, index, xml, ev_type);
	}
	KConfigResult update(const kgl_ref_str_t& path, uint32_t index, const KString* text, KXmlAttribute* attribute, KConfigEventType ev_type) {
		const char* name = (const char*)kgl_memrchr(path.data, '/', path.len);
		if (!name) {
			name = path.data;
		}
		size_t path_len = name - path.data;
		auto xml = kconfig::new_xml(name, path.len - path_len);
		if (attribute) {
			xml->get_last()->attributes.swap(*attribute);
		}
		if (text) {
			xml->get_last()->set_text(*text);
		}
		return update(path, index, xml.get(), ev_type);
	}
	KConfigResult add(const kgl_ref_str_t& file, const kgl_ref_str_t& path, uint32_t index, khttpd::KXmlNode* xml) {
		return update(file, path, index, xml, EvNew);
	}
	KConfigResult update(const kgl_ref_str_t& file_name, const kgl_ref_str_t& path, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type) {
		KFiberLocker lock(locker);
		auto file = find_file(file_name);
		if (!file) {
			return KConfigResult::ErrNotFound;
		}
		return update(file, path, index, xml, ev_type);
	}
	KConfigResult move_down(const kgl_ref_str_t& file_name, const kgl_ref_str_t& path, uint32_t index) {
		KFiberLocker lock(locker);
		auto file = find_file(file_name);
		if (!file) {
			klog(KLOG_ERR, "move config item failed: source [%.*s] was not found, path=[%.*s], index=%u\n",
				(int)file_name.len, file_name.data, (int)path.len, path.data, index);
			return KConfigResult::ErrNotFound;
		}
		if (!file->get_source_driver()->enable_save()) {
			klog(KLOG_ERR, "move config item failed: source [%.*s] is read-only, path=[%.*s], index=%u\n",
				(int)file_name.len, file_name.data, (int)path.len, path.data, index);
			return KConfigResult::ErrSaveFile;
		}
		if (!file->move_down(path.data, path.len, index)) {
			klog(KLOG_ERR, "move config item failed: path [%.*s] index=%u was not found in source [%.*s] (%s)\n",
				(int)path.len, path.data, index, (int)file_name.len, file_name.data,
				file->get_filename() ? file->get_filename()->data : "");
			return KConfigResult::ErrNotFound;
		}
		if (!file->save()) {
			klog(KLOG_ERR, "move config item failed: cannot save source [%.*s] (%s)\n",
				(int)file_name.len, file_name.data, file->get_filename() ? file->get_filename()->data : "");
			return KConfigResult::ErrSaveFile;
		}
		return KConfigResult::Success;
	}
	KConfigResult rename_table(const kgl_ref_str_t& path, const KString& new_name) {
		KFiberLocker lock(locker);
		const char* tree_path = path.data;
		size_t tree_len = path.len;
		auto tree = kconfig::find(&tree_path, &tree_len);
		if (!tree || !tree->node || tree->node->next) {
			return KConfigResult::ErrNotFound;
		}
		auto file = tree->node->file;
		if (!file || !file->get_source_driver()->enable_save()) {
			return KConfigResult::ErrSaveFile;
		}
		if (!file->rename_table(path.data, path.len, new_name)) {
			return KConfigResult::ErrNotFound;
		}
		return file->save() ? KConfigResult::Success : KConfigResult::ErrSaveFile;
	}
	KConfigResult rename_table(const std::vector<KConfigTableRename>& accesses,
		const KString& old_name, const KString& new_name) {
		struct rename_operation {
			KString path;
			bool rename_definition;
		};
		struct rename_file {
			KConfigFile* file = nullptr;
			std::vector<rename_operation> operations;
			khttpd::KSafeXmlNode original;
			khttpd::KSafeXmlNode updated;
			bool changed = false;
			bool has_definition = false;
		};
		KFiberLocker lock(locker);
		std::map<KConfigFile*, rename_file> files;
		for (auto&& access : accesses) {
			if (!access.access_tree) {
				return KConfigResult::ErrNotFound;
			}
			for (auto node = access.access_tree->node; node; node = node->next) {
				auto file = node->file;
				if (!file || !file->get_source_driver()->enable_save()) {
					return KConfigResult::ErrSaveFile;
				}
				KString path;
				if (!build_relative_config_path(file, access.access_tree, path)) {
					return KConfigResult::ErrNotFound;
				}
				auto& work = files[file];
				work.file = file;
				bool duplicate = false;
				for (auto& operation : work.operations) {
					if (operation.path == path) {
						operation.rename_definition |= access.rename_definition;
						duplicate = true;
						break;
					}
				}
				if (!duplicate) {
					work.operations.push_back({ path, access.rename_definition });
				}
			}
		}
		bool found_definition = false;
		for (auto&& entry : files) {
			auto& work = entry.second;
			work.original = work.file->clone_nodes();
			if (!work.original) {
				return KConfigResult::ErrNotFound;
			}
			work.updated = work.original->clone();
			for (auto&& operation : work.operations) {
				auto access_node = find_xml_path(work.updated->get_first(),
					operation.path.c_str(), operation.path.size());
				if (!access_node) {
					return KConfigResult::ErrNotFound;
				}
				for (uint32_t index = 0;; ++index) {
					auto access_body = access_node->get_body(index);
					if (!access_body) {
						break;
					}
					bool this_definition = false;
					work.changed |= rewrite_access_table(access_body, old_name, new_name,
						operation.rename_definition, this_definition);
					work.has_definition |= this_definition;
					found_definition |= this_definition;
				}
			}
		}
		if (!found_definition) {
			return KConfigResult::ErrNotFound;
		}
		std::vector<rename_file*> ordered;
		for (auto&& entry : files) {
			if (entry.second.changed && entry.second.has_definition) {
				ordered.push_back(&entry.second);
			}
		}
		for (auto&& entry : files) {
			if (entry.second.changed && !entry.second.has_definition) {
				ordered.push_back(&entry.second);
			}
		}
		size_t applied = 0;
		for (; applied < ordered.size(); ++applied) {
			auto work = ordered[applied];
			work->file->update(work->updated);
			if (work->file->save()) {
				continue;
			}
			for (size_t rollback = 0; rollback <= applied; ++rollback) {
				ordered[rollback]->file->update(ordered[rollback]->original);
				ordered[rollback]->file->save();
			}
			return KConfigResult::ErrSaveFile;
		}
		return KConfigResult::Success;
	}
	KFiberLocker lock() {
		return KFiberLocker(locker);
	}
#ifndef NDEBUG
	struct test_config_context
	{
		int ev_count;
		int int_value;
		int vh_count;
		int attr_value;
		kgl_config_diff diff;
	};

	bool test() {
		//return true;
		int prev_ev;
		register_qname(_KS("a"));
		register_qname(_KS("b"));
		register_qname(_KS("c"));
		char order_xml_text[] = "<config><a id='1'/><b/><a id='2'/><c/></config>";
		auto order_xml = parse_xml(order_xml_text);
		assert(order_xml);
		auto assert_document_order = [](const khttpd::KXmlNodeBody* body) {
			assert(body->child_order.size() == 4);
			assert(body->child_order[0].node->is_tag(_KS("a")));
			assert(body->child_order[0].body->attributes.get_int("id") == 1);
			assert(body->child_order[1].node->is_tag(_KS("b")));
			assert(body->child_order[2].node->is_tag(_KS("a")));
			assert(body->child_order[2].body->attributes.get_int("id") == 2);
			assert(body->child_order[3].node->is_tag(_KS("c")));
		};
		assert_document_order(order_xml->get_first());
		auto order_xml_clone = order_xml->clone();
		assert_document_order(order_xml_clone->get_first());
		assert(move_xml_node_down(order_xml_clone->get_first(), _KS("a"), 0));
		auto moved_a = find_child(order_xml_clone->get_first(), _KS("a"));
		assert(moved_a && moved_a->get_body(0)->attributes.get_int("id") == 2);
		assert(moved_a->get_body(1)->attributes.get_int("id") == 1);
		assert(order_xml_clone->get_first()->child_order[0].body == moved_a->get_body(0));
		assert(order_xml_clone->get_first()->child_order[2].body == moved_a->get_body(1));
		char rename_xml_text[] = "<config><request><table name='OLD'><chain action='continue'/></table></request></config>";
		auto renamed_xml = parse_xml(rename_xml_text);
		assert(renamed_xml && rename_xml_table(renamed_xml->get_first(), _KS("request/table@OLD"), "NEW"));
		auto request_node = find_child(renamed_xml->get_first(), _KS("request"));
		assert(request_node && !find_child(request_node->get_first(), _KS("table@OLD")));
		auto renamed_table = find_child(request_node->get_first(), _KS("table@NEW"));
		assert(renamed_table && renamed_table->get_first()->attributes["name"] == "NEW");
		assert(find_child(renamed_table->get_first(), _KS("chain")));
		char rename_ref_xml_text[] = "<config><request action='table:OLD'><table name='OLD'/><table name='CALLER'><chain action='table:OLD'/></table></request></config>";
		auto renamed_ref_xml = parse_xml(rename_ref_xml_text);
		auto renamed_ref_request = find_child(renamed_ref_xml->get_first(), _KS("request"));
		bool found_definition = false;
		assert(renamed_ref_request && rewrite_access_table(renamed_ref_request->get_first(),
			"OLD", "NEW", true, found_definition));
		assert(found_definition);
		assert(renamed_ref_request->get_first()->attributes["action"] == "table:NEW");
		auto caller_table = find_child(renamed_ref_request->get_first(), _KS("table@CALLER"));
		auto caller_chain = find_child(caller_table->get_first(), _KS("chain"));
		assert(caller_chain->get_first()->attributes["action"] == "table:NEW");
		KConfigTree test_ev(nullptr, _KS("config"));
		test_config_context ctx;
		memset(&ctx, 0, sizeof(ctx));
		KSafeConfigFile t(new KConfigFile(&test_ev, nullptr, nullptr, KConfigFileSource::System));
		auto listener = config_listen([&](KConfigTree* tree, KConfigEvent* ev) {
			auto xml = ev->get_xml();
			ctx.ev_count++;
			if (ev->type != EvRemove) {
				assert(strcmp(xml->key.tag->data, "int") == 0);
				ctx.int_value = atoi(xml->get_text_cstr());
				ctx.attr_value = atoi(xml->attributes()["v"].c_str());
			}
			return true;
			}, ev_self);
		assert(listen(_KS("int"), &listener, &test_ev));
		t->reload(_KS("<config><int>3</int></config>"));
		assert(ctx.int_value == 3);
		assert(ctx.attr_value == 0);
		t->reload(_KS("<config><int>4</int></config>"));
		assert(ctx.int_value == 4);
		assert(ctx.attr_value == 0);
		t->reload(_KS("<config><int v='3'>4</int></config>"));
		assert(ctx.int_value == 4);
		assert(ctx.attr_value == 3);
		t->reload(_KS("<config/>"));
		auto listener2 = config_listen([&](KConfigTree* tree, KConfigEvent* ev) {
			ctx.ev_count++;
			if (ev->type == (EvSubDir | EvRemove)) {
				ctx.vh_count--;
			} else if (ev->type == (EvSubDir | EvNew)) {
				ctx.vh_count++;
			}
			return true;
			}, ev_subdir);
		listen(_KS("/vh2"), &listener2, &test_ev);
		t->reload(_KS("<config><vh2><a/><c/><b/></vh2></config>"));
		assert(ctx.vh_count == 3);
		//no change
		prev_ev = ctx.ev_count;
		t->reload(_KS("<config><vh2><a/><c/><b/></vh2></config>"));
		assert(prev_ev == ctx.ev_count);
		assert(ctx.vh_count == 3);

		t->reload(_KS("<config><vh2><c><d/><d/></c><c/><b/></vh2></config>"));
		assert(ctx.vh_count == 2);
		t->reload(_KS("<config/>"));
		assert(ctx.vh_count == 0);
		/* test sub node update */

		assert(remove_listen(_KS("int"), &test_ev));
		auto listener3 = config_listen([&](KConfigTree* tree, KConfigEvent* ev) {
			++ctx.ev_count;
			return true;
			}, ev_subdir);
		assert(listen(_KS(""), &listener3, &test_ev));

		t->reload(_KS("<config><int><aa/></int></config>"));
		prev_ev = ctx.ev_count;
		t->reload(_KS("<config><int><aa name='aa'/></int></config>"));
		assert(ctx.ev_count == prev_ev + 1);
		/* test vector */
		assert(remove_listen(_KS("vh2"), &test_ev));
		assert(remove_listen(_KS(""), &test_ev));
		ctx.vh_count = 0;
		auto test_vh_event = config_listen([&](KConfigTree* tree, KConfigEvent* ev) -> bool {
			auto xml = ev->get_xml();
			ctx.ev_count++;
			assert(xml);
			if (!xml->is_tag(_KS("vh"))) {
				return false;
			}
			if (ev->type == (EvSubDir | EvRemove)) {
				ctx.vh_count--;
			} else if (ev->type == (EvSubDir | EvNew)) {
				ctx.vh_count++;
			}

			return true;
			}, ev_subdir | ev_skip_vary);
		assert(listen(_KS("/vh"), &test_vh_event, &test_ev));
		t->reload(_KS("<config></config>"));

		assert(ctx.vh_count == 0);
		t->reload(_KS("<config><vh name='b'/><vh name='a'/><dd/></config>"));
		assert(ctx.vh_count == 2);
		t->reload(_KS("<config><vh name='c'/><vh name='a'/><vh/></config>"));
		assert(ctx.vh_count == 3);
		t->reload(_KS("<config></config>"));
		assert(ctx.vh_count == 0);
		assert(remove_listen(_KS("/vh"), &test_ev));

		assert(listen(_KS(""), &test_vh_event, &test_ev));
		t->reload(_KS("<config><vh name='b'/><vh name='a'/><dd/></config>"));
		assert(ctx.vh_count == 2);
		t->reload(_KS("<config><vh name='c'/><vh name='a'/><vh/></config>"));
		assert(ctx.vh_count == 3);
		t->reload(_KS("<config></config>"));
		assert(ctx.vh_count == 0);
		assert(remove_listen(_KS(""), &test_ev));
		//test diff
		ctx.int_value = 0;
		auto on_diff_event = config_listen([&](KConfigTree* tree, KConfigEvent* ev) -> bool {
			ctx.ev_count++;
			auto xml = ev->new_xml;
			int int_value = ctx.int_value;
			ctx.int_value = 0;
			if (xml) {
				for (uint32_t index = 0;; ++index) {
					auto body = xml->get_body(index);
					if (!body) {
						break;
					}
					int id = body->attributes.get_int("id");
					ctx.int_value += id;
				}
			}
			ctx.diff = *ev->diff;
			for (uint32_t index = ev->diff->from; index < ev->diff->old_to; ++index) {
				auto body = ev->old_xml->get_body(index);
				assert(body);
				int id = body->attributes.get_int("id");
				int_value -= id;
			}
			for (uint32_t index = ev->diff->from; index < ev->diff->new_to; ++index) {
				auto body = ev->new_xml->get_body(index);
				assert(body);
				int id = body->attributes.get_int("id");
				int_value += id;
			}
			assert(int_value == ctx.int_value);
			assert(ev->get_xml()->is_tag(_KS("listen")));
			return true;
			}, ev_self);
		assert(listen(_KS("listen"), &on_diff_event, &test_ev));
		t->reload(_KS("<config><listen id='1'/></config>"));
		assert(ctx.int_value == 1);
		prev_ev = ctx.ev_count;

		t->reload(_KS("<config><listen id='2'/></config>"));
		assert(ctx.int_value == 2);
		assert(ctx.ev_count == ++prev_ev);


		t->reload(_KS("<config><listen id='1'/><listen id='2'/></config>"));
		assert(ctx.int_value == 3);
		assert(ctx.ev_count == ++prev_ev);
		t->reload(_KS("<config><listen id='2'/><listen id='1'/></config>"));
		assert(ctx.int_value == 3);
		assert(ctx.ev_count == ++prev_ev);
		t->reload(_KS("<config><listen id='3'/><listen id='2'/><listen id='1'/></config>"));
		assert(ctx.int_value == 6);
		assert(ctx.ev_count == ++prev_ev);
		t->reload(_KS("<config><listen id='4'/><listen id='2'/><listen id='1'/></config>"));
		assert(ctx.int_value == 7);
		assert(ctx.ev_count == ++prev_ev);
		t->reload(_KS("<config><listen id='4'/><listen id='3'/><listen id='1'/></config>"));
		assert(ctx.int_value == 8);
		assert(ctx.ev_count == ++prev_ev);
		t->reload(_KS("<config><listen id='4'/></config>"));
		assert(ctx.int_value == 4);
		assert(ctx.ev_count == ++prev_ev);

		t->reload(_KS("<config><listen id='4'/><listen id='3'/><listen id='1'/></config>"));
		assert(ctx.int_value == 8 && ctx.ev_count == ++prev_ev);
		t->reload(_KS("<config><listen id='1'/></config>"));
		assert(ctx.int_value == 1 && ctx.ev_count == ++prev_ev);
		t->reload(_KS("<config></config>"));
		assert(ctx.int_value == 0 && ctx.ev_count == ++prev_ev);
		return true;
	}
#endif
}
