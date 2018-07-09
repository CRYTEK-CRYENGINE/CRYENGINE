#include <CryEntitySystem/IEntitySystem.h>

void SpawnEntity(const Matrix34& initialWorldTransformation)
{
	// Create a spawn parameters structure, used to specify the initial state of the entity we want to spawn
	SEntitySpawnParams spawnParameters;
	// Specify position in world coordinates
	spawnParameters.vPosition = initialWorldTransformation.GetTranslation();
	// Specify world rotation
	spawnParameters.qRotation = Quat(initialWorldTransformation);
	// Specify scale
	spawnParameters.vScale = initialWorldTransformation.GetScale();

	// Now spawn the entity in the world
	if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParameters))
	{
		/* pEntity can now be used, and will persist until manually removed or when the level is unloaded */
	}
}