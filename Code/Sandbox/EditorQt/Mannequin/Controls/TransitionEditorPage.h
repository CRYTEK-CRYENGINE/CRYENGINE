// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANN_TRANSITION_EDITOR_PAGE__H__
#define __MANN_TRANSITION_EDITOR_PAGE__H__

#include <ICryMannequin.h>

#include "MannequinEditorPage.h"
#include "../MannequinBase.h"
#include "../MannequinNodes.h"
#include "Controls/PropertiesPanel.h"
#include "../SequencerSplitter.h"
#include "Dialogs/ToolbarDialog.h"
#include "../MannKeyPropertiesDlgFE.h"
#include "../SequencerDopeSheetToolbar.h"
#include "QMfcApp/qmfcviewporthost.h"

#include "MannDopeSheet.h"

struct SMannequinContexts;
class CMannequinModelViewport;
class CSequencerSequence;
class CFragmentSequencePlayback;

class CTransitionEditorPage : public CMannequinEditorPage
{
	DECLARE_DYNAMIC(CTransitionEditorPage)

public:
	CTransitionEditorPage(CWnd* pParent = NULL);
	~CTransitionEditorPage();

	enum { IDD = IDD_MANN_TRANSITION_EDITOR_PAGE };

	virtual CMannequinModelViewport* ModelViewport() const   { return m_modelViewport; }
	virtual CMannDopeSheet*          TrackPanel()            { return &m_wndTrackPanel; }
	virtual CMannNodesCtrl*          Nodes()                 { return &m_wndNodes; }
	CMannKeyPropertiesDlgFE*         KeyProperties()         { return &m_wndKeyProperties; }

	CSequencerSequence*              Sequence() const        { return m_sequence.get(); }
	SFragmentHistoryContext*         FragmentHistory() const { return m_fragmentHistory.get(); }

	void                             SaveLayout(const XmlNodeRef& xmlLayout);
	void                             LoadLayout(const XmlNodeRef& xmlLayout);

	virtual void                     ValidateToolbarButtonsState();
	void                             Update();
	void                             PopulateAllClipTracks();
	void                             InsertContextHistoryItems(const SScopeData& scopeData, const float keyTime);
	void                             SetUIFromHistory();
	void                             SetHistoryFromUI();
	void                             OnUpdateTV(bool forceUpdate = false);
	void                             OnSequenceRestart(float newTime);
	void                             InitialiseToPreviewFile(const XmlNodeRef& xmlSequenceRoot);
	void                             SetTagState(const TagState& tagState);

	void                             SetTime(float fTime);

	float                            GetMarkerTimeStart() const;
	float                            GetMarkerTimeEnd() const;

	void                             LoadSequence(const int scopeContextIdx, const FragmentID fromID, const FragmentID toID, const SFragTagState fromFragTag, const SFragTagState toFragTag, const SFragmentBlendUid blendUid);
	void                             ClearSequence();

protected:
	virtual BOOL OnInitDialog();

	afx_msg void OnGoToStart();
	afx_msg void OnGoToEnd();
	afx_msg void OnPlay();
	afx_msg void OnLoop();
	afx_msg void OnToggle1P();
	afx_msg void OnShowDebugOptions();
	afx_msg void OnToggleTimelineUnits();
	afx_msg void OnReloadAnimations();
	afx_msg void OnToggleLookat();
	afx_msg void OnClickRotateLocator();
	afx_msg void OnClickTranslateLocator();
	afx_msg void OnToggleShowSceneRoots();
	afx_msg void OnToggleAttachToEntity();

	afx_msg void OnToolbarDropDown(NMHDR* pnhdr, LRESULT* plr);

	afx_msg void OnUpdateGoToStart(CCmdUI* pCmdUI);
	afx_msg void OnUpdateGoToEnd(CCmdUI* pCmdUI);
	afx_msg void OnUpdatePlay(CCmdUI* pCmdUI);
	afx_msg void OnUpdateLoop(CCmdUI* pCmdUI);

	afx_msg void OnSetFocus(CWnd* pOldWnd);

	DECLARE_MESSAGE_MAP()

	void         OnPlayMenu(CPoint pos);
	float        PopulateClipTracks(CSequencerNode* node, const int scopeID);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

private:
	void          InitToolbar();

	void          PopulateTagList();
	void          SetTagStateOnCtrl(const SFragTagState& tagState);
	SFragTagState GetTagStateFromCtrl() const;
	void          OnInternalVariableChange(IVariable* pVar);
	void          ResetForcedBlend();
	void          SelectFirstKey();

	CMannDopeSheet                         m_wndTrackPanel;
	CMannNodesCtrl                         m_wndNodes;
	CPropertiesPanel                       m_tagsPanel;
	CMannKeyPropertiesDlgFE                m_wndKeyProperties;
	CSequencerSplitter                     m_wndSplitterTracks;
	CClampedSplitterWnd                    m_wndSplitterHorz;
	CClampedSplitterWnd                    m_wndSplitterVert;
	CSequencerDopeSheetToolbar             m_cDlgToolBar;
	CMannequinModelViewport*               m_modelViewport;
	QMfcContainer                          m_viewportHost;
	QMfcViewportHost*                      m_pViewportWidget;
	SMannequinContexts*                    m_contexts;
	CFragmentSequencePlayback*             m_sequencePlayback;

	std::auto_ptr<SFragmentHistoryContext> m_contextHistory;
	std::auto_ptr<SFragmentHistoryContext> m_fragmentHistory;
	_smart_ptr<CSequencerSequence>         m_sequence;

	float                m_fTime;
	float                m_fMaxTime;
	float                m_playSpeed;
	uint32               m_seed;
	bool                 m_bPlay;
	bool                 m_bLoop;
	bool                 m_draggingTime;
	bool                 m_bRestartingSequence;

	SLastChange          m_lastChange;
	TagState             m_tagState;
	bool                 m_bRefreshingTagCtrl;
	TSmartPtr<CVarBlock> m_tagVars;
	CTagControl          m_tagControls;

	SFragmentBlendUid    m_forcedBlendUid;

	FragmentID           m_fromID;
	FragmentID           m_toID;
	SFragTagState        m_fromFragTag;
	SFragTagState        m_toFragTag;
	ActionScopes         m_scopeMaskFrom;
	ActionScopes         m_scopeMaskTo;

	HACCEL               m_hAccelerators;
};

#endif

