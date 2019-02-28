// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntitySystem.h"
#include <CryEntitySystem/IEntityClass.h>
#include <CryEntitySystem/IEntityComponent.h>
#include <CryNetwork/ISerialize.h>
#include <CryRenderer/IRenderMesh.h>

//////////////////////////////////////////////////////////////////////////
// Description:
//    Proxy for storage of entity attributes.
//////////////////////////////////////////////////////////////////////////
class CEntityComponentClipVolume final : public IClipVolumeComponent
{
	CRY_ENTITY_COMPONENT_CLASS_GUID(CEntityComponentClipVolume, IClipVolumeComponent, "CEntityComponentClipVolume", "80652532-9245-4cd7-a906-2e7839ebb7a4"_cry_guid);

	CEntityComponentClipVolume();
	virtual ~CEntityComponentClipVolume();

public:
	// IEntityComponent.h interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void   Initialize() override;
	virtual void   ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const final;
	//////////////////////////////////////////////////////////////////////////

	// IEntityComponent
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetProxyType() const override                { return ENTITY_PROXY_CLIPVOLUME; }
	virtual void         Release() final                              { delete this; };
	virtual void         LegacySerializeXML(XmlNodeRef& entityNode, XmlNodeRef& componentNode, bool bLoading) override;
	virtual void         GameSerialize(TSerialize serialize) override {};
	virtual bool         NeedGameSerialize() override                 { return false; }
	virtual void         GetMemoryUsage(ICrySizer* pSizer) const override;
	//////////////////////////////////////////////////////////////////////////

	//~IClipVolumeComponent
	virtual void         SetGeometryFilename(const char* sFilename) final;
	virtual void         UpdateRenderMesh(IRenderMesh* pRenderMesh, const DynArray<Vec3>& meshFaces) override;
	virtual IClipVolume* GetClipVolume() const override { return m_pClipVolume; }
	virtual IBSPTree3D*  GetBspTree() const override    { return m_pBspTree; }
	virtual void         SetProperties(bool bIgnoresOutdoorAO, uint8 viewDistRatio) override;
	//~IClipVolumeComponent

private:
	bool LoadFromFile(const char* szFilePath);

private:
	// Engine clip volume
	IClipVolume* m_pClipVolume;

	// Render Mesh
	_smart_ptr<IRenderMesh> m_pRenderMesh;

	// BSP tree
	IBSPTree3D* m_pBspTree;

	// In-game stat obj
	string m_GeometryFileName;

	// Clip volume flags
	uint32 m_nFlags;

	// View dist ratio
	uint32 m_viewDistRatio;
};

DECLARE_SHARED_POINTERS(CEntityComponentClipVolume)
