/***********************************************************************
*	File: system/osdep/common_win32.h
*	Author: Andrey Kolesnikov
*	Date: 26 jun 2007
***********************************************************************/

#ifndef COMMON_WIN32_H
#define COMMON_WIN32_H

#include <windows.h>

#include <stdexcept>

// void initializeAllAccessSecurityDescriptor( SECURITY_DESCRIPTOR* const sa ) throw ( std::runtime_error );^M
void initializeAllAccessSecurityDescriptor( SECURITY_DESCRIPTOR* const sa );

#endif	//COMMON_WIN32_H
