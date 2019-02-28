#include <CryPhysics/physinterface.h>

// Gets the position of a physical entity using the pe_status_pos structure
void GetPhysicalEntityPosition(IPhysicalEntity& physicalEntity)
{
	// The pe_status_pos structure is used to query the world-coordinates of an entity
	pe_status_pos positionStatus;
	if (physicalEntity.GetStatus(&positionStatus))
	{
		/* positionStatus can now be used */
	}
}