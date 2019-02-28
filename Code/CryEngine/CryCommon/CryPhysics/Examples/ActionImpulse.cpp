#include <CryPhysics/physinterface.h>

// Applies an impulse on a physical entity
void ActionImpulse(IPhysicalEntity& physicalEntity)
{
	pe_action_impulse actionImpulse;
	// Apply the impulse one unit upwards
	// This can be combined with pe_action_impulse::point to apply the impulse on a specific point
	actionImpulse.impulse = Vec3(0, 0, 1);
	// Apply spin to the entity
	actionImpulse.angImpulse = Vec3(0, 0, 1);

	// IPhysicalEntity::Action may be queued if the physics thread is empty or there are other commands queued.
	// We can change isThreadSafe to 1 if we are operating on a physics thread to have to event processed immediately
	int isThreadSafe = 0;
	// Apply the impulse on the entity
	physicalEntity.Action(&actionImpulse, isThreadSafe);
}