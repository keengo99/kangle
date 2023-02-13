#ifndef KCONFIGTREE_H_INCLUDED
#define KCONFIGTREE_H_INCLUDED
#include <map>
#include <string>
#include "krbtree.h"
#include "kmalloc.h"
#include "KXmlDocument.h"
#define CONFIG_FILE_SIGN  "<!--configfileisok-->"
#ifndef CONFIG_FILE
#define CONFIG_FILE 		"/config.xml"
#define CONFIG_DEFAULT_FILE "/config-default.xml"
#endif
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

	class KStackName
	{
	public:
		KStackName(int max_level) {
			names = (kgl_ref_str_t**)malloc(sizeof(kgl_ref_str_t*) * (max_level + 1));
			this->max_level = max_level;
			cur_level = 0;
		}
		~KStackName() {
			while (auto v = pop()) {
				kstring_release(v);
			}
			xfree(names);
		}
		bool push(kgl_ref_str_t* name) {
			if (cur_level >= max_level) {
				return false;
			}
			names[cur_level++] = name;
			return true;
		}
		kgl_ref_str_t* pop() {
			if (cur_level == 0) {
				return nullptr;
			}
			cur_level--;
			return names[cur_level];
		}
		kgl_ref_str_t** get() {
			names[cur_level] = nullptr;
			return names;
		}
		int max_level;
		int cur_level;
		kgl_ref_str_t** names;
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
	typedef void (*listen_callback)(KConfigTree* tree, KXmlNode* xml, KConfigEventType ev);
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
		KConfigTree(KConfigTree* parent, kgl_ref_str_t* key) {
			this->name = kstring_refs(key);
			this->parent = parent;
			init();
		}
		KConfigTree(KConfigTree* parent, const char* key, size_t len) {
			this->name = kstring_from2(key, len);
			this->parent = parent;
			init();
		}
		~KConfigTree();
		KConfigTree *add(kgl_ref_str_t** names, KConfigListen* ev);
		KConfigListen* remove(kgl_ref_str_t** names);
		void remove_node(KMapNode<KConfigTree>* node);
		KMapNode<KConfigTree>* find_child(kgl_ref_str_t* name, bool create_flag);
		KConfigTree* find(const char** names, size_t* len);
		int cmp(kgl_ref_str_t* key) {
			return kgl_cmp(name->data, name->len, key->data, key->len);
			//return name.cmp(a);
		}
		void clean();
		bool empty() {
			return !child && !ev && !wide_ev && !node;
		}

		void notice(KConfigListen* ev, KConfigFile* file, KXmlNode* xml, KConfigEventType ev_type);
		kgl_ref_str_t* name;
		KMap<kgl_ref_str_t, KConfigTree>* child;
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
	class KConfigFile
	{
	public:
		KConfigFile(KConfigTree* ev, const std::string& filename) {
			this->filename = kstring_from2(filename.c_str(), filename.size());
			this->ref = 1;
			this->ev = ev;
		}
		KConfigFile(KConfigTree* ev, kgl_ref_str_t *filename) {
			this->filename = kstring_refs(filename);
			this->ref = 1;
			this->ev = ev;
		}
		void release() {
			if (katom_dec((void*)&ref) == 0) {
				delete this;
			}			
		}
		int cmp(kgl_ref_str_t* key) {
			return kgl_cmp(filename->data, filename->len, key->data, key->len);
		}
		KConfigFile* add_ref() {
			katom_inc((void*)&ref);
			return this;
		}
		bool is_removed() {
			return remove_flag;
		}
		KXmlNode* load();
		void clear() {
			update(nullptr);
		}
		bool reload();
		bool reload(const char* str, size_t len);
		uint16_t get_index() {
			return filename->id;
		}
		void set_remove_flag(bool flag) {
			this->remove_flag = flag;
		}
		int get_source() {
			return (int)source;
		}
		void set_default_config() {
			is_default_config = 1;
		}
		void update(KXmlNode* new_nodes);
		bool update(const char* name, size_t size, int index, KXmlNode* xml, KConfigEventType ev_type);
		KXmlNode* nodes = nullptr;
		KConfigTree* ev;
		kgl_ref_str_t* filename;
		time_t last_modified = 0;
	private:
		union
		{
			struct
			{
				uint32_t remove_flag : 1;
				uint32_t is_default_config : 1;
				uint32_t readonly : 1;
				uint32_t source : 4;
			};
			uint32_t flags = 0;
		};
		volatile uint32_t ref;
		~KConfigFile() {
			if (nodes) {
				nodes->release();
			}
			kstring_release(filename);
		}
		bool save();
		bool diff(KConfigTree* name, KXmlNode* o, KXmlNode* n);
		bool diff_nodes(KConfigTree* name, KMap<KXmlKey, KXmlNode>* o, KMap<KXmlKey, KXmlNode>* n);
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
	KConfigTree *listen(const char* name, size_t size, KConfigListen* ls);
	KConfigListen* remove_listen(const char* name, size_t size);
	bool update(const char* name, size_t size, int index, KConfigFile* file, KXmlNode* xml, KConfigEventType ev_type);
	KConfigTree* find(const char** name, size_t* size);
	void reload();
	void init();
	void lock();
	void unlock();
	uint16_t register_qname(const char* name, size_t len);
	KXmlNode* find_child(KXmlNode* node, const char* name, size_t len);
	KXmlNode* find_child(KXmlNode* node, uint16_t name_id, const char* vary, size_t len);
	KXmlNode* parse_xml(char* buf);
	void shutdown();
	void test();
};
#endif
