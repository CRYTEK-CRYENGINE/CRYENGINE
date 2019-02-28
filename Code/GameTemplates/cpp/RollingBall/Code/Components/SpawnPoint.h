#pragma once
#include <CryEntitySystem/IEntityComponent.h>
#include <CryEntitySystem/IEntity.h>

////////////////////////////////////////////////////////
// Spawn point to spawn the player
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
		desc.SetGUID("{41316132-8A1E-4073-B0CD-A242FD3D2E90}"_cry_guid);
		desc.SetEditorCategory("Game");
		desc.SetLabel("SpawnPoint");
		desc.SetDescription("This spawn point can be used to spawn entities");
		desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });
	}

	static Vec3 GetFirstSpawnPointPos()
	{
		IEntityItPtr pEntityIterator = gEnv->pEntitySystem->GetEntityIterator();
		pEntityIterator->MoveFirst();

		while (!pEntityIterator->IsEnd())
		{
			IEntity *pEntity = pEntityIterator->Next();

			if (IEntityComponent* pSpawner = pEntity->GetComponent<CSpawnPointComponent>())
			{
				return pSpawner->GetEntity()->GetPos();
			}
		}
		return Vec3(ZERO);
	}
};
