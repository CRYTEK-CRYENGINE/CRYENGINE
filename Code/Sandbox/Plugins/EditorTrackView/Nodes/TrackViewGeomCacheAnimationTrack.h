// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once

#if defined(USE_GEOM_CACHES)
	#include <CryMovie/IMovieSystem.h>
	#include "TrackViewTrack.h"

////////////////////////////////////////////////////////////////////////////
// This class represents a time range track of a geom cache node in TrackView
////////////////////////////////////////////////////////////////////////////
class CTrackViewGeomCacheAnimationTrack : public CTrackViewTrack
{
public:
	CTrackViewGeomCacheAnimationTrack(IAnimTrack* pTrack, CTrackViewAnimNode* pTrackAnimNode,
	                                  CTrackViewNode* pParentNode, bool bIsSubTrack = false, unsigned int subTrackIndex = 0)
		: CTrackViewTrack(pTrack, pTrackAnimNode, pParentNode, bIsSubTrack, subTrackIndex) {}

private:
	virtual CTrackViewKeyHandle CreateKey(const SAnimTime time) override;

	virtual bool                KeysHaveDuration() const override { return true; }

	virtual SAnimTime           GetKeyDuration(const uint index) const override;
	virtual SAnimTime           GetKeyAnimDuration(const uint index) const override;
	virtual SAnimTime           GetKeyAnimStart(const uint index) const override;
	virtual bool                IsKeyAnimLoopable(const uint index) const override;
};
#endif

