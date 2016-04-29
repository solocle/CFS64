// teratime.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "teratime.h"

//typedef int_fast64_t teratime;

static uint_fast16_t months[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
static uint_fast16_t monthsly[] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 };

bool isleapyear(int year)
{
	return ((year % 4 == 0) && (year % 100 != 0) || (year % 400 == 0));
}
inline bool isly(int year)
{
	return isleapyear(year);
}

teratime CTeratime::get_ttime(tm* time)
{
	int year_days = (time->tm_year - 112) * 365 + (int)((time->tm_year - 112 + 3) / 4) - (int)((time->tm_year - 112 + 12) / 100)
		+ (int)((time->tm_year - 112 + 12) / 400);
	int month_days = isly(time->tm_year) ? monthsly[time->tm_mon] : months[time->tm_mon];
	uint_fast64_t days = year_days + month_days + time->tm_mday - 1;
	uint_fast64_t hours = days * 24 + time->tm_hour;
	uint_fast64_t minutes = hours * 60 + time->tm_min;
	uint_fast64_t seconds = minutes * 60 + time->tm_sec;
	return seconds;
}


tm CTeratime::from_ttime(teratime time)
{
	int s = time % 60;
	int m = (time / 60) % 60;
	int h = (time / 3600) % 24;
	int64_t days = time / 86400;
	int years = days/365.248;
	int leapdays = (years + 3) / 4 - (years + 12) / 100 + (years + 12) / 400;
	while (years != (days - leapdays) / 365)
	{
		years = (days - leapdays) / 365;
		leapdays = (years + 3) / 4 - (years + 12) / 100 + (years + 12) / 400;
	}
	int year = years + 2012;
	int dayinyear = 0;
	int month = 0;
	int day = 0;
	if (isleapyear(year))
	{
		dayinyear = (days - leapdays) % 366;
		while (monthsly[month] < dayinyear)
			month++;
		day = dayinyear - monthsly[month - 1]+1;
	}
	else
	{
		dayinyear = (days - leapdays) % 365;
		while (months[month] < dayinyear)
			month++;
		day = dayinyear - months[month - 1]+1;
	}
	tm thetime;
	thetime.tm_year = year - 1900;
	thetime.tm_mon = month - 1;
	thetime.tm_mday = day;
	thetime.tm_hour = h;
	thetime.tm_min = m;
	thetime.tm_sec = s;
	return thetime;
}

time_t TimeFromSystemTime(const SYSTEMTIME * pTime)
{
	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	tm.tm_year = pTime->wYear-1900;
	tm.tm_mon = pTime->wMonth-1;
	tm.tm_mday = pTime->wDay;

	tm.tm_hour = pTime->wHour;
	tm.tm_min = pTime->wMinute;
	tm.tm_sec = pTime->wSecond;

	return mktime(&tm);
}

SYSTEMTIME SystemTimeFromTime(const tm * pTime)
{
	SYSTEMTIME systm;
	memset(&systm, 0, sizeof(systm));

	systm.wYear = pTime->tm_year + 1900;
	systm.wMonth = pTime->tm_mon + 1;
	systm.wDay = pTime->tm_mday;

	systm.wHour = pTime->tm_hour;
	systm.wMinute = pTime->tm_min;
	systm.wSecond = pTime->tm_sec;

	return systm;
}

SYSTEMTIME SystemTimeFromTime(const time_t Time)
{
	return SystemTimeFromTime(gmtime(&Time));
}


