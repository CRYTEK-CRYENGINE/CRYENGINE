// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ISEQUENCER_SYSTEM__H__
#define __ISEQUENCER_SYSTEM__H__
#pragma once

class CSequencerTrack;
class CSequencerNode;
class CSequencerSequence;

class XmlNodeRef;

enum ESequencerParamType
{
	SEQUENCER_PARAM_UNDEFINED = 0,
	SEQUENCER_PARAM_FRAGMENTID,
	SEQUENCER_PARAM_TAGS,
	SEQUENCER_PARAM_PARAMS,
	SEQUENCER_PARAM_ANIMLAYER,
	SEQUENCER_PARAM_PROCLAYER,
	SEQUENCER_PARAM_TRANSITIONPROPS,
	SEQUENCER_PARAM_TOTAL
};

enum ESequencerNodeType
{
	SEQUENCER_NODE_UNDEFINED,
	SEQUENCER_NODE_SEQUENCE_ANALYZER_GLOBAL,
	SEQUENCER_NODE_SCOPE,
	SEQUENCER_NODE_FRAGMENT_EDITOR_GLOBAL,
	SEQUENCER_NODE_FRAGMENT,
	SEQUENCER_NODE_FRAGMENT_CLIPS,
	SEQUENCER_NODE_TOTAL
};

enum ESequencerKeyFlags
{
	AKEY_SELECTED = 0x01
};

struct SKeyColour
{
	uint8 base[3];
	uint8 high[3];
	uint8 back[3];
};

struct CSequencerKey
{
	CSequencerKey()
		: m_time()
		, flags()
		, m_fileState()
		, m_filePath()
		, m_fileName()
	{
	}

	virtual bool IsFileInsidePak() const { return m_fileState & eIsInsidePak; }
	virtual bool IsFileInsideDB() const  { return m_fileState & eIsInsideDB; }
	virtual bool IsFileOnDisk() const    { return m_fileState & eIsOnDisk; }
	virtual bool HasValidFile() const    { return m_fileState & eHasFile; }
	virtual bool HasFileRepresentation() const
	{
		std::vector<CString> extensions;
		CString editableExtension;
		GetExtensions(extensions, editableExtension);
		return extensions.size() > 0;
	}

	virtual const CString GetFileName() const { return m_fileName; }
	virtual const CString GetFilePath() const { return m_filePath; }

	// Returns a list of all paths relevant to this file
	// E.G. for i_cafs will return paths to .ma, .icaf and .animsetting
	// N.B. path is relative to game root
	virtual void GetFilePaths(std::vector<CString>& paths, CString& relativePath, CString& fileName, CString& editableExtension) const
	{
		std::vector<CString> extensions;
		GetExtensions(extensions, editableExtension);

		relativePath = m_filePath;
		fileName = m_fileName;

		CString fullPath = PathUtil::AddSlash(m_filePath.GetString());
		fullPath += m_fileName;

		paths.reserve(extensions.size());
		for (std::vector<CString>::const_iterator iter = extensions.begin(), itEnd = extensions.end(); iter != itEnd; ++iter)
		{
			paths.push_back(fullPath + (*iter));
		}
	}

	virtual void UpdateFlags()
	{
		m_fileState = eNone;
	};

	float m_time;
	int   flags;

	bool operator<(const CSequencerKey& key) const  { return m_time < key.m_time; }
	bool operator==(const CSequencerKey& key) const { return m_time == key.m_time; }
	bool operator>(const CSequencerKey& key) const  { return m_time > key.m_time; }
	bool operator<=(const CSequencerKey& key) const { return m_time <= key.m_time; }
	bool operator>=(const CSequencerKey& key) const { return m_time >= key.m_time; }
	bool operator!=(const CSequencerKey& key) const { return m_time != key.m_time; }

protected:

	// Provides all extensions relevant to the clip in extensions, and the one used to edit the clip
	// in editableExtension...
	// E.G. for i_cafs will return .ma, .animsetting, .i_caf and return .ma also as editableExtension
	virtual void GetExtensions(std::vector<CString>& extensions, CString& editableExtension) const
	{
		extensions.clear();
		editableExtension.Empty();
	};

	enum ESequencerKeyFileState
	{
		eNone        = 0,
		eHasFile     = BIT(0),
		eIsInsidePak = BIT(1),
		eIsInsideDB  = BIT(2),
		eIsOnDisk    = BIT(3),
	};

	ESequencerKeyFileState m_fileState; // Flags to represent the state of (m_filePath + m_fileName + m_fileExtension)
	CString                m_filePath;  // The path relative to game root
	CString                m_fileName;  // The filename of the key file (e.g. filename.i_caf for CClipKey)
};

class CSequencerTrack
	: public _i_reference_target_t
{
public:
	enum ESequencerTrackFlags
	{
		SEQUENCER_TRACK_HIDDEN   = BIT(1), //!< Set when track is hidden in track view.
		SEQUENCER_TRACK_SELECTED = BIT(2), //!< Set when track is selected in track view.
	};

public:
	CSequencerTrack()
		: m_nParamType(SEQUENCER_PARAM_UNDEFINED)
		, m_bModified(false)
		, m_flags(0)
		, m_changeCount(0)
		, m_muted(false)
	{
	}

	virtual ~CSequencerTrack() {}

	virtual void                 SetNumKeys(int numKeys) = 0;
	virtual int                  GetNumKeys() const = 0;

	virtual int                  CreateKey(float time) = 0;

	virtual void                 SetKey(int index, CSequencerKey* key) = 0;
	virtual void                 GetKey(int index, CSequencerKey* key) const = 0;

	virtual const CSequencerKey* GetKey(int index) const = 0;
	virtual CSequencerKey*       GetKey(int index) = 0;

	virtual void                 RemoveKey(int num) = 0;

	virtual void                 GetKeyInfo(int key, const char*& description, float& duration) = 0;
	virtual void                 GetTooltip(int key, const char*& description, float& duration) { return GetKeyInfo(key, description, duration); }
	virtual float                GetKeyDuration(const int key) const = 0;
	virtual const SKeyColour& GetKeyColour(int key) const = 0;
	virtual const SKeyColour& GetBlendColour(int key) const = 0;

	virtual bool              CanEditKey(int key) const   { return true; }
	virtual bool              CanMoveKey(int key) const   { return true; }

	virtual bool              CanAddKey(float time) const { return true; }
	virtual bool              CanRemoveKey(int key) const { return true; }

	virtual int               CloneKey(int key) = 0;
	virtual int               CopyKey(CSequencerTrack* pFromTrack, int nFromKey) = 0;

	virtual int               GetNumSecondarySelPts(int key) const { return 0; }

	// Look for a secondary selection point in the key & time range specified.
	//
	// Returns an id that can be passed into the other secondary selection functions.
	// Returns 0 when it could not find a secondary selection point.
	virtual int GetSecondarySelectionPt(int key, float timeMin, float timeMax) const
	{
		return 0;
	}

	// Look for a secondary selection point in the selected keys, in the time range specified.
	//
	// Returns the key index as well as an id that can be passed into the other secondary selection functions.
	// Returns 0 when it could not find a secondary selection point.
	virtual int FindSecondarySelectionPt(int& key, float timeMin, float timeMax) const
	{
		return 0;
	}

	virtual void        SetSecondaryTime(int key, int id, float time)    {}
	virtual float       GetSecondaryTime(int key, int id) const          { return 0.0f; }
	virtual CString     GetSecondaryDescription(int key, int id) const   { return ""; }
	virtual bool        CanMoveSecondarySelection(int key, int id) const { return true; }

	virtual void        InsertKeyMenuOptions(CMenu& menu, int keyID)     {}
	virtual void        ClearKeyMenuOptions(CMenu& menu, int keyID)      {}
	virtual void        OnKeyMenuOption(int menuOption, int keyID)       {}

	virtual ColorB      GetColor() const                                 { return ColorB(220, 220, 220); }

	ESequencerParamType GetParameterType() const                         { return m_nParamType; }
	void                SetParameterType(ESequencerParamType type)       { m_nParamType = type; }

	void                UpdateKeys()                                     { MakeValid(); }

	int                 NextKeyByTime(int key) const
	{
		if (key + 1 < GetNumKeys())
			return key + 1;
		else
			return -1;
	}

	int FindKey(float time) const
	{
		const int keyCount = GetNumKeys();
		for (int i = 0; i < keyCount; i++)
		{
			const CSequencerKey* pKey = GetKey(i);
			assert(pKey);
			if (pKey->m_time == time)
				return i;
		}
		return -1;
	}

	virtual void SetKeyTime(int index, float time)
	{
		CSequencerKey* key = GetKey(index);
		assert(key);
		if (CanMoveKey(index))
		{
			key->m_time = time;
			Invalidate();
		}
	}

	float GetKeyTime(int index) const
	{
		const CSequencerKey* key = GetKey(index);
		assert(key);
		return key->m_time;
	}

	void SetKeyFlags(int index, int flags)
	{
		CSequencerKey* key = GetKey(index);
		assert(key);
		key->flags = flags;
		Invalidate();
	}

	int GetKeyFlags(int index) const
	{
		const CSequencerKey* key = GetKey(index);
		assert(key);
		return key->flags;
	}

	virtual void SelectKey(int index, bool select)
	{
		CSequencerKey* key = GetKey(index);
		assert(key);
		if (select)
			key->flags |= AKEY_SELECTED;
		else
			key->flags &= ~AKEY_SELECTED;
	}

	bool IsKeySelected(int index) const
	{
		const CSequencerKey* key = GetKey(index);
		assert(key);
		const bool isKeySelected = (key->flags & AKEY_SELECTED);
		return isKeySelected;
	}

	void SortKeys()
	{
		DoSortKeys();
		m_bModified = false;
	}

	void SetFlags(int flags) { m_flags = flags; }
	int  GetFlags()          { return m_flags; }

	void SetSelected(bool bSelect)
	{
		if (bSelect)
			m_flags |= SEQUENCER_TRACK_SELECTED;
		else
			m_flags &= ~SEQUENCER_TRACK_SELECTED;
	}

	void         SetTimeRange(const Range& timeRange) { m_timeRange = timeRange; }
	const Range& GetTimeRange() const                 { return m_timeRange; }

	void         OnChange()
	{
		m_changeCount++;
		OnChangeCallback();
	}
	uint32       GetChangeCount() const { return m_changeCount; }
	void         ResetChangeCount()     { m_changeCount = 0; }

	virtual bool Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks = true) = 0;
	virtual bool SerializeSelection(XmlNodeRef& xmlNode, bool bLoading, bool bCopySelected = false, float fTimeOffset = 0) = 0;

	void         Mute(bool bMute) { m_muted = bMute; }
	bool         IsMuted() const  { return m_muted; }

protected:
	virtual void OnChangeCallback() {}
	virtual void DoSortKeys() = 0;

	void         MakeValid()
	{
		if (m_bModified)
			SortKeys();
		assert(!m_bModified);
	}

	void Invalidate() { m_bModified = true; }

private:
	ESequencerParamType m_nParamType;
	bool                m_bModified;
	Range               m_timeRange;
	int                 m_flags;
	uint32              m_changeCount;
	bool                m_muted;
};

#endif // __ISequencerSystem_h__

