#pragma once

#include <CryEntitySystem/IEntityComponent.h>

////////////////////////////////////////////////////////
// Sample entity for creating a camera source
////////////////////////////////////////////////////////
class CCameraSource : public IEntityComponent
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CCameraSource, "CameraSource", "19d1ad13-1867-49a0-a93e-a7ca58b1f698"_cry_guid);

	virtual ~CCameraSource() {}

	// IEntityComponent
	virtual void Initialize() override;
	// ~IEntityComponent
};
