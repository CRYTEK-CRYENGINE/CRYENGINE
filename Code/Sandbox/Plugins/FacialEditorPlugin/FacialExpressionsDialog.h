// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FacialExpressionsDialog_h__
#define __FacialExpressionsDialog_h__
#pragma once

#include "FacialEdContext.h"
#include "EffectorInfoWnd.h"
#include "Controls/SplitterCtrl.h"
#include "Controls/ImageListCtrl.h"

class CFacialEdContext;

//////////////////////////////////////////////////////////////////////////
class CExpressionsTreeCtrl : public CXTTreeCtrl, public IFacialEdListener
{
	DECLARE_DYNAMIC(CExpressionsTreeCtrl)
public:
	CExpressionsTreeCtrl();
	~CExpressionsTreeCtrl();

	void                                Reload();
	HTREEITEM                           AddEffector(IFacialEffector* pEffector, IFacialEffCtrl* pCtrl, HTREEITEM hParent);
	IFacialEffector*                    GetSelectedEffectorParent();
	IFacialEffector*                    GetSelectedEffector();
	IFacialEffCtrl*                     GetSelectedCtrl();

	void                                SetContext(CFacialEdContext* pContext);
	_smart_ptr<IFacialEffectorsLibrary> CreateLibraryOfSelectedEffectors();

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg void                    OnRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void                    OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void                    OnItemExpanded(NMHDR* pNMHDR, LRESULT* pResult);

	class CFacialExpressionsDialog* GetExpressionDialog();
	virtual void                    OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels);
	void                            HandleOrphans();

private:
	bool                                  m_bIgnoreEvent;
	HTREEITEM                             m_hRootItem;
	CImageList                            m_imageList;
	std::map<IFacialEffector*, HTREEITEM> m_itemsMap;
	CFacialEdContext*                     m_pContext;
};

//////////////////////////////////////////////////////////////////////////
class CExpressionsTabCtrl : public CTabCtrl
{
public:
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
};

//////////////////////////////////////////////////////////////////////////
class CFacialExpressionsDialog : public CDialog, public IFacialEdListener
{
	DECLARE_DYNAMIC(CFacialExpressionsDialog)
	friend class CExpressionsDialogDropTarget;
public:
	static void RegisterViewClass();

	CFacialExpressionsDialog();
	~CFacialExpressionsDialog();

	void                                SetContext(CFacialEdContext* pContext);

	void                                OnNewFolder();
	void                                OnNewExpression(int nType);
	void                                OnRename();
	void                                OnRemove();
	void                                OnDelete();
	void                                OnEmptyGarbage();
	void                                OnCopy();
	void                                OnSetPoseFromExpression();
	void                                OnPaste();
	_smart_ptr<IFacialEffectorsLibrary> CreateLibraryOfSelectedEffectors();

protected:
	virtual void OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels);

	virtual void OnOK()     {};
	virtual void OnCancel() {};

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTabSelect(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDestroy();

private:
	CSplitterCtrl        m_splitWnd;
	CExpressionsTreeCtrl m_treeCtrl;
	CImageListCtrl       m_imagesCtrl;
	CExpressionsTabCtrl  m_tabCtrl;

	CEffectorInfoWnd     m_effectorInfoWnd;

	CFacialEdContext*    m_pContext;
	HACCEL               m_hAccelerators;
	COleDropTarget*      m_pDropTarget;
};

#endif // __FacialExpressionsDialog_h__

