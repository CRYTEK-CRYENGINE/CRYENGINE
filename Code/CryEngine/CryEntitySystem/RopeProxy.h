// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// forward declarations.
struct SEntityEvent;
struct IPhysicalEntity;
struct IPhysicalWorld;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Implements rope proxy class for entity.
//////////////////////////////////////////////////////////////////////////
class CEntityComponentRope : public IEntityRopeComponent
{
	CRY_ENTITY_COMPONENT_CLASS_GUID(CEntityComponentRope, IEntityRopeComponent, "CEntityComponentRope", "dfae2b7e-15bb-4f3d-bd09-e0c8e560bf85"_cry_guid);

	CEntityComponentRope();
	virtual ~CEntityComponentRope();

public:
	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void   Initialize() final;
	virtual void   ProcessEvent(const SEntityEvent& event) final;
	virtual uint64 GetEventMask() const final;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetProxyType() const final { return ENTITY_PROXY_ROPE; }
	virtual void         Release() final            { delete this; };
	virtual void         Update(SEntityUpdateContext& ctx) final;
	virtual void         LegacySerializeXML(XmlNodeRef& entityNode, XmlNodeRef& componentNode, bool bLoading) override final;
	virtual bool         NeedGameSerialize() final;
	virtual void         GameSerialize(TSerialize ser) final;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	/// IEntityRopeComponent
	//////////////////////////////////////////////////////////////////////////
	virtual IRopeRenderNode* GetRopeRenderNode() final { return m_pRopeRenderNode; };
	//////////////////////////////////////////////////////////////////////////

	virtual void GetMemoryUsage(ICrySizer* pSizer) const final
	{
		pSizer->AddObject(this, sizeof(*this));
	}
	void PreserveParams();

protected:
	IRopeRenderNode* m_pRopeRenderNode;
	int              m_nSegmentsOrg;
	float            m_texTileVOrg;

	int              m_segmentsCount = 0;
	float            m_texureTileV = 0;
};
