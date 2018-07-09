// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once

#include <CryMovie/IMovieSystem.h>
#include "TrackViewNode.h"

struct IAnimKeyContainer;
class CTrackViewAnimNode;

// Represents a bundle of tracks
class CTrackViewTrackBundle
{
public:
	CTrackViewTrackBundle() : m_bAllOfSameType(true), m_bHasRotationTrack(false) {}

	uint                   GetCount() const                 { return m_tracks.size(); }
	CTrackViewTrack*       GetTrack(const uint index)       { return m_tracks[index]; }
	const CTrackViewTrack* GetTrack(const uint index) const { return m_tracks[index]; }

	void                   AppendTrack(CTrackViewTrack* pTrack);
	void                   AppendTrackBundle(const CTrackViewTrackBundle& bundle);

	bool                   IsOneTrack() const;
	bool                   AreAllOfSameType() const { return m_bAllOfSameType; }
	bool                   HasRotationTrack() const { return m_bHasRotationTrack; }

private:
	bool                          m_bAllOfSameType;
	bool                          m_bHasRotationTrack;
	std::vector<CTrackViewTrack*> m_tracks;
};

// Track Memento for Undo/Redo
class CTrackViewTrackMemento
{
private:
	friend class CTrackViewTrack;
	XmlNodeRef m_serializedTrackState;
};

////////////////////////////////////////////////////////////////////////////
//
// This class represents a IAnimTrack in TrackView and contains
// the editor side code for changing it
//
// It does *not* have ownership of the IAnimTrack, therefore deleting it
// will not destroy the CryMovie track
//
////////////////////////////////////////////////////////////////////////////
class CTrackViewTrack : public CTrackViewNode, public ITrackViewKeyBundle
{
	friend class CTrackViewKeyHandle;
	friend class CTrackViewKeyConstHandle;
	friend class CTrackViewKeyBundle;
	friend class CAbstractUndoTrackTransaction;

public:
	CTrackViewTrack(IAnimTrack* pTrack, CTrackViewAnimNode* pTrackAnimNode, CTrackViewNode* pParentNode,
	                bool bIsSubTrack = false, uint subTrackIndex = 0);

	CTrackViewAnimNode* GetAnimNode() const;

	// Name getter
	virtual const char* GetName() const;

	// CTrackViewNode
	virtual ETrackViewNodeType GetNodeType() const override { return eTVNT_Track; }

	// Check for compound/sub track
	bool IsCompoundTrack() const { return m_bIsCompoundTrack; }

	// Sub track index
	bool IsSubTrack() const       { return m_bIsSubTrack; }
	uint GetSubTrackIndex() const { return m_subTrackIndex; }

	// Snap time value to prev/next key in track
	virtual bool SnapTimeToPrevKey(SAnimTime& time) const override;
	virtual bool SnapTimeToNextKey(SAnimTime& time) const override;

	// Key getters
	virtual const char*              GetKeyType() const           { return m_pAnimTrack->GetKeyType(); }
	virtual uint                     GetKeyCount() const override { return m_pAnimTrack->GetNumKeys(); }
	virtual CTrackViewKeyHandle      GetKey(uint index) override;
	virtual CTrackViewKeyConstHandle GetKey(uint index) const;

	virtual CTrackViewKeyHandle      GetKeyByTime(const SAnimTime time);

	virtual CTrackViewKeyBundle      GetSelectedKeys() override;
	virtual CTrackViewKeyBundle      GetAllKeys() override;
	virtual CTrackViewKeyBundle      GetKeysInTimeRange(const SAnimTime t0, const SAnimTime t1) override;

	// Key modifications
	void                        ClearKeys();
	virtual CTrackViewKeyHandle CreateKey(const SAnimTime time);

	// Value getters
	TMovieSystemValue GetValue(const SAnimTime time) const            { return m_pAnimTrack->GetValue(time); }
	TMovieSystemValue GetDefaultValue() const                         { return m_pAnimTrack->GetDefaultValue(); }
	virtual void      SetValue(const SAnimTime time, const TMovieSystemValue& value);
	void              SetDefaultValue(const TMovieSystemValue& value) { return m_pAnimTrack->SetDefaultValue(value); }

	void              GetKeyValueRange(float& min, float& max) const;

	// Type getters
	CAnimParamType GetParameterType() const { return m_pAnimTrack->GetParameterType(); }
	EAnimValue     GetValueType() const     { return m_pAnimTrack->GetValueType(); }

	// Mask
	bool IsMasked(uint32 mask) const { return m_pAnimTrack->IsMasked(mask); }

	// Flag getter
	IAnimTrack::EAnimTrackFlags GetFlags() const;

	// Color
	ColorB GetCustomColor() const;
	void   SetCustomColor(ColorB color);
	bool   HasCustomColor() const;
	void   ClearCustomColor();

	// Memento
	virtual CTrackViewTrackMemento GetMemento() const;
	virtual void                   RestoreFromMemento(const CTrackViewTrackMemento& memento);

	// Disabled state
	virtual void SetDisabled(bool bDisabled) override;
	virtual bool IsDisabled() const override;

	// Muted state
	void SetMuted(bool bMuted);
	bool IsMuted() const;

	// Key selection
	virtual void SelectKeys(const bool bSelected) override;

	// Copy selected keys to XML representation for clipboard
	void CopyKeysToClipboard(XmlNodeRef& xmlNode, const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks);

	// Paste from XML representation with time offset
	void PasteKeys(XmlNodeRef xmlNode, const SAnimTime time);

	// Key types
	virtual bool AreAllKeysOfSameType() const override { return true; }

	// Animation layer index
	void SetAnimationLayerIndex(const int index);
	int  GetAnimationLayerIndex() const;

	// Offset keys
	virtual void OffsetKeys(const TMovieSystemValue& offset);

	// Gets the wrapped key
	virtual _smart_ptr<IAnimKeyWrapper> GetWrappedKey(int key) { return m_pAnimTrack->GetWrappedKey(key); }

	// Returns true if the track has keys with duration
	virtual bool    KeysHaveDuration() const;

	virtual CryGUID GetGUID() const override { return m_pAnimTrack->GetGUID(); }

	virtual void Serialize(Serialization::IArchive& ar) override;

protected:
	// Those are called from CTrackViewKeyHandle
	virtual void SetKey(uint keyIndex, const STrackKey* pKey);
	virtual void GetKey(uint keyIndex, STrackKey* pKey) const;

private:
	using SAnimTimeVector       = std::vector<SAnimTime>;
	using SerializationCallback = std::function<void(SAnimTimeVector&)>;

	CTrackViewTrack*    GetSubTrack(uint index);

	CTrackViewKeyHandle GetPrevKey(const SAnimTime time);
	CTrackViewKeyHandle GetNextKey(const SAnimTime time);

	void                SelectKey(uint keyIndex, bool bSelect);
	bool                IsKeySelected(uint keyIndex) const;

	SAnimTime           GetKeyTime(const uint index) const;
	virtual string      GetKeyDescription(const uint index) const;

	virtual void        SetKeyDuration(const uint index, SAnimTime duration) {}
	virtual SAnimTime   GetKeyDuration(const uint index) const;
	virtual SAnimTime   GetKeyAnimDuration(const uint index) const           { return SAnimTime(0); }
	virtual SAnimTime   GetKeyAnimStart(const uint index) const              { return SAnimTime(0); }
	virtual SAnimTime   GetKeyAnimEnd(const uint index) const                { return SAnimTime(0); }
	virtual bool        IsKeyAnimLoopable(const uint index) const            { return false; }

	void                RemoveKey(const int index);
	void                GetSelectedKeysTimes(std::vector<SAnimTime>& selectedKeysTimes);
	void                SelectKeysByAnimTimes(const std::vector<SAnimTime>& selectedKeys);
	void                SerializeSelectedKeys(XmlNodeRef& xmlNode, bool bLoading, SerializationCallback callback = [](SAnimTimeVector&) {}, const SAnimTime time = SAnimTime(0), size_t index = 0,	SAnimTimeVector keysTimes = SAnimTimeVector());
	
	CTrackViewKeyBundle GetKeys(bool bOnlySelected, SAnimTime t0, SAnimTime t1);
	CTrackViewKeyHandle GetSubTrackKeyHandle(uint index) const;

	bool                   m_bIsCompoundTrack;
	bool                   m_bIsSubTrack;
	uint                   m_subTrackIndex;
	_smart_ptr<IAnimTrack> m_pAnimTrack;
	CTrackViewAnimNode*    m_pTrackAnimNode;
	std::vector<bool>      m_keySelectionStates;
};

