// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once
#include <CryMovie/AnimKey.h>

class CTrackViewTrack;
class CTrackViewSequence;

class CTrackViewKeyConstHandle
{
public:
	CTrackViewKeyConstHandle() : m_bIsValid(false), m_keyIndex(0), m_pTrack(nullptr) {}

	CTrackViewKeyConstHandle(const CTrackViewTrack* pTrack, unsigned int keyIndex)
		: m_bIsValid(true), m_keyIndex(keyIndex), m_pTrack(pTrack) {}

	void                   GetKey(STrackKey* pKey) const;
	SAnimTime              GetTime() const;
	const CTrackViewTrack* GetTrack() const { return m_pTrack; }

	SAnimTime              GetDuration() const;
	SAnimTime              GetAnimDuration() const;
	SAnimTime              GetAnimStart() const;
	bool                   IsAnimLoopable() const;

private:
	bool                   m_bIsValid;
	unsigned int           m_keyIndex;
	const CTrackViewTrack* m_pTrack;
};

// Represents one CryMovie key
class CTrackViewKeyHandle
{
public:
	CTrackViewKeyHandle() : m_bIsValid(false), m_keyIndex(0), m_pTrack(nullptr) {}

	CTrackViewKeyHandle(CTrackViewTrack* pTrack, unsigned int keyIndex)
		: m_bIsValid(true), m_keyIndex(keyIndex), m_pTrack(pTrack) {}

	void                        SetKey(const STrackKey* pKey);
	void                        GetKey(STrackKey* pKey) const;

	CTrackViewTrack*            GetTrack()       { return m_pTrack; }
	const CTrackViewTrack*      GetTrack() const { return m_pTrack; }

	bool                        IsValid() const  { return m_bIsValid; }
	unsigned int                GetIndex() const { return m_keyIndex; }

	void                        Select(bool bSelect);
	bool                        IsSelected() const;

	SAnimTime                   GetTime() const;

	void                        SetDuration(SAnimTime duration);
	SAnimTime                   GetDuration() const;
	SAnimTime                   GetCycleDuration() const;
	SAnimTime                   GetAnimStart() const;
	SAnimTime                   GetAnimEnd() const;
	bool                        IsAnimLoopable() const;

	string                      GetDescription() const;

	_smart_ptr<IAnimKeyWrapper> GetWrappedKey();

	bool                        operator==(const CTrackViewKeyHandle& keyHandle) const;
	bool                        operator!=(const CTrackViewKeyHandle& keyHandle) const;

	// Deletes key. Note that handle will be invalid afterwards
	void Delete();

private:
	bool             m_bIsValid;
	uint             m_keyIndex;
	CTrackViewTrack* m_pTrack;
};

// Abstract base class that defines common
// operations for key bundles and tracks
class ITrackViewKeyBundle
{
public:
	virtual bool                AreAllKeysOfSameType() const = 0;

	virtual unsigned int        GetKeyCount() const = 0;
	virtual CTrackViewKeyHandle GetKey(unsigned int index) = 0;

	virtual void                SelectKeys(const bool bSelected) = 0;
};

// Represents a bundle of keys
class CTrackViewKeyBundle : public ITrackViewKeyBundle
{
	friend class CTrackViewTrack;
	friend class CTrackViewAnimNode;

public:
	CTrackViewKeyBundle() : m_bAllOfSameType(true) {}

	virtual bool                AreAllKeysOfSameType() const override { return m_bAllOfSameType; }

	virtual unsigned int        GetKeyCount() const override          { return m_keys.size(); }
	virtual CTrackViewKeyHandle GetKey(unsigned int index) override   { return m_keys[index]; }

	virtual void                SelectKeys(const bool bSelected) override;

	CTrackViewKeyHandle         GetSingleSelectedKey();

private:
	void AppendKey(const CTrackViewKeyHandle& keyHandle);
	void AppendKeyBundle(const CTrackViewKeyBundle& bundle);

	bool                             m_bAllOfSameType;
	std::vector<CTrackViewKeyHandle> m_keys;
};

// Types of nodes that derive from CTrackViewNode
enum ETrackViewNodeType
{
	eTVNT_Sequence,
	eTVNT_AnimNode,
	eTVNT_Track
};

////////////////////////////////////////////////////////////////////////////
//
// This is the base class for all sequences, nodes and tracks in TrackView,
// which provides a interface for common operations
//
////////////////////////////////////////////////////////////////////////////
class CTrackViewNode
{
	friend class CAbstractUndoTrackTransaction;

public:
	CTrackViewNode(CTrackViewNode* pParent);
	virtual ~CTrackViewNode() {}

	// Name
	virtual const char* GetName() const = 0;
	virtual bool        SetName(const char* pName) { return false; };
	virtual bool        CanBeRenamed() const       { return false; }

	// CryMovie node type
	virtual ETrackViewNodeType GetNodeType() const = 0;

	// Get the sequence of this node
	CTrackViewSequence*       GetSequence();
	const CTrackViewSequence* GetSequence() const;

	// Get parent
	CTrackViewNode* GetParentNode() const { return m_pParentNode; }

	// Children
	unsigned int    GetChildCount() const              { return m_childNodes.size(); }
	CTrackViewNode* GetChild(unsigned int index) const { return m_childNodes[index].get(); }

	// Snap time value to prev/next key in sequence
	virtual bool SnapTimeToPrevKey(SAnimTime& time) const = 0;
	virtual bool SnapTimeToNextKey(SAnimTime& time) const = 0;

	// Selection state
	virtual void SetSelected(bool bSelected);
	virtual bool IsSelected() const { return m_bSelected; }

	// Clear selection of this node and all sub nodes
	void ClearSelection();

	// Disabled state
	virtual void SetDisabled(bool bDisabled) {}
	virtual bool IsDisabled() const          { return false; }

	// Key getters
	virtual CTrackViewKeyBundle GetSelectedKeys() = 0;
	virtual CTrackViewKeyBundle GetAllKeys() = 0;
	virtual CTrackViewKeyBundle GetKeysInTimeRange(const SAnimTime t0, const SAnimTime t1) = 0;

	// Check if it's a group node
	virtual bool IsGroupNode() const { return false; }

	// Copy selected keys to XML representation for clipboard
	virtual void CopyKeysToClipboard(XmlNodeRef& xmlNode, const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks) = 0;

	// Sorting
	bool operator<(const CTrackViewNode& pOtherNode) const;

	// Get first selected node in tree
	CTrackViewNode* GetFirstSelectedNode();

	// Get director of this node
	CTrackViewAnimNode* GetDirector();

	// Used to uniquely identify this node at runtime. Not serialized.
	// Nodes can be retrieved using CTrackViewSequence::GetNodeFromUID
	virtual CryGUID GetGUID() const = 0;

	// Called when node is added/removed from sequence
	virtual void OnAdded()   {}
	virtual void OnRemoved() {};

	// Only for displaying sequence properties at the moment
	virtual void Serialize(Serialization::IArchive& ar);

protected:
	void AddNode(CTrackViewNode* pNode);
	void SortNodes();

	CTrackViewNode*                              m_pParentNode;
	std::vector<std::unique_ptr<CTrackViewNode>> m_childNodes;

	bool m_bSelected;
};

