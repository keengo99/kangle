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
#include "KConfig.h"


namespace kconfig {
	constexpr int max_file_size = 1048576;
	static KConfigTree events(nullptr, _KS("config"));
	static uint16_t cur_know_qname_id = 1;
	KMap<kgl_ref_str_t, KXmlKey> qname_config;
	KMap<kgl_ref_str_t, KConfigFile> config_files;
	class KConfigFileInfo
	{
	public:
		KConfigFileInfo(KConfigFile* file) {
			this->file = file->add_ref();
			buf = nullptr;
			last_modified = 0;
		}
		~KConfigFileInfo() {
			file->release();
			if (buf) {
				free(buf);
			}
		}
		int cmp(kgl_ref_str_t* a) {
			int result = (int)a->id - (int)file->filename->id;
			if (result != 0) {
				return result;
			}			
			return file->cmp(a);
		}
		bool load() {
			auto fp = kfiber_file_open(file->filename->data, fileRead, KFILE_ASYNC);
			if (!fp) {
				return false;
			}
			defer(kfiber_file_close(fp));
			this->last_modified = last_modified;
			auto size = (int)kfiber_file_size(fp);
			if (size > max_file_size) {
				klog(KLOG_ERR, "config file [%s] is too big. size=[%d]\n", file->filename->data, size);
				return false;
			}
			buf = (char*)malloc(size + 1);
			if (!buf) {
				return false;
			}
			buf[size] = '\0';
			if (!kfiber_file_read_full(fp, buf, &size)) {
				return false;
			}
			KExtConfigDynamicString ds(file->filename->data);
			ds.dimModel = false;
			ds.blockModel = false;
			ds.envChar = '%';
			char* content = ds.parseDirect(buf);
			free(buf);
			buf = content;
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
			file->filename->id = id;
			return true;
		}
		void parse() {
			file->set_remove_flag(false);
			auto nodes = parse_xml(buf);
			file->update(nodes);
			file->last_modified = last_modified;
		}
		KConfigFile* file;
		time_t last_modified;
		char* buf;
	};
	//KMap<kgl_ref_str_t, KConfigFileInfo> config_files;

	KConfigTree::~KConfigTree() {
		clean();
		if (node) {
			delete node;
		}
		kstring_release(name);
	}

	bool update_xml_node(KXmlNode* nodes, const char* name, size_t size, int index, KXmlNode* xml, KConfigEventType ev_type) {
		size_t name_len;
		while (size > 1 && *name == '/') {
			name++;
			size--;
		}
		if (size <= 0 || *name == '/') {
			switch (ev_type) {
			case KConfigEventType::New:
				nodes->insert(xml, index);
				return true;
			case KConfigEventType::Update:
				return nodes->update(&xml->key, index, xml);
			case KConfigEventType::Remove:
				return nodes->update(&xml->key, index, nullptr);
			default:
				return false;
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
	void KConfigTree::clean() {
		if (child) {
			child->iterator(config_tree_clean_iterator, NULL);
			delete child;
			child = nullptr;
		}
	}
	KConfigTree* KConfigTree::find(const char** name, size_t* size) {
		size_t name_len;
		while (*size > 1 && **name == '/') {
			(*name)++;
			(*size)--;
		}
		if (*size <= 0 || **name == '/') {
			return this;
		}
		auto path = (const char*)memchr(*name, '/', *size);
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
	KConfigTree *KConfigTree::add(kgl_ref_str_t** name, KConfigListen* ev) {
		if (!*name) {
			if (this->ev) {
				return nullptr;
			}
			this->ev = ev;
			return this;
		}
		if ('*' == *(*name)->data) {
			if (this->wide_ev) {
				return nullptr;
			}
			this->wide_ev = ev;
			return this;
		}
		int new_flag = 0;
		if (!child) {
			child = new KMap<kgl_ref_str_t, KConfigTree>();
		}
		auto node = child->insert(*name, &new_flag);
		if (new_flag) {
			node->value(new KConfigTree(this, *name));
		}
		return node->value()->add(name + 1, ev);
	}
	KConfigListen* KConfigTree::remove(kgl_ref_str_t** name) {
		if (!*name) {
			KConfigListen* result = ev;
			this->ev = nullptr;
			return result;
		}
		//if ((*name)->is_wide()) {
		if ('*' == *(*name)->data) {
			KConfigListen* result = wide_ev;
			wide_ev = nullptr;
			return result;
		}
		if (!child) {
			return nullptr;
		}
		auto node = child->find(*name);
		if (!node) {
			return nullptr;
		}
		KConfigTree* rn = node->value();
		KConfigListen* ret = rn->remove(name + 1);
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
	void KConfigTree::notice(KConfigListen* listener, KConfigFile* file, KXmlNode* xml, KConfigEventType ev_type) {
		assert(listener);
		assert(xml);
		KConfigEventNode* file_node;
		KConfigEventNode* last_node;
		if (ev_type != KConfigEventType::None) {
			klog(KLOG_NOTICE, "notice %s%s%s  [%d] tree=[%p] xml=[%p]\n",
				xml->key.tag->data,
				xml->key.vary ? "@" : "",
				xml->key.vary ? xml->key.vary->data : "",
				ev_type,
				this,
				xml);
		}

		if (ev_type != KConfigEventType::New) {
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
			if (ev_type == KConfigEventType::Remove) {
				//remove
				if (!last_node) {
					node = file_node->next;
					if (node && !listener->is_merge()) {
						xml = node->xml;
						ev_type = KConfigEventType::Update;
					}
				} else {
					last_node = file_node->next;
				}
				if (!last_node || listener->is_merge()) {
					listener->on_event(this, xml, ev_type);
				}
				delete file_node;
				return;
			}
			//update
			file_node->update(xml);
			if (ev_type == KConfigEventType::None) {
				return;
			}
			assert(ev_type == KConfigEventType::Update);
			if (!last_node || listener->is_merge()) {
				listener->on_event(this, xml, ev_type);
			}
			return;
		}
		last_node = node;
		//new
		assert(ev_type == KConfigEventType::New);
		while (last_node && file->get_index() > last_node->file->get_index()) {
			last_node = last_node->next;
			continue;
		}
		file_node = new KConfigEventNode(file, xml);
		if (!last_node) {
			file_node->next = node;
			if (node && !listener->is_merge()) {
				//o = (*node)->xml;
				ev_type = KConfigEventType::Update;
			}
			node = file_node;
		} else {
			file_node->next = last_node->next;
			last_node->next = file_node;
		}
		if (!last_node || listener->is_merge()) {
			listener->on_event(this, xml, ev_type);
		}
		return;
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
	KMapNode<KConfigTree>* KConfigTree::find_child(kgl_ref_str_t* name, bool create_flag) {
		if (!child) {
			if (!create_flag) {
				return nullptr;
			}
			child = new KMap<kgl_ref_str_t, KConfigTree>();
		}
		if (create_flag) {
			int new_flag;
			auto it = child->insert(name, &new_flag);
			if (new_flag) {
				it->value(new KConfigTree(this, name));
			}
			return it;
		}
		return child->find(name);
	}
	KXmlNode* KConfigFile::load() {
		auto fp = kfiber_file_open(this->filename->data, fileRead, KFILE_ASYNC);
		if (!fp) {
			return nullptr;
		}
		defer(kfiber_file_close(fp));
		this->last_modified = last_modified;
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
	bool KConfigFile::diff_nodes(KConfigTree* name, KMap<KXmlKey, KXmlNode>* o, KMap<KXmlKey, KXmlNode>* n) {
		bool result = false;
		if (!o) {
			assert(n != nullptr);
			assert(name);
			for (auto it = n->first(); it; it = it->next()) {
				auto node = it->value();
				if (diff(name, nullptr, node)) {
					result = true;
				}
			}
			return result;
		}
		if (!n) {
			assert(o != nullptr);
			assert(name);
			for (auto it = o->last(); it; it = it->prev()) {
				auto node = it->value();
				if (diff(name, node, nullptr)) {
					result = true;
				}
			}
			return result;
		}
		for (auto itn = n->first(); itn; itn = itn->next()) {
			auto node = itn->value();
			auto it = o->find(&node->key);
			if (it) {
				auto child = it->value();
				if (diff(name, child, node)) {
					result = true;
					if (!name) {
						return true;
					}
				}
				child->release();
				o->erase(it);
				continue;
			}
			if (diff(name, nullptr, node)) {
				result = true;
				if (!name) {
					return true;
				}
			}
			return name;
		};
		if (!name && !o->empty()) {
			return true;
		}
		for (auto it = o->last(); it; it = it->prev()) {
			auto node = it->value();
			if (diff(name, node, nullptr)) {
				result = true;
			}
		}
		return result;
	}
	bool KConfigFile::diff(KConfigTree* ev_node, KXmlNode* o, KXmlNode* n) {
		KConfigListen* listener = nullptr;
		KMapNode<KConfigTree>* child_node = nullptr;
		if (ev_node) {
			KXmlKey* xml = o ? &o->key : &n->key;
			kgl_ref_str_t* key;
			if (xml->vary) {
				child_node = ev_node->find_child(xml->tag, false);
				if (child_node) {
					ev_node = child_node->value();
				} else {
					ev_node = nullptr;
				}
				key = xml->vary;
			} else {
				key = xml->tag;
			}
			if (ev_node) {
				child_node = ev_node->find_child(key, ev_node->wide_ev);
				if (child_node) {
					auto child_tree = child_node->value();
					if (child_tree->ev) {
						listener = child_tree->ev;
						child_node = nullptr;
					} else {
						listener = ev_node->wide_ev;
					}
					ev_node = child_tree;
				} else {
					ev_node = nullptr;
				}
			}
		}
		if (!o) {
			if (listener) {
				assert(ev_node);
				ev_node->notice(listener, this, n, KConfigEventType::New);
			}
			if (ev_node) {
				diff_nodes(ev_node, nullptr, &n->childs);
			}
			return !listener;
		}
		if (!n) {
			if (ev_node) {
				diff_nodes(ev_node, &o->childs, nullptr);
			}
			if (listener) {
				assert(ev_node);
				ev_node->notice(listener, this, o, KConfigEventType::Remove);
			}
			if (child_node) {
				assert(ev_node);
				assert(ev_node->parent);
				if (ev_node->empty()) {
					ev_node->parent->remove_node(child_node);
				}
			}
			return !listener;
		}
		auto o_next = o;
		auto n_next = n;
		while (o_next && n_next) {
			if (diff_nodes(ev_node, &o_next->childs, &n_next->childs) || !o_next->is_same(n_next)) {
				break;
			}
			o_next = o_next->next;
			n_next = n_next->next;
		}
		bool is_same = (!o_next && !n_next);
		if (listener) {
			assert(ev_node);
			ev_node->notice(listener, this, n, is_same ? KConfigEventType::None : KConfigEventType::Update);
			return false;
		}
		return !is_same;
	}
	KXmlNode* parse_xml(char* buf) {
		KXmlDocument document;
		document.set_qname_config(&qname_config);
		auto nodes = document.parse(buf);
		if (nodes) {
			return nodes->add_ref();
		}
		return nullptr;
	}
	bool KConfigFile::reload(const char* str, size_t size) {
		//printf("now update xml=[%s]\n", str);
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
	bool KConfigFile::update(const char* name, size_t size, int index, KXmlNode* xml, KConfigEventType ev_type) {
		if (readonly) {
			return false;
		}
		auto nodes = this->nodes->clone();
		if (!update_xml_node(nodes, name, size, index, xml, ev_type)) {
			nodes->release();
			return false;
		}
		update(nodes);
		return save();
	}
	bool KConfigFile::save() {
		return false;
	}
	void KConfigFile::update(KXmlNode* new_nodes) {
		diff_nodes(ev, nodes ? &nodes->childs : nullptr, new_nodes ? &new_nodes->childs : nullptr);
		if (nodes) {
			nodes->release();
		}
		nodes = new_nodes;
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
	KConfigListen* remove_listen(kgl_ref_str_t** name, KConfigTree* ev_tree) {
		return ev_tree->remove(name);
	}
	KConfigTree *listen(kgl_ref_str_t** name, KConfigListen* listen, KConfigTree* ev_tree) {
		return ev_tree->add(name, listen);
	}
	bool convert(KStackName* names, const char* name, size_t size) {
		size_t lable_size;
		while (size > 0) {
			if (*name == '/') {
				name++;
				size--;
			}
			const char* p = (const char*)memchr(name, '/', size);
			if (p == NULL) {
				lable_size = size;
			} else {
				lable_size = p - name;
			}
			if (lable_size > 255) {
				return false;
			}
			names->push(kstring_from2(name, lable_size));
			if (*name == '*') {
				//* must at end
				break;
			}
			name += lable_size;
			size -= lable_size;
		}
		return true;
	}
	KConfigTree *listen(const char* name, size_t size, KConfigListen* ev, KConfigTree* ev_tree) {
		KStackName names(31);
		auto result = convert(&names, name, size);
		if (!result) {
			return nullptr;
		}
		return kconfig::listen(names.get(), ev, ev_tree);
	}
	KConfigTree *listen(const char* name, size_t size, KConfigListen* ev) {
		return listen(name, size, ev, &events);
	}
	bool update(const char* name, size_t size, int index, KConfigFile* file, KXmlNode* xml, KConfigEventType type) {
		if (!file || !file->nodes) {
			return false;
		}
		return file->update(name, size, index, xml, type);

	}
	KConfigTree* find(const char** name, size_t* size) {
		return events.find(name, size);
	}

	KConfigListen* remove_listen(const char* name, size_t size, KConfigTree* ev_tree) {
		KStackName names(31);
		auto result = convert(&names, name, size);
		if (!result) {
			return nullptr;
		}
		return remove_listen(names.get(), ev_tree);
	}
	KConfigListen* remove_listen(const char* name, size_t size) {
		return remove_listen(name, size, &events);
	}
	struct kgl_ext_config_context
	{
		KMap<kgl_ref_str_t, KConfigFileInfo> files;
		std::string dir;
	};
	void load_config_file(KFileName* file, kgl_ext_config_context *ctx, bool is_default=false) {
		time_t last_modified = file->getLastModified();
		kgl_ref_str_t* name = kstring_from(file->getName());
		defer(kstring_release(name));
		int new_flag;
		auto it = config_files.insert(name, &new_flag);
		//printf("file [%s] new_flag=[%d]\n",name->data,new_flag);
		KConfigFileInfo* info;
		KConfigFile* cfg_file;
		if (new_flag) {
			cfg_file = new KConfigFile(&events, name);
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
			info = new KConfigFileInfo(cfg_file);
			if (!info->load()) {
				delete info;
				return;
			}
			info->last_modified = last_modified;
			ctx->files.add(info->file->filename, info);
		} else {
			cfg_file->set_remove_flag(false);
		}
	}
	int handle_ext_config_dir(const char* file, void* param) {
		kgl_ext_config_context* ctx = (kgl_ext_config_context*)param;
		KFileName configFile;
		if (!configFile.setName(ctx->dir.c_str(), file, FOLLOW_LINK_ALL)) {
			return 0;
		}
		if (configFile.isDirectory()) {
			KFileName dirConfigFile;
			if (!dirConfigFile.setName(configFile.getName(), "config.xml", FOLLOW_LINK_ALL)) {
				return 0;
			}
			load_config_file(&dirConfigFile, ctx);
			return 0;
		}
		load_config_file(&configFile, ctx);
		return 0;
	}
	void reload() {
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
		load_config_file(&file, &ctx, true);

#ifdef KANGLE_EXT_DIR
		ctx.dir = KANGLE_EXT_DIR;
#else
		ctx.dir = conf.path + "/ext";
#endif		
		list_dir(ctx.dir.c_str(), handle_ext_config_dir, (void*)&ctx);
		ctx.dir = conf.path + "etc/vh.d/";
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
				//printf("file [%s] is removed\n", file->filename->data);
				file->clear();
				file->release();
				return iterator_remove_continue;
			}
			return iterator_continue;
		}, NULL);
	}
	KXmlNode* find_child(KXmlNode* node, uint16_t name_id, const char* vary, size_t len) {
		KXmlKey key;
		kgl_ref_str_t tag = { 0 };
		tag.id = name_id;
		tag.ref = 2;
		key.tag = &tag;
		if (vary) {
			key.vary = kstring_from2(vary, len);
		}
		auto it = node->childs.find(&key);
		if (!it) {
			return nullptr;
		}
		return it->value();
	}
	KXmlNode* find_child(KXmlNode* node, const char* name, size_t len) {
		KXmlKey key(name, len);
		auto it2 = qname_config.find(key.tag);
		if (it2) {
			auto tag = it2->value();
			key.tag->id = tag->tag->id;
		}
		auto it = node->childs.find(&key);
		if (!it) {
			return nullptr;
		}
		return it->value();
	}
	uint16_t register_qname(const char* name, size_t len) {
		auto key = new KXmlKey(name, len);		
		auto old_key = qname_config.add(key->tag, key);
		if (old_key) {
			delete key;
			return old_key->tag->id;
		}
		key->tag->id = cur_know_qname_id++;
		return key->tag->id;
	}
	void init() {
		register_qname(_KS("dso_extend@name"));
		register_qname(_KS("vh@name"));	
	}
	void shutdown() {
		config_files.iterator([](void* data, void* arg) {
			KConfigFile* file = (KConfigFile*)data;
			file->clear();
			file->release();
			return iterator_remove_continue;
		}, NULL);
		qname_config.iterator([](void* data, void* arg) {
			delete (KXmlKey*)data;
			return iterator_remove_continue;
			}, NULL);	
		events.clean();
	}
	void lock() {

	}
	void unlock() {

	}
	void test() {
		return;
		struct test_config_context
		{
			int ev_count;
			int int_value;
			int vh_count;
			int attr_value;
		};
		int prev_ev;
		
		register_qname(_KS("a"));
		register_qname(_KS("b"));
		register_qname(_KS("c"));
		KConfigTree test_ev(nullptr, _KS("config"));
		test_config_context ctx;
		memset(&ctx, 0, sizeof(ctx));
		KConfigFile* t = new KConfigFile(&test_ev, "");
		defer(t->release());

		auto listener = config_listen([&](KConfigTree* tree, KXmlNode* xml, KConfigEventType ev) {
			ctx.ev_count++;
			if (ev != KConfigEventType::Remove) {
				assert(strcmp(xml->key.tag->data, "int") == 0);
				ctx.int_value = atoi(xml->getCharacter().c_str());
				ctx.attr_value = atoi(xml->attributes["v"].c_str());
			}
			});
		listen(_KS("int"), &listener, &test_ev);

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
		auto listener2 = config_listen([&](KConfigTree* tree, KXmlNode* xml, KConfigEventType ev) {
			ctx.ev_count++;
			if (ev == KConfigEventType::Remove) {
				ctx.vh_count--;
			} else if (ev == KConfigEventType::New) {
				ctx.vh_count++;
			}
			});

		listen(_KS("/vh2/*"), &listener2, &test_ev);
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
		auto listen3 = config_listen([&](KConfigTree* file, KXmlNode* xml, KConfigEventType ev) {
			ctx.ev_count++;
			});
		assert(listen(_KS("/*"), &listen3, &test_ev));
		t->reload(_KS("<config><int><aa/></int></config>"));
		prev_ev = ctx.ev_count;
		t->reload(_KS("<config><int><aa name='aa'/></int></config>"));
		assert(ctx.ev_count == prev_ev + 1);
		return;
		/* test vector */
		assert(remove_listen(_KS("/vh2/*"), &test_ev));
		assert(remove_listen(_KS("/*"), &test_ev));
		ctx.vh_count = 0;


		auto listen4 = config_listen([&](KConfigTree* tree, KXmlNode* xml, KConfigEventType ev) {
			ctx.ev_count++;
			assert(xml);
			if (kgl_cmp(xml->key.tag->data, xml->key.tag->len, _KS("vh")) != 0) {
				return;
			}
			if (ev == KConfigEventType::Remove) {
				ctx.vh_count--;
			} else if (ev == KConfigEventType::New) {
				ctx.vh_count++;
			}
			});
		assert(listen(_KS("/vh/*"), &listen4, &test_ev));
		t->reload(_KS("<config></config>"));

		assert(ctx.vh_count == 0);
		t->reload(_KS("<config><vh name='b'/><vh name='a'/><dd/></config>"));
		assert(ctx.vh_count == 2);
		t->reload(_KS("<config><vh name='c'/><vh name='a'/><vh/></config>"));
		assert(ctx.vh_count == 3);
	}
}
