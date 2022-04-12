#include "KInputFilter.h"
#include "KMultiPartInputFilter.h"
#include "KHttpRequest.h"
#include "utils.h"
#include "http.h"
#include "KUrlParser.h"
#ifdef ENABLE_INPUT_FILTER
void parseUrlParam(char *buf,int len,char **name,int *name_len,char **value,int *value_len)
{
	char *eq = strchr(buf,'=');
	*name_len = len;
	if (eq) {
		*eq = '\0';
		*name_len = eq-buf;
		eq++;
		*value_len = url_decode(eq,len-(*name_len)-1,NULL,true);
		*value = eq;
	} else {
		*value = NULL;
	}
	*name_len = url_decode(buf,*name_len,NULL,true);
	*name = buf;
}
int KInputFilter::hookParamFilter(KInputFilterContext *fc,const char *name,int name_len,const char *value,int value_len)
{
	KInputFilterHelper<KParamFilterHook> *hook = head;
	while (hook) {
		if (hook->t->matchFilter(fc,name,name_len,value,value_len)) {
			if (hook->action==JUMP_DENY) {
				return JUMP_DENY;
			}
		}
		hook = hook->next;
	}
	return JUMP_ALLOW;
}
int KInputFilter::checkItem(KInputFilterContext *rq,char *buf,int len)
{
	char *name,*value;
	int name_len,value_len;
	parseUrlParam(buf,len,&name,&name_len,&value,&value_len);
	return hookParamFilter(rq,name,name_len,value,value_len);
}
int KInputFilter::check(KInputFilterContext *rq,const char *str,int len,bool isLast)
{
	char *buf;
	if (last_buf) {
		int new_len = last_buf_len + len;
		buf = (char *)malloc(new_len + 1);
		kgl_memcpy(buf,last_buf,last_buf_len);
		kgl_memcpy(buf+last_buf_len,str,len);
		len = new_len;
		free(last_buf);
		last_buf = NULL;
	} else {
		buf = (char *)malloc(len + 1);
		kgl_memcpy(buf,str,len);
	}
	buf[len] = '\0';
	char *hot = buf;
	for (;;) {
		char *p = strchr(hot,'&');
		if(p==NULL){
			break;
		}
		*p = '\0';
		if (JUMP_DENY==checkItem(rq,hot,p-hot)) {
			free(buf);
			return JUMP_DENY;
		}
		hot = p+1;
	}
	last_buf_len = strlen(hot);
	if (isLast) {
		int ret = checkItem(rq,hot,last_buf_len);
		free(buf);
		return ret;
	}
	last_buf = strdup(hot);
	free(buf);
	return JUMP_ALLOW;
}
bool KInputFilterContext::checkGetParam(KParamFilterHook *hook)
{
	if (rq->url->param==NULL) {
		return false;
	}
	KParamPair *last;
	if (gBuffer==NULL) {
		last = NULL;
		assert(gParamHeader==NULL);
		gBuffer = strdup(rq->url->param);
		char *hot = gBuffer;
		for (;;) {
			char *p = strchr(hot,'&');
			if(p==NULL){
				break;
			}
			*p = '\0';
			KParamPair *pair = new KParamPair;
			pair->next = NULL;
			parseUrlParam(hot,p-hot,&pair->name,&pair->name_len,&pair->value,&pair->value_len);
			hot = p+1;
			if (last==NULL) {
				gParamHeader = last = pair;
			} else {
				last->next = pair;
				last = pair;
			}		
		}
		KParamPair *pair = new KParamPair;
		pair->next = NULL;
		parseUrlParam(hot,strlen(hot),&pair->name,&pair->name_len,&pair->value,&pair->value_len);
		if (last==NULL) {
			gParamHeader =  pair;
		} else {
			last->next = pair;
		}
	}
	last = gParamHeader;
	while (last) {
		if(hook->matchFilter(this,last->name,last->name_len,last->value,last->value_len)){
			return true;
		}
		last = last->next;
	}
	return false;
}
KInputFilter *KInputFilterContext::getFilter()
{
	if (filter) {
		return filter;
	}
	if(KBIT_TEST(rq->flags,RQ_POST_UPLOAD)){
		filter = new KMultiPartInputFilter;
	} else if (rq->content_length!=0) {
		filter = new KInputFilter;
	}
	return filter;
}
bool KInputFilterContext::parseBoundary(char *val)
{
	if (mb) {
		return false;
	}
    char *hot = strstr(val, "boundary");
    if (!hot) {
            char *content_type_lcase = strdup(val);
			string2lower2(content_type_lcase);
            hot = strstr(content_type_lcase, "boundary");
            if (hot) {
                    hot = val + (hot - content_type_lcase);
            }
            free(content_type_lcase);
    }

    if (!hot || !(hot = strchr(hot, '='))) {
            //sapi_module.sapi_error(E_WARNING, "Missing boundary in multipart/form-data POST data");
            return false;
    }
	hot++;
    int boundary_len = strlen(hot);

	char *boundary_end;
    if (hot[0] == '"') {
            hot++;
            boundary_end = strchr(hot, '"');
            if (!boundary_end) {
                  //  sapi_module.sapi_error(E_WARNING, "Invalid boundary in multipart/form-data POST data");
                    return false;
            }
    } else {
            /* search for the end of the boundary */
            boundary_end = strpbrk(hot, ",;");
    }
    if (boundary_end) {
           boundary_len = boundary_end-hot;
    }
	mb = new multipart_buffer;
	mb->boundary = (char *)malloc(boundary_len+3);
	mb->boundary[0] = mb->boundary[1] = '-';
	kgl_memcpy(mb->boundary+2,hot,boundary_len);
	mb->boundary[boundary_len+2] = '\0';
	mb->boundary_next = (char *)malloc(boundary_len+4);
	mb->boundary_next[0] = '\n';
	kgl_memcpy(mb->boundary_next+1,mb->boundary,boundary_len+2);
	mb->boundary_next_len = boundary_len+3;
	mb->boundary_next[mb->boundary_next_len] = '\0';
	return true;
}

#endif
