#ifndef KCONFIGTREE_H_INCLUDED
#define KCONFIGTREE_H_INCLUDED
#include <map>
#include <string>
#include "krbtree.h"
#include "kmalloc.h"
#include "KXmlDocument.h"

typedef unsigned char* domain_t;
int kgl_domain_cmp(domain_t s1, domain_t s2);
namespace kconfig {
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
	class KConfigFile
	{
	public:
		KConfigFile(KConfigTree* ev, std::string filename) {
			this->filename.swap(filename);
			this->id = 0;
			this->ev = ev;
		}
		~KConfigFile() {
			if (nodes) {
				nodes->release();
			}
		}
		KXmlNode* load();
		bool reload();
		bool reload(const char* str, size_t len);
		uint32_t id;
		uint32_t index = 50;
		time_t last_modified = 0;
		KXmlNode* nodes = nullptr;
		std::string filename;
	private:
		void update(KXmlNode* new_nodes);
		KXmlNode* parse_xml(char* buf, size_t len);
		KConfigTree* ev;
		bool diff(KConfigTree* name, KXmlNode* o, KXmlNode* n);
		bool diff_nodes(KConfigTree* name, KMap<KXmlKey, KXmlNode>* o, KMap<KXmlKey, KXmlNode>* n);
	};
	class KConfigEvent;
	typedef void (*kconfig_event_callback)(void* data, KConfigFile* file, KXmlNode* o, KXmlNode* n);

	class KConfigEventNode
	{
	public:
		KConfigEventNode() {

		}
		~KConfigEventNode() {
		}
		void update(uint32_t file_id, uint32_t index) {
			this->file_id = file_id;
			this->index = index;
		}
		uint32_t file_id = 0;
		uint32_t index = 0;
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
		void notice(KConfigFile* file, KXmlNode* o, KXmlNode* n);
		union
		{
			KConfigEventNode* node;
			KMap<KXmlKey, KConfigEventDir>* child;
		};
		void* data;
		kconfig_event_callback cb;
		event_flag flag;
	private:
		void notice(KConfigEventNode** node, KConfigFile* file, KXmlNode* o, KXmlNode* n);
	};
	bool listen(const char* name, size_t size, void* data, kconfig_event_callback cb);
	void reload();
	void init();
	void shutdown();
	void test();
};
#endif
