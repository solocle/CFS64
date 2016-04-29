// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <tchar.h>
#include <math.h>
#include <iostream>
#include <sstream>
#include <string>
#include <time.h>
#include <list>

#ifdef UNICODE
#define tstring wstring
#define to_tstring to_wstring
#define tcout wcout
#define tcin wcin
#else
#define tstring string
#define to_tstring to_string
#define tcout cout
#define tcin cin
#endif

using namespace std;

#include <Windows.h>
#include <AclAPI.h>

typedef int64_t teratime;

// TODO: reference additional headers your program requires here
