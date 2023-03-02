#ifndef KCONFIGTREE_H_INCLUDED
#define KCONFIGTREE_H_INCLUDED
#include <map>
#include <string>
#include "krbtree.h"
#include "kmalloc.h"
#include "KXmlDocument.h"
#include "KStackName.h"
#include "KStringBuf.h"
#include "KFiberLocker.h"
#include "ksapi.h"
#define CONFIG_FILE_SIGN  "<!--configfileisok-->"
#ifndef CONFIG_FILE
#define CONFIG_FILE 		"/config.xml"
#define CONFIG_DEFAULT_FILE "/config-default.xml"
#endif
namespace kconfig {
	class KConfigFile;
	class KConfigTree;
	class KConfigEventNode;

	constexpr int max_file_size = 10485760;

	using KConfigEventFlag = uint16_t;
	constexpr KConfigEventFlag ev_subdir = 1;
	constexpr KConfigEventFlag ev_self = 2;
	constexpr KConfigEventFlag ev_merge = 4;
	constexpr KConfigEventFlag ev_at_once = 8;
	constexpr KConfigEventFlag ev_skip_vary = 16;

	using KConfigEventType = int;
	constexpr KConfigEventType EvNone = 0;
	constexpr KConfigEventType EvNew = 1;
	constexpr KConfigEventType EvUpdate = 2;
	constexpr KConfigEventType EvRemove = 3;
	constexpr KConfigEventType EvSubDir = (1 << 4);
	constexpr KConfigEventType FlagCopyChilds = (1 << 5);
	constexpr KConfigEventType FlagCreate = (1 << 6);

	enum class KConfigFileSource : uint8_t
	{
		System = 0,
		Vh = 1,
		Htaccess = 2,
		Db = 3,
		Size = Db + 1,
	};

	enum class KConfigResult
	{
		Success,
		ErrSaveFile,
		ErrNotFound,
		ErrUnknow
	};
	struct KXmlChanged
	{
		khttpd::KXmlNode* old_xml;
		khttpd::KXmlNode* new_xml;
		kgl_config_diff* diff;
		KConfigEventType type;
		const khttpd::KXmlNode* get_xml() const {
			return new_xml ? new_xml : old_xml;
		}
	};
	struct KConfigEvent : public KXmlChanged
	{
		KConfigFile* file;
	};
	class KConfigListen
	{
	public:
		virtual ~KConfigListen() {

		}
		virtual KConfigEventFlag config_flag() const = 0;
		virtual bool on_config_event(KConfigTree* tree, KConfigEvent* ev) = 0;
	};

	template <typename F>
	class KConfigListenImp : public KConfigListen
	{
	public:
		KConfigListenImp(F f, KConfigEventFlag ev) : f(f) {
			this->ev = ev;
		}
		KConfigEventFlag config_flag() const {
			return ev;
		}
		bool on_config_event(KConfigTree* tree, KConfigEvent* ev) {
			return f(tree, ev);
		}
		~KConfigListenImp() {

		}
	private:
		F f;
		KConfigEventFlag ev;
	};
	template <typename F>
	KConfigListenImp<F> config_listen(F f, KConfigEventFlag flag) {
		return KConfigListenImp<F>(f, flag);
	}
	typedef void (*on_event_f)(void* data, KConfigTree* tree, KConfigEvent* ev);
	typedef bool (*on_begin_parse_f)(KConfigFile* file, khttpd::KXmlNode* node);
	class KConfigTree
	{
	public:
		KConfigTree(KConfigTree* parent, khttpd::KXmlKey* a) :KConfigTree(parent, a->tag, a->vary) {
		}
		KConfigTree(KConfigTree* parent, kgl_ref_str_t* tag, kgl_ref_str_t* vary) :key(kstring_refs(tag), kstring_refs(vary)) {
			this->parent = parent;
			init();
		}
		KConfigTree(KConfigTree* parent, const char* name, size_t len) :key(name, len) {
			this->parent = parent;
			init();
		}
		~KConfigTree() noexcept;
		bool bind(KConfigListen* ls);
		KConfigListen* unbind();
		KConfigTree* add(const char* name, size_t len, KConfigListen* ls);
		KConfigListen* remove(const char* name, size_t len);
		void remove_node(KMapNode<KConfigTree>* node);
		KMapNode<KConfigTree>* find_child(khttpd::KXmlKey* name);
		KConfigTree* find(const char** names, size_t* len);
		int cmp(const khttpd::KXmlKey* a) const {
			if (key.tag->len > 0) {
				int result = key.cmp(a->tag);
				if (result != 0) {
					return result;
				}
				if (is_skip_vary()) {
					return result;
				}
			}
			return kgl_string_cmp(key.vary, a->vary);
		}
		void clean();
		bool empty() const {
			return child.empty() && !ls && !node;
		}
		bool is_skip_vary() const {
			return key.vary == &skip_vary;
		}
		bool is_subdir() const {
			return ls && (ls->config_flag() & ev_subdir);
		}
		bool is_merge() const {
			return ls && (ls->config_flag() & ev_merge);
		}
		bool is_self() const {
			return ls && (ls->config_flag() & ev_self);
		}
		bool is_at_once() const {
			return ls && (ls->config_flag() & ev_at_once);
		}
		void check_at_once();
		bool notice(KConfigFile* file, khttpd::KXmlNode* xml, KConfigEventType ev_type, kgl_config_diff* diff);

		khttpd::KXmlKey key;
		KMap<khttpd::KXmlKey, KConfigTree> child;
		KConfigTree* parent;
		KConfigEventNode* node;
		KConfigListen* ls;
	private:
		void notice(KConfigTree* ev_tree, KConfigFile* file, khttpd::KXmlNode* xml, KConfigEventType ev_type, kgl_config_diff* diff);
		void init() {
			this->node = nullptr;
			this->ls = nullptr;
		}
		static kgl_refs_string skip_vary;
	};
	class KConfigFileSourceDriver
	{
	public:
		virtual ~KConfigFileSourceDriver() noexcept {
		}
		virtual khttpd::KSafeXmlNode load(KConfigFile* file) = 0;
		virtual time_t get_last_modified(KConfigFile* file) = 0;
		virtual bool enable_save(KConfigFile* file) {
			return true;
		}
		virtual bool save(KConfigFile* file, khttpd::KXmlNode* node) {
			return false;
		}
	};
	class KConfigFile
	{
	public:
		KConfigFile(KConfigTree* ev, kgl_ref_str_t* name, kgl_ref_str_t* filename) {
			this->name = kstring_refs(name);
			this->filename = kstring_refs(filename);
			this->ev = ev;
			this->source = KConfigFileSource::System;
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
		khttpd::KSafeXmlNode load();
		void clear() {
			update(nullptr);
		}
		bool reload(bool force);
		bool reload(const char* str, size_t len);
		void set_index(uint16_t index) {
			this->index = index;
		}
		uint16_t get_index() const {
			return index;
		}
		void set_remove_flag(bool flag) {
			this->remove_flag = flag;
		}
		KConfigFileSource get_source() const {
			return source;
		}
		void set_source(KConfigFileSource source) {
			this->source = source;
		}
		void set_default_config() {
			default_config = 1;
		}
		bool is_default() const {
			return default_config;
		}
		kgl_ref_str_t* get_name() const {
			return name;
		}
		kgl_ref_str_t* get_filename() const {
			return filename;
		}
		bool save();
		void update(khttpd::KSafeXmlNode new_nodes);
		bool update(const char* name, size_t size, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type);

		void set_last_modified(time_t last_modified) {
			this->last_modified = last_modified;
			need_save = 0;
		}
		time_t last_modified = 0;
	private:
		khttpd::KSafeXmlNode nodes;
		KConfigTree* ev;
		kgl_ref_str_t* name;
		kgl_ref_str_t* filename;
		union
		{
			struct
			{
				uint8_t remove_flag : 1;
				uint8_t default_config : 1;
				uint8_t readonly : 1;
				uint8_t need_save : 1;
				KConfigFileSource source;
			};
			uint16_t flags = 0;
		};
		uint16_t index = 50;
		volatile uint32_t ref = 1;
		~KConfigFile() {
			kstring_release(filename);
			kstring_release(name);
		}

		bool diff(KConfigTree* name, khttpd::KXmlNode* o, khttpd::KXmlNode* n, int* notice_count);
		bool diff_body(KConfigTree* name, khttpd::KXmlNodeBody* o, khttpd::KXmlNodeBody* n, int* notice_count);
	};
	using KSafeConfigFile = KSharedObj<KConfigFile>;
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

	KConfigTree* listen(const char* name, size_t size, KConfigListen* ls);
	KConfigListen* remove_listen(const char* name, size_t size);

	KConfigResult remove(const kgl_ref_str_t& file, const kgl_ref_str_t& path, uint32_t index);
	KConfigResult remove(const kgl_ref_str_t& path, uint32_t index);
	KConfigResult add(const kgl_ref_str_t& path, uint32_t index, khttpd::KXmlNode* xml);
	KConfigResult update(const kgl_ref_str_t& path, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type);
	KConfigResult update(const kgl_ref_str_t& path, uint32_t index, const std::string* text, KXmlAttribute* attribute, KConfigEventType ev_type);

	KConfigResult add(const kgl_ref_str_t& file, const kgl_ref_str_t& path, uint32_t index, khttpd::KXmlNode* xml);
	KConfigResult update(const kgl_ref_str_t& file, const kgl_ref_str_t& path, uint32_t index, khttpd::KXmlNode* xml, KConfigEventType ev_type);
	KConfigTree* find(const char** name, size_t* size);
	KConfigTree* find(const char* name, size_t size);
	bool remove_config_file(const kgl_ref_str_t* name);
	bool add_config_file(kgl_ref_str_t* name, kgl_ref_str_t* filename, KConfigTree* tree, KConfigFileSource source);
	bool reload_config(kgl_ref_str_t* name, bool force);
	void reload();
	void init(on_begin_parse_f cb);
	uint32_t register_qname(const char* name, size_t len);
	khttpd::KSafeXmlNode new_xml(const char* name, size_t len);
	khttpd::KSafeXmlNode new_xml(const char* name, size_t len, const char* vary, size_t vary_len);
	khttpd::KXmlNode* find_child(const khttpd::KXmlNodeBody* node, const char* name, size_t len);
	khttpd::KXmlNode* find_child(const khttpd::KXmlNodeBody* node, uint32_t name_id, const char* vary, size_t len);
	khttpd::KSafeXmlNode parse_xml(char* buf);
	void register_source_driver(KConfigFileSource s, KConfigFileSourceDriver* dr);
	KFiberLocker lock();
	bool is_first();
	bool need_reboot();
	void set_need_reboot();
	void shutdown();
	bool test();
};
#endif
