#ifndef KCONFIGTREE_H_INCLUDED
#define KCONFIGTREE_H_INCLUDED
#include <map>
#include <string>
#include "krbtree.h"
#include "kmalloc.h"
#include "KXmlDocument.h"

namespace kconfig {
	class KConfigFile;
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

	class KConfigTree
	{
	public:
		KConfigTree(KXmlKey* key) :name(key->tag, key->vary) {
			init();
		}
		KConfigTree(const char* key, size_t len) :name(key, len) {
			init();
		}
		~KConfigTree();
		KConfigTree* add(KXmlKey** names);
		void* remove(KXmlKey** names);
		KConfigTree* find_child(KXmlKey* name);
		//void remove_child(KMapNode<KConfigTree>* node);
		//bool add_child(KConfigTree* n);
		int cmp(KXmlKey* a) {
			return name.cmp(a);
		}
		void clean();
		KXmlKey name;
		KMap<KXmlKey, KConfigTree>* child;
		void* data;
		KConfigTree* wide_leaf;
	private:
		void init() {
			child = nullptr;
			this->data = nullptr;
			this->wide_leaf = nullptr;
		}
		bool empty() {
			return !child && !data && !wide_leaf;
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
		void convert_nodes(KMap<KXmlKey, KXmlNode>* nodes, KIndexNode &index_nodes);
	};
	class KConfigEvent;
	class KConfigEventNode;
	enum class KConfigEventType
	{
		New,
		Update,
		Remove
	};
	typedef void (*kconfig_event_callback)(void* data, KConfigEventNode* node, KXmlNode* xml, KConfigEventType ev);

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
	class KConfigEventDir
	{
	public:
		KConfigEventDir(KXmlKey* key) : name(key->tag, key->vary) {
			this->node = nullptr;
		}
		~KConfigEventDir() {
			if (node) {
				delete node;
			}
		}
		int cmp(KXmlKey* a) {
			return name.cmp(a);
		}
		KXmlKey name;
		KConfigEventNode* node;
	};
	typedef int event_flag;
	constexpr int event_is_dir = 1;
	constexpr int event_is_merge = 2;
	class KConfigEvent
	{
	public:
		KConfigEvent(void* data, kconfig_event_callback cb, event_flag flag) {
			this->data = data;
			this->cb = cb;
			this->flag = flag;
			node = nullptr;
			if (is_dir()) {
				child = new KMap<KXmlKey, KConfigEventDir>();
			}
		}
		~KConfigEvent() {
			if (node) {
				if (!is_dir()) {
					delete node;
				} else {
					child->iterator([](void* data, void* argv) ->iterator_ret {
						KConfigEventDir* dir = (KConfigEventDir*)data;
						delete dir;
						return iterator_remove_continue;
						}, NULL);
					delete child;
				}
			}
		}
		bool is_dir() {
			return (flag & event_is_dir) > 0;
		}
		bool is_merge() {
			return (flag & event_is_merge) > 0;
		}
		void notice(KConfigFile* file, KXmlNode* o, KXmlNode* n, bool is_same);
		union
		{
			KConfigEventNode* node;
			KMap<KXmlKey, KConfigEventDir>* child;
		};
		void* data;
		kconfig_event_callback cb;
		event_flag flag;
	private:
		void notice(KConfigEventNode** node, KConfigFile* file, KXmlNode* o, KXmlNode* n, bool is_same);
	};
	bool listen(const char* name, size_t size, void* data, kconfig_event_callback cb);
	bool update(const char* name, size_t size, KXmlNode* xml, KConfigEventType type);
	void reload();
	void init();
	void set_name_vary(const char* name, size_t len);
	void set_name_index(const char* name,size_t len, int index);
	void shutdown();
	void test();
};
#endif
