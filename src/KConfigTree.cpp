#include <string.h>
#include "KConfigTree.h"
#include "kmalloc.h"
#include "KConfig.h"
#include "kfiber.h"
#include "KDefer.h"
#include "KXmlDocument.h"
#include "klog.h"
#include "KHttpLib.h"


namespace kconfig {
	constexpr int max_file_size = 1048576;
	static KConfigTree events(nullptr, _KS("config"));
	KConfigFile* file = nullptr;
	KMap<kgl_ref_str_t, KXmlKey> config_vary;
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
		KXmlKey xml_key(name, name_len);
		nodes = nodes->find_child(&xml_key);
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
	bool KConfigTree::add(kgl_ref_str_t** name, KConfigListen* ev) {
		if (!*name) {
			if (this->ev) {
				return false;
			}
			this->ev = ev;
			return true;
		}
		if ('*' == *(*name)->data) {
			if (this->wide_ev) {
				return false;
			}
			this->wide_ev = ev;
			return true;
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
			klog(KLOG_ERR, "notice %s%s%s  [%d] tree=[%p] xml=[%p]\n",
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
		auto fp = kfiber_file_open(this->filename.c_str(), fileRead, KFILE_ASYNC);
		if (!fp) {
			return nullptr;
		}
		defer(kfiber_file_close(fp));
		this->last_modified = last_modified;
		auto size = (int)kfiber_file_size(fp);
		if (size > max_file_size) {
			klog(KLOG_ERR, "config file [%s] is too big. size=[%d]\n", this->filename.c_str(), size);
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
		return parse_xml(buf, size);
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
	KXmlNode* KConfigFile::parse_xml(char* buf, size_t len) {
		KXmlDocument document;
		document.set_vary(&config_vary);
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
		auto tree_node = parse_xml(buf, size);
		if (!tree_node) {
			return false;
		}
		update(tree_node);
		return true;
	}
	bool KConfigFile::update(const char* name, size_t size, int index, KXmlNode* xml, KConfigEventType ev_type) {
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
		time_t last_modified = kfile_last_modified(this->filename.c_str());
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
	bool listen(kgl_ref_str_t** name, KConfigListen* listen, KConfigTree* ev_tree) {
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
	bool listen(const char* name, size_t size, KConfigListen* ev, KConfigTree* ev_tree) {
		KStackName names(31);
		auto result = convert(&names, name, size);
		if (!result) {
			return false;
		}
		return kconfig::listen(names.get(), ev, ev_tree);
	}
	bool listen(const char* name, size_t size, KConfigListen* ev) {
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
	void reload() {
		file->reload();
	}
	void set_name_vary(const char* name, size_t len, uint16_t index) {
		auto key = new KXmlKey(name, len);
		key->tag->id = index;
		auto old_key = config_vary.insert(key->tag, key);
		if (old_key) {
			delete old_key;
		}
	}
	void init() {
		set_name_vary(_KS("vh@name"));
		assert(file == nullptr);
		std::string configFile = conf.path + "/etc/test.xml";
		file = new KConfigFile(&events, configFile);
		auto listener = new KConfigListenImp<listen_callback>([](KConfigTree* tree, KXmlNode* xml, KConfigEventType ev) {

			}, false);
		listen(_KS("int"), listener, &events);
		listen(_KS("/vh/*"), listener, &events);
	}
	void shutdown() {
		config_vary.iterator([](void* data, void* arg) {
			delete (KXmlKey*)data;
			return iterator_remove_continue;
			}, NULL);
		file->release();
		file = nullptr;
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
		set_name_vary(_KS("a"), 10);
		set_name_vary(_KS("b"), 2);
		set_name_vary(_KS("c"), 6);
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
