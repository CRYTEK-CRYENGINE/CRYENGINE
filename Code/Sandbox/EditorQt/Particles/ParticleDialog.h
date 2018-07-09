// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __particledialog_h__
#define __particledialog_h__
#pragma once

#include "BaseLibraryDialog.h"
#include "Controls\SplitterCtrl.h"
#include "Controls\TreeCtrlEx.h"
#include "Controls\PropertyCtrlEx.h"
#include "Controls\PreviewModelCtrl.h"

class CParticleItem;
class CParticleManager;

/** Dialog which hosts entity prototype library.
 */
class CParticleDialog : public CBaseLibraryDialog
{
	DECLARE_DYNAMIC(CParticleDialog)
public:
	CParticleDialog(CWnd* pParent);
	~CParticleDialog();

	// Called every frame.
	void         Update();

	virtual void SelectItem(CBaseLibraryItem* item, bool bForceReload = false);
	virtual UINT GetDialogMenuID();

	static CParticleDialog* s_poCurrentInstance;
	static CParticleDialog* GetCurrentInstance();
	CParticleItem*          GetSelected();

	afx_msg void            OnAssignToSelection();
	afx_msg void            OnGetFromSelection();
	afx_msg void            OnResetParticles();
	afx_msg void            OnReloadParticles();
	afx_msg void            OnEnable()    { Enable(false); }
	afx_msg void            OnEnableAll() { Enable(true); }
	afx_msg void            OnExpandAll();
	afx_msg void            OnCollapseAll();

protected:
	void         ExpandCollapseAll(bool expand = true);
	void         DeleteTreeItem(HTREEITEM hItem);
	void         DoDataExchange(CDataExchange* pDX);
	BOOL         OnInitDialog();
	void         DeleteAllTreeItems();

	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);

	afx_msg void OnAddItem() { AddItem(false); }
	afx_msg void OnPlay();
	afx_msg void OnUpdatePlay(CCmdUI* pCmdUI);
	afx_msg void OnUpdateItemSelected(CCmdUI* pCmdUI);
	afx_msg void OnUpdateObjectSelected(CCmdUI* pCmdUI);
	afx_msg void OnUpdateAssignToSelection(CCmdUI* pCmdUI);
	afx_msg void OnUpdateConvertFromEntity(CCmdUI* pCmdUI);
	afx_msg void OnKeyDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNotifyTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPick();
	afx_msg void OnUpdatePick(CCmdUI* pCmdUI);
	afx_msg void OnCopy();
	afx_msg void OnPaste();
	afx_msg void OnClone();
	afx_msg void OnSelectAssignedObjects();

	void         Enable(bool bAll);
	bool         AddItem(bool bFromParent);

	//////////////////////////////////////////////////////////////////////////
	// Some functions can be overriden to modify standard functionality.
	//////////////////////////////////////////////////////////////////////////
	virtual void      InitToolbar(UINT nToolbarResID);
	virtual HTREEITEM InsertItemToTree(CBaseLibraryItem* pItem, HTREEITEM hParent);
	virtual void      OnEditorNotifyEvent(EEditorNotifyEvent event);

	//////////////////////////////////////////////////////////////////////////
	void                 OnUpdateProperties(IVariable* var);

	bool                 AssignToEntity(CParticleItem* pItem, CBaseObject* pObject, Vec3 const* pPos = NULL);
	void                 AssignToSelection();

	CBaseObject*         CreateParticleEntity(CParticleItem* pItem, Vec3 const& pos, CBaseObject* pParent = NULL);

	void                 UpdateItemState(CParticleItem* pItem, bool bRecursive = false);

	void                 ReleasePreviewControl();
	void                 DropToItem(HTREEITEM hItem, HTREEITEM hSrcItem, CParticleItem* pItem);
	class CEntityObject* GetItemFromEntity();

	DECLARE_MESSAGE_MAP()

	CSplitterCtrl m_wndHSplitter;
	CSplitterCtrl     m_wndVSplitter;

	CPreviewModelCtrl m_previewCtrl;
	CPropertyCtrlEx   m_propsCtrl;
	CImageList        m_imageList;

	CImageList*       m_dragImage;

	bool              m_bForceReloadPropsCtrl;
	bool              m_bAutoAssignToSelection;
	bool              m_bRealtimePreviewUpdate;

	// Particle manager.
	CParticleManager*            m_pPartManager;

	CVarBlockPtr                 m_vars;
	class CParticleUIDefinition* pParticleUI;

	HTREEITEM                    m_hDropItem;
	HTREEITEM                    m_hDraggedItem;

	TSmartPtr<CParticleItem>     m_pDraggedItem;
};

#endif // __particledialog_h__

