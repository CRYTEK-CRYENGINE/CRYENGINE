// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File: PhysRenderer.h
//  Description: declaration of a simple dedicated renderer for the physics subsystem
//
//	History:
//
//////////////////////////////////////////////////////////////////////

#ifndef PHYSRENDERER_H
#define PHYSRENDERER_H

#include <CryPhysics/IPhysicsDebugRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>

#if _MSC_VER > 1000
	#pragma once
#endif

struct SRayRec
{
	Vec3  origin;
	Vec3  dir;
	float time;
	int   idxColor;
};

struct SGeomRec
{
	int      itype;
	char     buf[sizeof(primitives::box)];
	Vec3     offset;
	Matrix33 R;
	float    scale;
	Vec3     sweepDir;
	float    time;
};

class CPhysRenderer : public IPhysicsDebugRenderer, public IPhysRenderer
{
public:
	CPhysRenderer();
	~CPhysRenderer();
	void Init();
	void DrawGeometry(IGeometry* pGeom, geom_world_data* pgwd, const ColorB& clr, const Vec3& sweepDir = Vec3(0));
	void DrawGeometry(int itype, const void* pGeomData, geom_world_data* pgwd, const ColorB& clr, const Vec3& sweepDir = Vec3(0));
	QuatT SetOffset(const Vec3& offs = Vec3(ZERO), const Quat& qrot = Quat(ZERO)) 
	{ 
		QuatT prev(m_qrot,m_offset); 
		m_offset = offs; 
		if ((qrot|qrot)>0)
			m_qrot = qrot;
		return prev; 
	}

	// IPhysRenderer
	virtual void DrawFrame(const Vec3& pnt, const Vec3* axes, const float scale, const Vec3* limits, const int axes_locked);
	virtual void DrawGeometry(IGeometry* pGeom, geom_world_data* pgwd, int idxColor = 0, int bSlowFadein = 0, const Vec3& sweepDir = Vec3(0), const ColorF& color = ColorF(1, 1, 1, 1));
	virtual void DrawLine(const Vec3& pt0, const Vec3& pt1, int idxColor = 0, int bSlowFadein = 0);
	virtual void DrawText(const Vec3& pt, const char* txt, int idxColor, float saturation = 0)
	{
		if (!m_camera.IsPointVisible(pt))
			return;
		float clr[4] = { min(saturation * 2, 1.0f), 0, 0, 1 };
		clr[1] = min((1 - saturation) * 2, 1.0f) * 0.5f;
		clr[idxColor + 1] = min((1 - saturation) * 2, 1.0f);
		IRenderAuxText::DrawLabelEx(pt, 1.5f, clr, true, true, txt);
	}
	static const char*  GetPhysForeignName(void* pForeignData, int iForeignData, int iForeignFlags);
	virtual const char* GetForeignName(void* pForeignData, int iForeignData, int iForeignFlags)
	{ return GetPhysForeignName(pForeignData, iForeignData, iForeignFlags); }
	// ^^^

	// IPhysicsDebugRenderer
	virtual void UpdateCamera(const CCamera& camera);
	virtual void DrawAllHelpers(IPhysicalWorld* world);
	virtual void DrawEntityHelpers(IPhysicalWorld* world, int entityId, int iDrawHelpers);
	virtual void DrawEntityHelpers(IPhysicalEntity* entity, int iDrawHelpers);
	virtual void Flush(float dt);
	// ^^^

	float m_cullDist, m_wireframeDist;
	float m_timeRayFadein;
	float m_rayPeakTime;
	int   m_maxTris, m_maxTrisRange;

protected:
	CCamera          m_camera;
	IRenderer*       m_pRenderer;
	SRayRec*         m_rayBuf;
	int              m_szRayBuf, m_iFirstRay, m_iLastRay, m_nRays;
	SGeomRec*        m_geomBuf;
	int              m_szGeomBuf, m_iFirstGeom, m_iLastGeom, m_nGeoms;
	IGeometry*       m_pRayGeom;
	primitives::ray* m_pRay;
	Vec3             m_offset;
	Quat             m_qrot;
	static ColorB    g_colorTab[9];
	volatile int     m_lockDrawGeometry;
};

#endif
