// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////////

// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the CRYLIVECREATE_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// CRYLIVECREATE_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.

#ifdef CRYLIVECREATE_EXPORTS
	#define CRYLIVECREATE_API DLL_EXPORT
#else
	#define CRYLIVECREATE_API DLL_IMPORT
#endif

struct ISystem;

namespace LiveCreate
{
struct IManager;
}

extern "C"
{
	CRYLIVECREATE_API LiveCreate::IManager* CreateLiveCreate(ISystem* pSystem);
	CRYLIVECREATE_API void                  DeleteLiveCreate(LiveCreate::IManager* pLC);
}
