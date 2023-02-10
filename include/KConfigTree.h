#ifndef KCONFIGTREE_H_INCLUDED
#define KCONFIGTREE_H_INCLUDED
#include <map>
#include <string>
#include "krbtree.h"
#include "kmalloc.h"
#include "KXmlDocument.h"

namespace kconfig {
	class KConfigFile;
	class KConfigEvent;
	class KConfigTree;
	class KConfigEventNode;
	enum class KConfigEventType
	{
		None,
		New,
		Update,
		Remove
	};
	template <typename T>
	class KStackName
	{
	public:
		KStackName(int max_level) {
			names = (T**)malloc(sizeof(T*) * (max_level + 1));
			this->max_level = max_level;
			cur_level = 0;
		}
		~KStackName() {
			while (auto v = pop()) {
				delete v;
			}
			xfree(names);
		}
		bool push(T* name) {
			if (cur_level >= max_level) {
				return false;
			}
			names[cur_level++] = name;
			return true;
		}
		T* pop() {
			if (cur_level == 0) {
				return nullptr;
			}
			cur_level--;
			return names[cur_level];
		}
		T** get() {
			names[cur_level] = nullptr;
			return names;
		}
		int max_level;
		int cur_level;
		T** names;
	};
	class KConfigListen
	{
	public:
		virtual ~KConfigListen() {
		}
		virtual void on_event(KConfigTree* tree, KXmlNode* xml, KConfigEventType ev) = 0;
		virtual bool is_merge() {
			return false;
		}
	};

	template <typename T>
	class KConfigListenImp : public KConfigListen
	{
	public:
		KConfigListenImp(T f, bool merge_flag) : f(f) {
			this->merge_flag = merge_flag;
		}
		void on_event(KConfigTree* tree, KXmlNode* xml, KConfigEventType ev) override {
			f(tree, xml, ev);
		}
		bool is_merge()  override {
			return merge_flag;
		}
		T f;
		bool merge_flag;
	};
	template <typename F>
	KConfigListenImp<F> config_listen(F f, bool merge_flag = false) {
		return KConfigListenImp<F>(f, merge_flag);
	}
	class KConfigTree
	{
	public:
		KConfigTree(KConfigTree* parent, KXmlKey* key) :name(key->tag, key->vary) {
			this->parent = parent;
			init();
		}
		KConfigTree(KConfigTree* parent, const char* key, size_t len) :name(key, len) {
			this->parent = parent;
			init();
		}
		~KConfigTree();
		bool add(KXmlKey** names, KConfigListen* ev);
		KConfigListen* remove(KXmlKey** names);
		void remove_node(KMapNode<KConfigTree>* node);
		KMapNode<KConfigTree>* find_child(KXmlKey* name, bool create_flag);
		//void remove_child(KMapNode<KConfigTree>* node);
		//bool add_child(KConfigTree* n);
		int cmp(KXmlKey* a) {
			return name.cmp(a);
		}
		void clean();
		bool empty() {
			return !child && !ev && !wide_ev && !node;
		}
		void notice(KConfigListen* ev, KConfigFile* file, KXmlNode* xml, KConfigEventType ev_type);
		KXmlKey name;
		KMap<KXmlKey, KConfigTree>* child;
		KConfigTree* parent;
		KConfigListen* wide_ev;
		KConfigListen* ev;
		KConfigEventNode* node;
	private:
		void init() {
			child = nullptr;
			this->ev = nullptr;
			this->wide_ev = nullptr;
			this->node = nullptr;
		}

	};
	class KNodeList
	{
	public:
		KNodeList(int index) {
			this->index = index;
		}
		int cmp(int* key) {
			return *key - index;
		}
		int index;
		std::list<KXmlNode*> nodes;
	};
	class KIndexNode
	{
	public:
		~KIndexNode() {
			nodes.iterator([](void* data, void* arg) {
				delete (KNodeList*)data;
				return iterator_remove_continue;
				}, NULL);
		}
		template <typename F>
		void iterator(F f) {
			for (auto it = nodes.first(); it; it = it->next()) {
				auto node_list = it->value();
				for (auto it2 = node_list->nodes.begin(); it2 != node_list->nodes.end(); it2++) {
					if (!f((*it2))) {
						return;
					}
				}
			}
		}
		template <typename F>
		void riterator(F f) {
			for (auto it = nodes.last(); it; it = it->prev()) {
				auto node_list = it->value();
				for (auto it2 = node_list->nodes.begin(); it2 != node_list->nodes.end(); it2++) {
					if (!f((*it2))) {
						return;
					}
				}
			}
		}
		KMap<int, KNodeList> nodes;
	};
	class KConfigFile
	{
	public:
		KConfigFile(KConfigTree* ev, std::string filename) {
			this->filename.swap(filename);
			this->ref = 1;
			this->ev = ev;
		}
		void release() {
			if (katom_dec((void*)&ref) == 0) {
				delete this;
			}
		}
		KConfigFile* add_ref() {
			katom_inc((void*)&ref);
			return this;
		}
		KXmlNode* load();
		void clear() {
			update(nullptr);
		}
		bool reload();
		bool reload(const char* str, size_t len);
		uint32_t get_index() {
			return index;
		}
	private:
		~KConfigFile() {
			if (nodes) {
				nodes->release();
			}
		}
		void update(KXmlNode* new_nodes);
		KXmlNode* parse_xml(char* buf, size_t len);
		KConfigTree* ev;
		KXmlNode* nodes = nullptr;
		uint32_t index = 50;
		volatile uint32_t ref;
		time_t last_modified = 0;
		std::string filename;
		bool diff(KConfigTree* name, KXmlNode* o, KXmlNode* n);
		bool diff_nodes(KConfigTree* name, KMap<KXmlKey, KXmlNode>* o, KMap<KXmlKey, KXmlNode>* n);
		void convert_nodes(KMap<KXmlKey, KXmlNode>* nodes, KIndexNode& index_nodes);
	};
	class KConfigEventNode
	{
	public:
		KConfigEventNode(KConfigFile* file, KXmlNode* xml) {
			this->xml = xml->add_ref();
			this->file = file->add_ref();
		}
		~KConfigEventNode() {
			assert(xml);
			xml->release();
			file->release();
		}
		void update(KXmlNode* xml) {
			this->xml->release();
			this->xml = xml->add_ref();
		}
		KConfigFile* file;
		KXmlNode* xml;
		KConfigEventNode* next = nullptr;
	};
	bool listen(const char* name, size_t size, KConfigListen* ls);
	bool update(const char* name, size_t size, KXmlNode* xml, KConfigEventType ev_type);
	void reload();
	void init();
	void set_name_vary(const char* name, size_t len);
	void set_name_index(const char* name, size_t len, int index);
	void shutdown();
	void test();
};
#endif
