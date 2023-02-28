#ifndef KCONFIGTREE_H_INCLUDED
#define KCONFIGTREE_H_INCLUDED
#include <map>
#include <string>
#include "krbtree.h"
#include "kmalloc.h"
#include "KXmlDocument.h"
#include "KStackName.h"
#include "KStringBuf.h"
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
		//[from,to)
		uint32_t from;
		uint32_t old_to;
		uint32_t new_to;
	};
	struct KConfigEvent
	{
		KConfigFile* file;
		khttpd::KXmlNode* old_xml;
		khttpd::KXmlNode* new_xml;
		KXmlBodyDiff& diff;
		KConfigEventType type;
		khttpd::KXmlNode* get_xml() {
			return new_xml ? new_xml : old_xml;
		}
	};
	typedef void (*on_event_f)(void* data, KConfigTree* tree, KConfigEvent* ev);
	typedef bool (*on_begin_parse_f)(KConfigFile* file, khttpd::KXmlNode* node);
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
		~KConfigTree() noexcept;
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
		bool notice(KConfigFile* file, khttpd::KXmlNode* xml, KConfigEventType ev_type, KXmlBodyDiff& diff);

		kgl_ref_str_t* name;
		KMap<kgl_ref_str_t, KConfigTree>* child;
		KConfigTree* parent;
		void* data;
		on_event_f on_event;
		KConfigEventNode* node;
	private:
		void notice(KConfigTree* ev_tree, KConfigFile* file, khttpd::KXmlNode* xml,KConfigEventType ev_type, KXmlBodyDiff &diff);
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
#if 0
		KConfigFile(KConfigTree* ev, const std::string& filename) {
			this->filename = kstring_from2(filename.c_str(), filename.size());
			this->ref = 1;
			this->ev = ev;
		}
#endif
		KConfigFile(KConfigTree* ev, kgl_ref_str_t *name, kgl_ref_str_t* filename) {
			this->name = kstring_refs(name);
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
			return kgl_cmp(name->data, name->len, key->data, key->len);
		}
		KConfigFile* add_ref() {
			katom_inc((void*)&ref);
			return this;
		}
		bool is_removed() const {
			return remove_flag;
		}
		khttpd::KXmlNode* load();
		void clear() {
			update(nullptr);
		}
		bool reload();
		bool reload(const char* str, size_t len);
		void set_index(uint16_t id) {
			name->id = id;
		}
		uint16_t get_index() const {
			return name->id;
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
		const kgl_ref_str_t* get_name() const {
			return name;
		}
		const kgl_ref_str_t* get_filename() const {
			return filename;
		}
		bool save();
		void update(khttpd::KXmlNode* new_nodes);
		bool update(const char* name, size_t size, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type);
		khttpd::KXmlNode* nodes = nullptr;
		KConfigTree* ev;
		void set_last_modified(time_t last_modified) {
			this->last_modified = last_modified;
			need_save = 0;
		}
		time_t last_modified = 0;
	private:
		kgl_ref_str_t* name;
		kgl_ref_str_t* filename;
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
			kstring_release(name);
		}

		bool diff(KConfigTree* name, khttpd::KXmlNode* o, khttpd::KXmlNode* n, int* notice_count);
		bool diff_nodes(KConfigTree* name, KMap<khttpd::KXmlKey, khttpd::KXmlNode>* o, KMap<khttpd::KXmlKey, khttpd::KXmlNode>* n, int* notice_count);
	};
	class KConfigEventNode
	{
	public:
		KConfigEventNode(KConfigFile* file, khttpd::KXmlNode* xml) {
			this->xml = xml->add_ref();
			this->file = file->add_ref();
		}
		~KConfigEventNode() {
			assert(xml);
			xml->release();
			file->release();
		}
		khttpd::KXmlNode* update(khttpd::KXmlNode* xml) {
			auto old = this->xml;
			this->xml = xml->add_ref();
			return old;
		}
		KConfigFile* file;
		khttpd::KXmlNode* xml;
		KConfigEventNode* next = nullptr;
	};

	KConfigTree* listen(const char* name, size_t size, void* data, on_event_f on_event, KConfigEventFlag flags);
	void* remove_listen(const char* name, size_t size);

	KConfigResult remove(const std::string &file, const kgl_str_t& path, uint32_t index);
	KConfigResult remove(const kgl_str_t& path, uint32_t index);
	KConfigResult add(const kgl_str_t& path, uint32_t index, khttpd::KXmlNode* xml);
	KConfigResult update(const kgl_str_t &path, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type);
	KConfigResult update(const kgl_str_t& path, uint32_t index, const std::string &text, KXmlAttribute *attribute, KConfigEventType ev_type);

	KConfigResult add(const std::string& file, const kgl_str_t& path, uint32_t index, khttpd::KXmlNode* xml);
	KConfigResult update(const std::string& file, const kgl_str_t& path, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type);
	KConfigTree* find(const char** name, size_t* size);
	bool reload_config(kgl_ref_str_t* name, bool force);
	void reload();
	void init(on_begin_parse_f cb);
	uint16_t register_qname(const char* name, size_t len);
	khttpd::KXmlNode* new_xml(const char* name, size_t len);
	khttpd::KXmlNode* new_xml(const char* name, size_t len, const char* vary, size_t vary_len);
	khttpd::KXmlNode* find_child(khttpd::KXmlNode* node, const char* name, size_t len);
	khttpd::KXmlNode* find_child(khttpd::KXmlNode* node, uint16_t name_id, const char* vary, size_t len);
	khttpd::KXmlNode* parse_xml(char* buf);
	bool is_first();
	bool need_reboot();
	void set_need_reboot();
	void shutdown();
	void test();
};
#endif
