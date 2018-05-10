// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// ErrorMsg.cpp
// 
//   Common error message handling functions
//
//--------------------------------------------------------------------------------------
// (C) 2005 ATI Research, Inc., All rights reserved.
//--------------------------------------------------------------------------------------

#include "Types.h"
#include "ErrorMsg.h"

uint32 sg_MessageType = EM_DEFAULT_MESSAGE_MEDIUM;

//MessageOutputCallback
void (*sg_MessageOutputCallback)(WCHAR *, WCHAR *) = DefaultErrorMessageCallback; 


//--------------------------------------------------------------------------------------
// SetErrorMessageCallback
//
//--------------------------------------------------------------------------------------
void DefaultErrorMessageCallback(WCHAR *a_Title, WCHAR *a_Message )
{
//   MessageBox(NULL, a_Message, a_Title, MB_OK);
}


//--------------------------------------------------------------------------------------
// SetErrorMessageCallback
//
//--------------------------------------------------------------------------------------
void SetErrorMessageCallback( void(*a_MessageOutputFunc)(WCHAR *, WCHAR *) )
{
   sg_MessageType = EM_MESSAGE_MEDIUM_CALLBACK_FUNCTION;
   sg_MessageOutputCallback = a_MessageOutputFunc;
}


//--------------------------------------------------------------------------------------
//  Pulls up a message box, or calls a custom message function with an error message
//
//  note: this function is used to route all error messages though a common output 
//   mechanism..  in the future output could be rerouted through the console using 
//   this mechanism.
//--------------------------------------------------------------------------------------
void OutputMessageString(WCHAR *a_Title, WCHAR *a_Message )
{
   switch(sg_MessageType)
   {
      case EM_MESSAGE_MEDIUM_MESSAGEBOX:
         MessageBoxW(NULL, a_Message, a_Title, MB_OK);
      break;
      case EM_MESSAGE_MEDIUM_CALLBACK_FUNCTION:
         sg_MessageOutputCallback(a_Title, a_Message);
      break;
      default:
      break;
   }
}


//--------------------------------------------------------------------------------------
// variable arguement version of output message
//--------------------------------------------------------------------------------------
void OutputMessage(WCHAR *a_Message, ... )
{   
    int32 numCharOutput = 0;
    WCHAR msgBuffer[EM_MAX_MESSAGE_LENGTH];
    va_list args;
    va_start(args, a_Message);

    numCharOutput = _vsnwprintf_s( msgBuffer, EM_MAX_MESSAGE_LENGTH, EM_MAX_MESSAGE_LENGTH, a_Message, args);

    va_end(args);
    OutputMessageString(L"Message:", msgBuffer);
}

//--------------------------------------------------------------------------------------
// displays the message with a YES / NO response; returns true on YES and false on NO
//--------------------------------------------------------------------------------------
bool OutputQuestion( WCHAR *a_Message, ... )
{
   WCHAR msgBuffer[ EM_MAX_MESSAGE_LENGTH ];
   va_list args;
   va_start( args, a_Message );

   _vsnwprintf_s( msgBuffer, EM_MAX_MESSAGE_LENGTH, EM_MAX_MESSAGE_LENGTH, a_Message, args );

   va_end(args);
	
   switch( sg_MessageType )
   {
      case EM_MESSAGE_MEDIUM_MESSAGEBOX:
         return MessageBoxW( NULL, msgBuffer, L"Question:", MB_YESNO ) == IDYES;
      break;
      case EM_MESSAGE_MEDIUM_CALLBACK_FUNCTION:
         sg_MessageOutputCallback( L"Question:", msgBuffer );
      break;
      default:
      break;
   }
   return false;
}

//--------------------------------------------------------------------------------------
// output message if HRESULT indicates failure
//  
//--------------------------------------------------------------------------------------
HRESULT OutputMessageOnFail(HRESULT a_hr, WCHAR *a_Message, ... )
{   
    if(FAILED(a_hr))
    {
		va_list args;
        va_start(args, a_Message);

		WCHAR msgBuffer[EM_MAX_MESSAGE_LENGTH];
		
        int32 numCharOutput = _vsnwprintf_s( msgBuffer, EM_MAX_MESSAGE_LENGTH, EM_MAX_MESSAGE_LENGTH, a_Message, args);

        va_end(args);

        OutputMessageString(L"Error!", msgBuffer);
    }

    return a_hr;
}


//--------------------------------------------------------------------------------------
// output message and exit program if HRESULT indicates failure
//  
//--------------------------------------------------------------------------------------
HRESULT OutputFatalMessageOnFail(HRESULT a_hr, WCHAR *a_Message, ... )
{   
    if(FAILED(a_hr))
    {
		va_list args;
        va_start(args, a_Message);

		WCHAR msgBuffer[EM_MAX_MESSAGE_LENGTH];
		
        int32 numCharOutput = _vsnwprintf_s( msgBuffer, EM_MAX_MESSAGE_LENGTH, EM_MAX_MESSAGE_LENGTH, a_Message, args);

        va_end(args);

        OutputMessageString(L"Fatal Error!", msgBuffer);
        exit(EM_FATAL_ERROR);
    }

    return a_hr;
}


