// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MannequinNodes_h__
#define __MannequinNodes_h__

//#include "Controls\MltiTree.h"
#include "Controls\TreeCtrlReport.h"
#include "ISequencerSystem.h"

#if _MSC_VER > 1000
	#pragma once
#endif

// forward declarations.
class CSequencerNode;
class CSequencerTrack;
class CSequencerSequence;
class CSequencerDopeSheetBase;

//////////////////////////////////////////////////////////////////////////
class CMannNodesCtrl : public CTreeCtrlReport
{
public:
	typedef std::vector<CSequencerNode*> AnimNodes;

	enum EColumn
	{
		eCOLUMN_NODE = 0,
		eCOLUMN_MUTE
	};

	static const int MUTE_TRACK_ICON_UNMUTED = 18;
	static const int MUTE_TRACK_ICON_MUTED = 19;
	static const int MUTE_TRACK_ICON_SOLO = 20;
	static const int MUTE_NODE_ICON_UNMUTED = 21;
	static const int MUTE_NODE_ICON_MUTED = 22;
	static const int MUTE_NODE_ICON_SOLO = 23;

	class CRecord : public CTreeItemRecord
	{
	public:
		CRecord(bool bIsGroup = true, const CString& name = "") : m_name(name), m_bIsGroup(bIsGroup) {}
		bool           IsGroup() const { return m_bIsGroup; }
		const CString& GetName() const { return m_name; }

	public:
		CString                     m_name;
		bool                        m_bIsGroup;
		int                         paramId;
		_smart_ptr<CSequencerNode>  node;
		_smart_ptr<CSequencerTrack> track;
	};
	typedef CRecord SItemInfo;

	void     SetSequence(CSequencerSequence* seq);
	void     SetKeyListCtrl(CSequencerDopeSheetBase* keysCtrl);
	void     SyncKeyCtrl();
	void     ExpandNode(CSequencerNode* node);
	bool     IsNodeExpanded(CSequencerNode* node);
	void     SelectNode(const char* sName);
	CRecord* GetSelectedNode();

	bool     GetSelectedNodes(AnimNodes& nodes);

	void     SetEditLock(bool bLock) { m_bEditLock = bLock; }

	float    SaveVerticalScrollPos() const;
	void     RestoreVerticalScrollPos(float fScrollPos);

public:
	CMannNodesCtrl();
	virtual ~CMannNodesCtrl();

	//////////////////////////////////////////////////////////////////////////
	// Callbacks.
	//////////////////////////////////////////////////////////////////////////
	virtual void Reload();
	virtual void OnFillItems();
	virtual void OnItemExpanded(CXTPReportRow* pRow, bool bExpanded);
	virtual void OnSelectionChanged();
	virtual void OnVerticalScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	virtual bool OnBeginDragAndDrop(CXTPReportSelectedRows* pRow, CPoint point);
	virtual void OnDragAndDrop(CXTPReportSelectedRows* pRow, CPoint absoluteCursorPos);
	virtual void OnItemDblClick(CXTPReportRow* pRow);
	//////////////////////////////////////////////////////////////////////////

protected:

	DECLARE_MESSAGE_MAP()
	afx_msg void OnNMLclick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMRclick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	void         ShowHideTrack(CSequencerNode* node, int trackIndex);
	void         RemoveTrack(SItemInfo* pItemInfo);
	void         AddTrack(int paramIndex, CSequencerNode* node);
	bool         CanPasteTrack(CSequencerNode* node);
	void         PasteTrack(CSequencerNode* node);

	void         MuteNodesRecursive(SItemInfo* pItemInfo, bool bMute);
	void         ToggleMute(SItemInfo* pItemInfo);
	void         MuteAllNodes();
	void         UnmuteAllNodes();
	void         RefreshTracks();

	void         SoloNodesRecursive(SItemInfo* pItemInfo, bool bSoloActive, bool bSoloTarget);
	void         MuteAllBut(SItemInfo* pItemInfo);

	void         GenerateMuteMasks(SItemInfo* pItem, uint32& mutedAnimLayerMask, uint32& mutedProcLayerMask);

	int          ShowPopupMenu(CPoint point, const SItemInfo* pItemInfo, uint32 column);
	int          ShowPopupMenuNode(CPoint point, const SItemInfo* pItemInfo);
	int          ShowPopupMenuMute(CPoint point, const SItemInfo* pItemInfo);

	void         SetPopupMenuLock(CMenu* menu);

	void         RecordSequenceUndo();
	void         InvalidateNodes();

	int          GetIconIndexForParam(ESequencerParamType nParamId) const;
	int          GetIconIndexForNode(ESequencerNodeType type) const;
	int          GetIconIndexForMute(SItemInfo* pItem) const;

	void         FillNodes(CRecord* pRecord);
	void         FillTracks(CRecord* pNodeRecord, CSequencerNode* pNode);

	bool         HasNode(const char* name) const;

	//! Set layer node animator for animation control with layer data in the editor.
	void SetLayerNodeAnimators();

protected:

	friend class CMannNodesCtrlDropTarget;
	SItemInfo* IsPointValidForAnimationInContextDrop(const CPoint& point, COleDataObject* pDataObject) const;
	bool       CreatePointForAnimationInContextDrop(SItemInfo* pItemInfo, const CPoint& point, COleDataObject* pDataObject);

	CSequencerSequence*      m_sequence;
	CSequencerDopeSheetBase* m_keysCtrl;

	// Must not be vector, vector may invalidate pointers on add/remove.
	typedef std::vector<SItemInfo*> ItemInfos;
	ItemInfos m_itemInfos;

	bool      m_bEditLock;

	// Drag / Drop
	COleDropTarget* m_pDropTarget;
};

#endif // __MannequinNodes_h__

