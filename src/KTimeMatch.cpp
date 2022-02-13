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
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "KTimeMatch.h"
#include "log.h"
#include "kforwin32.h"
#include "kmalloc.h"

KTimeMatch::KTimeMatch() {
	init();
}
KTimeMatch::~KTimeMatch() {

}
void KTimeMatch::init()
{
	lastCheck = 0;
	memset(min, 0, sizeof(min));
	memset(hour, 0, sizeof(hour));
	memset(day, 0, sizeof(day));
	memset(month, 0, sizeof(month));
	memset(week, 0, sizeof(week));
	open = false;
}
bool KTimeMatch::set(const char * time_model) {
	init();
	if (time_model == NULL || *time_model == '\0') {
		open = true;
		return true;
	}
	open = false;
	//int len = strlen(time_model);
	/*
	for (i = 0; i < len; i++) {
		if (!((time_model[i] <= '9' && time_model[i] >= '0') || time_model[i]
				== ' ' || time_model[i] == '\t' || time_model[i] == '*'
				|| time_model[i] == '-' || time_model[i] == ',')) {
			klog(KLOG_INFO, "time_str \"%s\" have unknow char :%c\n",
					time_model, time_model[i]);
			return false;
		}
	}
	*/
	char *buf = strdup(time_model);
	char *hot = buf;
	for (int i=0;i<5;i++) {
		//略过前面的空格
		while (*hot && isspace((unsigned char)*hot)) {
			hot++;
		}
		if (*hot=='\0') {
			break;
		}
		char *next_item = hot;
		//找下一个
		while (*next_item && !isspace((unsigned char)*next_item)) {
			next_item++;
		}
		if (next_item!=hot && *next_item) {
			*next_item = '\0';
			next_item++;
		}
		bool result = false;
		switch(i){
		case 0:
			result = setItem(hot,min, FIRST_MIN, LAST_MIN);
			break;
		case 1:
			result = setItem(hot, hour, FIRST_HOUR, LAST_HOUR);
			break;
		case 2:
			result = setItem(hot, day, FIRST_DAY, LAST_DAY);
			break;
		case 3:
			result = setItem(hot, month, FIRST_MONTH, LAST_MONTH);
			break;
		case 4:
			result = setItem(hot, week, FIRST_WEEK, LAST_WEEK);
			break;
		default:
			break;
		}
		if (next_item==hot || *next_item=='\0') {
			break;
		}
		hot = next_item;
	}
	free(buf);
	if (week[LAST_WEEK] || week[0]) {// sunday is 0 or 7
		week[0] = true;
		week[LAST_WEEK] = true;
	}
	return true;
}
bool KTimeMatch::setItem(char *hot,bool item[],int low,int hight)
{
	while (hot && *hot) {
		char *next_list = strchr(hot,',');
		if (next_list) {
			*next_list = '\0';
			next_list++;
		}
		int step = 1;
		char *s =  strchr(hot,'/');
		if (s) {
			step = atoi(s+1);
			*s = '\0';
		}
		if (step < 1) {
			step = 1;
		}
		int min,max;
		if (*hot=='*') {
			min = low;
			max = hight;
		} else {
			if (!isdigit(*hot)) {
				return false;
			}
			min = atoi(hot);
			max = min;
			char *range = strchr(hot,'-');			
			if (range) {			
				max = atoi(range+1);
			}
		}
		if (min<low) {
			min = low;
		}
		if (max>hight) {
			max = hight;
		}
		for (int i=min;i<=max;i+=step) {
			item[i] = true;
		}
		hot = next_list;
	}
	return true;
}
/*
bool KTimeMatch::Set(const char * time_model, bool s[], int low, int hight,
		bool first) {
	int i;
	int len = strlen(time_model);
	bool have_atoi = false;
	bool have_add = false;
	int num;
	int end;
	//	printf("low=%d,hight=%d\n",low,hight);
	if (!first) {

		while (true) {
			if (point > len)
				break;
			if (time_model[point] == '\t' || time_model[point] == ' ')
				break;
			point++;
		}

	}
	while (true) {
		if (point > len)
			break;
		if ((time_model[point] != '\t') && (time_model[point] != ' '))
			break;
		point++;
	}
	while (!((time_model[point] == '\t') || (time_model[point] == ' '))) {
		if (point > len) {
			//			printf("~~~~~~~~~~~~~~\n");
			return have_add;
		}
		if (time_model[point] == '*') {
			point++;
			for (i = low; i <= hight; i++) {
				s[i] = true;
			}
			return true;
		}

		if (time_model[point] == ',') {
			point++;
			have_atoi = false;
			continue;
		}
		if (time_model[point] == '-') {
			//	printf("_____________________\n");
			point++;
			end = atoi(time_model + point);
			if (end < low || end > hight || end <= num) {
				klog(KLOG_INFO,
						"end value=%d is error in time_str \"%s\" point %d\n",
						end, time_model, point);
				return false;
			}
			for (i = num; i <= end; i++)
				s[i] = true;
			have_atoi = true;
		}
		//	printf("******************\n");
		if (!have_atoi) {
			have_atoi = true;
			have_add = true;
			num = atoi(time_model + point);
			//	printf("char=%c,num=%d,point=%d\n",time_model[point],num,point);
			if (num < low || num > hight) {
				klog(
						KLOG_INFO,
						"value %d is out of range in time_str \"%s\" point %d,allowed values :%d-%d \n",
						num, time_model, point, low, hight);
				return false;
			}
			s[num] = true;
		}
		point++;

	}
	return true;
}
*/
void KTimeMatch::Show() {
	int i;
	printf("min:");
	for (i = FIRST_MIN; i <= LAST_MIN; i++) {
		if (min[i]) {
			printf("%d,", i);
		}
	}
	printf("\nhour:");
	for (i = FIRST_HOUR; i <= LAST_HOUR; i++) {
		if (hour[i])
			printf("%d,", i);
	}
	printf("\nday:");
	for (i = FIRST_DAY; i <= LAST_DAY; i++) {
		if (day[i])
			printf("%d,", i);
	}
	printf("\nmonth:");
	for (i = FIRST_MONTH; i <= LAST_MONTH; i++) {
		if (month[i])
			printf("%d,", i);
	}
	printf("\nweek:");
	for (i = FIRST_WEEK; i <= LAST_WEEK; i++) {
		if (week[i])
			printf("%d,", i);
	}
	printf("\n");
}
bool KTimeMatch::check(time_t now_tm) {
	if (now_tm - lastCheck < 60) {
		//too faster
		return false;
	}
	if (lastCheck == 0) {
		return checkTime(now_tm);
	}
	time_t i = lastCheck + 60;
	for (; i <= now_tm; i += 60) {
		if (checkTime(i)) {
			return true;
		}
	}
	return false;
}
bool KTimeMatch::check() {
	return check(time(NULL));
}
//const char *KTimeMatch::GetTime() {
//	return time_str.c_str();

//}
bool KTimeMatch::checkTime(time_t now_tm) {
	struct tm tm;
	lastCheck = now_tm;
	if (open) {
		return true;
	}
	//printf("set lastCheck=%d\n",lastCheck);
	localtime_r(&now_tm, &tm);
	//printf("min=%d,hour=%d,day=%d,mon=%d,wday=%d.\n",tm.tm_min,tm.tm_hour,tm.tm_mday,tm.tm_mon,tm.tm_wday);
	if (min[tm.tm_min] 
		&& hour[tm.tm_hour] 
		&& day[tm.tm_mday]
		&& month[tm.tm_mon + 1] 
		&& week[tm.tm_wday]) {
		return true;
	}
	return false;
}

