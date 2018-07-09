// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ITransformationPinning_h
#define ITransformationPinning_h

#include <CryAnimation/ICryAnimation.h>

struct ITransformationPinning : public IAnimationPoseModifier
{
	CRYINTERFACE_DECLARE_GUID(ITransformationPinning, "cc34ddea-972e-47db-93f9-cdcb98c28c8f"_cry_guid);

	virtual void SetBlendWeight(float factor)		= 0;
	virtual void SetJoint(uint32 jntID)				= 0;
	virtual void SetSource(ICharacterInstance* source)	= 0;
};

DECLARE_SHARED_POINTERS(ITransformationPinning);


#endif // ITransformationPinning_h
