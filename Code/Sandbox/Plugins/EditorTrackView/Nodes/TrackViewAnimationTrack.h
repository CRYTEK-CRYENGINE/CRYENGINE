// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMovie/IMovieSystem.h>
#include "TrackViewTrack.h"

// Small specialization for animation tracks
class CTrackViewAnimationTrack : public CTrackViewTrack
{
public:
	CTrackViewAnimationTrack(IAnimTrack* pTrack, CTrackViewAnimNode* pTrackAnimNode, CTrackViewNode* pParentNode, bool bIsSubTrack = false, uint subTrackIndex = 0);

	virtual bool      KeysHaveDuration() const override { return true; }
	virtual SAnimTime GetKeyDuration(const uint index) const override;
	virtual SAnimTime GetKeyAnimDuration(const uint index) const override;
	virtual SAnimTime GetKeyAnimStart(const uint index) const override;
	virtual SAnimTime GetKeyAnimEnd(const uint index) const override;

	virtual _smart_ptr<IAnimKeyWrapper> GetWrappedKey(int key);

private:
	float GetKeyDurationFromAnimationData(const SCharacterKey& key) const;
};
