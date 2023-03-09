#ifndef KMARKACL_H
#define KMARKACL_H
#define MARKACL_OP_EQ   0
#define MARKACL_OP_LT   1
#define MARKACL_OP_GT   2


class KMarkAcl: public KAcl
{
public:
	KMarkAcl()
	{
		op = 0;
		mark = 0;
	}
	~KMarkAcl()
	{
	}
	KAcl *new_instance() override {
		return new KMarkAcl();
	}
	const char *getName() override {
		return "mark";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override  {
		switch (op) {
		case MARKACL_OP_EQ:
			return rq->sink->data.mark==mark;
		case MARKACL_OP_LT:
			return rq->sink->data.mark<mark;
		case MARKACL_OP_GT:
			return rq->sink->data.mark>mark;
		}
		return false;
	}
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		KMarkAcl *m = (KMarkAcl *)model;
		s << "mark:<select name='op'>";
		for(int i=0;i<3;i++){
			s << "<option value='" << getMarkOp(i) << "' ";
			if (m && m->op==i) {
				s << "selected";
			}
			s << ">" << getMarkOp2(i) << "</option>";		
		}
		s << "</select>\r\n";
		s << " <input name=v value='";
		if (m) {
			s << (int)m->mark;
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay() override {
		std::stringstream s;
		s << getMarkOp2(op) << (int)mark;
		return s.str();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		op = getMarkOp(attribute["op"].c_str());
		mark = atoi(attribute["v"].c_str());
	}
	
private:
	const char *getMarkOp(int op)
	{
		switch(op){
		case MARKACL_OP_LT:
			return "lt";
		case MARKACL_OP_EQ:
			return "eq";
		case MARKACL_OP_GT:
			return "gt";
		}
		return "eq";
	}
	int getMarkOp(const char *op)
	{
		if(strcasecmp(op,"lt")==0){
			return MARKACL_OP_LT;
		}
		if(strcasecmp(op,"gt")==0){
			return MARKACL_OP_GT;
		}
		return MARKACL_OP_EQ;
	}
	const char *getMarkOp2(int op)
	{
		switch (op) {
		case MARKACL_OP_EQ:
			return "=";
		case MARKACL_OP_LT:
			return "&lt;";
		case MARKACL_OP_GT:
			return "&gt;";
		}
		return "=";
	}
	int op;
	uint32_t mark;
};
#endif
