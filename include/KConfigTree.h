#ifndef KCONFIGTREE_H_INCLUDED
#define KCONFIGTREE_H_INCLUDED
#include <map>
#include <string>
#include "krbtree.h"
#include "kmalloc.h"
#include "KXmlDocument.h"
#include "KStackName.h"
#define CONFIG_FILE_SIGN  "<!--configfileisok-->"
#ifndef CONFIG_FILE
#define CONFIG_FILE 		"/config.xml"
#define CONFIG_DEFAULT_FILE "/config-default.xml"
#endif
namespace kconfig {
	class KConfigFile;
	class KConfigTree;
	class KConfigEventNode;

	using KConfigEventFlag = uint16_t;
	constexpr KConfigEventFlag ev_subdir = 1;
	constexpr KConfigEventFlag ev_self = 2;
	constexpr KConfigEventFlag ev_merge = 4;

	using KConfigEventType = int;
	constexpr KConfigEventType EvNone = 0;
	constexpr KConfigEventType EvNew = 1;
	constexpr KConfigEventType EvUpdate = 2;
	constexpr KConfigEventType EvRemove = 3;
	constexpr KConfigEventType EvSubDir = (1 << 4);
	constexpr KConfigEventType FlagCopyChilds = (1 << 5);
	constexpr KConfigEventType FlagCreate = (1 << 6);
	enum class KConfigResult
	{
		Success,
		ErrSaveFile,
		ErrNotFound,
		ErrUnknow
	};
	struct KXmlBodyDiff
	{
		uint32_t from;
		uint32_t old_to;
		uint32_t new_to;
	};
	struct KConfigEvent
	{
		KConfigFile* file;
		/* only type is update have old. */
		KXmlNode* old;
		KXmlNode* xml;
		KXmlBodyDiff *detail;
		KConfigEventType type;
	};
	typedef void (*on_event_f)(void* data, KConfigTree* tree, KConfigEvent* ev);
	typedef bool (*on_begin_parse_f)(KConfigFile* file, KXmlNode* node);
	class KConfigTree
	{
	public:
		KConfigTree(KConfigTree* parent, kgl_ref_str_t* key) {
			this->name = kstring_clone(key);
			this->parent = parent;
			init();
		}
		KConfigTree(KConfigTree* parent, const char* key, size_t len) {
			this->name = kstring_from2(key, len);
			this->parent = parent;
			init();
		}
		~KConfigTree();
		bool bind(void* data, on_event_f on_event, KConfigEventFlag flags);
		KConfigTree* add(const char* name, size_t len, void* data, on_event_f on_event, KConfigEventFlag flags);
		void* remove(const char* name, size_t len);
		void remove_node(KMapNode<KConfigTree>* node);
		KMapNode<KConfigTree>* find_child(kgl_ref_str_t* name);
		KConfigTree* find(const char** names, size_t* len);
		int cmp(const kgl_ref_str_t* key) const {
			return kgl_cmp(name->data, name->len, key->data, key->len);
		}
		void clean();
		bool empty() const {
			return !child && !data && !node;
		}
		bool is_subdir() const {
			return name->flags & ev_subdir;
		}
		bool is_merge() const {
			return name->flags & ev_merge;
		}
		bool is_self() const {
			return name->flags & ev_self;
		}
		bool notice(KConfigFile* file, KXmlNode* xml, KConfigEventType ev_type, KXmlBodyDiff *diff);

		kgl_ref_str_t* name;
		KMap<kgl_ref_str_t, KConfigTree>* child;
		KConfigTree* parent;
		void* data;
		on_event_f on_event;
		KConfigEventNode* node;
	private:
		void notice(KConfigTree* ev_tree, KConfigFile* file, KXmlNode* xml,KConfigEventType ev_type, KXmlBodyDiff* diff);
		void init() {
			child = nullptr;
			this->on_event = 0;
			this->data = nullptr;
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
		KConfigFile(KConfigTree* ev, kgl_ref_str_t* filename) {
			this->filename = kstring_refs(filename);
			this->ref = 1;
			this->ev = ev;
		}
		void release() {
			assert(katom_get((void*)&ref) < 0xfffffff);
			if (katom_dec((void*)&ref) == 0) {
				delete this;
			}
		}
		int cmp(const kgl_ref_str_t* key) const {
			return kgl_cmp(filename->data, filename->len, key->data, key->len);
		}
		KConfigFile* add_ref() {
			katom_inc((void*)&ref);
			return this;
		}
		bool is_removed() const {
			return remove_flag;
		}
		KXmlNode* load();
		void clear() {
			update(nullptr);
		}
		bool reload();
		bool reload(const char* str, size_t len);
		uint16_t get_index() const {
			return filename->id;
		}
		void set_remove_flag(bool flag) {
			this->remove_flag = flag;
		}
		int get_source() const {
			return (int)source;
		}
		void set_default_config() {
			default_config = 1;
		}
		bool is_default() const {
			return default_config;
		}
		bool save();
		void update(KXmlNode* new_nodes);
		bool update(const char* name, size_t size, int index, KXmlNode* xml, KConfigEventType ev_type);
		KXmlNode* nodes = nullptr;
		KConfigTree* ev;
		kgl_ref_str_t* filename;
		void set_last_modified(time_t last_modified) {
			this->last_modified = last_modified;
			need_save = 0;
		}
		time_t last_modified = 0;
	private:
		union
		{
			struct
			{
				uint32_t remove_flag : 1;
				uint32_t default_config : 1;
				uint32_t readonly : 1;
				uint32_t need_save : 1;
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

		bool diff(KConfigTree* name, KXmlNode* o, KXmlNode* n, int* notice_count);
		bool diff_nodes(KConfigTree* name, KMap<KXmlKey, KXmlNode>* o, KMap<KXmlKey, KXmlNode>* n, int* notice_count);
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
		KXmlNode* update(KXmlNode* xml) {
			auto old = this->xml;
			this->xml = xml->add_ref();
			return old;
		}
		KConfigFile* file;
		KXmlNode* xml;
		KConfigEventNode* next = nullptr;
	};

	KConfigTree* listen(const char* name, size_t size, void* data, on_event_f on_event, KConfigEventFlag flags);
	void* remove_listen(const char* name, size_t size);

	KConfigResult remove(const char* path, size_t path_len, int index);
	KConfigResult add(const char* path, size_t path_len, int index, KXmlNode* xml);
	KConfigResult update(const char* path, size_t path_len, int index, KXmlNode* xml, KConfigEventType ev_type);

	KConfigTree* find(const char** name, size_t* size);
	void reload();
	void init(on_begin_parse_f cb);
	void lock();
	void unlock();
	uint16_t register_qname(const char* name, size_t len);
	KXmlNode* new_xml(const char* name, size_t len);
	KXmlNode* new_xml(const char* name, size_t len, const char* vary, size_t vary_len);
	KXmlNode* find_child(KXmlNode* node, const char* name, size_t len);
	KXmlNode* find_child(KXmlNode* node, uint16_t name_id, const char* vary, size_t len);
	KXmlNode* parse_xml(char* buf);
	bool is_first();
	bool need_reboot();
	void set_need_reboot();
	void shutdown();
	void test();
};
#endif
