#ifndef KVARYMARK_H
#define KVARYMARK_H
class KVaryMark : public KMark
{
public:
	KVaryMark()
	{
		header = NULL;
	}
	~KVaryMark()
	{
		if (header) {
			xfree(header);
		}
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		if (header == NULL) {
			return KF_STATUS_REQ_FALSE;
		}
		kassert(obj);
		if (KBIT_TEST(obj->index.flags, OBJ_HAS_VARY)) {
			return KF_STATUS_REQ_FALSE;
		}
		if (!obj->AddVary(rq, header, header_len)) {
			obj->insert_http_header(kgl_header_vary, header, header_len);
		}
		return KF_STATUS_REQ_TRUE;
	}
	void get_display(KWStream& s) override {
		if (header) {
			s << header;
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		if (header) {
			xfree(header);
		}
		header = strdup(attribute["header"].c_str());
		header_len = strlen(header);
	}
	void get_html(KWStream& s) override {
		s << "header:<input name='header' value='";
		if (header) {
			s << header;
		}
		s << "'>";
	}
	KMark* new_instance() override {
		return new KVaryMark;
	}
	const char* getName() override {
		return "vary";
	}
private:
	char* header;
	int header_len;
};
#endif
