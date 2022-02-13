#ifndef KTimeMatch_H_LKDJLDlsjdflksjdf098038234
#define KTimeMatch_H_LKDJLDlsjdflksjdf098038234

#define	FIRST_MIN		0
#define	LAST_MIN		59

#define	FIRST_HOUR		0	
#define	LAST_HOUR		23

#define	FIRST_DAY		1
#define	LAST_DAY		31

#define	FIRST_MONTH		1
#define	LAST_MONTH		12

/* note on DOW: 0 and 7 are both Sunday, for compatibility reasons. */
#define	FIRST_WEEK		0
#define	LAST_WEEK		7
#include<string>
class KTimeMatch
{
public:
	KTimeMatch();
	~KTimeMatch();
	bool set(const char * time_model);
	bool check();
	bool check(time_t now_tm);
	void Show();	
	bool checkTime(time_t now_tm);
	bool isOpen()
	{
		return this->open;
	}
private:	
	void init();
	//bool Set(const char * time_model,bool s[],int low,int hight,bool first=false);
	bool setItem(char *hot,bool s[],int low,int hight);
	bool min[LAST_MIN+1];
	bool hour[LAST_HOUR+1];
	bool day[LAST_DAY+1];
	bool month[LAST_MONTH+1];
	bool week[LAST_WEEK+1];
	bool open;
	time_t lastCheck;
};
#endif
