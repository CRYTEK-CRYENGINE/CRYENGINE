#include <CryEntitySystem/IEntitySystem.h>

void TraverseEntities()
{
	// Retrieves an iterator that can be used to traverse all the entities in the scene
	// Note that this can easily become expensive due to the sheer number of entities that can be in each scene
	IEntityItPtr pEntityIterator = gEnv->pEntitySystem->GetEntityIterator();

	// Traverse the entities until IEntityIt::Next returns a nullptr
	while (IEntity* pEntity = pEntityIterator->Next())
	{
		// Skip entities that have been marked for deletion (clean-up will occur at the start of the next frame)
		if (!pEntity->IsGarbage())
		{
			/* pEntity can now be safely used */
		}
	}
}