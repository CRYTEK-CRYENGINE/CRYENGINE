#include <CryPhysics/physinterface.h>

// Gets the dynamic state of an entity, such as its velocity, mass and angular velocity
void GetDynamicsStatus(IPhysicalEntity& physicalEntity)
{
	pe_status_dynamics dynStatus;
	if (physicalEntity.GetStatus(&dynStatus))
	{
		/* The pe_status_dynamics can now be used */
		const Vec3& velocity = dynStatus.v;
		const Vec3& angularVelocity = dynStatus.w;
	}
}