// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PrefabDialog.h"

#include "Dialogs/QGroupDialog.h"
#include "Dialogs/ToolbarDialog.h"
#include "Util/MFCUtil.h"
#include "PrefabManager.h"
#include "PrefabLibrary.h"
#include "PrefabItem.h"

#include "Objects\PrefabObject.h"

#include "ViewManager.h"
#include "Util/Clipboard.h"

#include <Cry3DEngine/I3DEngine.h>
//#include <IEntityRenderState.h>
#include <CryEntitySystem/IEntitySystem.h>

#define IDC_PREFABS_TREE AFX_IDW_PANE_FIRST

IMPLEMENT_DYNAMIC(CPrefabDialog, CBaseLibraryDialog)

/////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CPrefabPickCallback : public IPickObjectCallback
{
public:
	CPrefabPickCallback() { m_bActive = true; };
	//! Called when object picked.
	virtual void OnPick(CBaseObject* picked)
	{
		/*
		   m_bActive = false;
		   CPrefabItem *pParticles = picked->GetParticle();
		   if (pParticles)
		   GetIEditorImpl()->OpenPrefabLibrary( pParticles );
		 */
		delete this;
	}
	//! Called when pick mode cancelled.
	virtual void OnCancelPick()
	{
		m_bActive = false;
		delete this;
	}
	//! Return true if specified object is pickable.
	virtual bool OnPickFilter(CBaseObject* filterObject)
	{
		/*
		   // Check if object have material.
		   if (filterObject->GetParticle())
		   return true;
		 */
		return false;
	}
	static bool IsActive() { return m_bActive; };
private:
	static bool m_bActive;
};
bool CPrefabPickCallback::m_bActive = false;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CPrefabDialog implementation.
//////////////////////////////////////////////////////////////////////////
CPrefabDialog::CPrefabDialog(CWnd* pParent)
	: CBaseLibraryDialog(IDD_DB_ENTITY, pParent)
{
	GetIEditorImpl()->GetObjectManager()->AddObjectEventListener(functor(*this, &CPrefabDialog::OnObjectEvent));
	m_pPrefabManager = GetIEditorImpl()->GetPrefabManager();
	m_pItemManager = m_pPrefabManager;

	m_dragImage = 0;
	m_hDropItem = 0;

	// Immidiatly create dialog.
	Create(IDD_DATABASE, pParent);
}

CPrefabDialog::~CPrefabDialog()
{
	GetIEditorImpl()->GetObjectManager()->RemoveObjectEventListener(functor(*this, &CPrefabDialog::OnObjectEvent));
}

void CPrefabDialog::DoDataExchange(CDataExchange* pDX)
{
	CBaseLibraryDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CPrefabDialog, CBaseLibraryDialog)
ON_COMMAND(ID_DB_ADD, OnAddItem)

ON_COMMAND(ID_DB_SELECTASSIGNEDOBJECTS, OnSelectAssignedObjects)
ON_COMMAND(ID_DB_ASSIGNTOSELECTION, OnAssignToSelection)
ON_COMMAND(ID_DB_GETFROMSELECTION, OnGetFromSelection)
ON_COMMAND(ID_DB_MAKE_FROM_SELECTION, OnMakeFromSelection)

ON_UPDATE_COMMAND_UI(ID_DB_ASSIGNTOSELECTION, OnUpdateAssignToSelection)
ON_UPDATE_COMMAND_UI(ID_DB_SELECTASSIGNEDOBJECTS, OnUpdateSelected)
ON_UPDATE_COMMAND_UI(ID_DB_GETFROMSELECTION, OnUpdateObjectSelected)

ON_COMMAND(ID_DB_MTL_PICK, OnPickPrefab)
ON_UPDATE_COMMAND_UI(ID_DB_MTL_PICK, OnUpdatePickPrefab)

ON_NOTIFY(TVN_BEGINDRAG, IDC_PREFABS_TREE, OnBeginDrag)
ON_NOTIFY(NM_RCLICK, IDC_PREFABS_TREE, OnNotifyMtlTreeRClick)
//ON_NOTIFY(TVN_GETDISPINFO, IDC_PREFABS_TREE, OnGetDisplayInfoTree)
ON_WM_SIZE()
ON_WM_DESTROY()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnDestroy()
{
	int temp;
	int HSplitter = 0, VSplitter = 0;
	//m_wndHSplitter.GetRowInfo( 0,HSplitter,temp );
	m_wndVSplitter.GetColumnInfo(0, VSplitter, temp);
	AfxGetApp()->WriteProfileInt("Dialogs\\PrefabsEditor", "HSplitter", HSplitter);
	AfxGetApp()->WriteProfileInt("Dialogs\\PrefabsEditor", "VSplitter", VSplitter);

	//ReleaseGeometry();
	CBaseLibraryDialog::OnDestroy();
}

// CTVSelectKeyDialog message handlers
BOOL CPrefabDialog::OnInitDialog()
{
	CBaseLibraryDialog::OnInitDialog();

	InitLibraryToolbar();

	CRect rc;
	GetClientRect(rc);

	// Create left panel tree control.
	m_treeCtrl.Create(WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_BORDER | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | TVS_LINESATROOT | TVS_HASLINES |
	                  TVS_FULLROWSELECT | TVS_NOHSCROLL | TVS_INFOTIP | TVS_TRACKSELECT, rc, this, IDC_LIBRARY_ITEMS_TREE);

	//int h2 = rc.Height()/2;
	int h2 = 200;

	int HSplitter = AfxGetApp()->GetProfileInt("Dialogs\\PrefabsEditor", "HSplitter", 200);
	int VSplitter = AfxGetApp()->GetProfileInt("Dialogs\\PrefabsEditor", "VSplitter", 200);

	m_wndVSplitter.CreateStatic(this, 1, 2, WS_CHILD | WS_VISIBLE);
	//m_wndHSplitter.CreateStatic( &m_wndVSplitter,2,1,WS_CHILD|WS_VISIBLE );

	//m_imageList.Create(IDB_PREFABS_TREE, 16, 1, RGB (255, 0, 255));
	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_PREFABS_TREE, 16, RGB(255, 0, 255));

	// TreeCtrl must be already created.
	m_treeCtrl.SetParent(&m_wndVSplitter);
	m_treeCtrl.SetImageList(&m_imageList, TVSIL_NORMAL);

	//m_propsCtrl.Create( WS_VISIBLE|WS_CHILD|WS_BORDER,rc,&m_wndHSplitter,2 );
	m_propsCtrl.Create(WS_VISIBLE | WS_CHILD | WS_BORDER, rc, &m_wndVSplitter, 2);
	/*
	   m_vars = gParticleUI.CreateVars();
	   m_propsCtrl.AddVarBlock( m_vars );
	   m_propsCtrl.ExpandAllChilds( m_propsCtrl.GetRootItem(),false );
	   m_propsCtrl.EnableWindow( FALSE );
	 */

	//m_wndHSplitter.SetPane( 0,0,&m_previewCtrl,CSize(100,HSplitter) );
	//m_wndHSplitter.SetPane( 1,0,&m_propsCtrl,CSize(100,HSplitter) );

	m_wndVSplitter.SetPane(0, 0, &m_treeCtrl, CSize(VSplitter, 100));
	//m_wndVSplitter.SetPane( 0,1,&m_wndHSplitter,CSize(VSplitter,100) );
	m_wndVSplitter.SetPane(0, 1, &m_propsCtrl, CSize(100, HSplitter));

	RecalcLayout();

	ReloadLibs();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
UINT CPrefabDialog::GetDialogMenuID()
{
	return IDR_DB_ENTITY;
};

//////////////////////////////////////////////////////////////////////////
// Create the toolbar
void CPrefabDialog::InitToolbar(UINT nToolbarResID)
{
	/*
	   VERIFY( m_toolbar.CreateEx(this, TBSTYLE_FLAT|TBSTYLE_WRAPABLE,
	   WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_FLYBY|CBRS_SIZE_DYNAMIC) );
	   VERIFY( m_toolbar.LoadToolBar24(IDR_DB_PREFAB_BAR) );

	   // Resize the toolbar
	   CRect rc;
	   GetClientRect(rc);
	   m_toolbar.SetWindowPos(NULL, 0, 0, rc.right, 70, SWP_NOZORDER);
	   CSize sz = m_toolbar.CalcDynamicLayout(TRUE,TRUE);

	   CBaseLibraryDialog::InitToolbar(nToolbarResID);
	 */
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	// resize splitter window.
	if (m_wndVSplitter.m_hWnd)
	{
		m_wndVSplitter.MoveWindow(CRect(0, 0, cx, cy), FALSE);
	}
	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnAddItem()
{
	if (!m_pLibrary)
		return;

	QGroupDialog dlg(QObject::tr("New Prefab Name"));
	dlg.SetGroup(m_selectedGroup);
	//dlg.SetString( entityClass );
	if (dlg.exec() != QDialog::Accepted || dlg.GetString().IsEmpty())
	{
		return;
	}

	string fullName = m_pItemManager->MakeFullItemName(m_pLibrary, dlg.GetGroup().GetString(), dlg.GetString().GetString());
	if (m_pItemManager->FindItemByName(fullName))
	{
		Warning("Item with name %s already exist", (const char*)fullName);
		return;
	}

	CUndo undo("Add prefab library item");
	CPrefabItem* pItem = (CPrefabItem*)m_pItemManager->CreateItem(m_pLibrary);

	// Make prototype name.
	SetItemName(pItem, dlg.GetGroup().GetString(), dlg.GetString().GetString());
	pItem->Update();

	ReloadItems();
	SelectItem(pItem);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::SelectItem(CBaseLibraryItem* item, bool bForceReload)
{
	bool bChanged = item != m_pCurrentItem || bForceReload;
	CBaseLibraryDialog::SelectItem(item, bForceReload);

	if (!bChanged)
		return;

	if (!item)
	{
		m_propsCtrl.EnableWindow(FALSE);
		return;
	}

	m_propsCtrl.EnableWindow(TRUE);
	m_propsCtrl.EnableUpdateCallback(false);

	// Render preview geometry with current material
	CPrefabItem* prefab = GetSelectedPrefab();

	//AssignMtlToGeometry();

	// Update variables.
	m_propsCtrl.EnableUpdateCallback(false);
	//gParticleUI.SetFromParticles( pParticles );
	m_propsCtrl.EnableUpdateCallback(true);

	//m_propsCtrl.SetUpdateCallback( functor(*this,OnUpdateProperties) );
	m_propsCtrl.EnableUpdateCallback(true);
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CPrefabDialog::InsertItemToTree(CBaseLibraryItem* pItem, HTREEITEM hParent)
{
	assert(pItem);

	string itemName;
	int itemCount = m_pPrefabManager->GetPrefabInstanceCount((CPrefabItem*)pItem);
	itemName.Format("%s (%d)", pItem->GetShortName(), itemCount);

	HTREEITEM hItem = GetTreeCtrl().InsertItem(itemName, 2, 3, hParent);
	GetTreeCtrl().SetItemData(hItem, (DWORD_PTR)pItem);
	m_itemsToTree[pItem] = hItem;
	return hItem;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::Update()
{
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnUpdateProperties(IVariable* var)
{
	CPrefabItem* pPrefab = GetSelectedPrefab();
	if (!pPrefab)
		return;

	//gParticleUI.SetToParticles( pParticles );

	//AssignMtlToGeometry();

	GetIEditorImpl()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////
CPrefabItem* CPrefabDialog::GetSelectedPrefab()
{
	CBaseLibraryItem* pItem = m_pCurrentItem;
	return (CPrefabItem*)pItem;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnAssignToSelection()
{
	CPrefabItem* pPrefab = GetSelectedPrefab();
	if (!pPrefab)
		return;

	CUndo undo("Assign Prefab");

	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	if (!pSel->IsEmpty())
	{
		for (int i = 0; i < pSel->GetCount(); i++)
		{
			CBaseObject* pObject = pSel->GetObject(i);
			if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
			{
				CPrefabObject* pPrefabObject = (CPrefabObject*)pObject;
				pPrefabObject->SetPrefab(pPrefab, false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnSelectAssignedObjects()
{
	CPrefabItem* pPrefab = GetSelectedPrefab();
	if (!pPrefab)
		return;

	CBaseObjectsArray objects;

	GetIEditorImpl()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CPrefabObject), objects);
	if (!objects.empty())
		GetIEditorImpl()->ClearSelection();

	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];
		CPrefabObject* pPrefabObject = static_cast<CPrefabObject*>(pObject);
		if (pPrefabObject->GetPrefabItem() == pPrefab)
		{
			GetIEditorImpl()->SelectObject(pPrefabObject);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnGetFromSelection()
{
	if (!m_pLibrary)
		return;

	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	if (pSel->IsEmpty())
		return;

	if (pSel->GetCount() == 1)
	{
		CBaseObject* pObject = pSel->GetObject(0);
		if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			CPrefabObject* pPrefabObject = (CPrefabObject*)pObject;
			SelectItem(pPrefabObject->GetPrefabItem());
			return;
		}
	}

	QGroupDialog dlg(QObject::tr("New Prefab Name"));
	dlg.SetGroup(m_selectedGroup);
	if (dlg.exec() != QDialog::Accepted || dlg.GetString().IsEmpty())
	{
		return;
	}

	CPrefabItem* pPrefab = (CPrefabItem*)m_pItemManager->CreateItem(m_pLibrary);
	SetItemName(pPrefab, dlg.GetGroup().GetString(), dlg.GetString().GetString());

	// Serialize these objects into prefab.
	pPrefab->MakeFromSelection(*pSel);

	ReloadItems();
	SelectItem(pPrefab);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnMakeFromSelection()
{
	if (!m_pLibrary)
		return;

	CPrefabItem* pPrefab = GetSelectedPrefab();
	if (!pPrefab)
		return;

	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	if (pSel->IsEmpty())
		return;

	CryGUID stItemGuid = pPrefab->GetGUID();
	for (int i = 0; i < pSel->GetCount(); ++i)
	{
		CBaseObject* pObject = pSel->GetObject(i);
		if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			CPrefabObject* pPrefabObject = (CPrefabObject*)pObject;
			CPrefabItem* pPerfabObjectItem = pPrefabObject->GetPrefabItem();
			if (pPerfabObjectItem->GetGUID() == stItemGuid)
			{
				GetIEditorImpl()->GetPrefabManager()->AddSelectionToPrefab();
				return;
			}
		}
	}

	// Serialize these objects into prefab.
	pPrefab->MakeFromSelection(*pSel);
	UpdatePrefabObjects(pPrefab);

	ReloadItems();
	SelectItem(pPrefab);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::UpdatePrefabObjects(CPrefabItem* pPrefab)
{
	assert(pPrefab);
	// Update all existing prefabs.
	CBaseObjectsArray objects;
	GetIEditorImpl()->GetObjectManager()->GetObjects(objects);

	std::vector<_smart_ptr<CPrefabObject>> prefabObjects;

	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];
		if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			CPrefabObject* pPrefabObject = (CPrefabObject*)pObject;
			if (pPrefabObject->GetPrefabItem() == pPrefab)
				prefabObjects.push_back(pPrefabObject);
		}
	}

	for (int i = 0, iPrefabObjectCount(prefabObjects.size()); i < iPrefabObjectCount; ++i)
		prefabObjects[i]->SetPrefab(pPrefab, true);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnUpdateAssignToSelection(CCmdUI* pCmdUI)
{
	if (GetSelectedPrefab() && !GetIEditorImpl()->GetSelection()->IsEmpty())
	{
		pCmdUI->Enable(TRUE);
	}
	else
	{
		pCmdUI->Enable(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnUpdateObjectSelected(CCmdUI* pCmdUI)
{
	if (!GetIEditorImpl()->GetSelection()->IsEmpty())
	{
		pCmdUI->Enable(TRUE);
	}
	else
	{
		pCmdUI->Enable(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

	HTREEITEM hItem = pNMTreeView->itemNew.hItem;

	CPrefabItem* pPrefab = (CPrefabItem*)m_treeCtrl.GetItemData(hItem);
	if (!pPrefab)
		return;

	m_pDraggedItem = pPrefab;

	m_treeCtrl.Select(hItem, TVGN_CARET);

	m_hDropItem = 0;
	m_dragImage = m_treeCtrl.CreateDragImage(hItem);
	if (m_dragImage)
	{
		m_hDraggedItem = hItem;
		m_hDropItem = hItem;
		m_dragImage->BeginDrag(0, CPoint(-10, -10));

		CRect rc;
		AfxGetMainWnd()->GetWindowRect(rc);

		CPoint p = pNMTreeView->ptDrag;
		ClientToScreen(&p);
		p.x -= rc.left;
		p.y -= rc.top;

		m_dragImage->DragEnter(AfxGetMainWnd(), p);
		SetCapture();
		GetIEditorImpl()->EnableUpdate(false);
	}

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_dragImage)
	{
		CPoint p;

		p = point;
		ClientToScreen(&p);
		m_treeCtrl.ScreenToClient(&p);

		TVHITTESTINFO hit;
		ZeroStruct(hit);
		hit.pt = p;
		HTREEITEM hHitItem = m_treeCtrl.HitTest(&hit);
		if (hHitItem)
		{
			if (m_hDropItem != hHitItem)
			{
				if (m_hDropItem)
					m_treeCtrl.SetItem(m_hDropItem, TVIF_STATE, 0, 0, 0, 0, TVIS_DROPHILITED, 0);
				// Set state of this item to drop target.
				m_treeCtrl.SetItem(hHitItem, TVIF_STATE, 0, 0, 0, TVIS_DROPHILITED, TVIS_DROPHILITED, 0);
				m_hDropItem = hHitItem;
				m_treeCtrl.Invalidate();
			}
		}

		CRect rc;
		AfxGetMainWnd()->GetWindowRect(rc);
		p = point;
		ClientToScreen(&p);
		p.x -= rc.left;
		p.y -= rc.top;
		m_dragImage->DragMove(p);

		SetCursor(m_hCursorNoDrop);
		// Check if can drop here.
		{
			CPoint p;
			GetCursorPos(&p);
			CViewport* viewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint(p);
			if (viewport)
			{
				SetCursor(m_hCursorCreate);
				CPoint vp = p;
				viewport->ScreenToClient(&vp);
				HitContext hit;
				if (viewport->HitTest(vp, hit))
				{
					if (hit.object && hit.object->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
					{
						SetCursor(m_hCursorReplace);
					}
				}
			}
		}
	}

	CBaseLibraryDialog::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	//CXTResizeDialog::OnLButtonUp(nFlags, point);

	if (m_hDropItem)
	{
		m_treeCtrl.SetItem(m_hDropItem, TVIF_STATE, 0, 0, 0, 0, TVIS_DROPHILITED, 0);
		m_hDropItem = 0;
	}

	if (m_dragImage)
	{
		CPoint p;
		GetCursorPos(&p);

		GetIEditorImpl()->EnableUpdate(true);

		m_dragImage->DragLeave(AfxGetMainWnd());
		m_dragImage->EndDrag();
		delete m_dragImage;
		m_dragImage = 0;
		ReleaseCapture();
		SetCursor(m_hCursorDefault);

		CPoint treepoint = p;
		m_treeCtrl.ScreenToClient(&treepoint);

		CRect treeRc;
		m_treeCtrl.GetClientRect(treeRc);

		if (treeRc.PtInRect(treepoint))
		{
			// Droped inside tree.
			TVHITTESTINFO hit;
			ZeroStruct(hit);
			hit.pt = treepoint;
			HTREEITEM hHitItem = m_treeCtrl.HitTest(&hit);
			if (hHitItem)
			{
				//DropToItem( hHitItem,m_hDraggedItem,m_pDraggedItem );
				m_hDraggedItem = 0;
				m_pDraggedItem = 0;
				return;
			}
			//DropToItem( 0,m_hDraggedItem,m_pDraggedItem );
		}
		else
		{
			// Not droped inside tree.

			CWnd* wnd = WindowFromPoint(p);

			CViewport* viewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint(p);
			if (viewport)
			{
				CUndo undo("Create Prefab");
				string guid = m_pDraggedItem->GetGUID().ToString();
				GetIEditorImpl()->StartObjectCreation(((CPrefabItem*)m_pDraggedItem)->GetPrefabObjectClassName(), guid);
			}
		}
		m_pDraggedItem = 0;
	}
	m_pDraggedItem = 0;
	m_hDraggedItem = 0;

	CBaseLibraryDialog::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnObjectEvent(CBaseObject* pObject, int nEvent)
{
	if (m_pLibrary)
	{
		if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			if (nEvent == OBJECT_ON_ADD || nEvent == OBJECT_ON_DELETE)
			{
				CPrefabObject* pPrefabObject = (CPrefabObject*)pObject;
				for (int i = 0; i < m_pLibrary->GetItemCount(); ++i)
				{
					CPrefabItem* pItem = (CPrefabItem*)m_pLibrary->GetItem(i);
					if (pItem && pItem->GetGUID() == pPrefabObject->GetPrefabGuid())
					{
						string itemName;
						HTREEITEM hItem = stl::find_in_map(m_itemsToTree, pItem, (HTREEITEM)0);
						int itemCount = m_pPrefabManager->GetPrefabInstanceCount((CPrefabItem*)pItem);
						itemName.Format("%s (%d)", pItem->GetShortName(), itemCount);
						GetTreeCtrl().SetItemText(hItem, itemName);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnNotifyMtlTreeRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	// Show helper menu.
	CPoint point;

	CPrefabItem* pParticles = 0;

	// Find node under mouse.
	GetCursorPos(&point);
	m_treeCtrl.ScreenToClient(&point);
	// Select the item that is at the point myPoint.
	UINT uFlags;
	HTREEITEM hItem = m_treeCtrl.HitTest(point, &uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pParticles = (CPrefabItem*)m_treeCtrl.GetItemData(hItem);
	}

	if (!pParticles)
		return;

	SelectItem(pParticles);

	// Create pop up menu.
	CMenu menu;
	menu.CreatePopupMenu();

	if (pParticles)
	{
		CClipboard clipboard;
		bool bNoPaste = clipboard.IsEmpty();
		int pasteFlags = 0;
		if (bNoPaste)
			pasteFlags |= MF_GRAYED;

		menu.AppendMenu(MF_STRING, ID_DB_CUT, "Cut");
		menu.AppendMenu(MF_STRING, ID_DB_COPY, "Copy");
		menu.AppendMenu(MF_STRING | pasteFlags, ID_DB_PASTE, "Paste");
		menu.AppendMenu(MF_STRING, ID_DB_CLONE, "Clone");
		menu.AppendMenu(MF_SEPARATOR, 0, "");
		menu.AppendMenu(MF_STRING, ID_DB_RENAME, "Rename");
		menu.AppendMenu(MF_STRING, ID_DB_REMOVE, "Delete");
		menu.AppendMenu(MF_SEPARATOR, 0, "");
		menu.AppendMenu(MF_STRING, ID_DB_MAKE_FROM_SELECTION, "Replace Prefab with Selected Objects");
		menu.AppendMenu(MF_STRING, ID_DB_ASSIGNTOSELECTION, "Assign to Selected Prefab");
		menu.AppendMenu(MF_STRING, ID_DB_SELECTASSIGNEDOBJECTS, "Select all Instances of this Prefab");
	}

	GetCursorPos(&point);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnGetDisplayInfoTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	/*
	   TV_DISPINFO* pTVDispInfo = (TV_DISPINFO*)pNMHDR;

	 * pResult = 0;
	   if (pTVDispInfo->item.mask & (TVIF_SELECTEDIMAGE | TVIF_IMAGE))
	   {
	   if (m_treeCtrl.GetChildItem(pTVDispInfo->item.hItem) != NULL)
	   {
	    UINT nState = m_treeCtrl.GetItemState(pTVDispInfo->item.hItem, TVIF_STATE);
	    pTVDispInfo->item.iSelectedImage = pTVDispInfo->item.iImage = nState & TVIS_EXPANDED? 1 : 0;
	   }
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnPickPrefab()
{
	if (!CPrefabPickCallback::IsActive())
		GetIEditorImpl()->PickObject(new CPrefabPickCallback);
	else
		GetIEditorImpl()->CancelPick();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnUpdatePickPrefab(CCmdUI* pCmdUI)
{
	if (CPrefabPickCallback::IsActive())
	{
		pCmdUI->SetCheck(1);
	}
	else
	{
		pCmdUI->SetCheck(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnCopy()
{
	CPrefabItem* pItem = GetSelectedPrefab();
	if (pItem)
	{
		XmlNodeRef node = XmlHelpers::CreateXmlNode("Prefab");
		CBaseLibraryItem::SerializeContext ctx(node, false);
		ctx.bCopyPaste = true;

		CClipboard clipboard;
		pItem->Serialize(ctx);
		clipboard.Put(node);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::OnPaste()
{
	if (!m_pLibrary)
		return;

	CPrefabItem* pItem = GetSelectedPrefab();
	if (!pItem)
		return;

	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return;
	XmlNodeRef node = clipboard.Get();
	if (!node)
		return;

	if (strcmp(node->getTag(), "Prefab") == 0)
	{
		// This is material node.
		CUndo undo("Add prefab library item");
		CBaseLibraryItem* pItem = (CBaseLibraryItem*)m_pPrefabManager->CreateItem(m_pLibrary);
		if (pItem)
		{
			CBaseLibraryItem::SerializeContext serCtx(node, true);
			serCtx.bCopyPaste = true;
			pItem->Serialize(serCtx);
			pItem->SetName(m_pPrefabManager->MakeUniqItemName(pItem->GetName()));
			ReloadItems();
			SelectItem(pItem);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabDialog::DropToItem(HTREEITEM hItem, HTREEITEM hSrcItem, CPrefabItem* pParticles)
{
	pParticles->GetLibrary()->SetModified();

	TSmartPtr<CPrefabItem> pHolder = pParticles; // Make usre its not release while we remove and add it back.

	/*
	   if (!hItem)
	   {
	   // Detach from parent.
	   if (pParticles->GetParent())
	    pParticles->GetParent()->RemoveChild( pParticles );

	   ReloadItems();
	   SelectItem( pParticles );
	   return;
	   }

	   CPrefabItem* pTargetItem = (CPrefabItem*)m_treeCtrl.GetItemData(hItem);
	   if (!pTargetItem)
	   {
	   // This is group.

	   // Detach from parent.
	   if (pParticles->GetParent())
	    pParticles->GetParent()->RemoveChild( pParticles );

	   // Move item to different group.
	   string groupName = m_treeCtrl.GetItemText(hItem);
	   SetItemName( pParticles,groupName,pParticles->GetShortName() );

	   m_treeCtrl.DeleteItem( hSrcItem );
	   InsertItemToTree( pParticles,hItem );
	   return;
	   }
	   // Ignore itself or immidiate target.
	   if (pTargetItem == pParticles || pTargetItem == pParticles->GetParent())
	   return;



	   // Detach from parent.
	   if (pParticles->GetParent())
	   pParticles->GetParent()->RemoveChild( pParticles );

	   // Attach to target.
	   pTargetItem->AddChild( pParticles );

	   ReloadItems();
	   SelectItem( pParticles );
	 */
}

//////////////////////////////////////////////////////////////////////////
CPrefabItem* CPrefabDialog::GetPrefabFromSelection()
{
	CPrefabItem* pItem = GetSelectedPrefab();
	OnGetFromSelection();
	CPrefabItem* pItemNew = GetSelectedPrefab();
	if (pItemNew != pItem)
		return pItemNew;
	return 0;
}

