// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PROCEDURAL_CLIPS_POSITIONING__H__
#define __PROCEDURAL_CLIPS_POSITIONING__H__

#include <ICryMannequin.h>

struct SProceduralClipPosAdjustTargetLocatorParams
	: public IProceduralParams
{
	SProceduralClipPosAdjustTargetLocatorParams()
	{
	}

	virtual void Serialize(Serialization::IArchive& ar) override;

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = targetScopeName.c_str();
	}

	SProcDataCRC    targetScopeName;
	TProcClipString targetJointName;
	SProcDataCRC    targetStateName;
};

#endif
