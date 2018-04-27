// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _BREAKABLE_GLASS_SYSTEM_
#define _BREAKABLE_GLASS_SYSTEM_
#pragma once

#include <CryEntitySystem/IBreakableGlassSystem.h>

// Forward decls
struct IBreakableGlassRenderNode;
struct SBreakableGlassCVars;
struct SBreakableGlassPhysData;

//==================================================================================================
// Name: CBreakableGlassSystem
// Desc: Management system for breakable glass nodes
// Authors: Chris Bunner
//==================================================================================================
class CBreakableGlassSystem : public IBreakableGlassSystem
{
public:
	CBreakableGlassSystem();
	virtual ~CBreakableGlassSystem();

	static int   HandleImpact(const EventPhys* pPhysEvent);
	static void  OnEnabledCVarChange(ICVar* pCVar);

	virtual void Update(const float frameTime);
	virtual bool BreakGlassObject(const EventPhysCollision& physEvent, const bool forceGlassBreak = false);
	virtual void ResetAll();
	virtual void GetMemoryUsage(ICrySizer* pSizer) const;

private:
	bool                       ExtractPhysDataFromEvent(const EventPhysCollision& physEvent, SBreakableGlassPhysData& data, SBreakableGlassInitParams& initParams);

	bool                       ValidatePhysMesh(mesh_data* pPhysMesh, const int thinAxis);
	bool                       ExtractPhysMesh(mesh_data* pPhysMesh, const int thinAxis, const primitives::box& bbox, TGlassDefFragmentArray& fragOutline);
	void                       MergeFragmentTriangles(mesh_data* pPhysMesh, const int tri, const Vec3& thinRow, TGlassFragmentIndexArray& touchedTris, TGlassFragmentIndexArray& frag);
	void                       ExtractUVCoords(IStatObj* const pStatObj, const primitives::box& bbox, const int thinAxis, SBreakableGlassPhysData& data);
	Vec2                       InterpolateUVCoords(const Vec4& uvPt0, const Vec4& uvPt1, const Vec4& uvPt2, const Vec2& pt);

	bool                       ValidateExtractedOutline(SBreakableGlassPhysData& data, SBreakableGlassInitParams& initParams);
	void                       CalculateGlassBounds(const phys_geometry* const pPhysGeom, Vec3& size, Matrix34& matrix);
	void                       CalculateGlassAnchors(SBreakableGlassPhysData& data, SBreakableGlassInitParams& initParams);
	void                       PassImpactToNode(IBreakableGlassRenderNode* pRenderNode, const EventPhysCollision* pPhysEvent);

	void                       ModifyEventToForceBreak(EventPhysCollision* pPhysEvent);
	void                       AssertUnusedIfDisabled();

	IBreakableGlassRenderNode* InitGlassNode(const SBreakableGlassPhysData& physData, SBreakableGlassInitParams& initParams, const Matrix34& transMat);
	void                       ReleaseGlassNodes();

	PodArray<IBreakableGlassRenderNode*> m_glassPlanes;

	SBreakableGlassCVars*                m_pGlassCVars;
	bool m_enabled;
};//------------------------------------------------------------------------------------------------

#endif // _BREAKABLE_GLASS_SYSTEM_
