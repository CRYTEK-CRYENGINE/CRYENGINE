// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __animtrack_h__
#define __animtrack_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryMovie/IMovieSystem.h>

#include <CrySerialization/IArchive.h>
#include <CrySerialization/IntrusiveFactory.h>

/** General templated track for event type keys.
    KeyType class must be derived from IKey.
 */
template<class KeyType>
class TAnimTrack : public IAnimTrack
{
public:
	TAnimTrack();

	virtual EAnimValue                  GetValueType() override                                { return eAnimValue_Unknown; }

	virtual int                         GetSubTrackCount() const override                      { return 0; };
	virtual IAnimTrack*                 GetSubTrack(int nIndex) const override                 { return 0; };
	virtual const char*                 GetSubTrackName(int nIndex) const override             { return NULL; };
	virtual void                        SetSubTrackName(int nIndex, const char* name) override { assert(0); }

	virtual int                         GetNumKeys() const override                            { return m_keys.size(); }
	virtual bool                        HasKeys() const override                               { return !m_keys.empty(); }
	virtual void                        RemoveKey(int num) override;
	virtual void                        ClearKeys() override;

	virtual int                         CreateKey(SAnimTime time) override;

	virtual const char*                 GetKeyType() const override;
	virtual bool                        KeysDeriveTrackDurationKey() const override;
	virtual _smart_ptr<IAnimKeyWrapper> GetWrappedKey(int key) override;

	//! Get key at specified location.
	//! @param key Must be valid pointer to compatible key structure, to be filled with specified key location.
	virtual void GetKey(int index, STrackKey* key) const override;

	//! Get time of specified key.
	//! @return key time.
	virtual SAnimTime GetKeyTime(int index) const override;

	//! Find key at given time.
	//! @return Index of found key, or -1 if key with this time not found.
	virtual int FindKey(SAnimTime time) override;

	//! Set key at specified location.
	//! @param key Must be valid pointer to compatible key structure.
	virtual void SetKey(int index, const STrackKey* key) override;

	//! Set or create a key at a specific time
	void SetKeyAtTime(SAnimTime time, STrackKey* key);

	//! Get track flags.
	virtual int GetFlags() override { return m_flags; };

	//! Check if track is masked
	virtual bool IsMasked(const uint32 mask) const override { return false; }

	//! Set track flags.
	virtual void SetFlags(int flags) override { m_flags = flags; };

	// Get track value at specified time. Interpolates keys if needed.
	virtual TMovieSystemValue GetValue(SAnimTime time) const override { return TMovieSystemValue(SMovieSystemVoid()); }

	// Get track default value
	virtual TMovieSystemValue GetDefaultValue() const override { return TMovieSystemValue(SMovieSystemVoid()); }

	// Set track default value
	virtual void SetDefaultValue(const TMovieSystemValue& defaultValue) override {}

	/** Assign active time range for this track.
	 */
	virtual void SetTimeRange(TRange<SAnimTime> timeRange) override { m_timeRange = timeRange; }
	
	//! Serialize unique parameters for this track.
	virtual void Serialize(Serialization::IArchive& ar) override {}

	/** Serialize this animation track to XML.
	    Do not override this method, prefer to override SerializeKey.
	 */
	virtual bool Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks = true) override;

	/** Serialize the keys of this animation track to XML.
	 */
	virtual bool SerializeKeys(XmlNodeRef& xmlNode, bool bLoading, std::vector<SAnimTime>& keys, const SAnimTime time = SAnimTime(0)) override;

	/** Serialize single key of this track.
	    override this in derived classes.
	    Do not save time attribute, it is already saved in Serialize of the track.
	 */
	virtual void SerializeKey(KeyType& key, XmlNodeRef& keyNode, bool bLoading) = 0;

	/** Get last key before specified time.
	   @return Index of key, or -1 if such key not exist.
	 */
	int             GetActiveKey(SAnimTime time, KeyType* key);

	virtual void    GetKeyValueRange(float& fMin, float& fMax) const override { fMin = m_fMinKeyValue; fMax = m_fMaxKeyValue; }
	virtual void    SetKeyValueRange(float fMin, float fMax) override         { m_fMinKeyValue = fMin; m_fMaxKeyValue = fMax; }

	virtual CryGUID GetGUID() const override                                  { return m_guid; }

protected:
	CryGUID m_guid;

	typedef std::vector<KeyType> Keys;
	Keys              m_keys;
	TRange<SAnimTime> m_timeRange;

	int               m_currKey : 31;
	SAnimTime         m_lastTime;
	int               m_flags;

	float             m_fMinKeyValue;
	float             m_fMaxKeyValue;

	struct SCompKeyTime
	{
		bool operator()(const KeyType& l, const KeyType& r) const { return l.m_time < r.m_time; }
		bool operator()(SAnimTime l, const KeyType& r) const      { return l < r.m_time; }
		bool operator()(const KeyType& l, SAnimTime r) const      { return l.m_time < r; }
	};
};

template<class KeyType>
inline TAnimTrack<KeyType>::TAnimTrack()
	: m_currKey(0)
	, m_flags(0)
	, m_fMinKeyValue(0.0f)
	, m_fMaxKeyValue(0.0f)
	, m_lastTime(SAnimTime::Min())
	, m_guid(CryGUID::Create())
{
}

template<class KeyType>
inline void TAnimTrack<KeyType >::RemoveKey(int index)
{
	assert(index >= 0 && index < (int)m_keys.size());
	m_keys.erase(m_keys.begin() + index);
}

template<class KeyType>
inline void TAnimTrack<KeyType >::ClearKeys()
{
	m_keys.clear();
}

template<class KeyType>
inline void TAnimTrack<KeyType >::GetKey(int index, STrackKey* key) const
{
	assert(index >= 0 && index < (int)m_keys.size());
	assert(key != 0);
	*(KeyType*)key = m_keys[index];
}

template<class KeyType>
inline void TAnimTrack<KeyType >::SetKey(int index, const STrackKey* key)
{
	assert(index >= 0 && index < (int)m_keys.size());
	assert(key != 0);

	m_keys[index] = *static_cast<const KeyType*>(key);
}

template<class KeyType>
inline SAnimTime TAnimTrack<KeyType >::GetKeyTime(int index) const
{
	assert(index >= 0 && index < (int)m_keys.size());
	return m_keys[index].m_time;
}

template<class KeyType>
inline void TAnimTrack<KeyType >::SetKeyAtTime(SAnimTime time, STrackKey* key)
{
	assert(key != 0);
	const int keyIndex = FindKey(time);
	if (keyIndex != -1)
	{
		SetKey(keyIndex, key);
	}
	else
	{
		// Key with this time not found.
		// Create a new one.
		int keyIndex = CreateKey(time);
		// Reserve the flag value.
		SetKey(keyIndex, key);
	}
}

template<class KeyType>
inline int TAnimTrack<KeyType >::CreateKey(SAnimTime time)
{
	const typename Keys::iterator iter = std::lower_bound(m_keys.begin(), m_keys.end(), time, SCompKeyTime());

	KeyType key;
	key.m_time = time;
	const typename Keys::iterator insert_iter = m_keys.insert(iter, key);

	return static_cast<int>(insert_iter - m_keys.begin());
}

template<class KeyType>
inline int TAnimTrack<KeyType >::FindKey(SAnimTime time)
{
	const typename Keys::iterator iter = std::lower_bound(m_keys.begin(), m_keys.end(), time, SCompKeyTime());

	if ((iter != m_keys.end()) && (iter->m_time == time))
	{
		return static_cast<int>(iter - m_keys.begin());
	}

	return -1;
}

template<class KeyType>
inline bool TAnimTrack<KeyType >::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	if (bLoading)
	{
		int num = xmlNode->getChildCount();

		TRange<SAnimTime> timeRange;
		timeRange.start.Serialize(xmlNode, bLoading, "startTimeTicks", "StartTime");
		timeRange.end.Serialize(xmlNode, bLoading, "endTimeTicks", "EndTime");
		int flags = m_flags;
		xmlNode->getAttr("Flags", flags);
		xmlNode->getAttr("GUIDLo", m_guid.lopart);
		xmlNode->getAttr("GUIDHi", m_guid.hipart);
		SetFlags(flags);
		SetTimeRange(timeRange);

		m_keys.resize(num);
		for (int i = 0; i < num; i++)
		{
			XmlNodeRef keyNode = xmlNode->getChild(i);
			m_keys[i].m_time.Serialize(keyNode, bLoading, "timeTicks", "time");
			SerializeKey(m_keys[i], keyNode, bLoading);
		}

		if ((!num) && (!bLoadEmptyTracks))
		{
			return false;
		}
	}
	else
	{
		int num = GetNumKeys();
		xmlNode->setAttr("Flags", GetFlags());
		m_timeRange.start.Serialize(xmlNode, bLoading, "startTimeTicks", "StartTime");
		m_timeRange.end.Serialize(xmlNode, bLoading, "endTimeTicks", "EndTime");
		xmlNode->setAttr("GUIDLo", m_guid.lopart);
		xmlNode->setAttr("GUIDHi", m_guid.hipart);

		for (int i = 0; i < num; i++)
		{
			XmlNodeRef keyNode = xmlNode->newChild("Key");
			m_keys[i].m_time.Serialize(keyNode, bLoading, "timeTicks", "time");
			SerializeKey(m_keys[i], keyNode, bLoading);
		}
	}

	return true;
}

template<class KeyType>
inline bool TAnimTrack<KeyType >::SerializeKeys(XmlNodeRef& xmlNode, bool bLoading, std::vector<SAnimTime>& keys, const SAnimTime time)
{
	if (bLoading)
	{
		int numCur = GetNumKeys();
		int numNew = xmlNode->getChildCount();

		TRange<SAnimTime> timeRange;
		timeRange.start.Serialize(xmlNode, bLoading, "startTimeTicks", "StartTime");
		timeRange.end.Serialize(xmlNode, bLoading, "endTimeTicks", "EndTime");
		int flags = m_flags;
		xmlNode->getAttr("Flags", flags);
		SetFlags(flags);
		SetTimeRange(timeRange);

		SAnimTime timeOffset(0);
		m_keys.resize(numCur + numNew);
		for (int i = 0; i < numNew; i++)
		{
			XmlNodeRef keyNode = xmlNode->getChild(i);
			m_keys[numCur + i].m_time.Serialize(keyNode, bLoading, "timeTicks", "time");
			if ((i == 0) && (numNew == 1))		//numNew == 1 condition means: place a new key under mouse only during single key selection
												//during multiple selection - place key as it is
			{
				timeOffset = (time - m_keys[numCur + i].m_time);
			}
			m_keys[numCur + i].m_time += timeOffset;
			if (keys.size() < keys.capacity())
			{
				keys.push_back(m_keys[numCur + i].m_time);
			}
			SerializeKey(m_keys[numCur + i], keyNode, bLoading);
		}

		std::sort(m_keys.begin(), m_keys.end());

		if ((!numNew))
		{
			return false;
		}
	}
	else
	{
		int numCur = GetNumKeys();
		xmlNode->setAttr("Flags", GetFlags());
		m_timeRange.start.Serialize(xmlNode, bLoading, "startTimeTicks", "StartTime");
		m_timeRange.end.Serialize(xmlNode, bLoading, "endTimeTicks", "EndTime");

		for (int i = 0; i < numCur; i++)
		{
			for (int j = 0; j < keys.size(); ++j)
			{
				if (m_keys[i].m_time == keys[j])
				{
					XmlNodeRef keyNode = xmlNode->newChild("Key");
					m_keys[i].m_time.Serialize(keyNode, bLoading, "timeTicks", "time");
					SerializeKey(m_keys[i], keyNode, bLoading);
					break;
				}
			}
		}
	}

	return true;
}

template<class KeyType>
const char* TAnimTrack<KeyType >::GetKeyType() const
{
	return KeyType::GetType();
}

template<class KeyType>
bool TAnimTrack<KeyType >::KeysDeriveTrackDurationKey() const
{
	return KeyType::DerivesTrackDurationKey();
}

template<class KeyType>
_smart_ptr<IAnimKeyWrapper> TAnimTrack<KeyType >::GetWrappedKey(int key)
{
	SAnimKeyWrapper<KeyType>* keyWrapper = new SAnimKeyWrapper<KeyType>();
	keyWrapper->m_key = m_keys[key];
	return keyWrapper;
}

template<class KeyType>
inline int TAnimTrack<KeyType >::GetActiveKey(SAnimTime time, KeyType* key)
{
	if (key == NULL)
	{
		return -1;
	}

	int nkeys = m_keys.size();

	if (nkeys == 0)
	{
		m_lastTime = time;
		m_currKey = -1;
		return m_currKey;
	}

	bool bTimeWrap = false;

	m_lastTime = time;

	// Time is before first key.
	if (m_keys[0].m_time > time)
	{
		if (bTimeWrap)
		{
			// If time wrapped, active key is last key.
			m_currKey = nkeys - 1;
			*key = m_keys[m_currKey];
		}
		else
		{
			m_currKey = -1;
		}

		return m_currKey;
	}

	if (m_currKey < 0)
	{
		m_currKey = 0;
	}

	// Start from current key.
	int i;

	for (i = m_currKey; i < nkeys; i++)
	{
		if (time >= m_keys[i].m_time)
		{
			if ((i >= nkeys - 1) || (time < m_keys[i + 1].m_time))
			{
				m_currKey = i;
				*key = m_keys[m_currKey];
				return m_currKey;
			}
		}
		else
		{
			break;
		}
	}

	// Start from beginning.
	for (i = 0; i < nkeys; i++)
	{
		if (time >= m_keys[i].m_time)
		{
			if ((i >= nkeys - 1) || (time < m_keys[i + 1].m_time))
			{
				m_currKey = i;
				*key = m_keys[m_currKey];
				return m_currKey;
			}
		}
		else
		{
			break;
		}
	}

	m_currKey = -1;
	return m_currKey;
}

#endif // __animtrack_h__
