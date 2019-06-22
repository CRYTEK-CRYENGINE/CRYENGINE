// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CDistanceCloudRenderNode final : public IDistanceCloudRenderNode, public Cry3DEngineBase
{
public:
	// implements IDistanceCloudRenderNode
	virtual void             SetProperties(const SDistanceCloudProperties& properties) override;

	// implements IRenderNode
	virtual void             SetMatrix(const Matrix34& mat) override;

	virtual EERType          GetRenderNodeType() const override { return eERType_DistanceCloud; }
	virtual const char*      GetEntityClassName() const override;
	virtual const char*      GetName() const override;
	virtual Vec3             GetPos(bool bWorldOnly = true) const override;
	virtual void             Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo) override;
	virtual IPhysicalEntity* GetPhysics() const override;
	virtual void             SetPhysics(IPhysicalEntity*) override;
	virtual void             SetMaterial(IMaterial* pMat) override;
	virtual IMaterial*       GetMaterial(Vec3* pHitPos = 0) const override;
	virtual IMaterial*       GetMaterialOverride() const override { return m_pMaterial; }
	virtual float            GetMaxViewDist() const override;
	virtual void             Precache() override;
	virtual void             GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual const AABB       GetBBox() const override { return m_WSBBox; }
	virtual void             SetBBox(const AABB& WSBBox) override { m_WSBBox = WSBBox; }
	virtual void             FillBBox(AABB& aabb) const override { aabb = GetBBox(); }
	virtual void             OffsetPosition(const Vec3& delta) override;
	virtual void             SetLayerId(uint16 nLayerId) override { m_nLayerId = nLayerId; Get3DEngine()->C3DEngine::UpdateObjectsLayerAABB(this); }
	virtual uint16           GetLayerId() const override { return m_nLayerId; }

public:
	CDistanceCloudRenderNode();
	SDistanceCloudProperties GetProperties() const;

private:
	~CDistanceCloudRenderNode();

private:
	Vec3                  m_pos;
	float                 m_sizeX;
	float                 m_sizeY;
	float                 m_rotationZ;
	_smart_ptr<IMaterial> m_pMaterial;
	AABB                  m_WSBBox;
	uint16                m_nLayerId;
};
