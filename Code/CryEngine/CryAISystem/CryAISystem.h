// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYAISYSTEM_H_
#define __CRYAISYSTEM_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#ifdef CRYAISYSTEM_EXPORTS
	#define CRYAIAPI DLL_EXPORT
#else
	#define CRYAIAPI DLL_IMPORT
#endif

struct IAISystem;
struct ISystem;

extern "C"
{
	CRYAIAPI IAISystem* CreateAISystem(ISystem* pSystem);
}

#endif //__CRYAISYSTEM_H_
