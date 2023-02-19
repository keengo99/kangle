#ifndef KGL_KSTACKNAME_H_INCLUDE
#define KGL_KSTACKNAME_H_INCLUDE
#include "kmalloc.h"
#include "kstring.h"

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
#endif
