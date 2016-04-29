#ifndef TERATIME_H
#define TERATIME_H

#include "stdafx.h"

time_t TimeFromSystemTime(const SYSTEMTIME * pTime);
SYSTEMTIME SystemTimeFromTime(const tm * pTime);
SYSTEMTIME SystemTimeFromTime(const time_t Time);

class CTeratime {
public:
	static teratime get_ttime(tm* time);
	static teratime get_ttime()
	{
		time_t tim;
		time(&tim);
		return get_ttime(gmtime(&tim));
	}
	static tm from_ttime(teratime time);
	static SYSTEMTIME from_ttime_sys(teratime time)
	{
		tm tim = from_ttime(time);
		return SystemTimeFromTime(time);
	}
};

#endif
