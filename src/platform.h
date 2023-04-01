#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef WINVER
#define WINVER 0x0601 // win 7 or later
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <objbase.h>
#include <Shellapi.h>
#include <Shlwapi.h>
#include <ShlObj.h>
#include <assert.h>

#define DLLEXPORT __declspec(dllexport)

#endif // _WIN32

#ifdef __linux__
#define DLLEXPORT
#endif // __linux__
