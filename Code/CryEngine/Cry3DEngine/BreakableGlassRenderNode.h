// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/RenderElements/CREBreakableGlass.h>
#include <CryRenderer/RenderElements/CREBreakableGlassHelpers.h>

class CREBreakableGlass;
struct IParticleEffect;

class CBreakableGlassRenderNode final : public IBreakableGlassRenderNode, Cry3DEngineBase
{
public:
	CBreakableGlassRenderNode();
	virtual ~CBreakableGlassRenderNode();

	// IBreakableGlassRenderNode interface
	virtual bool   InitialiseNode(const SBreakableGlassInitParams& params, const Matrix34& matrix) override;
	virtual void   ReleaseNode(bool bImmediate) override;

	virtual void   SetId(const uint16 id) override;
	virtual uint16 GetId() override;

	virtual void   Update(SBreakableGlassUpdateParams& params) override;
	virtual bool   HasGlassShattered() override;
	virtual bool   HasActiveFragments() override;
	virtual void   ApplyImpactToGlass(const EventPhysCollision* pPhysEvent) override;
	virtual void   ApplyExplosionToGlass(const EventPhysCollision* pPhysEvent) override;
	virtual void   DestroyPhysFragment(SGlassPhysFragment* pPhysFrag) override;

	virtual void   SetCVars(const SBreakableGlassCVars* pCVars) override;

	// IRenderNode interface
	virtual const char*      GetName() const override;
	virtual EERType          GetRenderNodeType() const override { return eERType_BreakableGlass; }
	virtual const char*      GetEntityClassName() const override;
	virtual void             GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual void             SetMaterial(IMaterial* pMaterial) override;
	virtual IMaterial*       GetMaterial(Vec3* pHitPos = NULL) const override;
	virtual void             SetMatrix(const Matrix34& matrix) override;
	virtual Vec3             GetPos(bool worldOnly = true) const override;
	virtual const AABB       GetBBox() const override { AABB bbox = m_planeBBox; bbox.Add(m_fragBBox); return bbox; }
	virtual void             SetBBox(const AABB& worldSpaceBoundingBox) override;
	virtual void             FillBBox(AABB& aabb) const override { aabb = GetBBox(); }
	virtual void             OffsetPosition(const Vec3& delta) override;
	virtual float            GetMaxViewDist() const override { return m_maxViewDist; }
	virtual IPhysicalEntity* GetPhysics() const override;
	virtual void             SetPhysics(IPhysicalEntity* pPhysics) override;
	virtual void             Render(const SRendParams& renderParams, const SRenderingPassInfo& passInfo) override;
	virtual IMaterial*       GetMaterialOverride() const override;

private:
	void PhysicalizeGlass();
	void DephysicalizeGlass();

	void PhysicalizeGlassFragment(SGlassPhysFragment& physFrag, const Vec3& centerOffset);
	void DephysicalizeGlassFragment(SGlassPhysFragment& physFrag);

	void CalculateImpactPoint(const Vec3& pt, Vec2& impactPt);
	void UpdateGlassState(const EventPhysCollision* pPhysEvent);
	void SetParticleEffectColours(IParticleEffect* pEffect, const Vec4& rgba);
	void PlayBreakageEffect(const EventPhysCollision* pPhysEvent);

	TGlassPhysFragmentArray            m_physFrags;
	TGlassPhysFragmentIdArray          m_deadPhysFrags;

	SBreakableGlassInitParams          m_glassParams;
	SBreakableGlassState               m_lastGlassState;

	AABB                               m_planeBBox; // Plane's world space bounding box
	AABB                               m_fragBBox;  // Phys fragments' world space bounding box
	Matrix34                           m_matrix;
	float                              m_maxViewDist;
	CREBreakableGlass*                 m_pBreakableGlassRE;
	IPhysicalEntity*                   m_pPhysEnt;

	static const SBreakableGlassCVars* s_pCVars;

	Vec4                               m_glassTintColour;
	uint16                             m_id;
	uint8                              m_state;
	uint8                              m_nextPhysFrag;
};
