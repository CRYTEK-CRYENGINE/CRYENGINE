// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once

#include "TrackViewNode.h"
#include "TrackViewTrack.h"
#include "Objects/TrackGizmo.h"

class CTrackViewAnimNode;
class CEntityObject;

// Represents a bundle of anim nodes
class CTrackViewAnimNodeBundle
{
public:
	unsigned int              GetCount() const                        { return m_animNodes.size(); }
	CTrackViewAnimNode*       GetNode(const unsigned int index)       { return m_animNodes[index]; }
	const CTrackViewAnimNode* GetNode(const unsigned int index) const { return m_animNodes[index]; }

	void                      Clear();
	const bool                DoesContain(const CTrackViewNode* pTargetNode);

	void                      AppendAnimNode(CTrackViewAnimNode* pNode);
	void                      AppendAnimNodeBundle(const CTrackViewAnimNodeBundle& bundle);

private:
	std::vector<CTrackViewAnimNode*> m_animNodes;
};

// Callback called by animation node when its animated.
class IAnimNodeAnimator
{
public:
	virtual ~IAnimNodeAnimator() {}

	virtual void Animate(CTrackViewAnimNode* pNode, const SAnimContext& ac) = 0;
	virtual void Render(CTrackViewAnimNode* pNode, const SAnimContext& ac) {}

	// Called when binding/unbinding the owning node
	virtual void Bind(CTrackViewAnimNode* pNode)   {}
	virtual void UnBind(CTrackViewAnimNode* pNode) {}
};

////////////////////////////////////////////////////////////////////////////
//
// This class represents a IAnimNode in TrackView and contains
// the editor side code for changing it
//
// It does *not* have ownership of the IAnimNode, therefore deleting it
// will not destroy the CryMovie track
//
////////////////////////////////////////////////////////////////////////////
class CTrackViewAnimNode : public CTrackViewNode, public IAnimNodeOwner
{
	friend class CAbstractUndoAnimNodeTransaction;
	friend class CAbstractUndoTrackTransaction;
	friend class CUndoAnimNodeReparent;

public:
	CTrackViewAnimNode(IAnimSequence* pSequence, IAnimNode* pAnimNode, CTrackViewNode* pParentNode);

	// Rendering
	virtual void Render(const SAnimContext& animContext);

	// Playback
	virtual void Animate(const SAnimContext& animContext);

	// Binding/Unbinding
	virtual void BindToEditorObjects();
	virtual void UnBindFromEditorObjects();
	virtual bool IsBoundToEditorObjects() const;

	// Node owner getter
	virtual CEntityObject* GetNodeEntity(const bool bSearch = true) { return nullptr; }

	// Console sync
	virtual void SyncToConsole(const SAnimContext& animContext);

	// CTrackViewAnimNode
	virtual ETrackViewNodeType GetNodeType() const override { return eTVNT_AnimNode; }

	// Create & remove sub anim nodes
	virtual CTrackViewAnimNode* CreateSubNode(const string& name, const EAnimNodeType animNodeType, CEntityObject* pEntity = nullptr);
	virtual void                RemoveSubNode(CTrackViewAnimNode* pSubNode);

	// Create & remove sub tracks
	virtual CTrackViewTrack* CreateTrack(const CAnimParamType& paramType);
	virtual void             RemoveTrack(CTrackViewTrack* pTrack);

	// Add selected entities from scene to group node
	virtual CTrackViewAnimNodeBundle AddSelectedEntities();

	// Add current layer to group node
	virtual void AddCurrentLayer();

	// Director related
	virtual void SetAsActiveDirector();
	virtual bool IsActiveDirector() const;

	// Checks if anim node is part of active sequence and of an active director
	virtual bool IsActive();

	// Name setter/getter
	virtual const char* GetName() const override { return m_pAnimNode->GetName(); }
	virtual bool        SetName(const char* pName) override;
	virtual bool        CanBeRenamed() const override;

	// Snap time value to prev/next key in sequence
	virtual bool SnapTimeToPrevKey(SAnimTime& time) const override;
	virtual bool SnapTimeToNextKey(SAnimTime& time) const override;

	// Node getters
	CTrackViewAnimNodeBundle GetAllAnimNodes();
	CTrackViewAnimNodeBundle GetSelectedAnimNodes();
	CTrackViewAnimNodeBundle GetAllOwnedNodes(const CEntityObject* pOwner);
	CTrackViewAnimNodeBundle GetAnimNodesByType(EAnimNodeType animNodeType);
	CTrackViewAnimNodeBundle GetAnimNodesByName(const char* pName);

	// Track getters
	virtual CTrackViewTrackBundle GetAllTracks();
	virtual CTrackViewTrackBundle GetSelectedTracks();
	virtual CTrackViewTrackBundle GetTracksByParam(const CAnimParamType& paramType);

	// Key getters
	virtual CTrackViewKeyBundle GetAllKeys() override;
	virtual CTrackViewKeyBundle GetSelectedKeys() override;
	virtual CTrackViewKeyBundle GetKeysInTimeRange(const SAnimTime t0, const SAnimTime t1) override;

	// Type getters
	EAnimNodeType GetType() const;

	// Flags
	EAnimNodeFlags GetFlags() const;

	// Disabled state
	virtual void SetDisabled(bool bDisabled) override;
	virtual bool IsDisabled() const override;

	// Return track assigned to the specified parameter.
	CTrackViewTrack* GetTrackForParameter(const CAnimParamType& paramType, uint32 index = 0) const;

	// Gets a list of addable node types
	std::vector<EAnimNodeType> GetAddableNodeTypes() const;

	// Param
	void                            RefreshDynamicParams();
	unsigned int                    GetParamCount() const;
	CAnimParamType                  GetParamType(unsigned int index) const;
	char*                           GetParamName(const CAnimParamType& paramType) const;
	bool                            IsParamValid(const CAnimParamType& param) const;
	IAnimNode::ESupportedParamFlags GetParamFlags(const CAnimParamType& paramType) const;
	EAnimValue                      GetParamValueType(const CAnimParamType& paramType) const;
	void                            UpdateDynamicParams();
	std::vector<CAnimParamType>     GetAddableParams() const;

	// Check if it's a group node
	virtual bool IsGroupNode() const override;

	// Generate a new node name
	virtual string GetAvailableNodeNameStartingWith(const string& name) const;

	// Copy keys to clipboard (in XML form)
	virtual void CopyKeysToClipboard(const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks);

	// Copy/Paste nodes
	virtual void CopyNodesToClipboard(const bool bOnlySelected);
	virtual bool PasteNodesFromClipboard();

	// Set new parent
	virtual void SetNewParent(CTrackViewAnimNode* pNewParent);

	// Check if this node may be moved to new parent
	virtual bool IsValidReparentingTo(CTrackViewAnimNode* pNewParent);

	// Sync from/to base transform. Returns false if nothing was synced.
	virtual bool    SyncToBase()             { return false; };
	virtual bool    SyncFromBase()           { return false; };

	virtual CryGUID GetGUID() const override { return m_pAnimNode->GetGUID(); }

	// Only for displaying sequence properties at the moment
	virtual void Serialize(Serialization::IArchive& ar) override;

protected:
	// IAnimNodeOwner
	virtual void OnNodeAnimated(IAnimNode* pNode) override {}
	// ~IAnimNodeOwner

	IAnimNode* GetAnimNode() { return m_pAnimNode; }

	bool       CheckTrackAnimated(const CAnimParamType& paramType) const;

private:
	virtual void Bind()                                       {}
	virtual void UnBind()                                     {}
	virtual void ConsoleSync(const SAnimContext& animContext) {}
	virtual bool IsNodeOwner(const CEntityObject* pOwner)     { return false; }

	// Copy selected keys to XML representation for clipboard
	virtual void          CopyKeysToClipboard(XmlNodeRef& xmlNode, const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks) override;

	void                  CopyNodesToClipboardRec(CTrackViewAnimNode* pCurrentAnimNode, XmlNodeRef& xmlNode, const bool bOnlySelected);

	CTrackViewTrackBundle GetTracks(const bool bOnlySelected, const CAnimParamType& paramType);

	void                  PasteNodeFromClipboard(XmlNodeRef xmlNode);

	// IAnimNodeOwner
	virtual void OnNodeVisibilityChanged(IAnimNode* pNode, const bool bHidden) override {}
	virtual void OnNodeReset(IAnimNode* pNode) override                                 {};
	// ~IAnimNodeOwner

	IAnimSequence*                     m_pAnimSequence;
	_smart_ptr<IAnimNode>              m_pAnimNode;
	std::unique_ptr<IAnimNodeAnimator> m_pNodeAnimator;
};

