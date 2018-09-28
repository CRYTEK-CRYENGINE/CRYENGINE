// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once

#include <CryMovie/IMovieSystem.h>
#include "TrackViewTrack.h"

// Small specialization for sequence tracks
class CTrackViewSequenceTrack : public CTrackViewTrack
{
public:
	CTrackViewSequenceTrack(IAnimTrack* pTrack, CTrackViewAnimNode* pTrackAnimNode,
	                        CTrackViewNode* pParentNode, bool bIsSubTrack = false, uint subTrackIndex = 0)
		: CTrackViewTrack(pTrack, pTrackAnimNode, pParentNode, bIsSubTrack, subTrackIndex) {}

private:
	virtual string                      GetKeyDescription(const uint index) const override;

	virtual void                        SetKey(uint keyIndex, const STrackKey* pKey) override;
	virtual _smart_ptr<IAnimKeyWrapper> GetWrappedKey(int key) override;

	virtual const char*                 GetKeyType() const override;

	virtual bool                        KeysHaveDuration() const override { return true; }

	virtual CTimeValue                   GetKeyDuration(const uint index) const override;
	virtual CTimeValue                   GetKeyAnimDuration(const uint index) const override;
	virtual CTimeValue                   GetKeyAnimStart(const uint index) const override;
};

