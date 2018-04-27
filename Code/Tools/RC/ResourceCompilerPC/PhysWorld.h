// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:PhysWorld.h
//  Declaration and implementation of the class CPhysWorldLoader
//
//	History:
//
//////////////////////////////////////////////////////////////////////
#ifndef _PHYSWORLD_HDR_
#define _PHYSWORLD_HDR_

#include <CryPhysics/primitives.h>
#include <CryPhysics/physinterface.h>

#include "MathHelpers.h"

#include <CryCore/Platform/CryWindows.h>   // HMODULE, LoadLibraryA()


class CPhysWorldLoader
{
public:
	CPhysWorldLoader() 
		: m_hPhysics(0)
		, m_pPhysWorld(0)
	{
		MathHelpers::AutoFloatingPointExceptions autoFpe(0);

		const char* const pName = GetLibName();

		RCLog("Loading %s...", pName);

		m_hPhysics = ::LoadLibraryA(pName);

		if (!m_hPhysics)
		{
			const DWORD errCode = ::GetLastError();

			char messageBuffer[1024] = { '?', 0  };
			::FormatMessageA(
				FORMAT_MESSAGE_FROM_SYSTEM, 
				NULL, 
				errCode, 
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				messageBuffer, 
				sizeof(messageBuffer) - 1, 
				NULL);

			RCLogError("%s failed to load. Error code: 0x%x (%s)", pName, errCode, messageBuffer);
			return;
		}

		RCLog("  Loaded %s", pName);

		IPhysicalWorld *(*pfnCreatePhysicalWorld)(void *pSystem) =
			(IPhysicalWorld*(*)(void*))::GetProcAddress(m_hPhysics, "CreatePhysicalWorld");

		if (!pfnCreatePhysicalWorld)
		{
			RCLogError("Physics is not initialized: failed to find CreatePhysicalWorld()");
			return;
		}

		m_pPhysWorld = pfnCreatePhysicalWorld(0);

		if (!m_pPhysWorld)
		{
			RCLogError("Physics is not initialized: a failure in CreatePhysicalWorld()");
			return;
		}

		m_pPhysWorld->Init();
	}

	~CPhysWorldLoader()
	{
		if (m_pPhysWorld)
		{
			m_pPhysWorld->Release();
		}

		if (m_hPhysics)
		{
			const char* const pName = GetLibName();

			RCLog("Unloading %s...", pName);

			::FreeLibrary(m_hPhysics);

			RCLog("  Unloaded %s", pName);
		}
	}

	IPhysicalWorld* GetWorldPtr()
	{
		return m_pPhysWorld;
	}

private:
	static const char* GetLibName()
	{
		return "CryPhysicsRC.dll";
	}

private:
	HMODULE m_hPhysics;
	IPhysicalWorld* m_pPhysWorld;
};

#endif
