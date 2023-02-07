#include <string.h>
#include "KConfigTree.h"
#include "kmalloc.h"
#include "KConfig.h"
#include "kfiber.h"
#include "KDefer.h"
#include "KXmlDocument.h"
#include "klog.h"
#include "KHttpLib.h"


int kgl_domain_cmp(domain_t s1, domain_t s2) {
	return kgl_cmp((char*)(s1 + 1), *s1, (char*)(s2 + 1), *s2);
#if 0
	unsigned char n;
	int  m, z;
	if (*s1 <= *s2) {
		n = *s1;
		z = -1;
	} else {
		n = *s2;
		z = 1;
	}
	m = memcmp(s1 + 1, s2 + 1, n);
	if (m || *s1 == *s2) {
		return m;
	}
	return z;
#endif
}
namespace kconfig {
	constexpr int max_file_size = 1048576;
	static KConfigTree events(_KS("config"));
	KConfigFile* file = nullptr;
	std::map<std::string, std::string> config_vary;
	template<typename T>
	int config_tree_find_cmp2(void* k, void* k2) {
		kgl_ref_str_t* s1 = (kgl_ref_str_t*)k;
		T* s2 = (T*)k2;
		return kgl_cmp(s1->data, s1->len, (char*)s2->name + 1, *s2->name);
	}
	template<typename T>
	int config_tree_find_cmp(void* k, void* k2) {
		domain_t s1 = (domain_t)k;
		T* s2 = (T*)k2;
		return kgl_domain_cmp(s1, s2->name);
	}
	KConfigTree::~KConfigTree() {
		clean();
		assert(empty());
		delete child;
	}
	iterator_ret config_tree_clean_iterator(void* data, void* argv) {
		KConfigTree* rn = (KConfigTree*)data;
		delete rn;
		return iterator_remove_continue;
	}
	void KConfigTree::clean() {
		data = NULL;
		if (child) {
			child->iterator(config_tree_clean_iterator, NULL);
			delete child;
			child = nullptr;
		}
		if (wide_leaf) {
			delete wide_leaf;
			wide_leaf = nullptr;
		}
	}
#if 0
	bool KConfigTree::add_child(KConfigTree* n) {
		if (!child) {
			child = new KMap<KXmlKey, KConfigTree>();
		}
		int new_flag = 0;
		auto it = child->insert(&n->name, &new_flag);
		if (!new_flag) {
			return false;
		}
		it->value(n);
		return true;
	}
#endif
	KConfigTree* KConfigTree::add(KXmlKey** name) {
		if (!*name) {
			return this;
		}
		if ((*name)->is_wide()) {
			if (!wide_leaf) {
				wide_leaf = new KConfigTree(*name);
			}
			return wide_leaf->add(name + 1);
		}
		int new_flag = 0;
		if (!child) {
			child = new KMap<KXmlKey, KConfigTree>();
		}
		auto node = child->insert(*name, &new_flag);
		if (new_flag) {
			node->value(new KConfigTree(*name));
		}
		return node->value()->add(name + 1);
	}
	void* KConfigTree::remove(KXmlKey** name) {
		if (!*name) {
			void* result = data;
			this->data = nullptr;
			return result;
		}
		if ((*name)->is_wide()) {
			if (!wide_leaf) {
				return nullptr;
			}
			void* ret = wide_leaf->remove(name + 1);
			if (!ret) {
				return nullptr;
			}
			if (wide_leaf->empty()) {
				delete wide_leaf;
				wide_leaf = nullptr;
			}
			return ret;
		}
		if (!child) {
			return nullptr;
		}
		auto node = child->find(*name);
		if (!node) {
			return nullptr;
		}
		KConfigTree* rn = node->value();
		void* ret = rn->remove(name + 1);
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
	KConfigTree* KConfigTree::find_child(KXmlKey* name) {
		if (!child) {
			return wide_leaf;
		}
		auto node = child->find(name);
		if (!node) {
			return wide_leaf;
		}
		return node->value();
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
				if (diff(name, nullptr, it->value())) {
					result = true;
				}
			}
			return result;
		}
		if (!n) {
			assert(o != nullptr);
			assert(name);
			for (auto it = o->first(); it; it = it->next()) {
				if (diff(name, it->value(), nullptr)) {
					result = true;
				}
			}
			return result;
		}
		for (auto it = n->first(); it; it = it->next()) {
			auto node = it->value();
			auto it2 = o->find(&node->key);
			if (it2) {
				auto child = it2->value();
				if (diff(name, child, node)) {
					if (!name) {
						return true;
					}
					result = true;
				}
				while (child) {
					child->parent = NULL;
					child = child->next;
				}
				it2->value()->release();
				o->erase(it2);
			} else {
				if (!name) {
					return true;
				}
				if (diff(name, nullptr, node)) {
					result = true;
				}
			}
		}
		if (!name && !o->empty()) {
			return true;
		}
		for (auto it = o->first(); it; it = it->next()) {
			if (diff(name, it->value(), nullptr)) {
				result = true;
			}
		}
		return result;
	}
	bool KConfigFile::diff(KConfigTree* ev_node, KXmlNode* o, KXmlNode* n) {
		KConfigEvent* notice_ev = nullptr;
		if (ev_node) {
			ev_node = ev_node->find_child(o ? &o->key : &n->key);
			if (ev_node) {
				notice_ev = (KConfigEvent*)ev_node->data;
			}
		}
		if (!o) {
			if (notice_ev) {
				notice_ev->notice(this, nullptr, n);
			}
			if (ev_node) {
				diff_nodes(ev_node, nullptr, &n->childs);
			}
			return !notice_ev;
		}
		if (!n) {
			if (ev_node) {
				diff_nodes(ev_node, &o->childs, nullptr);
			}
			if (notice_ev) {
				notice_ev->notice(this, o, nullptr);
			}
			return !notice_ev;
		}
		auto o_next = o;
		auto n_next = n;
		while (o_next && n_next) {
			if (diff_nodes(ev_node, &o_next->childs, &n_next->childs) || !o_next->is_same(n_next)) {
				if (notice_ev) {
					notice_ev->notice(this, o, n);
				}
				return !notice_ev;
			}
			o_next = o_next->next;
			n_next = n_next->next;
		}
		if (o_next || n_next) {
			if (notice_ev) {
				notice_ev->notice(this, o, n);
			}
			return !notice_ev;
		}
		return false;
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
	void KConfigEvent::notice(KConfigEventNode** node, KConfigFile* file, KXmlNode* o, KXmlNode* n) {
		if (is_merge()) {
			cb(data, file, o, n);
			return;
		}
		KConfigEventNode* last_node = nullptr;
		if (!o) {
			//new
			if (!*node) {
				*node = new KConfigEventNode();
			} else if (file->index >= (*node)->index) {
				return;
			}
			(*node)->update(file->id, file->index);
			cb(data, file, o, n);
			return;
		}
		if (!*node) {
			return;
		}
		if (file->id != (*node)->file_id && file->index >= (*node)->index) {
			return;
		}
		(*node)->update(file->id, file->index);
		cb(data, file, o, n);
		if (!n) {
			delete (*node);
			*node = nullptr;
			return;
		}
	}
	void KConfigEvent::notice(KConfigFile* file, KXmlNode* o, KXmlNode* n) {
		KXmlNode* xml = o ? o : n;
		klog(KLOG_ERR, "notice %s/%s%s%s  [%s] [%p] o=[%p] n=[%p]\n",
			xml->parent ? xml->parent->key.tag->data : "",
			xml->key.tag->data,
			xml->key.vary ? "@" : "",
			xml->key.vary ? xml->key.vary->data : "",
			(o ? (n ? "update" : "remove") : "new"),
			this,
			o,
			n);
		if (is_dir()) {
			KMapNode<KConfigEventDir>* node;
			KConfigEventDir* dir;
			if (!n) {
				node = child->find(&xml->key);
				if (!node) {
					return;
				}
				dir = (KConfigEventDir*)node->data;
			} else {
				int new_flag = 0;
				node = child->insert(&xml->key, &new_flag);
				if (new_flag) {
					dir = new KConfigEventDir(&xml->key);
					node->value(dir);
				} else {
					dir = node->value();
				}
			}
			notice(&dir->node, file, o, n);
			if (!dir->node) {
				child->erase(node);
				delete dir;
			}
			return;
		}
		notice(&node, file, o, n);
	}
	bool remove_listen(KXmlKey** name, KConfigTree* ev_tree) {
		void* data = ev_tree->remove(name);
		if (data) {
			delete (KConfigEvent*)data;
			return true;
		}
		return false;
	}
	bool listen(KXmlKey** name, void* data, kconfig_event_callback cb, event_flag flag, KConfigTree* ev_tree) {
		auto tree = ev_tree->add(name);
		if (!tree) {
			return false;
		}
		if (tree->data) {
			return false;
		}
		if (tree->name.is_wide()) {
			flag |= event_is_dir;
		}
		KConfigEvent* ev = new KConfigEvent(data, cb, flag);
		tree->data = ev;
		return true;
	}
	bool convert(KStackName<KXmlKey>* names, const char* name, size_t size, event_flag& flag) {
		size_t lable_size;
		while (size > 0) {
			if (*name == '/') {
				name++;
				size--;
			}
			if (*name == '!') {
				//merge
				flag = event_is_merge;
				break;
			}
			/*
			if (*name == '*') {
				flag = event_is_dir;
				break;
			}
			*/
			const char* p = (const char*)memchr(name, '/', size);
			if (p == NULL) {
				lable_size = size;
			} else {
				lable_size = p - name;
			}
			if (lable_size > 255) {
				return false;
			}
			names->push(new KXmlKey(name, lable_size));
			name += lable_size;
			size -= lable_size;
		}
		return true;
	}
	bool listen(const char* name, size_t size, void* data, kconfig_event_callback cb, KConfigTree* ev_tree) {
		event_flag is_dir = 0;
		KStackName<KXmlKey> names(31);
		auto result = convert(&names, name, size, is_dir);
		defer(while (auto v = names.pop()) {
			delete v;
		});
		if (!result) {
			return false;
		}
		return listen(names.get(), data, cb, is_dir, ev_tree);
	}
	bool listen(const char* name, size_t size, void* data, kconfig_event_callback cb) {
		return listen(name, size, data, cb, &events);
	}
	bool remove_listen(const char* name, size_t size, KConfigTree* ev_tree) {
		event_flag is_dir = 0;
		KStackName<KXmlKey> names(31);
		auto result = convert(&names, name, size, is_dir);
		defer(while (auto v = names.pop()) {
			delete v;
		});
		if (!result) {
			return false;
		}
		return remove_listen(names.get(), ev_tree);
	}
	void reload() {
		//file->reload();
	}
	void init() {
		config_vary["vh"] = "name";
		assert(file == nullptr);
		std::string configFile = conf.path + "/etc/test.xml";
		file = new KConfigFile(&events, configFile);
#if 0
		listen((domain_t)"\0", NULL, [](void* data, KXmlNode* node, KConfigEventType type) {
			printf("config [%s] event =[%d]\n", node->tag.c_str(), type);
			}, &events);
		listen(_KS("vh"), NULL, [](void* data, KXmlNode* node, KConfigEventType type) {
			printf("vh [%s] event =[%d]\n", node->tag.c_str(), type);
			});
#endif
	}
	void shutdown() {

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
		KConfigTree test_ev(_KS("config"));
		test_config_context ctx;
		memset(&ctx, 0, sizeof(ctx));
		KConfigFile t(&test_ev, "");
		listen(_KS("int"), &ctx, [](void* data, KConfigFile* file, KXmlNode* o, KXmlNode* node) {
			test_config_context* v = (test_config_context*)data;
			v->ev_count++;
			if (node) {
				assert(strcmp(node->key.tag->data, "int") == 0);
				v->int_value = atoi(node->getCharacter().c_str());
				v->attr_value = atoi(node->attributes["v"].c_str());
			}
			}, &test_ev);

		t.reload(_KS("<config><int>3</int></config>"));
		assert(ctx.int_value == 3);
		assert(ctx.attr_value == 0);
		t.reload(_KS("<config><int>4</int></config>"));
		assert(ctx.int_value == 4);
		assert(ctx.attr_value == 0);
		t.reload(_KS("<config><int v='3'>4</int></config>"));
		assert(ctx.int_value == 4);
		assert(ctx.attr_value == 3);
		t.reload(_KS("<config/>"));

		listen(_KS("/vh/*"), &ctx, [](void* data, KConfigFile* file, KXmlNode* o, KXmlNode* n) {
			test_config_context* v = (test_config_context*)data;
			auto node = o ? o : n;
			v->ev_count++;
			if (!o) {
				v->vh_count++;
			} else if (!n) {
				v->vh_count--;
			}
			}, &test_ev);
		t.reload(_KS("<config><vh><a/><b/></vh></config>"));
		assert(ctx.vh_count == 2);
		//no change
		int prev_ev = ctx.ev_count;
		t.reload(_KS("<config><vh><a/><b/></vh></config>"));
		assert(prev_ev == ctx.ev_count);
		assert(ctx.vh_count == 2);

		t.reload(_KS("<config><vh><c><d/><d/></c><c/><b/></vh></config>"));
		assert(ctx.vh_count == 2);
		t.reload(_KS("<config/>"));
		assert(ctx.vh_count == 0);
		/* test subnode update */

		assert(remove_listen(_KS("int"), &test_ev));
		assert(listen(_KS("/*"), &ctx, [](void* data, KConfigFile* file, KXmlNode* o, KXmlNode* n) {
			test_config_context* v = (test_config_context*)data;
			v->ev_count++;
			}, &test_ev));
		t.reload(_KS("<config><int><aa/></int></config>"));
		prev_ev = ctx.ev_count;
		t.reload(_KS("<config><int><aa name='aa'/></int></config>"));
		assert(ctx.ev_count == prev_ev + 1);

		/* test vector */
		assert(remove_listen(_KS("/vh/*"), &test_ev));
		assert(remove_listen(_KS("/*"), &test_ev));
		ctx.vh_count = 0;
		assert(listen(_KS("*"), &ctx, [](void* data, KConfigFile* file, KXmlNode* o, KXmlNode* n) {
			test_config_context* v = (test_config_context*)data;
			v->ev_count++;
			auto node = o ? o : n;
			v->ev_count++;
			if (kgl_cmp(node->key.tag->data, node->key.tag->len, _KS("vh")) != 0) {
				return;
			}
			while (o) {
				assert(v->vh_count > 0);
				v->vh_count--;
				o = o->next;
			}
			while (n) {
				v->vh_count++;
				n = n->next;
			}
			}, &test_ev));
		t.reload(_KS("<config></config>"));

		assert(ctx.vh_count == 0);
		t.reload(_KS("<config><vh name='b'/><vh name='a'/><dd/></config>"));

		t.reload(_KS("<config><vh name='b'/><vh name='a'/><sss/></config>"));
		assert(ctx.vh_count == 2);
	}
}
