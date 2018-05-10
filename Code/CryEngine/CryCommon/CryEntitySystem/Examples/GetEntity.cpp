#include <CryEntitySystem/IEntitySystem.h>
#include <CryGame/IGameFramework.h>

void GetEntity()
{
	// Identifier of the entity that we want to find, for example found by calling IEntity::GetId.
	// For the sake of this example we will query the default player entity identifier.
	EntityId id = LOCAL_PLAYER_ENTITY_ID;

	// Query the entity system for the entity, this will result in a constant-time lookup.
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
	{
		/* Entity was present in the world, and the pointer can now be used. 
		   Note that the lifetime of pEntity is managed by the entity system, avoid storing the raw pointer to be safe. */
	}
}