#include <CryEntitySystem/IEntitySystem.h>

void FindEntityByName()
{
	// The name of the entity that we want to query for
	// This is the name specified with SEntitySpawnParams::sName, IEntity::SetName and in the properties panel inside the Editor when an entity is selected.
	const char* szMyEntityName = "MyEntity";

	// Query the entity system for the entity, searching the by-name entity map
	// Note that this operation is more costly than looking up by Id, and is *not* reliable as entity names are not unique!
	if (IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(szMyEntityName))
	{
		/* Entity was present in the world, and the pointer can now be used. 
		   Note that the lifetime of pEntity is managed by the entity system, avoid storing the raw pointer to be safe. */
	}
}