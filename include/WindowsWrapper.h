#ifndef WINDOWS_WRAPPER_H
#define WINDOWS_WRAPPER_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define _WIN32_WINDOWS 0x0601
// clang-format off
#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>
// clang-format on
#endif

#endif