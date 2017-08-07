#pragma once
#include <CryEntitySystem/IEntityComponent.h>
#include <CryEntitySystem/IEntity.h>

////////////////////////////////////////////////////////
// Spawn point
////////////////////////////////////////////////////////
class CSpawnPointComponent final : public IEntityComponent
{
public:
	CSpawnPointComponent() = default;
	virtual ~CSpawnPointComponent() {}

	// Reflect type to set a unique identifier for this component
	// and provide additional information to expose it in the sandbox
	static void ReflectType(Schematyc::CTypeDesc<CSpawnPointComponent>& desc)
	{
		desc.SetGUID("{63F4C0C6-32AF-4ACB-8FB0-57D45DD14725}"_cry_guid);
		desc.SetEditorCategory("Game");
		desc.SetLabel("SpawnPoint");
		desc.SetDescription("This spawn point can be used to spawn entities");
		desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });
	}

public:
	void SpawnEntity(IEntity* otherEntity);
};
