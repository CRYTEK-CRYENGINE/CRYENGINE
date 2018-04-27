// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Definitions for various modifiers to target tracks

   -------------------------------------------------------------------------
   History:
   - 02:08:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __TARGET_TRACK_MODIFIERS_H__
#define __TARGET_TRACK_MODIFIERS_H__

#include "TargetTrackCommon.h"

struct SStimulusInvocation;

struct ITargetTrackModifier
{
	virtual ~ITargetTrackModifier()
	{
	}

	virtual uint32 GetUniqueId() const = 0;

	// Returns if this modifier matches the given xml tag
	virtual bool        IsMatchingTag(const char* szTag) const = 0;
	virtual char const* GetTag() const = 0;

	// Returns the modifier value
	virtual float GetModValue(const CTargetTrack* pTrack, TargetTrackHelpers::EAIEventStimulusType stimulusType,
	                          const Vec3& vPos, const TargetTrackHelpers::SEnvelopeData& envelopeData,
	                          const TargetTrackHelpers::STargetTrackModifierConfig& modConfig) const = 0;
};

class CTargetTrackDistanceModifier : public ITargetTrackModifier
{
public:
	CTargetTrackDistanceModifier();
	virtual ~CTargetTrackDistanceModifier();

	enum { UNIQUE_ID = 1 };
	virtual uint32      GetUniqueId() const { return UNIQUE_ID; }

	virtual bool        IsMatchingTag(const char* szTag) const;
	virtual char const* GetTag() const;
	virtual float       GetModValue(const CTargetTrack* pTrack, TargetTrackHelpers::EAIEventStimulusType stimulusType,
	                                const Vec3& vPos, const TargetTrackHelpers::SEnvelopeData& envelopeData,
	                                const TargetTrackHelpers::STargetTrackModifierConfig& modConfig) const;
};

class CTargetTrackHostileModifier : public ITargetTrackModifier
{
public:
	CTargetTrackHostileModifier();
	virtual ~CTargetTrackHostileModifier();

	enum { UNIQUE_ID = 2 };
	virtual uint32      GetUniqueId() const { return UNIQUE_ID; }

	virtual bool        IsMatchingTag(const char* szTag) const;
	virtual char const* GetTag() const;
	virtual float       GetModValue(const CTargetTrack* pTrack, TargetTrackHelpers::EAIEventStimulusType stimulusType,
	                                const Vec3& vPos, const TargetTrackHelpers::SEnvelopeData& envelopeData,
	                                const TargetTrackHelpers::STargetTrackModifierConfig& modConfig) const;
};

class CTargetTrackClassThreatModifier : public ITargetTrackModifier
{
public:
	CTargetTrackClassThreatModifier();
	virtual ~CTargetTrackClassThreatModifier();

	enum { UNIQUE_ID = 3 };
	virtual uint32      GetUniqueId() const { return UNIQUE_ID; }

	virtual bool        IsMatchingTag(const char* szTag) const;
	virtual char const* GetTag() const;
	virtual float       GetModValue(const CTargetTrack* pTrack, TargetTrackHelpers::EAIEventStimulusType stimulusType,
	                                const Vec3& vPos, const TargetTrackHelpers::SEnvelopeData& envelopeData,
	                                const TargetTrackHelpers::STargetTrackModifierConfig& modConfig) const;
};

class CTargetTrackDistanceIgnoreModifier : public ITargetTrackModifier
{
public:
	CTargetTrackDistanceIgnoreModifier();
	virtual ~CTargetTrackDistanceIgnoreModifier();

	enum { UNIQUE_ID = 4 };
	virtual uint32      GetUniqueId() const { return UNIQUE_ID; }

	virtual bool        IsMatchingTag(const char* szTag) const;
	virtual char const* GetTag() const;
	virtual float       GetModValue(const CTargetTrack* pTrack, TargetTrackHelpers::EAIEventStimulusType stimulusType,
	                                const Vec3& vPos, const TargetTrackHelpers::SEnvelopeData& envelopeData,
	                                const TargetTrackHelpers::STargetTrackModifierConfig& modConfig) const;
};

class CTargetTrackPlayerModifier : public ITargetTrackModifier
{
public:
	enum { UNIQUE_ID = 5 };
	virtual uint32      GetUniqueId() const { return UNIQUE_ID; }

	virtual bool        IsMatchingTag(const char* szTag) const;
	virtual char const* GetTag() const;
	virtual float       GetModValue(const CTargetTrack* pTrack, TargetTrackHelpers::EAIEventStimulusType stimulusType,
	                                const Vec3& vPos, const TargetTrackHelpers::SEnvelopeData& envelopeData,
	                                const TargetTrackHelpers::STargetTrackModifierConfig& modConfig) const;
};

#endif //__TARGET_TRACK_MODIFIERS_H__
