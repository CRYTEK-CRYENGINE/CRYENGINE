#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/physinterface.h>

// Simple example for physicalizing an entity slot
void PhysicalizeSlot(IEntity& entity, const int slotId)
{
	// Entity must have been physicalized using IEntity::Physicalize first
	if (IPhysicalEntity* pPhysicalEntity = entity.GetPhysicalEntity())
	{
		SEntityPhysicalizeParams partPhysicalizeParams;
		// Specify the type of physicalization to use, has to be the same as the entity we are applying to
		partPhysicalizeParams.type = pPhysicalEntity->GetType();
		// Specify the slot identifier inside the physicalization parameters
		partPhysicalizeParams.nSlot = slotId;
		// Mass of this entity in kilograms
		// Note that mass and density cannot be specified at the same time, if density is specified then mass is calculated based on volume
		partPhysicalizeParams.mass = 10.f;

		// Now add the slot geometry to our physical entity
		entity.PhysicalizeSlot(slotId, partPhysicalizeParams);
	}
}