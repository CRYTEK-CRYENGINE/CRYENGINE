// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CLOUDBLOCKER_RENDERNODE_
#define _CLOUDBLOCKER_RENDERNODE_

#pragma once

class CCloudBlockerRenderNode : public ICloudBlockerRenderNode, public Cry3DEngineBase
{
public:
	CCloudBlockerRenderNode();

	// implements IRenderNode
	virtual const char*             GetName() const override                      { return "CloudBlocker"; }
	virtual const char*             GetEntityClassName() const override           { return "CloudBlocker"; }
	virtual void                    SetMatrix(const Matrix34& mat) override;
	virtual Vec3                    GetPos(bool bWorldOnly = true) const override { return m_position; }
	virtual const AABB              GetBBox() const override                      { return m_WSBBox; }
	virtual void                    FillBBox(AABB& aabb) override;
	virtual void                    SetBBox(const AABB& WSBBox) override          { m_WSBBox = WSBBox; }
	virtual void                    OffsetPosition(const Vec3& delta) override;
	virtual void                    Render(const struct SRendParams& EntDrawParams, const SRenderingPassInfo& passInfo) override;
	virtual struct IPhysicalEntity* GetPhysics() const override                 { return nullptr; }
	virtual void                    SetPhysics(IPhysicalEntity* pPhys) override {}
	virtual void                    SetMaterial(IMaterial* pMat) override       {}
	virtual IMaterial*              GetMaterial(Vec3* pHitPos) const override   { return nullptr; }
	virtual IMaterial*              GetMaterialOverride() override              { return nullptr; }
	virtual float                   GetMaxViewDist() override                   { return 1000000.0f; }
	virtual EERType                 GetRenderNodeType() override                { return eERType_CloudBlocker; }
	virtual void                    GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual void                    SetOwnerEntity(IEntity* pEntity) override { m_pOwnerEntity = pEntity; }
	virtual IEntity*                GetOwnerEntity() const override { return m_pOwnerEntity; }

	// implements ICloudBlockerRenderNode
	void SetProperties(const SCloudBlockerProperties& properties) override;

private:
	~CCloudBlockerRenderNode();

	AABB m_WSBBox;
	Vec3 m_position;
	f32  m_decayStart;
	f32  m_decayEnd;
	f32  m_decayInfluence;
	bool m_bScreenspace;
	IEntity* m_pOwnerEntity;
};

#endif // _CLOUDBLOCKER_RENDERNODE_
