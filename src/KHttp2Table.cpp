#include "KHttp2.h"
#include "kconnection.h"
#ifdef ENABLE_HTTP2
#define KGL_HTTP_V2_TABLE_SIZE  4096
static kgl_http_v2_header_t  kgl_http_v2_static_table[] = {
	{ kgl_string(":authority"), kgl_string("") },
	{ kgl_string(":method"), kgl_string("GET") },
	{ kgl_string(":method"), kgl_string("POST") },
	{ kgl_string(":path"), kgl_string("/") },
	{ kgl_string(":path"), kgl_string("/index.html") },
	{ kgl_string(":scheme"), kgl_string("http") },
	{ kgl_string(":scheme"), kgl_string("https") },
	{ kgl_string(":status"), kgl_string("200") },
	{ kgl_string(":status"), kgl_string("204") },
	{ kgl_string(":status"), kgl_string("206") },
	{ kgl_string(":status"), kgl_string("304") },
	{ kgl_string(":status"), kgl_string("400") },
	{ kgl_string(":status"), kgl_string("404") },
	{ kgl_string(":status"), kgl_string("500") },
	{ kgl_string("accept-charset"), kgl_string("") },
	{ kgl_string("accept-encoding"), kgl_string("gzip, deflate") },
	{ kgl_string("accept-language"), kgl_string("") },
	{ kgl_string("accept-ranges"), kgl_string("") },
	{ kgl_string("accept"), kgl_string("") },
	{ kgl_string("access-control-allow-origin"), kgl_string("") },
	{ kgl_string("age"), kgl_string("") },
	{ kgl_string("allow"), kgl_string("") },
	{ kgl_string("authorization"), kgl_string("") },
	{ kgl_string("cache-control"), kgl_string("") },
	{ kgl_string("content-disposition"), kgl_string("") },
	{ kgl_string("content-encoding"), kgl_string("") },
	{ kgl_string("content-language"), kgl_string("") },
	{ kgl_string("content-length"), kgl_string("") },
	{ kgl_string("content-location"), kgl_string("") },
	{ kgl_string("content-range"), kgl_string("") },
	{ kgl_string("content-type"), kgl_string("") },
	{ kgl_string("cookie"), kgl_string("") },
	{ kgl_string("date"), kgl_string("") },
	{ kgl_string("etag"), kgl_string("") },
	{ kgl_string("expect"), kgl_string("") },
	{ kgl_string("expires"), kgl_string("") },
	{ kgl_string("from"), kgl_string("") },
	{ kgl_string("host"), kgl_string("") },
	{ kgl_string("if-match"), kgl_string("") },
	{ kgl_string("if-modified-since"), kgl_string("") },
	{ kgl_string("if-none-match"), kgl_string("") },
	{ kgl_string("if-range"), kgl_string("") },
	{ kgl_string("if-unmodified-since"), kgl_string("") },
	{ kgl_string("last-modified"), kgl_string("") },
	{ kgl_string("link"), kgl_string("") },
	{ kgl_string("location"), kgl_string("") },
	{ kgl_string("max-forwards"), kgl_string("") },
	{ kgl_string("proxy-authenticate"), kgl_string("") },
	{ kgl_string("proxy-authorization"), kgl_string("") },
	{ kgl_string("range"), kgl_string("") },
	{ kgl_string("referer"), kgl_string("") },
	{ kgl_string("refresh"), kgl_string("") },
	{ kgl_string("retry-after"), kgl_string("") },
	{ kgl_string("server"), kgl_string("") },
	{ kgl_string("set-cookie"), kgl_string("") },
	{ kgl_string("strict-transport-security"), kgl_string("") },
	{ kgl_string("transfer-encoding"), kgl_string("") },
	{ kgl_string("user-agent"), kgl_string("") },
	{ kgl_string("vary"), kgl_string("") },
	{ kgl_string("via"), kgl_string("") },
	{ kgl_string("www-authenticate"), kgl_string("") },
};

#define KGL_HTTP_V2_STATIC_TABLE_ENTRIES                                      \
    (sizeof(kgl_http_v2_static_table)                                         \
     / sizeof(kgl_http_v2_header_t))

bool KHttp2::get_indexed_header(uintptr_t index, bool name_only)
{
	u_char                *p;
	size_t                 rest;
	kgl_http_v2_header_t  *entry;

	if (index == 0) {
		klog(KLOG_ERR, "client sent invalid hpack table index 0");
		return false;
	}

	//printf("http2 get indexed %s: %u\n",	name_only ? "header" : "header name", index);

	index--;

	if (index < KGL_HTTP_V2_STATIC_TABLE_ENTRIES) {
		state.header = kgl_http_v2_static_table[index];
		return true;
	}

	index -= KGL_HTTP_V2_STATIC_TABLE_ENTRIES;

	if (index < hpack.added - hpack.deleted) {
		index = (hpack.added - index - 1) % hpack.allocated;
		entry = hpack.entries[index];
		assert(state.pool);
		p = (u_char *)kgl_pnalloc(state.pool,entry->name.len + 1);
		if (p == NULL) {
			return false;
		}

		state.header.name.len = entry->name.len;
		state.header.name.data = (char *)p;

		rest = hpack.storage + KGL_HTTP_V2_TABLE_SIZE - (u_char *)entry->name.data;

		if (entry->name.len > rest) {
			p = (u_char *)kgl_cpymem(p, entry->name.data, rest);
			p = (u_char *)kgl_cpymem(p, hpack.storage, entry->name.len - rest);

		} else {
			p = (u_char *)kgl_cpymem(p, entry->name.data, entry->name.len);
		}

		*p = '\0';

		if (name_only) {
			return true;
		}
		p = (u_char *)kgl_pnalloc(state.pool,entry->value.len + 1);
		if (p == NULL) {
			return false;
		}

		state.header.value.len = entry->value.len;
		state.header.value.data = (char *)p;

		rest = hpack.storage + KGL_HTTP_V2_TABLE_SIZE - (u_char *)entry->value.data;

		if (entry->value.len > rest) {
			p = (u_char *)kgl_cpymem(p, entry->value.data, rest);
			p = (u_char *)kgl_cpymem(p, hpack.storage, entry->value.len - rest);

		} else {
			p = (u_char *)kgl_cpymem(p, entry->value.data, entry->value.len);
		}

		*p = '\0';

		return true;
	}

	klog(KLOG_INFO,"client sent out of bound hpack table index: %ui", index);

	return false;
}

bool KHttp2::table_size(size_t size)
{
	int                needed;
	kgl_http_v2_header_t  *entry;

	if (size > KGL_HTTP_V2_TABLE_SIZE) {
		klog(KLOG_WARNING, "client sent invalid table size update: %u\n", size);
		return false;
	}

	klog(KLOG_DEBUG,"http2 new hpack table size: %uz was:%u\n",	size, hpack.size);

	needed = hpack.size - size;

	while (needed > (int)hpack.free) {
		entry = hpack.entries[hpack.deleted++ % hpack.allocated];
		hpack.free += 32 + entry->name.len + entry->value.len;
	}
	hpack.size = size;
	hpack.free -= needed;
	return true;
}
bool KHttp2::table_account(size_t size)
{
	kgl_http_v2_header_t  *entry;

	size += 32;

	klog(KLOG_DEBUG,"http2 hpack table account: %uz free:%uz",size, hpack.free);

	if (size <= hpack.free) {
		hpack.free -= size;
		return true;
	}

	if (size > hpack.size) {
		hpack.deleted = hpack.added;
		hpack.free = hpack.size;
		return false;
	}

	do {
		entry = hpack.entries[hpack.deleted++ % hpack.allocated];
		hpack.free += 32 + entry->name.len + entry->value.len;
	} while (size > hpack.free);

	hpack.free -= size;

	return true;
}
bool KHttp2::add_header(kgl_http_v2_header_t *header)
{
	size_t                 avail;
	uintptr_t             index;
	kgl_http_v2_header_t  *entry, **entries;

	//printf("http2 add header to hpack table: \"%s[%d]: %s[%d]\"\n",header->name.data, header->name.len,header->value.data, header->value.len);

	if (hpack.entries == NULL) {
		hpack.allocated = 64;
		hpack.size = KGL_HTTP_V2_TABLE_SIZE;
		hpack.free = KGL_HTTP_V2_TABLE_SIZE;

		hpack.entries = (kgl_http_v2_header_t **)kgl_palloc(c->pool,sizeof(kgl_http_v2_header_t *)* hpack.allocated);
		if (hpack.entries == NULL) {
			return false;
		}

		hpack.storage = (u_char *)kgl_palloc(c->pool,hpack.free);
		if (hpack.storage == NULL) {
			return false;
		}
		hpack.pos = hpack.storage;
	}

	if (!table_account(header->name.len + header->value.len)) {
		return true;
	}

	if (hpack.reused == hpack.deleted) {
		entry = (kgl_http_v2_header_t *)kgl_palloc(c->pool, sizeof(kgl_http_v2_header_t));
		if (entry == NULL) {
			return false;
		}
	} else {
		entry = hpack.entries[hpack.reused++ % hpack.allocated];
	}

	avail = hpack.storage + KGL_HTTP_V2_TABLE_SIZE - hpack.pos;

	entry->name.len = header->name.len;
	entry->name.data = (char *)hpack.pos;

	if (avail >= header->name.len) {
		hpack.pos = (u_char *)kgl_cpymem(hpack.pos, header->name.data,header->name.len);
	} else {
		kgl_memcpy(hpack.pos, header->name.data, avail);
		hpack.pos = (u_char *)kgl_cpymem(hpack.storage,
			header->name.data + avail,
			header->name.len - avail);
		avail = KGL_HTTP_V2_TABLE_SIZE;
	}

	avail -= header->name.len;

	entry->value.len = header->value.len;
	entry->value.data = (char *)hpack.pos;

	if (avail >= header->value.len) {
		hpack.pos = (u_char *)kgl_cpymem(hpack.pos, header->value.data,	header->value.len);
	} else {
		kgl_memcpy(hpack.pos, header->value.data, avail);
		hpack.pos = (u_char *)kgl_cpymem(hpack.storage,
			header->value.data + avail,
			header->value.len - avail);
	}

	if (hpack.allocated == hpack.added - hpack.deleted) {
		entries = (kgl_http_v2_header_t **)kgl_palloc(c->pool, sizeof(kgl_http_v2_header_t *) * (hpack.allocated + 64));
		if (entries == NULL) {
			return false;
		}

		index = hpack.deleted % hpack.allocated;

		kgl_memcpy(entries, &hpack.entries[index],(hpack.allocated - index) * sizeof(kgl_http_v2_header_t *));

		kgl_memcpy(&entries[hpack.allocated - index], hpack.entries,index * sizeof(kgl_http_v2_header_t *));

		kgl_pfree(c->pool,hpack.entries);

		hpack.entries = entries;

		hpack.added = hpack.allocated;
		hpack.deleted = 0;
		hpack.reused = 0;
		hpack.allocated += 64;
	}
	hpack.entries[hpack.added++ % hpack.allocated] = entry;
	return true;
}
#endif
