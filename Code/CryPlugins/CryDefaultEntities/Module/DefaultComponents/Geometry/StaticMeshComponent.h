#pragma once

#include "BaseMeshComponent.h"

#include <CrySchematyc/Utils/SharedString.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
namespace DefaultComponents
{
class CStaticMeshComponent
	: public CBaseMeshComponent
#ifndef RELEASE
	, public IEntityComponentPreviewer
#endif
{
protected:
	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void   Initialize() final;

	virtual void   ProcessEvent(const SEntityEvent& event) final;
	// ~IEntityComponent

#ifndef RELEASE
	virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
#endif
	// ~IEntityComponent

#ifndef RELEASE
	// IEntityComponentPreviewer
	virtual void SerializeProperties(Serialization::IArchive& archive) final {}
	virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
	// ~IEntityComponentPreviewer
#endif

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

		desc.AddMember(&CStaticMeshComponent::m_filePath, 'file', "FilePath", "File", "Determines the CGF to load", "%ENGINE%/EngineAssets/Objects/primitive_sphere.cgf");
		desc.AddMember(&CStaticMeshComponent::m_materialPath, 'mat', "Material", "Material", "Specifies the override material for the selected object", "");

		desc.AddMember(&CStaticMeshComponent::m_renderParameters, 'rend', "Render", "Rendering Settings", "Settings for the rendered representation of the component", SRenderParameters());
		desc.AddMember(&CStaticMeshComponent::m_physics, 'phys', "Physics", "Physics Settings", "Physical properties for the object, only used if a simple physics or character controller is applied to the entity.", SPhysicsParameters());
	}

	virtual void    SetFilePath(const char* szPath);
	const char*     GetFilePath() const { return m_filePath.value.c_str(); }

	virtual void LoadFromDisk();
	// Sets an new object for this component. 
	virtual void SetObjectDirect(IStatObj* pObject, bool bSetDefaultMass = false);
	// Sets an new object for this component. Path is relative to the asset directory. Example path: "Objects/MyAsset.cfg"
	virtual void SetObject(Schematyc::CSharedString path, bool bSetDefaultMass = false);
	// Reloads the object
	virtual void ResetObject();

	// Helper to allow exposing derived function to Schematyc
	virtual void SetMeshType(EMeshType type) { SetType(type); }

protected:
	Schematyc::GeomFileName m_filePath = "%ENGINE%/EngineAssets/Objects/primitive_sphere.cgf";
	Schematyc::MaterialFileName m_materialPath;

	_smart_ptr<IStatObj>    m_pCachedStatObj = nullptr;
};
}
}
