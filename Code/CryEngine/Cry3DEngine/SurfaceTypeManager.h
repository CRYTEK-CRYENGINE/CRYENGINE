// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   SurfaceManager.h
//  Version:     v1.00
//  Created:     29/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SurfaceManager_h__
#define __SurfaceManager_h__
#pragma once

#include <Cry3DEngine/I3DEngine.h>

#define MAX_SURFACE_ID 255

//////////////////////////////////////////////////////////////////////////
// SurfaceManager is implementing ISurfaceManager interface.
// Register and manages all known to game surface typs.
//////////////////////////////////////////////////////////////////////////
class CSurfaceTypeManager : public ISurfaceTypeManager, public Cry3DEngineBase
{
public:
	CSurfaceTypeManager();
	virtual ~CSurfaceTypeManager();

	virtual void                    LoadSurfaceTypes();

	virtual ISurfaceType*           GetSurfaceTypeByName(const char* sName, const char* sWhy = NULL, bool warn = true);
	virtual ISurfaceType*           GetSurfaceType(int nSurfaceId, const char* sWhy = NULL);
	virtual ISurfaceTypeEnumerator* GetEnumerator();

	bool                            RegisterSurfaceType(ISurfaceType* pSurfaceType, bool bDefault = false);
	void                            UnregisterSurfaceType(ISurfaceType* pSurfaceType);

	ISurfaceType*                   GetSurfaceTypeFast(int nSurfaceId, const char* sWhy = NULL);

	void                            RemoveAll();

	void                            GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_nameToSurface);
		for (int i = 0; i < MAX_SURFACE_ID + 1; ++i)
			pSizer->AddObject(m_idToSurface[i]);
	}
private:
	int                         m_lastSurfaceId;
	int                         m_lastDefaultId;

	class CMaterialSurfaceType* m_pDefaultSurfaceType;

	void                        RegisterAllDefaultTypes();
	CMaterialSurfaceType*       RegisterDefaultType(const char* szName);
	void                        ResetSurfaceTypes();

	struct SSurfaceRecord
	{
		bool          bLoaded;
		ISurfaceType* pSurfaceType;

		void          GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(this, sizeof(*this));
		}
	};

	SSurfaceRecord* m_idToSurface[MAX_SURFACE_ID + 1];

	typedef std::map<string, SSurfaceRecord*> NameToSurfaceMap;
	NameToSurfaceMap m_nameToSurface;
};

#endif //__SurfaceManager_h__
