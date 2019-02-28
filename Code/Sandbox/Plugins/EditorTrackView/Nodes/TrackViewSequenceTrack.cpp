// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackViewSequenceTrack.h"
#include "TrackViewPlugin.h"
#include "TrackViewSequenceManager.h"

// Facade key for editing for translating sequence GUID <-> name
struct SSequenceKeyFacade : public SSequenceKey
{
	static const char* GetType() { return "SequenceKeyFacade"; }

	SSequenceKeyFacade()
		: SSequenceKey()
		, m_owningSequenceGUID(CryGUID::Null())
	{}

	SSequenceKeyFacade(CryGUID owningSequenceGUID)
		: SSequenceKey()
		, m_owningSequenceGUID(owningSequenceGUID)
	{}

	void Serialize(Serialization::IArchive& ar)
	{
		STrackKey::Serialize(ar);
		ar(m_owningSequenceGUID, "owningSequenceGUID");

		CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
		CTrackViewSequence* pOwningSequence = pSequenceManager->GetSequenceByGUID(m_owningSequenceGUID);

		Serialization::StringList sequenceNames;
		assert(pOwningSequence);
		if (pOwningSequence)
		{
			const uint numSequences = pSequenceManager->GetCount();
			for (uint i = 0; i < numSequences; ++i)
			{
				// To prevent evaluation loops, to be selected sequence can't be an ancestor of the sequence of this key or the same sequence.
				CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByIndex(i);
				if (pSequence != pOwningSequence && !pSequence->IsAncestorOf(pOwningSequence))
				{
					const char* pSequenceName = pSequence->GetName();
					sequenceNames.push_back(pSequenceName);
				}
			}
		}

		string sequenceName;
		if (ar.isOutput())
		{
			CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByGUID(m_sequenceGUID);
			if (pSequence)
			{
				Serialization::StringListValue sequenceNameListValue(sequenceNames, pSequence->GetName());
				ar(sequenceNameListValue, "sequence", "Sequence");
			}
			else
			{
				Serialization::StringListValue sequenceNameListValue(sequenceNames, 0);
				ar(sequenceNameListValue, "sequence", "Sequence");
			}
		}
		else if (ar.isInput())
		{
			Serialization::StringListValue sequenceNameListValue(sequenceNames, 0);
			ar(sequenceNameListValue, "sequence", "Sequence");
			CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByName(sequenceNameListValue.c_str());
			m_sequenceGUID = pSequence ? pSequence->GetGUID() : CryGUID::Null();
		}

		ar(m_startTime, "startTime", "Start Time");
		ar(m_endTime, "endTime", "End Time");
		ar(m_speed, "speed", "Speed");
		ar(m_boverrideTimes, "overrideTimes", "Override Times");
		ar(m_bDoNotStop, "doNotStop", "Do Not Stop");
	}

private:
	CryGUID m_owningSequenceGUID;
};

SERIALIZATION_ANIM_KEY(SSequenceKeyFacade);

string CTrackViewSequenceTrack::GetKeyDescription(const uint index) const
{
	SSequenceKey key;
	GetKey(index, &key);

	CTrackViewSequence* pSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(key.m_sequenceGUID);
	if (pSequence)
	{
		return pSequence->GetName();
	}

	return "";
}

void CTrackViewSequenceTrack::SetKey(uint keyIndex, const STrackKey* pKey)
{
	const SSequenceKeyFacade* pKeyFacade = static_cast<const SSequenceKeyFacade*>(pKey);
	SSequenceKey sequenceKey = *pKeyFacade;

	if (sequenceKey.m_speed <= 0.0f)
	{
		sequenceKey.m_speed = 1.0f;
	}

	CTrackViewTrack::SetKey(keyIndex, &sequenceKey);
}

_smart_ptr<IAnimKeyWrapper> CTrackViewSequenceTrack::GetWrappedKey(int key)
{
	const CryGUID sequenceGUID = GetSequence()->GetGUID();
	SSequenceKeyFacade sequenceKey(sequenceGUID);
	GetKey(key, &sequenceKey);

	SAnimKeyWrapper<SSequenceKeyFacade>* pWrappedKey = new SAnimKeyWrapper<SSequenceKeyFacade>();
	pWrappedKey->m_key = sequenceKey;
	return pWrappedKey;
}

const char* CTrackViewSequenceTrack::GetKeyType() const
{
	return SSequenceKeyFacade::GetType();
}

SAnimTime CTrackViewSequenceTrack::GetKeyDuration(const uint index) const
{
	SSequenceKey key;
	GetKey(index, &key);

	SAnimTime duration;
	if (key.m_boverrideTimes)
	{
		duration = (key.m_endTime - key.m_startTime);
	}
	else
	{
		CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
		CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByGUID(key.m_sequenceGUID);
		duration = pSequence ? pSequence->GetTimeRange().Length() : SAnimTime(0);
	}

	return SAnimTime(duration / key.m_speed);
}

SAnimTime CTrackViewSequenceTrack::GetKeyAnimDuration(const uint index) const
{
	SSequenceKey key;
	GetKey(index, &key);

	CTrackViewSequence* pSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(key.m_sequenceGUID);
	if (pSequence)
	{
		return pSequence->GetTimeRange().Length() / key.m_speed;
	}

	return SAnimTime(0);
}

SAnimTime CTrackViewSequenceTrack::GetKeyAnimStart(const uint index) const
{
	SSequenceKey key;
	GetKey(index, &key);
	return key.m_startTime;
}

