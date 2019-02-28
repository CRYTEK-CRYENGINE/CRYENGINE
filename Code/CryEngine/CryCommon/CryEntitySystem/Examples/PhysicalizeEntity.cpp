#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/physinterface.h>

// Simple example for physicalizing an entity and all of its slots
void Physicalize(IEntity& entity)
{
	// The SEntityPhysicalizeParams is used to describe how an entity should be physicalized
	SEntityPhysicalizeParams physicalizeParams;
	// The type indicates which IPhysicalEntity implementation to use, in our case we want a rigid entity that can be moved around in the world and influence other entities
	physicalizeParams.type = PE_RIGID;
	// Physical entities can specify either a mass, or a density - but never both. In our case, we specify a mass of 50 kilograms.
	// If density is specified, the mass is calculated based on volume.
	physicalizeParams.mass = 50.f;
	// It is possible to automatically physicalize a specific slot, or all. In our case, we choose to automatically create physics parts for each entity slot in our entity.
	physicalizeParams.nSlot = -1;

	// Call IEntity::Physicalize to create the physical entity
	entity.Physicalize(physicalizeParams);

	// Now query the physical entity, will return a valid entity if Physicalize succeeded.
	if (IPhysicalEntity* pPhysicalEntity = entity.GetPhysicalEntity())
	{
		/* pPhysicalEntity can now be used, valid until entity is dephysicalized */
	}
}