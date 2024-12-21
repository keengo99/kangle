#ifndef KSTATUSCODEACL_H
#define KSTATUSCODEACL_H
#define ACL_OP_EQ   0
#define ACL_OP_LT   1
#define ACL_OP_GT   2


class KStatusCodeAcl: public KAcl
{
public:
	KStatusCodeAcl()
	{
		op = 0;
		code = 0;
	}
	~KStatusCodeAcl()
	{
	}
	KAcl *new_instance() override {
		return new KStatusCodeAcl();
	}
	const char *getName() override {
		return "status_code";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		if (obj && obj->data) {
			switch (op) {
			case ACL_OP_EQ:
				return obj->data->i.status_code==code;
			case ACL_OP_LT:
				return obj->data->i.status_code<code;
			case ACL_OP_GT:
				return obj->data->i.status_code>code;
			}
		}
		return false;
	}
	void get_html(KWStream& s) override {
		KStatusCodeAcl *m = (KStatusCodeAcl *)this;
		s << "code:<select name='op'>";
		for(int i=0;i<3;i++){
			s << "<option value='" << getMarkOp(i) << "' ";
			if (m && m->op==i) {
				s << "selected";
			}
			s << ">" << getMarkOp2(i) << "</option>";		
		}
		s << "</select>\r\n";
		s << " <input name=code value='";
		if (m) {
			s << (int)m->code;
		}
		s << "'>";
	}
	void get_display(KWStream& s) override {
		s << getMarkOp2(op) << code;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		op = getMarkOp(attribute["op"].c_str());
		code = atoi(attribute["code"].c_str());
	}
private:
	const char *getMarkOp(int op)
	{
		switch(op){
		case ACL_OP_LT:
			return "lt";
		case ACL_OP_EQ:
			return "eq";
		case ACL_OP_GT:
			return "gt";
		}
		return "eq";
	}
	int getMarkOp(const char *op)
	{
		if(strcasecmp(op,"lt")==0){
			return ACL_OP_LT;
		}
		if(strcasecmp(op,"gt")==0){
			return ACL_OP_GT;
		}
		return ACL_OP_EQ;
	}
	const char *getMarkOp2(int op)
	{
		switch (op) {
		case ACL_OP_EQ:
			return "=";
		case ACL_OP_LT:
			return "&lt;";
		case ACL_OP_GT:
			return "&gt;";
		}
		return "=";
	}
	int op;
	int code;
};
#endif
