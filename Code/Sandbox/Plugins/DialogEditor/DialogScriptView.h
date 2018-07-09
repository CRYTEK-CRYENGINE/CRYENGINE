// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __DIALOGSCRIPTVIEW_H__
#define __DIALOGSCRIPTVIEW_H__

#pragma once

#include <CryAudio/IAudioSystem.h>

class CEditorDialogScript;
class CDialogScriptView;
class CDialogEditorDialog;

class CDialogColumn;
class MyCXTPReportInplaceList;

class CDialogScriptView : public CXTPReportControl
{
	DECLARE_DYNCREATE(CDialogScriptView)

public:
	CDialogScriptView();
	~CDialogScriptView();

public:
	CDialogColumn* m_pColLine;
	CDialogColumn* m_pColActor;
	CDialogColumn* m_pColSoundName;
	CDialogColumn* m_pColAnimStopWithSound;
	CDialogColumn* m_pColAnimUseEx;
	CDialogColumn* m_pColAnimType;
	CDialogColumn* m_pColAnimName;
	CDialogColumn* m_pColFacialExpression;
	CDialogColumn* m_pColFacialWeight;
	CDialogColumn* m_pColFacialFadeTime;
	CDialogColumn* m_pColLookAtSticky;
	CDialogColumn* m_pColLookAtTarget;
	CDialogColumn* m_pColDelay;
	CDialogColumn* m_pColDesc;

	void                     SetDialogEditor(CDialogEditorDialog* pDialogEditor);
	void                     SetScript(CEditorDialogScript* pScript);
	CEditorDialogScript*     GetScript() const  { return m_pScript; }
	bool                     IsModified() const { return m_bModified; }
	void                     SetModified(bool bModified);
	void                     SaveToScript();
	MyCXTPReportInplaceList* GetNewInplaceList() { return m_pNewInplaceList; }

	void                     PlayLine(int index);

	static void              OnAudioTriggerFinished(CryAudio::SRequestInfo const* const pAudioRequestInfo);
	//static void OnPlayingLineFinished() { ms_currentPlayLine = CryAudio::InvalidControlId; }

	//static void DialogTriggerFinishedCallback(TAudioObjectID const nObjectID, TAudioControlID const nTriggerID, void* const pCookie);
protected:
	class CPaintManager;

	void DrawDropTarget();
	void Reload();
	void SetupEditConstraints();
	void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics);
	void AddNewLine(bool bForceEnd);
	void UpdateNoItemText();
	void SetupHelp();
	void OnSelectionChanged();
	void StopSound();

	//{{AFX_VIRTUAL(CDragReportCtrl)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CDragReportCtrl)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd* pWnd);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint pos);
	afx_msg void OnValueChanged(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnRequestEdit(NMHDR* pNotifyStruct, LRESULT* result);
	afx_msg void OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* /*result*/);
	afx_msg void OnDestroy();

	afx_msg void OnAddLine();
	afx_msg void OnDelLine();
	afx_msg void OnUpdateCmdUI(CCmdUI* target);

	virtual BOOL PreTranslateMessage(MSG* pMsg);

	// virtual BOOL OnBeforeCopyToText(CXTPReportRecord* pRecord, CStringArray& rarStrings);
	// virtual BOOL OnBeforePasteFromText(CStringArray& arStrings, CXTPReportRecord** ppRecord);

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
protected:
	CEditorDialogScript*       m_pScript;
	CDialogEditorDialog*       m_pDialogEditor;
	CDialogColumn*             m_pFC;

	HCURSOR                    m_hCursorNoDrop;
	HCURSOR                    m_hCursorNormal;
	CXTPReportRow*             m_pSourceRow;
	CXTPReportRow*             m_pTargetRow;
	MyCXTPReportInplaceList*   m_pNewInplaceList;
	CPoint                     m_ptDrag;
	bool                       m_bDragging;
	bool                       m_bModified;

	static CryAudio::ControlId ms_currentPlayLine;
	CryAudio::IObject*         m_pIAudioObject;
};

class CDialogScriptViewView : public CXTPReportView
{
	DECLARE_DYNAMIC(CDialogScriptViewView)
public:
	CDialogScriptViewView();
	~CDialogScriptViewView();

	// Creates view window.
	BOOL               Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);

	CDialogScriptView* GetReport()
	{
		return static_cast<CDialogScriptView*>(m_pReport);
	}

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg void OnDestroy();
	afx_msg int  OnCreate(LPCREATESTRUCT lpcs);
	virtual void PostNcDestroy();

};

//===========================================================================
// Summary:
//     CXTPReportInplaceList is the CXTPReportInplaceControl derived  class,
//     it represents list box with constraints of item.
// See Also: CXTPReportRecordItemConstraints
//===========================================================================
class MyCXTPReportInplaceList : public CListBox, public CXTPReportInplaceControl
{
public:
	//-------------------------------------------------------------------------
	// Summary:
	//     Constructs a CXTPReportInplaceList object
	//-------------------------------------------------------------------------
	MyCXTPReportInplaceList();
	~MyCXTPReportInplaceList();

public:

	//-----------------------------------------------------------------------
	// Summary:
	//     Call this method to create in-place list control.
	// Parameters:
	//     pItemArgs - Parameters of item cell.
	//     pConstaints - constraints of item
	//-----------------------------------------------------------------------
	void Create(XTP_REPORTRECORDITEM_ARGS* pItemArgs, CXTPReportRecordItemConstraints* pConstaints);

	//-------------------------------------------------------------------------
	// Summary:
	//     This method is called to cancel user selection.
	//-------------------------------------------------------------------------
	void Cancel(void);

	//-------------------------------------------------------------------------
	// Summary:
	//     This method is called to save selected value of list box.
	//-------------------------------------------------------------------------
	void Apply(void);

protected:
	//{{AFX_CODEJOCK_PRIVATE
	DECLARE_MESSAGE_MAP()

	//{{AFX_VIRTUAL(CXTPPropertyGridInplaceEdit)
	void PostNcDestroy();
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CXTPReportInplaceList)
	afx_msg int  OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnLButtonUp(UINT, CPoint point);
	//}}AFX_MSG

	//}}AFX_CODEJOCK_PRIVATE

private:
	BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
};

#endif //

