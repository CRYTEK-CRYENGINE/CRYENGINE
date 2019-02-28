#include <CryPhysics/physinterface.h>

// Sets the position of a physical entity using the pe_params_pos structure
void SetPhysicalEntityPosition(IPhysicalEntity& physicalEntity)
{
	// The pe_params_pos structure is used to set position, orientation and scale
	pe_params_pos positionParams;
	// Move the entity to {50,50,50} in world coordinates
	positionParams.pos = Vec3(50, 50, 50);
	// Reset the entity orientation
	positionParams.q = IDENTITY;
	// Make sure we use default scaling (100%)
	positionParams.scale = 1.f;

	// IPhysicalEntity::SetParams may be queued if the physics thread is empty or there are other commands queued.
	// We can change isThreadSafe to 1 if we are operating on a physics thread to have to event processed immediately
	int isThreadSafe = 0;
	// Set the position on the entity
	physicalEntity.SetParams(&positionParams, isThreadSafe);
}