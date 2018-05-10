// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANN_FRAGMENT_EDITOR_PAGE__H__
#define __MANN_FRAGMENT_EDITOR_PAGE__H__

#include <ICryMannequin.h>

#include "MannequinEditorPage.h"
#include "../MannequinBase.h"
#include "../MannequinNodes.h"
#include "../SequencerSplitter.h"
#include "Dialogs/ToolbarDialog.h"
#include "../MannKeyPropertiesDlgFE.h"
#include "../FragmentEditor.h"
#include "../SequencerDopeSheetToolbar.h"
#include "../Controls/ClampedSplitterWnd.h"
#include "QMfcApp/qmfcviewporthost.h"

struct SMannequinContexts;
class CMannequinModelViewport;
class CFragmentPlayback;
class CFragmentSequencePlayback;

class CFragmentEditorPage : public CMannequinEditorPage
{
	DECLARE_DYNAMIC(CFragmentEditorPage)

public:
	CFragmentEditorPage(CWnd* pParent = NULL);
	~CFragmentEditorPage();

	enum { IDD = IDD_MANN_FRAGMENT_EDITOR_PAGE };

	virtual CMannequinModelViewport* ModelViewport() const { return m_modelViewport; }
	virtual CMannFragmentEditor*     TrackPanel()          { return &m_wndTrackPanel; }
	virtual CMannNodesCtrl*          Nodes()               { return &m_wndNodes; }
	CMannKeyPropertiesDlgFE*         KeyProperties()       { return &m_wndKeyProperties; }

	void                             SaveLayout(const XmlNodeRef& xmlLayout);
	void                             LoadLayout(const XmlNodeRef& xmlLayout);

	virtual void                     ValidateToolbarButtonsState();
	void                             Update();
	void                             UpdateSelectedFragment();
	void                             CreateLocators();
	void                             CheckForLocatorChanges(const float lastTime, const float newTime);
	void                             OnSequenceRestart(float timePassed);
	void                             InitialiseToPreviewFile(const XmlNodeRef& xmlSequenceRoot);
	void                             Reset();

	void                             SetTimeToSelectedKey();
	void                             SetTime(float fTime);

	float                            GetMarkerTimeStart() const;
	float                            GetMarkerTimeEnd() const;

	void                             SetTagState(const FragmentID fragID, const SFragTagState& tagState);
	TagState                         GetGlobalTags() const { return m_globalTags; }

protected:
	virtual BOOL OnInitDialog();

	afx_msg void OnGoToStart();
	afx_msg void OnGoToEnd();
	afx_msg void OnPlay();
	afx_msg void OnLoop();
	afx_msg void OnToggle1P();
	afx_msg void OnShowDebugOptions();
	afx_msg void OnSetTimeToKey();
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
	afx_msg void OnUpdateSetTimeToKey(CCmdUI* pCmdUI);

	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnDestroy();

	DECLARE_MESSAGE_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	void         OnPlayMenu(CPoint pos);

private:
	void          InitToolbar();
	void          InstallAction(float time);
	void          PopulateTagList();
	void          SetTagStateOnCtrl(const SFragTagState& tagState);
	SFragTagState GetTagStateFromCtrl() const;
	void          OnInternalVariableChange(IVariable* pVar);

	CSequencerSplitter                       m_wndSplitterTracks;
	CClampedSplitterWnd                      m_wndSplitterVert;
	CClampedSplitterWnd                      m_wndSplitterHorz;
	CPropertiesPanel                         m_tagsPanel;
	CMannFragmentEditor                      m_wndTrackPanel;
	CMannNodesCtrl                           m_wndNodes;
	CMannKeyPropertiesDlgFE                  m_wndKeyProperties;
	CSequencerDopeSheetToolbar               m_cDlgToolBar;
    CMannequinModelViewport*                 m_modelViewport;

	QMfcContainer                            m_viewportHost;
	QMfcViewportHost*                        m_pViewportWidget;

	SMannequinContexts*                      m_contexts;

	float                      m_fTime;
	float                      m_playSpeed;
	bool                       m_bPlay;
	bool                       m_bLoop;
	bool                       m_draggingTime;
	bool                       m_bRestartingSequence;
	bool                       m_bRefreshingTagCtrl;
	bool                       m_bReadingTagCtrl;
	bool                       m_bTimeWasChanged;

	SLastChange                m_lastChange;
	TagState                   m_globalTags;
	TSmartPtr<CVarBlock>       m_tagVars;
	CTagControl                m_tagControls;

	CFragmentPlayback*         m_fragmentPlayback;
	CFragmentSequencePlayback* m_sequencePlayback;

	HACCEL                     m_hAccelerators;
};

#endif

