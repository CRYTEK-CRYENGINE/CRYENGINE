// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once

class CTrackViewTrack;
class CTrackViewAnimNode;
class CTrackViewNode;
class CEntityObject;
struct IAnimSequence;
struct IAnimNode;
struct IAnimTrack;

class CTrackViewAnimNodeFactory
{
public:
	CTrackViewAnimNode* BuildAnimNode(IAnimSequence* pSequence, IAnimNode* pAnimNode, CTrackViewNode* pParentNode, CEntityObject* pEntity);
};

class CTrackViewTrackFactory
{
public:
	CTrackViewTrack* BuildTrack(IAnimTrack* pTrack, CTrackViewAnimNode* pTrackAnimNode,
	                            CTrackViewNode* pParentNode, bool bIsSubTrack = false, unsigned int subTrackIndex = 0);
};

