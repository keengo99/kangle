/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include "global.h"
#ifdef MALLOCDEBUG
#include 	<map>
#endif
#ifndef 	_WIN32
#include	<syslog.h>
#endif
#include	<time.h>
#include 	<ctype.h>
#include	"do_config.h"
#include	"kforwin32.h"
#include "kmalloc.h"
#include "KHttpObjectHash.h"
#include "KHttpObject.h"
#include "lib.h"
static const char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun","Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static int make_month(const char *s);
static int make_num(const char *s);
static int timz_minutes = 0;
void init_time_zone()
{
	time_t tt = time(NULL);
	struct tm t;
#if defined(HAVE_GMTOFF)
	localtime_r(&tt, &t);
	timz_minutes = (int)(t.tm_gmtoff / 60);
#else
	struct tm gmt;	
	int days, hours;
	gmtime_r(&tt, &gmt);
	localtime_r(&tt, &t);
	days = t.tm_yday - gmt.tm_yday;
	hours = ((days < -1 ? 24 : 1 < days ? -24 : days * 24) + t.tm_hour - gmt.tm_hour);
	timz_minutes = hours * 60 + t.tm_min - gmt.tm_min;
#endif
}
void CTIME_R(time_t *a, char *b, size_t l) {
#if	defined(HAVE_CTIME_R)
#if	defined(CTIME_R_3)
	ctime_r(a, b, l);
#else
	ctime_r(a, b);
#endif /* SOLARIS */
#else
	struct tm tm;
	memset(b, 0, l);
	localtime_r(a, &tm);
	snprintf(b, l - 1, "%s %02d %s %02d:%02d:%02d\n", days[tm.tm_wday],
			tm.tm_mday, months[tm.tm_mon], tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif /* HAVE_CTIME_R */
}

void makeLastModifiedTime(time_t *a, char *b, size_t l) {
	struct tm tm;
	memset(b, 0, l);
	localtime_r(a, &tm);
	snprintf(b, l - 1, "%02d-%s-%04d %02d:%02d", tm.tm_mday, months[tm.tm_mon],
			1900 + tm.tm_year, tm.tm_hour, tm.tm_min);

}
void do_exit(int code) {
	//   flush_log();
	assert(0);
	//  exit(code);
}

static int make_num(const char *s) {
	if (*s >= '0' && *s <= '9')
		return 10 * (*s - '0') + *(s + 1) - '0';
	else
		return *(s + 1) - '0';
}

static int make_month(const char *s) {
	int i;
	for (i = 0; i < 12; i++) {
		if (!strncasecmp(months[i], s, 3))
			return i;
	}
	return -1;
}

static bool isRightTime(struct tm *tm) {
	if (tm->tm_sec < 0 || tm->tm_sec > 59)
		return false;
	if (tm->tm_min < 0 || tm->tm_min > 59)
		return false;
	if (tm->tm_hour < 0 || tm->tm_hour > 23)
		return false;
	if (tm->tm_mday < 1 || tm->tm_mday > 31)
		return false;
	if (tm->tm_mon < 0 || tm->tm_mon > 11)
		return false;
	return true;
}

bool parse_date_elements(const char *day, const char *month, const char *year,
		const char *aTime, const char *zone, struct tm *tm) {
	const char *t;
	memset(tm, 0, sizeof(struct tm));

	if (!day || !month || !year || !aTime)
		return false;
	tm->tm_mday = atoi(day);
	tm->tm_mon = make_month(month);
	if (tm->tm_mon < 0)
		return false;
	tm->tm_year = atoi(year);
	if (strlen(year) == 4)
		tm->tm_year -= 1900;
	else if (tm->tm_year < 70)
		tm->tm_year += 100;
	else if (tm->tm_year > 19000)
		tm->tm_year -= 19000;
	tm->tm_hour = make_num(aTime);
	t = strchr(aTime, ':');
	if (!t)
		return false;
	t++;
	tm->tm_min = atoi(t);
	t = strchr(t, ':');
	if (t)
		tm->tm_sec = atoi(t + 1);
	return isRightTime(tm);
}

bool parse_date(const char *str, struct tm *tm) {
	char *day = NULL;
	char *month = NULL;
	char *year = NULL;
	char *aTime = NULL;
	char *zone = NULL;
	char *buf = xstrdup(str);
	char *hot = buf;
	for (;;) {
		hot = strchr(hot, ' ');
		if (hot == NULL) {
			break;
		}
		*hot = 0;
		hot += 1;
		if (!day) {
			day = hot;
		} else if (!month) {
			month = hot;
		} else if (!year) {
			year = hot;
		} else if (!aTime) {
			aTime = hot;
		} else {
			zone = hot;
			break;
		}
	}
	bool result = parse_date_elements(day, month, year, aTime, zone, tm);
	xfree(buf);
	return result;
}

time_t parse1123time(const char *str) {
	struct tm tm;
	time_t t;
	if (NULL == str)
		return -1;
	if (!parse_date(str, &tm)) {
		return -1;
	}
	tm.tm_isdst = -1;
#ifdef HAVE_TIMEGM
	t = timegm(&tm);
#elif HAVE_GMTOFF
	t = mktime(&tm);
	if (t != -1) {
		struct tm local;
		localtime_r(&t,&local);
		t += local.tm_gmtoff;
	}
#else
	t = mktime(&tm);
	if (t != -1) {
		time_t dst = 0;
#if defined (_TIMEZONE)
#elif defined (_timezone)
#elif defined(AIX)
#elif defined(CYGWIN)
#elif defined(MSWIN)
#else
		extern long timezone;
#endif
		if (tm.tm_isdst > 0)
			dst = -3600;
#if defined ( _timezone) || defined(_WIN32)
		t -= (_timezone + dst);
#else
		t -= (timezone + dst);
#endif
	}
#endif
	return t;
}
int make_http_time(time_t time,char *buf,int size)
{
	struct tm tm;
	time_t holder = time;
	gmtime_r(&holder, &tm);
	return snprintf(buf, size, "%s, %02d %s %d %02d:%02d:%02d GMT", days[tm.tm_wday],
			tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour,
			tm.tm_min, tm.tm_sec);
}
const char * mk1123time(time_t time, char *buf, int size) {
	make_http_time(time, buf, size);
	return buf;
}
#if	!defined(_WIN32)
#if	!defined(HAVE_DAEMON)
int daemon(int nochdir, int noclose) {
	pid_t child;

	/* this is not complete */
	child = fork();
	if (child < 0) {
		fprintf(stderr, "daemon(): Can't fork.\n");
		return (1);
	}
	if (child > 0) {
		/* parent */
		exit(0);
	}
	if (!nochdir) {
		chdir("/");
	}
	if (!noclose) {
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
		close(3);
	}
	return (0);
}
#endif	/* !HAVE_DAEMON */
#else
int
daemon(int nochdir, int noclose)
{
	return(0);
}
#endif	


void my_msleep(int msec) {
#if	defined(OSF)
	/* DU don't want to sleep in poll when number of descriptors is 0 */
	usleep(msec*1000);
#elif	defined(_WIN32)
	Sleep(msec);
#else
	struct timeval tv;
	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec % 1000) * 1000;
	select(1, NULL, NULL, NULL, &tv);
#endif
}
#define	BU_FREE	1
#define	BU_BUSY	2
#if 0
#if defined(HAVE_GMTOFF)
struct tm * get_gmtoff(int *tz, struct tm *t) {
	time_t tt = time(NULL);
	localtime_r(&tt, t);
	*tz = (int) (t->tm_gmtoff / 60);
	return t;
}
#else
struct tm * get_gmtoff(time_t tt,int *tz, struct tm *t) {
	//time_t tt = time(NULL);
	struct tm gmt;
	int days, hours, minutes;
	gmtime_r(&tt, &gmt);
	localtime_r(&tt, t);
	days = t->tm_yday - gmt.tm_yday;
	hours = ((days < -1 ? 24 : 1 < days ? -24 : days * 24) + t->tm_hour	- gmt.tm_hour);
	minutes = hours * 60 + t->tm_min - gmt.tm_min;
	*tz = minutes;
	return t;
}
#endif
#endif
const char *log_request_time(time_t time,char *buf, size_t buf_size) {
	int timz = timz_minutes;
	struct tm t;
	localtime_r(&time, &t);
	char sign = (timz < 0 ? '-' : '+');
	if (timz < 0) {
		timz = -timz;
	}
	snprintf(buf, buf_size - 1, "[%02d/%s/%d:%02d:%02d:%02d %c%.2d%.2d]",
			t.tm_mday, months[t.tm_mon], t.tm_year + 1900, t.tm_hour, t.tm_min,
			t.tm_sec, sign, timz / 60, timz % 60);
	return buf;
}

static unsigned char hexchars[] = "0123456789ABCDEF";
char *url_value_encode(const char *s, size_t len, size_t *new_length) {
	register unsigned char c;
	unsigned char *to, *start;
	unsigned char const *from, *end;
	if (len == 0) {
		//assert(false);
		return strdup("");
	}
	from = (unsigned char *) s;
	end = (unsigned char *) s + len;
	start = to = (unsigned char *) xmalloc(3*len+1);

	while (from < end) {
		c = *from++;
		if (c == '/') {
			*to++ = c;
		} else if (!isascii((int)c)) {
			to[0] = '%';
			to[1] = hexchars[c >> 4];
			to[2] = hexchars[c & 15];
			to += 3;
		} else {
			*to++ = c;
		}
	}
	*to = 0;
	if (new_length) {
		*new_length = to - start;
	}
	return (char *) start;
}

char *url_encode(const char *s, size_t len, size_t *new_length) {
	register unsigned char c;
	unsigned char *to, *start;
	unsigned char const *from, *end;
	if (len == 0) {
		//assert(false);
		return strdup("");
	}
	from = (unsigned char *) s;
	end = (unsigned char *) s + len;
	start = to = (unsigned char *) xmalloc(3*len+1);

	while (from < end) {
		c = *from++;
		if (c == '/') {
			*to++ = c;
		/*
		} else if (c == ' ') {
			*to++ = '+';
		*/
		} else if ((c < '0' && c != '-' && c != '.') || (c < 'A' && c > '9')
				|| (c > 'Z' && c < 'a' && c != '_') || (c > 'z') || c=='\\') {
			to[0] = '%';
			to[1] = hexchars[c >> 4];
			to[2] = hexchars[c & 15];
			to += 3;
		} else {
			*to++ = c;
		}
	}
	*to = 0;
	if (new_length) {
		*new_length = to - start;
	}
	return (char *) start;
}
std::string url_encode(const char *str, size_t len_string) {
	std::string s;
	if (len_string == 0) {
		len_string = strlen(str);
	}
	char *new_string = url_encode(str, len_string, NULL);
	if (new_string) {
		s = new_string;
		xfree(new_string);
	}
	return s;
}
