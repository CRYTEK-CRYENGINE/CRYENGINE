// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ICryMannequin.h"

#include <Mannequin/Serialization.h>

struct SProceduralClipEventParams
	: public IProceduralParams
{
	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(eventName, "EventName", "Event Name");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = eventName.c_str();
	}

	SProcDataCRC eventName;
};

class CProceduralClipEvent : public TProceduralClip<SProceduralClipEventParams>
{
public:
	CProceduralClipEvent()
	{
	}

	virtual void OnEnter(const CTimeValue& blendTime, const CTimeValue& duration, const SProceduralClipEventParams& params)
	{
		SendActionEvent(params.eventName.crc);
	}

	virtual void OnExit(const CTimeValue& blendTime)  {}

	virtual void Update(const CTimeValue& timePassed) {}

};

REGISTER_PROCEDURAL_CLIP(CProceduralClipEvent, "ActionEvent");
