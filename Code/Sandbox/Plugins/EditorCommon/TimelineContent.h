// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Color.h>
#include <CrySerialization/SmartPtr.h>
#include "Serialization.h"
#include <qnamespace.h>

#include <CryCore/smartptr.h>
#include <CryMovie/AnimTime.h>

struct STimelineElement
{
	enum EType
	{
		KEY,
		CLIP
	};

	enum ECaps
	{
		CAP_SELECT               = BIT(0),
		CAP_DELETE               = BIT(1),
		CAP_MOVE                 = BIT(2),
		CAP_CHANGE_DURATION      = BIT(3),
		CAP_CHANGE_BLEND_IN_TIME = BIT(4),
		CAP_CLIP_DRAW_KEY        = BIT(5)
	};

	EType          type;
	int            caps;
	SAnimTime      start;
	SAnimTime      end;
	SAnimTime      blendInDuration;
	SAnimTime      clipAnimDuration;
	SAnimTime      clipAnimStart;
	SAnimTime      clipAnimEnd;
	ColorB         color;
	float          baseWeight;
	uint64         userId;
	string         description;
	DynArray<char> userSideLoad;
	// state flags
	bool           selected        : 1;
	bool           added           : 1;
	bool           deleted         : 1;
	bool           sideLoadChanged : 1;
	bool           clipLoopable    : 1;
	bool           highlighted     : 1;
	SAnimTime      truncatedDuration;
	SAnimTime      nextKeyTime;
	SAnimTime      nextBlendInTime;

	STimelineElement()
		: type(KEY)
		, start(0)
		, end(0)
		, clipAnimDuration(0)
		, clipAnimStart(0)
		, color(212, 212, 212, 255)
		, caps(CAP_SELECT | CAP_MOVE)
		, selected(false)
		, added(false)
		, deleted(false)
		, sideLoadChanged(false)
		, clipLoopable(false)
		, highlighted(false)
		, userId(0)
		, blendInDuration()
	{
	}

	void Serialize(IArchive& ar)
	{
		ar(type, "type", "^>80>");
		ar(start, "start", "^");
		if (type == CLIP)
			ar(end, "end", "^");
		ar(BitFlags<ECaps>(caps), "caps", "Capabilities");
		ar(blendInDuration, "blendInDuration", "Blend In Duration");
		ar(color, "color", "Color");
		ar(userId, "userId", "User ID");
		ar(description, "description", "Description");
	}

	bool operator<(const STimelineElement& rhs) const { return start < rhs.start; }
};
typedef std::vector<STimelineElement> STimelineElements;

struct STimelineTrack;
typedef std::vector<_smart_ptr<STimelineTrack>> STimelineTracks;

struct STimelineTrack : public _i_reference_target_t
{
	enum ECaps
	{
		CAP_ADD_ELEMENTS               = BIT(0),
		CAP_DESCRIPTION_TRACK          = BIT(1), // No keys
		CAP_COMPOUND_TRACK             = BIT(2), // No own keys, but will show combined keys for child tracks
		CAP_TOGGLE_TRACK               = BIT(3), // For boolean tracks that are either on or off between keys. Used to key visibility etc.
		CAP_CLIP_TRUNCATED_BY_NEXT_KEY = BIT(4)
	};
	bool              expanded            : 1;
	bool              modified            : 1;
	bool              selected            : 1;
	bool              deleted             : 1;
	bool              keySelectionChanged : 1;
	bool              toggleDefaultState  : 1; // Default state for toggle tracks (on or off)
	bool              disabled            : 1;
	int               height;
	int               caps;
	SAnimTime         startTime;
	SAnimTime         endTime;
	string            type;
	string            name;
	string            icon;
	DynArray<char>    userSideLoad;
	STimelineElements elements;
	STimelineElement  defaultElement;
	STimelineTracks   tracks;

	STimelineTrack()
		: expanded(true)
		, modified(false)
		, selected(false)
		, deleted(false)
		, keySelectionChanged(false)
		, toggleDefaultState(false)
		, disabled(false)
		, height(64)
		, startTime(0.0f)
		, endTime(1.0f)
		, caps(CAP_ADD_ELEMENTS)
	{}

	void Serialize(IArchive& ar)
	{
		ar(name, "name", "^");
		ar(type, "type", "^");
		ar(icon, "icon", "^");
		ar(height, "height", "Height");
		ar(startTime, "startTime", "Start Time");
		ar(endTime, "endTime", "End Time");
		ar(elements, "elements", "Elements");
		ar(tracks, "tracks", "+Tracks");
	}

	bool HasIcon()
	{
		return (icon.empty() == false);
	}
};

struct STimelineContent
{
	STimelineTrack track;
	DynArray<char> userSideLoad;

	void           Serialize(IArchive& ar)
	{
		ar(track, "track", "Track");
	}
};

struct SHeaderElement
{
	string        description;
	SAnimTime     time;
	Qt::Alignment alignment;
	string        pixmap;
	bool          visible;
};

