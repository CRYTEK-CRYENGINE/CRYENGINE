#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/physinterface.h>
#include <Cry3DEngine/I3DEngine.h>

// Example of how a component can listen to collision events
// Note that this example assumes that the entity has already been physicalized
class CPhysicsMonitorCompoent final : public IEntityComponent
{
public:
	// Provide a virtual destructor, ensuring correct destruction of IEntityComponent members
	virtual ~CPhysicsMonitorCompoent() = default;

	static void ReflectType(Schematyc::CTypeDesc<CPhysicsMonitorCompoent>& desc) { /* Reflect type here */ }

	virtual void ProcessEvent(const SEntityEvent& event) override
	{
		if (event.event == ENTITY_EVENT_COLLISION)
		{
			// Get the EventPhysCollision structure describing the collision that occurred
			const EventPhysCollision* pPhysCollision = reinterpret_cast<const EventPhysCollision*>(event.nParam[0]);

			// The EventPhysCollision provides information for both the source and target entities
			// Therefore, we store the indices of this entity (and the other collider)
			// This can for example be used to get the surface identifier (and thus the material) of the part of our entity that collided
			const int thisEntityIndex = static_cast<int>(event.nParam[1]);
			// Calculate the index of the other entity
			const int otherEntityIndex = (thisEntityIndex + 1) % 2;

			// Get the contact point (in world coordinates) of the two entities
			const Vec3& contactPoint = pPhysCollision->pt;
			// Get the collision normal vector
			const Vec3& contactNormal = pPhysCollision->n;

			// Get properties for our entity, starting with the local velocity of our entity at the contact point
			const Vec3& relativeContactVelocity = pPhysCollision->vloc[thisEntityIndex];
			// Get the mass of our entity
			const float contactMass = pPhysCollision->mass[thisEntityIndex];
			// Get the identifier of the part of our entity that collided
			// This is the same identifier that is added with IPhysicalEntity::AddGeometry
			const int contactPartId = pPhysCollision->partid[thisEntityIndex];
			// Get the surface on our entity that collided
			const int contactSurfaceId = pPhysCollision->idmat[thisEntityIndex];
			// Get the ISurfaceType representation of the surface that collided
			if (const ISurfaceType* pContactSurface = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(contactSurfaceId))
			{
				/* Interact with pContactSurface here*/
			}
		}
	}

	virtual uint64 GetEventMask() const override
	{
		// Indicate that we want to receive the ENTITY_EVENT_COLLISION entity event
		return ENTITY_EVENT_BIT(ENTITY_EVENT_COLLISION);
	}
};