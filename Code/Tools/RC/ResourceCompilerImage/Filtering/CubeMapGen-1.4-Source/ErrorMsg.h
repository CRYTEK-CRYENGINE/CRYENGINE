// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// ErrorMsg.h
// 
//   Common error message handling functions
//
//--------------------------------------------------------------------------------------
// (C) 2005 ATI Research, Inc., All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef ERRORMSG_H
#define ERRORMSG_H 

//#include "LocalDXUT\\dxstdafx.h"
#include <windows.h>
#include <stdio.h>
#include <wtypes.h>
#include <stdarg.h>

//do not allow "dxstdafx.h" to depricate any core string functions
#pragma warning( disable : 4995 )

#define EM_EXIT_NO_ERROR                  0
#define EM_EXIT_NONFATAL_ERROR_OCCURRED  -1
#define EM_FATAL_ERROR                  -15


#define EM_MAX_MESSAGE_LENGTH             16384

#define EM_MESSAGE_MEDIUM_MESSAGEBOX         0
#define EM_MESSAGE_MEDIUM_CALLBACK_FUNCTION  1

//default message output method
#define EM_DEFAULT_MESSAGE_MEDIUM  EM_MESSAGE_MEDIUM_MESSAGEBOX

//default error message callback
void DefaultErrorMessageCallback(WCHAR *a_Title, WCHAR *a_Message );

//set error function
void SetErrorMessageCallback( void(*a_MessageOutputFunc)(WCHAR *, WCHAR *) );


#if defined(DEBUG) | defined(_DEBUG)
    #ifndef V_MSG
        #define V_MSG(x, m) {hr = x; if( FAILED(hr) ){OutputMessage(L"Warning", m ); {DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true );} }}
        #endif
    #ifndef V_MSG_RETURN
        #define V_MSG_RETURN(x, m) {hr = x; if( FAILED(hr) ){OutputMessage(L"Warning", m); {return DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); }}
    #endif
    #ifndef V_MSG_FATAL
        #define V_MSG_FATAL(x, m) {hr = x; if(FAILED(hr)) {OutputMessage(L"Fatal Error", m); {DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); exit(EM_FATAL_ERROR); }}
    #endif
#else
    #ifndef V_MSG
        #define V_MSG(x, m) {hr = x; if(FAILED(hr)) {OutputMessage(L"Warning", m); } }
    #endif
    #ifndef V_MSG_RETURN
        #define V_MSG_RETURN(x, m) {hr = x; if(FAILED(hr)) {OutputMessage(L"Warning", m);} } 
    #endif
    #ifndef V_MSG_FATAL
        #define V_MSG_FATAL(x, m) {hr = x; if(FAILED(hr)) {OutputMessage(L"Fatal Error", m); exit(EM_FATAL_ERROR);} }
    #endif
#endif

//no variable arguementlist
void OutputMessageString(WCHAR *a_Title, WCHAR *a_Message);

//has variable arguement list
void OutputMessage(WCHAR *a_Message, ... );
bool OutputQuestion( WCHAR *a_Message, ... );
HRESULT OutputMessageOnFail(HRESULT a_hr, WCHAR *a_Message, ... );
HRESULT OutputFatalMessageOnFail(HRESULT a_hr, WCHAR *a_Message, ... );

#endif //ERRORMSG_H 
