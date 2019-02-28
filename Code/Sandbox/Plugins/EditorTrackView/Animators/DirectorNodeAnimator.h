// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once

#include "Nodes/TrackViewAnimNode.h"

// This is used to bind/unbind sub sequences in director nodes
// when the sequence time changes. A sequence only gets bound if it was already
// referred in time before.
class CDirectorNodeAnimator : public IAnimNodeAnimator
{
public:
	CDirectorNodeAnimator(CTrackViewAnimNode* pDirectorNode);

	virtual void Animate(CTrackViewAnimNode* pNode, const SAnimContext& ac) override;
	virtual void Render(CTrackViewAnimNode* pNode, const SAnimContext& ac) override;
	virtual void UnBind(CTrackViewAnimNode* pNode) override;

private:
	void ForEachActiveSequence(const SAnimContext& animContext, CTrackViewTrack* pSequenceTrack,
	                           const bool bHandleOtherKeys, std::function<void(CTrackViewSequence*, const SAnimContext&)> animateFunction,
	                           std::function<void(CTrackViewSequence*, const SAnimContext&)> resetFunction);

	CTrackViewAnimNode* m_pDirectorNode;
};

