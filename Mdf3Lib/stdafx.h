// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef WIN32
#include "targetver.h"
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef  WIN32
#include<Windows.h>
#include <tchar.h>
#include <io.h>
#include <assert.h>
#else
#include<sys/io.h>
#include<string.h>
#endif //  WIN32




// TODO: reference additional headers your program requires here
