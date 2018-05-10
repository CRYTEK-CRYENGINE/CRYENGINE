// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _DISTANCECLOUD_RENDERNODE_
#define _DISTANCECLOUD_RENDERNODE_

#pragma once

class CDistanceCloudRenderNode : public IDistanceCloudRenderNode, public Cry3DEngineBase
{
public:
	// implements IDistanceCloudRenderNode
	virtual void SetProperties(const SDistanceCloudProperties& properties);

	// implements IRenderNode
	virtual void             SetMatrix(const Matrix34& mat);

	virtual EERType          GetRenderNodeType();
	virtual const char*      GetEntityClassName() const;
	virtual const char*      GetName() const;
	virtual Vec3             GetPos(bool bWorldOnly = true) const;
	virtual void             Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo);
	virtual IPhysicalEntity* GetPhysics() const;
	virtual void             SetPhysics(IPhysicalEntity*);
	virtual void             SetMaterial(IMaterial* pMat);
	virtual IMaterial*       GetMaterial(Vec3* pHitPos = 0) const;
	virtual IMaterial*       GetMaterialOverride() { return m_pMaterial; }
	virtual float            GetMaxViewDist();
	virtual void             Precache();
	virtual void             GetMemoryUsage(ICrySizer* pSizer) const;
	virtual const AABB       GetBBox() const             { return m_WSBBox; }
	virtual void             SetBBox(const AABB& WSBBox) { m_WSBBox = WSBBox; }
	virtual void             FillBBox(AABB& aabb);
	virtual void             OffsetPosition(const Vec3& delta);
	virtual void             SetLayerId(uint16 nLayerId) { m_nLayerId = nLayerId; Get3DEngine()->C3DEngine::UpdateObjectsLayerAABB(this); }
	virtual uint16           GetLayerId()                { return m_nLayerId; }

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

#endif // #ifndef _DISTANCECLOUD_RENDERNODE_
