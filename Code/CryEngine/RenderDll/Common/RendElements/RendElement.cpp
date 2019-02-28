// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   RendElement.cpp : common RE functions.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#include "StdAfx.h"

CRenderElement CRenderElement::s_RootGlobal(true);
CRenderElement *CRenderElement::s_pRootRelease[4];

//===============================================================

CryCriticalSection CRenderElement::s_accessLock;

int CRenderElement::s_nCounter;

//============================================================================

void CRenderElement::ShutDown()
{
	if (!CRenderer::CV_r_releaseallresourcesonexit)
		return;

	AUTO_LOCK(s_accessLock); // Not thread safe without this

	CRenderElement* pRE;
	CRenderElement* pRENext;
	for (pRE = CRenderElement::s_RootGlobal.m_NextGlobal; pRE != &CRenderElement::s_RootGlobal; pRE = pRENext)
	{
		if (CRenderer::CV_r_printmemoryleaks)
			iLog->Log("Warning: CRenderElement::ShutDown: RenderElement %s was not deleted", pRE->mfTypeString());

		pRENext = pRE->m_NextGlobal;
		SAFE_DELETE(pRE);
	}
}

void CRenderElement::Tick()
{
	FUNCTION_PROFILER_RENDERER();

#ifndef STRIP_RENDER_THREAD
	assert(gRenDev->m_pRT->IsMainThread(true));
#endif
	uint32 nFrame = (uint32)(gRenDev->GetMainFrameID()) - 3;
	CRenderElement& Root = *CRenderElement::s_pRootRelease[nFrame & 3];
	CRenderElement* pRENext = NULL;

	for (CRenderElement* pRE = Root.m_NextGlobal; pRE != &Root; pRE = pRENext)
	{
		pRENext = pRE->m_NextGlobal;
		SAFE_DELETE(pRE);
	}
}

void CRenderElement::Cleanup()
{
	gRenDev->m_pRT->FlushAndWait();

	AUTO_LOCK(s_accessLock); // Not thread safe without this

	for (int i = 0; i < 4; ++i)
	{
		CRenderElement& Root = *CRenderElement::s_pRootRelease[i];
		CRenderElement* pRENext = nullptr;

		for (CRenderElement* pRE = Root.m_NextGlobal; pRE != &Root && pRE != nullptr; pRE = pRENext)
		{
			pRENext = pRE->m_NextGlobal;
			SAFE_DELETE(pRE);
		}
	}
}

void CRenderElement::Release(bool bForce)
{
	m_Flags |= FCEF_DELETED;

#ifdef _DEBUG
	//if (gRenDev && gRenDev->m_pRT)
	//  assert(gRenDev->m_pRT->IsMainThread(true));
#endif
	m_Type = eDATA_Unknown;
	if (bForce)
	{
		//sDeleteRE(this);
		delete this;
		return;
	}
	int nFrame = gRenDev->GetFrameID();

	AUTO_LOCK(s_accessLock);
	CRenderElement& Root = *CRenderElement::s_pRootRelease[nFrame & 3];
	UnlinkGlobal();
	LinkGlobal(&Root);
	//sDeleteRE(this);
}

CRenderElement::CRenderElement(bool bGlobal)
{
	m_Type = eDATA_Unknown;
	if (!s_RootGlobal.m_NextGlobal)
	{
		s_RootGlobal.m_NextGlobal = &s_RootGlobal;
		s_RootGlobal.m_PrevGlobal = &s_RootGlobal;
		for (int i = 0; i < 4; i++)
		{
			s_pRootRelease[i] = new CRenderElement(true);
			s_pRootRelease[i]->m_NextGlobal = s_pRootRelease[i];
			s_pRootRelease[i]->m_PrevGlobal = s_pRootRelease[i];
		}
	}

	m_Flags = 0;
	m_nFrameUpdated = 0xffff;
	m_CustomData = NULL;
	m_nID = CRenderElement::s_nCounter++;
	int i;
	for (i = 0; i < CRenderElement::MAX_CUSTOM_TEX_BINDS_NUM; i++)
		m_CustomTexBind[i] = -1;
}

CRenderElement::CRenderElement()
{
	m_Type = eDATA_Unknown;

	m_Flags = 0;
	m_nFrameUpdated = 0xffff;
	m_CustomData = NULL;
	m_NextGlobal = NULL;
	m_PrevGlobal = NULL;
	m_nID = CRenderElement::s_nCounter++;
	int i;
	for (i = 0; i < CRenderElement::MAX_CUSTOM_TEX_BINDS_NUM; i++)
		m_CustomTexBind[i] = -1;

	//sAddRE(this);

	AUTO_LOCK(s_accessLock);
  LinkGlobal(&s_RootGlobal);
}
CRenderElement::~CRenderElement()
{
	assert(m_Type == eDATA_Unknown || m_Type == eDATA_Particle || m_Type == eDATA_ClientPoly);

	//@TODO: Fix later, prevent crash on exit in single executable
	if (this == s_pRootRelease[0] || this == s_pRootRelease[1] || this == s_pRootRelease[2] || this == s_pRootRelease[3] || this == &s_RootGlobal)
		return;

	AUTO_LOCK(s_accessLock);
	UnlinkGlobal();
}

CRenderChunk*      CRenderElement::mfGetMatInfo()     { return NULL; }
TRenderChunkArray* CRenderElement::mfGetMatInfoList() { return NULL; }
int                CRenderElement::mfGetMatId()       { return -1; }
void               CRenderElement::mfReset()          {}

const char*        CRenderElement::mfTypeString()
{
	switch (m_Type)
	{
	case eDATA_Sky:
		return "Sky";
	case eDATA_ClientPoly:
		return "ClientPoly";
	case eDATA_Flare:
		return "Flare";
	case eDATA_Terrain:
		return "Terrain";
	case eDATA_SkyZone:
		return "SkyZone";
	case eDATA_Mesh:
		return "Mesh";
	case eDATA_LensOptics:
		return "LensOptics";
	case eDATA_OcclusionQuery:
		return "OcclusionQuery";
	case eDATA_Particle:
		return "Particle";
	case eDATA_HDRSky:
		return "HDRSky";
	case eDATA_FogVolume:
		return "FogVolume";
	case eDATA_WaterVolume:
		return "WaterVolume";
	case eDATA_WaterOcean:
		return "WaterOcean";
	case eDATA_GameEffect:
		return "GameEffect";
	case eDATA_BreakableGlass:
		return "BreakableGlass";
	case eDATA_GeomCache:
		return "GeomCache";
	default:
		{
			CRY_ASSERT(false);
			return "Unknown";
		}
	}
}

CRenderElement* CRenderElement::mfCopyConstruct(void)
{
	CRenderElement* re = new CRenderElement;
	*re = *this;
	return re;
}
void CRenderElement::mfCenter(Vec3& centr, CRenderObject* pObj, const SRenderingPassInfo& passInfo)
{
	centr(0, 0, 0);
}
void CRenderElement::mfGetPlane(Plane& pl)
{
	pl.n = Vec3(0, 0, 1);
	pl.d = 0;
}

void* CRenderElement::mfGetPointer(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags) { return NULL; }
