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

namespace kconfig {
	bool is_first_config = true;
	bool need_reboot_flag = false;
	constexpr int default_file_index = 50;
	constexpr int max_file_size = 1048576;
	constexpr kgl_str_t default_file_name{ _KS("default") };
	static KConfigTree events(nullptr, _KS("config"));
	static uint16_t cur_know_qname_id = 0xffff;
	static kfiber_mutex* locker = nullptr;
	KMap<kgl_ref_str_t, khttpd::KXmlKey> qname_config;
	KMap<kgl_ref_str_t, KConfigFile> config_files;
	static on_begin_parse_f begin_parse_cb = nullptr;
	KConfigFile* find_file(const kgl_str_t& name);
	KConfigFile* add_file(kgl_ref_str_t* name, kgl_ref_str_t* filename);
	class KConfigFileInfo
	{
	public:
		KConfigFileInfo(KConfigFile* file, time_t last_modified) {
			this->file = file->add_ref();
			this->last_modified = last_modified;
			node = nullptr;
		}
		~KConfigFileInfo() {
			file->release();
			if (node) {
				node->release();
			}
		}
		int cmp(const kgl_ref_str_t* a) const {
			int result = (int)a->id - (int)file->get_name()->id;
			if (result != 0) {
				return result;
			}
			return file->cmp(a);
		}
		bool load() {
			auto fp = kfiber_file_open(file->get_filename()->data, fileRead, KFILE_ASYNC);
			if (!fp) {
				return false;
			}
			defer(kfiber_file_close(fp));
			auto size = (int)kfiber_file_size(fp);
			if (size > max_file_size) {
				klog(KLOG_ERR, "config file [%s %s] is too big. size=[%d]\n", file->get_name()->data, file->get_filename()->data, size);
				return false;
			}
			char* buf = (char*)malloc(size + 1);
			if (!buf) {
				return false;
			}
			defer(free(buf));
			buf[size] = '\0';
			if (!kfiber_file_read_full(fp, buf, &size)) {
				return false;
			}
			KExtConfigDynamicString ds(file->get_filename()->data);
			ds.dimModel = false;
			ds.blockModel = false;
			ds.envChar = '%';
			char* content = ds.parseDirect(buf);
			defer(free(content));
			char* hot = content;
			while (*hot && isspace((unsigned char)*hot)) {
				hot++;
			}
			char* start = hot;
			//默认启动顺序为50
			uint16_t id = 50;
			if (strncmp(hot, "<!--#", 5) == 0) {
				hot += 5;
				if (strncmp(hot, "stop", 4) == 0) {
					/*
					 * 扩展没有启动
					 */
					return false;
				} else if (strncmp(hot, "start", 5) == 0) {
					char* end = strchr(hot, '>');
					if (end) {
						start = end + 1;
					}
					hot += 6;
					id = atoi(hot);
				}
			}
			file->set_index(id);
			file->set_remove_flag(false);
			assert(!node);
			node = parse_xml(content);
			if (begin_parse_cb) {
				if (!begin_parse_cb(file, node)) {
					node->release();
					node = nullptr;
				}
			}
			return true;
		}
		void parse() {
			klog(KLOG_ERR, "now parse config [%s %s]\n", file->get_name()->data, file->get_filename()->data);
			file->update(node);
			node = nullptr;
			file->set_last_modified(last_modified);
		}
		KConfigFile* file;
		time_t last_modified;
		khttpd::KXmlNode* node;
	};
	KConfigTree::~KConfigTree() noexcept {
		clean();
		if (node) {
			delete node;
		}
		kstring_release(name);
	}

	bool update_xml_node(khttpd::KXmlNode* nodes, const char* name, size_t size, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type) {
		size_t name_len;
		while (size > 1 && *name == '/') {
			name++;
			size--;
		}
		if (size <= 0 || *name == '/') {
			switch (ev_type) {
			case kconfig::EvNew:
				nodes->insert(xml, index);
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
		nodes = find_child(nodes, name, name_len);
		if (!nodes) {
			return false;
		}
		name += name_len;
		size -= name_len;
		return update_xml_node(nodes, name, size, index, xml, ev_type);
	}
	iterator_ret config_tree_clean_iterator(void* data, void* argv) {
		KConfigTree* rn = (KConfigTree*)data;
		delete rn;
		return iterator_remove_continue;
	}
	void KConfigTree::check_at_once() {
		if (is_at_once() && !node && on_event) {
			on_event(data, this, nullptr);
		}
		if (child) {
			child->iterator([](void* data, void* arg) {
				KConfigTree* rn = (KConfigTree*)data;
				rn->check_at_once();
				return iterator_continue;
				}, nullptr);
		}
	}
	void KConfigTree::clean() {
		if (child) {
			child->iterator(config_tree_clean_iterator, NULL);
			delete child;
			child = nullptr;
		}
	}
	KConfigTree* KConfigTree::find(const char** name, size_t* size) {
		size_t name_len;
		while (*size > 1 && (**name == '/' || **name == '@')) {
			(*name)++;
			(*size)--;
		}
		if (*size <= 0 || **name == '/') {
			return this;
		}
		auto path = kgl_mempbrk(*name, (int)*size, _KS("/@"));
		if (path) {
			name_len = path - (*name);
		} else {
			name_len = *size;
		}
		if (!child) {
			return this;
		}
		kgl_ref_str_t key;
		key.data = *name;
		key.len = (int)name_len;
		auto node = child->find(&key);
		if (!node) {
			return this;
		}
		*name += name_len;
		*size -= name_len;
		return node->value()->find(name, size);
	}
	bool KConfigTree::bind(void* data, on_event_f on_event, KConfigEventFlag flags) {
		if (this->data) {
			return false;
		}
		this->data = data;
		this->on_event = on_event;
		this->name->flags = flags;
		return true;
	}
	KConfigTree* KConfigTree::add(const char* name, size_t size, void* data, on_event_f on_event, KConfigEventFlag flags) {
		size_t name_len;
		while (size > 1 && *name == '/') {
			name++;
			size--;
		}
		if (size <= 0 || *name == '/') {
			if (bind(data, on_event, flags)) {
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
		kgl_ref_str_t key;
		key.data = name;
		key.len = (uint16_t)name_len;

		int new_flag = 0;
		if (!child) {
			child = new KMap<kgl_ref_str_t, KConfigTree>();
		}
		auto node = child->insert(&key, &new_flag);
		if (new_flag) {
			node->value(new KConfigTree(this, name, name_len));
		}
		return node->value()->add(name + name_len, size - name_len, data, on_event, flags);
	}
	void* KConfigTree::remove(const char* name, size_t size) {
		size_t name_len;
		while (size > 1 && *name == '/') {
			name++;
			size--;
		}
		if (size <= 0 || *name == '/') {
			void* result = data;
			this->data = nullptr;
			this->on_event = 0;
			this->name->flags = 0;
			return result;
		}
		auto path = (const char*)memchr(name, '/', size);
		if (path) {
			name_len = path - name;
		} else {
			name_len = size;
		}
		if (!child) {
			return nullptr;
		}
		kgl_ref_str_t key;
		key.data = name;
		key.len = (uint16_t)name_len;
		auto node = child->find(&key);
		if (!node) {
			return nullptr;
		}
		KConfigTree* rn = node->value();
		void* ret = rn->remove(name + name_len, size - name_len);
		if (!ret) {
			return nullptr;
		}
		if (rn->empty()) {
			delete rn;
			child->erase(node);
			if (child->empty()) {
				delete child;
				child = nullptr;
			}
		}
		return ret;
	}
	void KConfigTree::notice(KConfigTree* ev_tree, KConfigFile* file, khttpd::KXmlNode* xml, KConfigEventType ev_type, KXmlBodyDiff& diff) {
		assert(xml);
		KConfigEventNode* file_node;
		KConfigEventNode* last_node;
		KConfigEvent ev = { file, nullptr, nullptr, diff, ev_type };
		if (ev_type != EvNone) {
			klog(KLOG_NOTICE, "notice %s%s%s  [%d] ev_tree=[%p] xml=[%p]\n",
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
				return;
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
						ev.diff.new_to = ev.new_xml->get_body_count();
						ev.type = EvUpdate;
					}
				} else {
					last_node = file_node->next;
				}
				if (!last_node || ev_tree->is_merge()) {
					if (this != ev_tree) {
						ev.type |= EvSubDir;
					}
					ev_tree->on_event(ev_tree->data, this, &ev);
				}
				return;
			}
			//update
			ev.old_xml = file_node->update(xml);
			defer(ev.old_xml->release());
			if (ev_type == EvNone) {
				return;
			}
			ev.new_xml = xml;
			assert(ev_type == EvUpdate);
			if (!last_node || ev_tree->is_merge()) {
				if (this != ev_tree) {
					ev.type |= EvSubDir;
				}
				ev_tree->on_event(ev_tree->data, this, &ev);
			}
			return;
		}
		last_node = node;
		//new
		ev.new_xml = xml;
		assert(ev_type == EvNew);
		while (last_node && file->get_index() > last_node->file->get_index()) {
			last_node = last_node->next;
			continue;
		}
		file_node = new KConfigEventNode(file, xml);
		if (!last_node) {
			file_node->next = node;
			if (node && !ev_tree->is_merge()) {
				ev.type = EvUpdate;
				ev.old_xml = node->xml;
				ev.diff.old_to = ev.old_xml->get_body_count();
			}
			node = file_node;
		} else {
			file_node->next = last_node->next;
			last_node->next = file_node;
		}
		if (!last_node || ev_tree->is_merge()) {
			if (this != ev_tree) {
				ev.type |= EvSubDir;
			}
			ev_tree->on_event(ev_tree->data, this, &ev);
		}
		return;
	}
	bool KConfigTree::notice(KConfigFile* file, khttpd::KXmlNode* xml, KConfigEventType ev_type, KXmlBodyDiff& diff) {
		if (is_self()) {
			notice(this, file, xml, ev_type, diff);
			return true;
		}
		if (parent->is_subdir()) {
			notice(parent, file, xml, ev_type, diff);
			return true;
		}
		return false;
	}
	void KConfigTree::remove_node(KMapNode<KConfigTree>* node) {
		assert(child);
		delete node->value();
		child->erase(node);
		if (child->empty()) {
			delete child;
			child = nullptr;
		}
	}
	KMapNode<KConfigTree>* KConfigTree::find_child(kgl_ref_str_t* name) {
		if (is_subdir()) {
			if (!child) {
				child = new KMap<kgl_ref_str_t, KConfigTree>();
			}
			int new_flag;
			auto it = child->insert(name, &new_flag);
			if (new_flag) {
				it->value(new KConfigTree(this, name));
			}
			return it;
		}
		if (!child) {
			return nullptr;
		}
		return child->find(name);
	}
	khttpd::KXmlNode* KConfigFile::load() {
		auto fp = kfiber_file_open(this->filename->data, fileRead, KFILE_ASYNC);
		if (!fp) {
			return nullptr;
		}
		defer(kfiber_file_close(fp));
		this->last_modified = kftime(kasync_file_get_handle(fp));
		auto size = (int)kfiber_file_size(fp);
		if (size > max_file_size) {
			klog(KLOG_ERR, "config file [%s] is too big. size=[%d]\n", this->filename->data, size);
			return nullptr;
		}
		char* buf = (char*)malloc(size + 1);
		if (!buf) {
			return nullptr;
		}
		defer(xfree(buf));
		buf[size] = '\0';
		if (!kfiber_file_read_full(fp, buf, &size)) {
			return nullptr;
		}
		return parse_xml(buf);
	}
	bool KConfigFile::diff_nodes(KConfigTree* name, KMap<khttpd::KXmlKey, khttpd::KXmlNode>* o, KMap<khttpd::KXmlKey, khttpd::KXmlNode>* n, int* notice_count) {
		bool result = false;
		if (!o) {
			assert(n != nullptr);
			assert(name);
			for (auto it = n->first(); it; it = it->next()) {
				auto node = it->value();
				if (diff(name, nullptr, node, notice_count)) {
					result = true;
				}
			}
			return result;
		}
		if (!n) {
			assert(o != nullptr);
			for (auto it = o->last(); it; it = it->prev()) {
				auto node = it->value();
				if (diff(name, node, nullptr, notice_count)) {
					result = true;
				}
			}
			return result;
		}
		for (auto itn = n->first(); itn; itn = itn->next()) {
			auto node = itn->value();
			auto it = o->find(&node->key);
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
		for (auto ito = o->last(); ito; ito = ito->prev()) {
			auto node = ito->value();
			if (n->find(&node->key)) {
				continue;
			}
			if (diff(name, node, nullptr, notice_count)) {
				if (!name) {
					return true;
				}
				result = true;
			}
		}
		return result;
	}
	bool KConfigFile::diff(KConfigTree* ev_node, khttpd::KXmlNode* o, khttpd::KXmlNode* n, int* notice_count) {
		KMapNode<KConfigTree>* child_node = nullptr;
		KXmlBodyDiff diff = { 0 };

		if (ev_node) {
			khttpd::KXmlKey* xml = o ? &o->key : &n->key;
			kgl_ref_str_t* key;
			if (xml->vary) {
				child_node = ev_node->find_child(xml->tag);
				ev_node = child_node ? child_node->value() : nullptr;
				key = xml->vary;
			} else {
				key = xml->tag;
			}
			if (ev_node) {
				child_node = ev_node->find_child(key);
				ev_node = child_node ? child_node->value() : nullptr;
			}
		}
		if (!o) {
			if (ev_node) {
				diff.new_to = n->get_body_count();
				bool notice_result = ev_node->notice(this, n, EvNew, diff);
				if (notice_result) {
					(*notice_count)++;
				}
				diff_nodes(ev_node, nullptr, &n->get_first()->childs, notice_count);
				return !notice_result;
			}
			return true;
		}
		if (!n) {
			if (ev_node) {
				diff_nodes(ev_node, &o->get_first()->childs, nullptr, notice_count);
				diff.old_to = o->get_body_count();
				bool notice_result = ev_node->notice(this, o, EvRemove, diff);
				if (notice_result) {
					(*notice_count)++;
				}
				if (child_node) {
					assert(ev_node);
					assert(ev_node->parent);
					if (ev_node->empty()) {
						assert(ev_node->parent->is_subdir());
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
			if (diff_nodes(diff.from == 0 ? ev_node : nullptr, &o_body->childs, &n_body->childs, notice_count)) {
				is_diff = true;
				break;
			}
			if (!o_body->is_same(n_body)) {
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
		for (; (diff.new_to > diff.from + 1 || diff.old_to > diff.from + 1); --diff.old_to, --diff.new_to) {
			if (diff.old_to == 0 || diff.new_to == 0) {
				break;
			}
			auto o_body = o->get_body(diff.old_to - 1);
			auto n_body = n->get_body(diff.new_to - 1);
			if (diff_nodes(nullptr, &o_body->childs, &n_body->childs, notice_count)) {
				is_diff = true;
				break;
			}
			if (!o_body->is_same(n_body)) {
				is_diff = true;
				break;
			}
		}
		if (ev_node) {
			assert(ev_node);
			if (ev_node->notice(this, n, is_diff ? EvUpdate : EvNone, diff)) {
				if (is_diff) {
					(*notice_count)++;
				}
				return false;
			}
		}
		return is_diff;
	}
	khttpd::KXmlNode* parse_xml(char* buf) {
		khttpd::KXmlDocument document;
		document.set_qname_config(&qname_config);
		auto nodes = document.parse(buf);
		if (nodes) {
			return nodes->add_ref();
		}
		return nullptr;
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
		update(tree_node);
		return true;
	}
	bool KConfigFile::update(const char* name, size_t size, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type) {
		if (readonly) {
			return false;
		}
		auto nodes = this->nodes->clone();
		if (!update_xml_node(nodes, name, size, index, xml, ev_type)) {
			return false;
		}
		update(nodes);
		return true;
	}
	bool KConfigFile::save() {
		if (!need_save) {
			return true;
		}
		std::string tmpfile = filename->data;
		tmpfile += std::string(".tmp");
		auto fp = kfiber_file_open(tmpfile.c_str(), fileWrite, 0);
		if (!fp) {
			return false;
		}
		KAsyncFileStream file(fp);
		if (this->get_index() != default_file_index) {
			file << "<!--#start " << this->get_index() << " -->\n";
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
		if (0 != unlink(filename->data)) {
			unlink(tmpfile.c_str());
			return false;
		}
		if (0 != rename(tmpfile.c_str(), filename->data)) {
			return false;
		}
		//update last_modified
		set_last_modified(kfile_last_modified(filename->data));
		return true;
	}
	void KConfigFile::update(khttpd::KXmlNode* new_nodes) {
		int notice_count = 0;
		diff_nodes(ev, nodes ? &nodes->get_first()->childs : nullptr, new_nodes ? &new_nodes->get_first()->childs : nullptr, &notice_count);
		if (nodes) {
			nodes->release();
		}
		nodes = new_nodes;
		need_save = !!notice_count;
	}
	bool KConfigFile::reload() {
		time_t last_modified = kfile_last_modified(this->filename->data);
		if (last_modified == 0) {
			return false;
		}
		auto tree_node = load();
		if (!tree_node) {
			return false;
		}
		update(tree_node);
		return true;
	}
	KConfigTree* listen(const char* name, size_t size, void* data, on_event_f on_event, uint16_t flags, KConfigTree* ev_tree) {
		return ev_tree->add(name, size, data, on_event, flags);
	}
	KConfigTree* listen(const char* name, size_t size, void* data, on_event_f on_event, uint16_t flags) {
		return events.add(name, size, data, on_event, flags);
	}
	KConfigTree* find(const char** name, size_t* size) {
		return events.find(name, size);
	}

	void* remove_listen(const char* name, size_t size, KConfigTree* ev_tree) {
		return ev_tree->remove(name, size);
	}
	void* remove_listen(const char* name, size_t size) {
		return events.remove(name, size);
	}
	struct kgl_ext_config_context
	{
		KMap<kgl_ref_str_t, KConfigFileInfo> files;
		std::string dir;
		std::string prefix;
	};
	void load_config_file(KStringBuf& name, KFileName* file, kgl_ext_config_context* ctx, bool is_default = false) {
		time_t last_modified = file->getLastModified();
		auto name2 = name.ref_str();
		if (!name2) {
			klog(KLOG_ERR, "cann't ref_str\n");
			return;
		}
		defer(kstring_release(name2));
		auto filename = kstring_from(file->getName());
		defer(kstring_release(filename));
		int new_flag;
		auto it = config_files.insert(name2, &new_flag);
		//printf("file [%s] new_flag=[%d]\n",name->data,new_flag);
		KConfigFileInfo* info;
		KConfigFile* cfg_file;
		if (new_flag) {
			cfg_file = new KConfigFile(&events, name2, filename);
			cfg_file->set_remove_flag(!is_default);
			if (is_default) {
				cfg_file->set_default_config();
			}
			it->value(cfg_file);
		} else {
			cfg_file = it->value();
		}
		//printf("file last_modified old=[" INT64_FORMAT_HEX "] new =[" INT64_FORMAT_HEX "]\n", cfg_file->last_modified, last_modified);
		if (cfg_file->last_modified != last_modified) {
			//changed
			info = new KConfigFileInfo(cfg_file, last_modified);
			if (!info->load()) {
				delete info;
				return;
			}
			ctx->files.add(info->file->get_name(), info);
		} else {
			cfg_file->set_remove_flag(false);
		}
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
			load_config_file(name, &dirConfigFile, ctx);
			return 0;
		}
		name << "_"_CS;
		load_config_file(name, &configFile, ctx);
		return 0;
	}
	bool reload_config(kgl_ref_str_t* name, bool force) {
		auto locker = lock();
		auto it = config_files.find(name);
		if (!it) {
			config_files.iterator([](void* data, void* arg) {
				KConfigFile* file = (KConfigFile*)data;
				printf("config [%s %s]\n", file->get_name()->data, file->get_filename()->data);
				return iterator_continue;
				}, NULL);
			return false;
		}
		auto file = it->value();
		auto last_modified = kfile_last_modified(file->get_filename()->data);
		if (last_modified == file->last_modified && !force) {
			return true;
		}
		KConfigFileInfo info(file, last_modified);
		if (!info.load()) {
			if (!file->is_default()) {
				//remove config
				file->clear();
				config_files.erase(it);
				file->release();
			}
			return true;
		}
		info.parse();
		return true;
	}
	void reload() {
		auto locker = lock();
		kgl_ext_config_context ctx;
		config_files.iterator([](void* data, void* arg) {
			KConfigFile* file = (KConfigFile*)data;
			//printf("set file [%s] remove_flag\n", file->filename->data);
			file->set_remove_flag(true);
			return iterator_continue;
			}, NULL);

#ifdef KANGLE_ETC_DIR
		std::string config_dir = KANGLE_ETC_DIR;
#else
		std::string config_dir = conf.path + "/etc";
#endif
		KFileName file;
		if (!file.setName(config_dir.c_str(), CONFIG_FILE, FOLLOW_LINK_ALL)) {
			file.setName(config_dir.c_str(), CONFIG_DEFAULT_FILE, FOLLOW_LINK_ALL);
		}
		KStringBuf default_name;
		default_name << default_file_name;
		load_config_file(default_name, &file, &ctx, true);
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
		ctx.files.iterator([](void* data, void* arg) {
			KConfigFileInfo* info = (KConfigFileInfo*)data;
			info->parse();
			delete info;
			return iterator_remove_continue;
			}, NULL);
		config_files.iterator([](void* data, void* arg) {
			KConfigFile* file = (KConfigFile*)data;
			if (file->is_removed()) {
				klog(KLOG_ERR, "remove config file [%s %s]\n", file->get_name()->data, file->get_filename()->data);
				file->clear();
				file->release();
				return iterator_remove_continue;
			}
			return iterator_continue;
			}, NULL);
		if (is_first_config) {
			events.check_at_once();
			is_first_config = false;
		}
	}
	khttpd::KXmlNode* find_child(khttpd::KXmlNode* node, uint16_t name_id, const char* vary, size_t len) {
		khttpd::KXmlKey key;
		kgl_ref_str_t tag = { 0 };
		tag.id = name_id;
		tag.ref = 2;
		key.tag = &tag;
		if (vary) {
			key.vary = kstring_from2(vary, len);
		}
		auto it = node->get_first()->childs.find(&key);
		if (!it) {
			return nullptr;
		}
		return it->value();
	}
	khttpd::KXmlNode* new_xml(const char* name, size_t len, const char* vary, size_t vary_len) {
		auto key_tag = kstring_from2(name, len);
		auto it = qname_config.find(key_tag);
		if (it) {
			auto tag = it->value();
			kstring_release(key_tag);
			key_tag = kstring_refs(tag->tag);
		}
		kgl_ref_str_t* key_vary = nullptr;
		if (vary) {
			key_vary = kstring_from2(vary, vary_len);
		}
		return new khttpd::KXmlNode(key_tag, key_vary);
	}
	khttpd::KXmlNode* new_xml(const char* name, size_t len) {
		khttpd::KXmlKey key(name, len);
		auto it = qname_config.find(key.tag);
		if (it) {
			auto tag = it->value();
			kstring_release(key.tag);
			key.tag = kstring_refs(tag->tag);
		}
		return new khttpd::KXmlNode(&key);
	}
	khttpd::KXmlNode* find_child(khttpd::KXmlNode* node, const char* name, size_t len) {
		khttpd::KXmlKey key(name, len);
		auto it2 = qname_config.find(key.tag);
		if (it2) {
			auto tag = it2->value();
			key.tag->id = tag->tag->id;
		}
		auto it = node->get_first()->childs.find(&key);
		if (!it) {
			return nullptr;
		}
		return it->value();
	}
	uint16_t register_qname(const char* name, size_t len) {
		auto key = new khttpd::KXmlKey(name, len);
		auto old_key = qname_config.add(key->tag, key);
		if (old_key) {
			key->tag->id = old_key->tag->id;
			delete old_key;
			return key->tag->id;
		}
		key->tag->id = cur_know_qname_id--;
		return key->tag->id;
	}
	void init(on_begin_parse_f cb) {
		locker = kfiber_mutex_init();
		register_qname(_KS("worker_thread"));
		register_qname(_KS("dso_extend@name"));
		register_qname(_KS("server@name"));
		register_qname(_KS("api@name"));
		register_qname(_KS("cmd@name"));
		register_qname(_KS("vhs"));
		register_qname(_KS("vh@name"));
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
	KConfigResult remove(KConfigFile* file, const kgl_str_t& path, uint32_t index) {
		const char* name = (const char*)kgl_memrchr(path.data, '/', path.len);
		if (!name) {
			name = path.data;
		}
		size_t path_len = name - path.data;

		auto xml = kconfig::new_xml(name, path.len - path_len);
		defer(xml->release());
		if (!file->update(path.data, path_len, index, xml, kconfig::EvRemove)) {
			return KConfigResult::ErrNotFound;
		}
		if (!file->save()) {
			return KConfigResult::ErrSaveFile;
		}
		return KConfigResult::Success;
	}
	KConfigResult remove(const std::string& file_name, const kgl_str_t& path, uint32_t index) {
		KFiberLocker lock(locker);
		kgl_str_t file2{ (char*)file_name.c_str(),file_name.size() };
		auto file = find_file(file2);
		if (!file) {
			return KConfigResult::ErrNotFound;
		}
		return remove(file, path, index);
	}
	KConfigResult remove(const kgl_str_t& path, uint32_t index) {
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
	KConfigResult add(const kgl_str_t& path, uint32_t index, khttpd::KXmlNode* xml) {
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
	KConfigFile* find_file(const kgl_str_t& name) {
		kgl_ref_str_t name2;
		name2.data = name.data;
		name2.len = (uint16_t)name.len;
		auto it = config_files.find(&name2);
		if (!it) {
			return nullptr;
		}
		return it->value();
	}
	KConfigFile* add_file(kgl_ref_str_t* name, kgl_ref_str_t* filename) {
		return nullptr;
	}
	KConfigResult update(KConfigFile* file, const kgl_str_t& path, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type) {
		const char* name = (const char*)kgl_memrchr(path.data, '/', path.len);
		if (!name) {
			name = path.data;
		}
		size_t path_len = name - path.data;
		if (!file->update(path.data, path_len, index, xml, ev_type)) {
			return KConfigResult::ErrNotFound;
		}
		if (!file->save()) {
			return KConfigResult::ErrSaveFile;
		}
		return KConfigResult::Success;
	}
	KConfigResult update(const kgl_str_t& path, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type) {
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
	KConfigResult update(const kgl_str_t& path, uint32_t index, const std::string& text, KXmlAttribute* attribute, KConfigEventType ev_type) {
		const char* name = (const char*)kgl_memrchr(path.data, '/', path.len);
		if (!name) {
			name = path.data;
		}
		size_t path_len = name - path.data;
		auto xml = kconfig::new_xml(name, path.len - path_len);
		if (!text.empty()) {
			xml->get_last()->character = kstring_from2(text.c_str(), text.size());
		}
		if (attribute) {
			xml->get_last()->attributes.swap(*attribute);
		}
		defer(xml->release());
		return update(path, index, xml, ev_type);
	}
	KConfigResult add(const std::string& file, const kgl_str_t& path, uint32_t index, khttpd::KXmlNode* xml) {
		return update(file, path, index, xml, EvNew);
	}
	KConfigResult update(const std::string& file_name, const kgl_str_t& path, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type) {
		KFiberLocker lock(locker);
		kgl_str_t file2{ (char*)file_name.c_str(),file_name.size() };
		auto file = find_file(file2);
		if (!file) {
			return KConfigResult::ErrNotFound;
		}
		return update(file, path, index, xml, ev_type);
	}
	KFiberLocker lock() {
		return KFiberLocker(locker);
	}
	void test() {
		return;
		struct test_config_context
		{
			int ev_count;
			int int_value;
			int vh_count;
			int attr_value;
			KXmlBodyDiff diff;
		};
		int prev_ev;
		register_qname(_KS("a"));
		register_qname(_KS("b"));
		register_qname(_KS("c"));
		KConfigTree test_ev(nullptr, _KS("config"));
		test_config_context ctx;
		memset(&ctx, 0, sizeof(ctx));
		KConfigFile* t = new KConfigFile(&test_ev, nullptr, nullptr);
		defer(t->release());

		assert(listen(_KS("int"), &ctx, [](void* data, KConfigTree* tree, KConfigEvent* ev) {
			auto xml = ev->get_xml();
			test_config_context* ctx = (test_config_context*)data;
			ctx->ev_count++;
			if (ev->type != EvRemove) {
				assert(strcmp(xml->key.tag->data, "int") == 0);
				ctx->int_value = atoi(xml->get_text());
				ctx->attr_value = atoi(xml->attributes()["v"].c_str());
			}
			}, ev_self, &test_ev));
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

		listen(_KS("/vh2"), &ctx, [](void* data, KConfigTree* tree, KConfigEvent* ev) {
			test_config_context* ctx = (test_config_context*)data;
			ctx->ev_count++;
			if (ev->type == (EvSubDir | EvRemove)) {
				ctx->vh_count--;
			} else if (ev->type == (EvSubDir | EvNew)) {
				ctx->vh_count++;
			}
			}, ev_subdir, &test_ev);
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

		assert(listen(_KS(""), &ctx, [](void* data, KConfigTree* tree, KConfigEvent* ev) {
			test_config_context* ctx = (test_config_context*)data;
			ctx->ev_count++;
			}, ev_subdir, &test_ev));

		t->reload(_KS("<config><int><aa/></int></config>"));
		prev_ev = ctx.ev_count;
		t->reload(_KS("<config><int><aa name='aa'/></int></config>"));
		assert(ctx.ev_count == prev_ev + 1);
		/* test vector */
		assert(remove_listen(_KS("vh2"), &test_ev));
		assert(remove_listen(_KS(""), &test_ev));
		ctx.vh_count = 0;
		assert(listen(_KS("/vh"), &ctx, [](void* data, KConfigTree* tree, KConfigEvent* ev) {
			auto xml = ev->get_xml();
			test_config_context* ctx = (test_config_context*)data;
			ctx->ev_count++;
			assert(xml);
			if (!xml->is_tag(_KS("vh"))) {
				return;
			}
			if (ev->type == (EvSubDir | EvRemove)) {
				ctx->vh_count--;
			} else if (ev->type == (EvSubDir | EvNew)) {
				ctx->vh_count++;
			}
			}, ev_subdir, &test_ev));
		t->reload(_KS("<config></config>"));

		assert(ctx.vh_count == 0);
		t->reload(_KS("<config><vh name='b'/><vh name='a'/><dd/></config>"));
		assert(ctx.vh_count == 2);
		t->reload(_KS("<config><vh name='c'/><vh name='a'/><vh/></config>"));
		assert(ctx.vh_count == 3);
		//test diff
		ctx.int_value = 0;
		assert(listen(_KS("listen"), &ctx, [](void* data, KConfigTree* tree, KConfigEvent* ev) {
			test_config_context* ctx = (test_config_context*)data;
			ctx->ev_count++;
			auto xml = ev->new_xml;
			int int_value = ctx->int_value;
			ctx->int_value = 0;
			if (xml) {
				for (uint32_t index = 0;; ++index) {
					auto body = xml->get_body(index);
					if (!body) {
						break;
					}
					int id = body->attributes.get_int("id");
					ctx->int_value += id;
				}
			}
			ctx->diff = ev->diff;
			for (uint32_t index = ev->diff.from; index < ev->diff.old_to; ++index) {
				auto body = ev->old_xml->get_body(index);
				assert(body);
				int id = body->attributes.get_int("id");
				int_value -= id;
			}
			for (uint32_t index = ev->diff.from; index < ev->diff.new_to; ++index) {
				auto body = ev->new_xml->get_body(index);
				assert(body);
				int id = body->attributes.get_int("id");
				int_value += id;
			}
			assert(int_value == ctx->int_value);
			assert(ev->get_xml()->is_tag(_KS("listen")));
			}, ev_self, &test_ev));
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
	}
}
