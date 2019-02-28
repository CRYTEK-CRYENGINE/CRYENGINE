// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
DesignerWarning.h
	- basic message box in windows to allow designers to be told something is wrong with their setup

-	[10/11/2009] : Created by James Bamford

*************************************************************************/

#ifndef __DESIGNER_WARNING_H__
#define __DESIGNER_WARNING_H__

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
	#define DESIGNER_WARNING_ENABLED 1  // needs a release build define to hook in here 
#elif CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#define DESIGNER_WARNING_ENABLED 1  // needs a release build define to hook in here 
#else
	#define DESIGNER_WARNING_ENABLED 0
#endif

#if defined(_RELEASE)
	#undef DESIGNER_WARNING_ENABLED
	#define DESIGNER_WARNING_ENABLED 0
#endif

#if DESIGNER_WARNING_ENABLED

#define DesignerWarning(cond, ...) ((!(cond)) && DesignerWarningFail(#cond, string().Format(__VA_ARGS__).c_str()))
#define DesignerWarningFail(condText, messageText) DesignerWarningFunc(string().Format("CONDITION:\n%s\n\nMESSAGE:\n%s", condText, messageText))
int DesignerWarningFunc(const char * message);
int GetNumDesignerWarningsHit();

#else // DESIGNER_WARNING_ENABLED

#define DesignerWarning(cond, ...) (0)
#define DesignerWarningFail(condText, messageText) (0)
#define GetNumDesignerWarningsHit() (0)

#endif // DESIGNER_WARNING_ENABLED

#endif // __DESIGNER_WARNING_H__
