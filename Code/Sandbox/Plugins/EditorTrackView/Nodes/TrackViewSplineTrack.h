// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2015.

#pragma once

#include <CryMovie/IMovieSystem.h>
#include "TrackViewTrack.h"

// We overload spline tracks to update key tangents whenever a key is set so the engine doesn't need to take care
class CTrackViewSplineTrack : public CTrackViewTrack
{
public:
	CTrackViewSplineTrack(IAnimTrack* pTrack, CTrackViewAnimNode* pTrackAnimNode,
	                      CTrackViewNode* pParentNode, bool bIsSubTrack = false, uint subTrackIndex = 0)
		: CTrackViewTrack(pTrack, pTrackAnimNode, pParentNode, bIsSubTrack, subTrackIndex) {}

	virtual CTrackViewKeyHandle CreateKey(const SAnimTime time) override;
	virtual void                SetKey(uint keyIndex, const STrackKey* pKey) override;
	virtual void                SetValue(const SAnimTime time, const TMovieSystemValue& value) override;
	virtual void                OffsetKeys(const TMovieSystemValue& offset) override;

private:
	virtual string GetKeyDescription(const uint index) const override;

	void           UpdateKeyTangents(uint keyIndex);
};

