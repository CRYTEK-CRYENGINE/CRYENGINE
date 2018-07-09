// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Cry3DEngine/IIndexedMesh.h>
#include <CryPhysics/IPhysics.h>
#include <CryPhysics/IDeferredCollisionEvent.h>

struct SProcessImpactIn;
struct SProcessImpactOut;
struct SExtractMeshIslandIn;
struct SExtractMeshIslandOut;

class CBreakablePlane : public IOwnedObject
{
public:
	CBreakablePlane()
		: m_cellSize(1.0f)
		, m_nudge(0.0f)
		, m_maxPatchTris(20)
		, m_jointhresh(0.3f)
		, m_density(900.0f)
		, m_pGrid(nullptr)
		, m_bAxisAligned(0)
		, m_matId(0)
		, m_pMat(nullptr)
		, m_matSubindex(0)
		, m_matFlags(0)
		, m_thicknessOrg(0.0f)
		, m_bStatic(1)
		, m_bOneSided(false)
		, m_pGeom(nullptr)
		, m_pSampleRay(nullptr)
	{
		m_z[0] = m_z[1] = 0.0f;
		m_refArea[0] = m_refArea[1] = 0.0f;
		m_nCells.set(20, 20);
		*m_mtlSubstName = 0;
	}
	~CBreakablePlane()
	{
		if (m_pGrid) m_pGrid->Release();
		if (m_pSampleRay) m_pSampleRay->Release();
		if (m_pGeom) m_pGeom->Release();
	}
	int         Release() { delete this; return 0;  }

	bool        SetGeometry(IStatObj* pStatObj, IMaterial* pRenderMat, int bStatic, int seed);
	void        FillVertexData(CMesh* pMesh, int ivtx, const Vec2& pos, int iside);
	IStatObj*   CreateFlatStatObj(int*& pIdx, Vec2* pt, Vec2* bounds, const Matrix34& mtxWorld, IParticleEffect* pEffect = 0,
	                              bool bNoPhys = false, bool bUseEdgeAlpha = false);
	int*        Break(const Vec3& pthit, float r, Vec2*& ptout, int seed, float filterAng, float ry);
	static int  ProcessImpact(const SProcessImpactIn& in, SProcessImpactOut& out);
	static void ExtractMeshIsland(const SExtractMeshIslandIn& in, SExtractMeshIslandOut& out);

	float             m_cellSize;
	Vec2i             m_nCells;
	float             m_nudge;
	int               m_maxPatchTris;
	float             m_jointhresh;
	float             m_density;
	IBreakableGrid2d* m_pGrid;
	Matrix33          m_R;
	Vec3              m_center;
	int               m_bAxisAligned;
	int               m_matId;
	IMaterial*        m_pMat;
	int               m_matSubindex;
	char              m_mtlSubstName[64];
	int               m_matFlags;
	float             m_z[2];
	float             m_thicknessOrg;
	float             m_refArea[2];
	Vec2              m_ptRef[2][3];
	SMeshTexCoord     m_texRef[2][3];
	SMeshTangents     m_TangentRef[2][3];
	int               m_bStatic;
	int               m_bOneSided;
	IGeometry*        m_pGeom, * m_pSampleRay;
	static int        g_nPieces;
	static float      g_maxPieceLifetime;
};
