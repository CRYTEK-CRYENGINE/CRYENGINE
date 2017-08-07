#pragma once

#include "BaseMeshComponent.h"

class CPlugin_CryDefaultEntities;

namespace Cry
{
namespace DefaultComponents
{
class CStaticMeshComponent
	: public CBaseMeshComponent
{
protected:
	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void   Initialize() final;

	virtual void   ProcessEvent(SEntityEvent& event) final;
	// ~IEntityComponent

public:
	CStaticMeshComponent() {}
	virtual ~CStaticMeshComponent() {}

	static void     ReflectType(Schematyc::CTypeDesc<CStaticMeshComponent>& desc)
	{
		desc.SetGUID("{6DDD0033-6AAA-4B71-B8EA-108258205E29}"_cry_guid);
		desc.SetEditorCategory("Geometry");
		desc.SetLabel("Mesh");
		desc.SetDescription("A component containing a simple mesh that can not be animated");
		desc.SetIcon("icons:ObjectTypes/object.ico");
		desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

		desc.AddMember(&CStaticMeshComponent::m_type, 'type', "Type", "Type", "Determines the behavior of the static mesh", EMeshType::RenderAndCollider);

		desc.AddMember(&CStaticMeshComponent::m_filePath, 'file', "FilePath", "File", "Determines the CGF to load", "%ENGINE%/EngineAssets/Objects/Default.cgf");
		desc.AddMember(&CStaticMeshComponent::m_renderParameters, 'rend', "Render", "Rendering Settings", "Settings for the rendered representation of the component", SRenderParameters());
		desc.AddMember(&CStaticMeshComponent::m_physics, 'phys', "Physics", "Physics Settings", "Physical properties for the object, only used if a simple physics or character controller is applied to the entity.", SPhysicsParameters());
	}

	virtual void    SetFilePath(const char* szPath);
	const char*     GetFilePath() const { return m_filePath.value.c_str(); }

	virtual void LoadFromDisk();
	virtual void SetObject(IStatObj* pObject, bool bSetDefaultMass = false);
	virtual void ResetObject();

protected:
	Schematyc::GeomFileName m_filePath = "%ENGINE%/EngineAssets/Objects/Default.cgf";
	
	_smart_ptr<IStatObj>    m_pCachedStatObj = nullptr;
};
}
}
