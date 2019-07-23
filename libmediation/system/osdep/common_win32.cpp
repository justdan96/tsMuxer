/***********************************************************************
*	File: system/osdep/common_win32.cpp
*	Author: Andrey Kolesnikov
*	Date: 26 jun 2007
***********************************************************************/

#include "common_win32.h"


void initializeAllAccessSecurityDescriptor( SECURITY_DESCRIPTOR* const sd )
{
	if( !InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION) )
	{
		char msgBuf[32*1024];
		memset( msgBuf, 0, sizeof(msgBuf) );
		DWORD dw = GetLastError();
		char* msgBufPos = msgBuf;
		strcpy( msgBufPos, "Cannot initialize SECURITY_DESCRIPTOR structure: " );
		msgBufPos += strlen( msgBufPos );

		DWORD bufSize = sizeof(msgBuf) - (msgBufPos - msgBuf);
		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)msgBufPos,
			bufSize,
			NULL );
		throw std::runtime_error( msgBuf );
	}

	if( !SetSecurityDescriptorDacl(sd, TRUE, NULL,	FALSE) )
	{
		char msgBuf[32*1024];
		memset( msgBuf, 0, sizeof(msgBuf) );
		DWORD dw = GetLastError();
		char* msgBufPos = msgBuf;
		strcpy( msgBufPos, "Cannot set NULL DACL to security descriptor: " );
		msgBufPos += strlen( msgBufPos );

		DWORD bufSize = sizeof(msgBuf) - (msgBufPos - msgBuf);
		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)msgBufPos,
			bufSize,
			NULL );
		throw std::runtime_error( msgBuf );
	}
}
