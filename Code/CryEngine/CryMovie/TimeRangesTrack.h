// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TIMERANGESTRACK_H__
#define __TIMERANGESTRACK_H__

#pragma once

//forward declarations.
#include <CryMovie/IMovieSystem.h>
#include "AnimTrack.h"

/** CTimeRangesTrack contains keys that represent generic time ranges
 */
class CTimeRangesTrack : public TAnimTrack<STimeRangeKey>
{
public:
	virtual void           SerializeKey(STimeRangeKey& key, XmlNodeRef& keyNode, bool bLoading) override;

	virtual CAnimParamType GetParameterType() const override { return eAnimParamType_TimeRanges; }

	int                    GetActiveKeyIndexForTime(const SAnimTime time);
};

#endif
