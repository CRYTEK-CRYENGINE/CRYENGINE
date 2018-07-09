// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleDialog.h"

#include "Dialogs/QGroupDialog.h"
#include "Dialogs/ToolbarDialog.h"

#include "ParticleManager.h"
#include "ParticleLibrary.h"
#include "ParticleItem.h"

#include "Objects/BrushObject.h"
#include "Objects/EntityObject.h"
#include "Objects/ParticleEffectObject.h"
#include "ViewManager.h"
#include "CryEditDoc.h"
#include "Util/Clipboard.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryParticleSystem/ParticleParams.h>
#include <CryCore/CryTypeInfo.h>
#include <CryEntitySystem/IEntitySystem.h>

#include "IResourceSelectorHost.h"
#include "Dialogs/GenericSelectItemDialog.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/AssetResourceSelector.h>
#include <Controls/EditorDialog.h>
#include <FilePathUtil.h>
#include <QtUtil.h>
#include <FileDialogs/EngineFileDialog.h>

#include "FileSystem/FileSystem_Enumerator.h"
#include "FileSystem/FileSystem_FileFilter.h"
#include "FileSystem/FileSystem_File.h"

#define CURVE_TYPE
#include "Util/VariableTypeInfo.h"
#include "Util/MFCUtil.h"

#include <CryParticleSystem/ParticleParams_TypeInfo.h>

#define IDC_PARTICLES_TREE  AFX_IDW_PANE_FIRST

#define EDITOR_OBJECTS_PATH string("%EDITOR%/Objects/")

IMPLEMENT_DYNAMIC(CParticleDialog, CBaseLibraryDialog);
//////////////////////////////////////////////////////////////////////////
// Particle UI structures.
//////////////////////////////////////////////////////////////////////////

/** User Interface definition of particle system.
 */
class CParticleUIDefinition
{
public:
	ParticleParams           m_localParams;
	ParticleParams           m_defaultParams;
	CVarBlockPtr             m_vars;
	IVariable::OnSetCallback m_onSetCallback;

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		m_vars = new CVarBlock;

		int nNumGroups = 0;

		// Create UI vars, using ParticleParams TypeInfo.
		CVariableArray* pVarTable = 0;

		const CTypeInfo& partInfo = TypeInfo(&m_localParams);
		ForAllSubVars(pVarInfo, partInfo)
		{
			if (*pVarInfo->GetName() == '_')
				continue;

			string sGroup;
			if (pVarInfo->GetAttr("Group", sGroup))
			{
				pVarTable = AddGroup(sGroup);
				if (nNumGroups++ > 5)
				{
					pVarTable->SetFlags(pVarTable->GetFlags() | IVariable::UI_ROLLUP2);
				}
				if (pVarInfo->Type.IsType<void>())
					continue;
			}

			IVariable* pVar = CVariableTypeInfo::Create(*pVarInfo, &m_localParams, &m_defaultParams);

			// Add to group.
			pVar->AddRef();               // Variables are local and must not be released by CVarBlock.
			if (pVarTable)
				pVarTable->AddVariable(pVar);
		}

		return m_vars;
	}

	//////////////////////////////////////////////////////////////////////////
	void SetFromParticles(CParticleItem* pParticles)
	{
		IParticleEffect* pEffect = pParticles->GetEffect();
		if (!pEffect)
			return;

		// Copy to local params, then run update on all vars.
		m_localParams = pEffect->GetParticleParams();
		m_defaultParams = pEffect->GetDefaultParams();
		m_vars->OnSetValues();
	}

	//////////////////////////////////////////////////////////////////////////
	bool SetToParticles(CParticleItem* pParticles)
	{
		IParticleEffect* pEffect = pParticles->GetEffect();
		if (!pEffect)
			return false;

		// Detect whether inheritance changed, update defaults.
		bool bInheritanceChanged = m_localParams.eInheritance != pEffect->GetParticleParams().eInheritance;

		pEffect->SetParticleParams(m_localParams);

		if (bInheritanceChanged)
			m_defaultParams = pEffect->GetDefaultParams();

		// Update particles.
		pParticles->Update();

		return bInheritanceChanged;
	}

	//////////////////////////////////////////////////////////////////////////
	void ResetParticles(CParticleItem* pParticles)
	{
		IParticleEffect* pEffect = pParticles->GetEffect();
		if (!pEffect)
			return;

		// Set params to defaults, using current inheritance value.
		auto eSave = m_localParams.eInheritance;
		m_localParams = m_defaultParams;
		m_localParams.eInheritance = eSave;

		pEffect->SetParticleParams(m_localParams);

		// Update particles.
		pParticles->Update();
	}

private:

	//////////////////////////////////////////////////////////////////////////
	CVariableArray* AddGroup(const char* sName)
	{
		CVariableArray* pArray = new CVariableArray;
		pArray->AddRef();
		pArray->SetFlags(IVariable::UI_BOLD);
		if (sName)
			pArray->SetName(sName);
		m_vars->AddVariable(pArray);
		return pArray;
	}
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CParticlePickCallback : public IPickObjectCallback
{
public:
	CParticlePickCallback() { m_bActive = true; };
	//! Called when object picked.
	virtual void OnPick(CBaseObject* picked)
	{
		/*
		   m_bActive = false;
		   CParticleItem *pParticles = picked->GetParticle();
		   if (pParticles)
		   GetIEditorImpl()->OpenParticleLibrary( pParticles );
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
bool CParticlePickCallback::m_bActive = false;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CParticleDialog implementation.
//////////////////////////////////////////////////////////////////////////
CParticleDialog::CParticleDialog(CWnd* pParent)
	: CBaseLibraryDialog(IDD_DB_ENTITY, pParent)
{
	s_poCurrentInstance = this;

	m_pPartManager = static_cast<CParticleManager*>(GetIEditorImpl()->GetParticleManager());
	m_pItemManager = m_pPartManager;

	m_sortRecursionType = SORT_RECURSION_ITEM;

	m_dragImage = 0;
	m_hDropItem = 0;

	m_bRealtimePreviewUpdate = true;
	m_bForceReloadPropsCtrl = true;
	m_bAutoAssignToSelection = false;

	m_hCursorDefault = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	m_hCursorCreate = AfxGetApp()->LoadCursor(IDC_HIT_CURSOR);
	m_hCursorReplace = AfxGetApp()->LoadCursor(IDC_HAND_INTERNAL);

	pParticleUI = new CParticleUIDefinition;

	// Immidiatly create dialog.
	Create(IDD_DATABASE, pParent);
}

CParticleDialog::~CParticleDialog()
{
	delete pParticleUI;
}

void CParticleDialog::DoDataExchange(CDataExchange* pDX)
{
	CBaseLibraryDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CParticleDialog, CBaseLibraryDialog)
ON_COMMAND(ID_DB_ADD, OnAddItem)
ON_COMMAND(ID_DB_PLAY, OnPlay)
ON_COMMAND(ID_DB_CLONE, OnClone)

ON_UPDATE_COMMAND_UI(ID_DB_PLAY, OnUpdatePlay)

ON_COMMAND(ID_DB_SELECTASSIGNEDOBJECTS, OnSelectAssignedObjects)
ON_COMMAND(ID_DB_ACTIVATE, OnEnable)
ON_COMMAND(ID_DB_ACTIVATE_ALL, OnEnableAll)
ON_COMMAND(ID_DB_ASSIGNTOSELECTION, OnAssignToSelection)
ON_COMMAND(ID_DB_GETFROMSELECTION, OnGetFromSelection)
ON_COMMAND(ID_DB_RELOAD, OnReloadParticles)
ON_COMMAND(IDC_RESET, OnResetParticles)
ON_COMMAND(ID_DB_EXPAND_ALL, OnExpandAll)
ON_COMMAND(ID_DB_COLLAPSE_ALL, OnCollapseAll)

ON_UPDATE_COMMAND_UI(ID_DB_ASSIGNTOSELECTION, OnUpdateAssignToSelection)
ON_UPDATE_COMMAND_UI(ID_DB_SELECTASSIGNEDOBJECTS, OnUpdateSelected)
ON_UPDATE_COMMAND_UI(ID_DB_GETFROMSELECTION, OnUpdateObjectSelected)

ON_COMMAND(ID_DB_MTL_PICK, OnPick)
ON_UPDATE_COMMAND_UI(ID_DB_MTL_PICK, OnUpdatePick)

ON_NOTIFY(TVN_KEYDOWN, IDC_LIBRARY_ITEMS_TREE, OnKeyDown)
ON_NOTIFY(TVN_BEGINDRAG, IDC_PARTICLES_TREE, OnBeginDrag)
ON_NOTIFY(NM_RCLICK, IDC_PARTICLES_TREE, OnNotifyTreeRClick)
ON_NOTIFY(TVN_SELCHANGED, IDC_PARTICLES_TREE, OnTvnSelchangedTree)
ON_WM_SIZE()
ON_WM_DESTROY()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

CParticleDialog * CParticleDialog::s_poCurrentInstance = NULL;

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnDestroy()
{
	int temp;
	int HSplitter, VSplitter;
	m_wndHSplitter.GetRowInfo(0, HSplitter, temp);
	m_wndVSplitter.GetColumnInfo(0, VSplitter, temp);
	AfxGetApp()->WriteProfileInt("Dialogs\\Particles", "HSplitter", HSplitter);
	AfxGetApp()->WriteProfileInt("Dialogs\\Particles", "VSplitter", VSplitter);

	s_poCurrentInstance = NULL;
	//ReleaseGeometry();
	CBaseLibraryDialog::OnDestroy();
}

// CTVSelectKeyDialog message handlers
BOOL CParticleDialog::OnInitDialog()
{
	CBaseLibraryDialog::OnInitDialog();

	InitToolbar(IDR_DB_PARTICLES_BAR);

	CRect rc;
	GetClientRect(rc);

	// Create left panel tree control.
	m_treeCtrl.Create(WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_BORDER | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | TVS_LINESATROOT | TVS_HASLINES |
	                  TVS_FULLROWSELECT | TVS_NOHSCROLL | TVS_INFOTIP /*|TVS_TRACKSELECT*/, rc, this, IDC_LIBRARY_ITEMS_TREE);

	int HSplitter = AfxGetApp()->GetProfileInt("Dialogs\\Particles", "HSplitter", 200);
	int VSplitter = AfxGetApp()->GetProfileInt("Dialogs\\Particles", "VSplitter", 200);

	m_wndVSplitter.CreateStatic(this, 1, 2, WS_CHILD | WS_VISIBLE);
	m_wndHSplitter.CreateStatic(&m_wndVSplitter, 2, 1, WS_CHILD | WS_VISIBLE);

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_PARTICLES_TREE, 16, RGB(255, 0, 255));

	// TreeCtrl must be already created.
	m_treeCtrl.SetParent(&m_wndHSplitter);
	m_treeCtrl.SetImageList(&m_imageList, TVSIL_NORMAL);

	m_previewCtrl.Create(&m_wndHSplitter, rc, WS_CHILD | WS_VISIBLE);
	m_previewCtrl.SetGrid(true);
	m_previewCtrl.SetAxis(true);
	m_previewCtrl.EnableUpdate(true);

	m_propsCtrl.Create(WS_VISIBLE | WS_CHILD | WS_BORDER, rc, &m_wndVSplitter, 2);
	m_vars = pParticleUI->CreateVars();
	m_propsCtrl.AddVarBlock(m_vars);
	m_propsCtrl.ExpandAllChilds(m_propsCtrl.GetRootItem(), false);
	m_propsCtrl.EnableWindow(FALSE);

	m_wndHSplitter.SetPane(0, 0, &m_treeCtrl, CSize(100, HSplitter));
	m_wndHSplitter.SetPane(1, 0, &m_previewCtrl, CSize(100, HSplitter));

	m_wndVSplitter.SetPane(0, 0, &m_wndHSplitter, CSize(VSplitter, 100));
	m_wndVSplitter.SetPane(0, 1, &m_propsCtrl, CSize(VSplitter, 100));

	RecalcLayout();

	ReloadLibs();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
UINT CParticleDialog::GetDialogMenuID()
{
	return IDR_DB_ENTITY;
};

//////////////////////////////////////////////////////////////////////////
// Create the toolbar
void CParticleDialog::InitToolbar(UINT nToolbarResID)
{
	InitLibraryToolbar();

	CXTPToolBar* pParticleToolBar = GetCommandBars()->Add(_T("ParticlesToolBar"), xtpBarTop);
	pParticleToolBar->EnableCustomization(FALSE);
	VERIFY(pParticleToolBar->LoadToolBar(nToolbarResID));
	CXTPToolBar* pItemBar = GetCommandBars()->GetToolBar(IDR_DB_LIBRARY_ITEM_BAR);
	if (pItemBar)
		DockRightOf(pParticleToolBar, pItemBar);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	// resize splitter window.
	if (m_wndVSplitter.m_hWnd)
	{
		/*
		   int cxCur,cxMin;
		   m_wndVSplitter.GetColumnInfo(0,cxCur,cxMin);
		   int nSize = max(cx-cxCur,0);
		   m_wndHSplitter.SetRowInfo( 0,nSize,100 );
		 */

		CRect rc;
		GetClientRect(rc);
		m_wndVSplitter.MoveWindow(rc, FALSE);
	}
	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CParticleDialog::InsertItemToTree(CBaseLibraryItem* pItem, HTREEITEM hParent)
{
	CParticleItem* pParticles = (CParticleItem*)pItem;

	if (pParticles->GetParent())
	{
		if (!hParent || hParent == TVI_ROOT || m_treeCtrl.GetItemData(hParent) == 0)
			return 0;
	}

	HTREEITEM hItem = CBaseLibraryDialog::InsertItemToTree(pItem, hParent);
	if (pItem && hItem)
		pItem->AddRef();
	UpdateItemState(pParticles);

	for (int i = 0; i < pParticles->GetChildCount(); i++)
	{
		CParticleItem* pSubItem = pParticles->GetChild(i);
		InsertItemToTree(pSubItem, hItem);
	}
	return hItem;
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnSelectionChange)
		m_bAutoAssignToSelection = false;
	CBaseLibraryDialog::OnEditorNotifyEvent(event);
}

//////////////////////////////////////////////////////////////////////////
bool CParticleDialog::AddItem(bool bFromParent)
{
	if (!m_pLibrary)
		return false;

	QGroupDialog dlg(QObject::tr("New particle effect name"));

	if (bFromParent && m_pCurrentItem)
	{
		dlg.SetGroup(m_pCurrentItem->GetGroupName());
		dlg.SetString(m_pCurrentItem->GetShortName());
	}
	else
	{
		dlg.SetGroup(m_pCurrentItem ? (bFromParent ? m_pCurrentItem->GetGroupName() : m_pCurrentItem->GetName()) : m_selectedGroup);
	}

	if (dlg.exec() != QDialog::Accepted || dlg.GetString().IsEmpty())
		return false;

	string fullName = m_pItemManager->MakeFullItemName(m_pLibrary, dlg.GetGroup().GetString(), dlg.GetString().GetString());
	if (m_pItemManager->FindItemByName(fullName))
	{
		Warning("Item with name %s already exists", (const char*)fullName);
		return false;
	}

	CUndo undo("Add particle library item");
	CParticleItem* pParticles = (CParticleItem*)m_pItemManager->CreateItem(m_pLibrary);
	if (!dlg.GetGroup().IsEmpty())
	{
		string parentName = m_pLibrary->GetName() + "." + dlg.GetGroup().GetString();
		if (CParticleItem* pParent = (CParticleItem*)m_pPartManager->FindItemByName(parentName))
			pParticles->SetParent(pParent);
	}
	SetItemName(pParticles, dlg.GetGroup().GetString(), dlg.GetString().GetString());
	pParticles->Update();

	ReloadItems();
	SelectItem(pParticles);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::SelectItem(CBaseLibraryItem* item, bool bForceReload)
{
	bool bChanged = item != m_pCurrentItem || bForceReload;
	m_propsCtrl.Flush();
	CBaseLibraryDialog::SelectItem(item, bForceReload);

	if (!item)
	{
		m_propsCtrl.EnableWindow(FALSE);
		return;
	}

	if (!bChanged)
		return;

	if (m_propsCtrl.IsWindowEnabled() != TRUE)
		m_propsCtrl.EnableWindow(TRUE);

	m_propsCtrl.EnableUpdateCallback(false);

	// Update variables.
	m_propsCtrl.EnableUpdateCallback(false);
	pParticleUI->SetFromParticles(GetSelected());
	m_propsCtrl.EnableUpdateCallback(true);

	m_propsCtrl.SetUpdateCallback(functor(*this, &CParticleDialog::OnUpdateProperties));
	m_propsCtrl.EnableUpdateCallback(true);

	if (m_bForceReloadPropsCtrl)
	{
		m_propsCtrl.ReloadItems();
		m_propsCtrl.ShowWindow(SW_HIDE);
		m_propsCtrl.ShowWindow(SW_SHOW);
		m_bForceReloadPropsCtrl = false;
	}

	if (m_bAutoAssignToSelection)
		AssignToSelection();
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::Update()
{
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnUpdateProperties(IVariable* var)
{
	CParticleItem* pParticles = GetSelected();
	if (!pParticles)
		return;

	if (!var || pParticleUI->SetToParticles(pParticles))
		SelectItem(pParticles, true);

	// Update visual cues of item and parents.
	CParticleItem* poEditedParticleItem(pParticles);
	for (; pParticles; pParticles = pParticles->GetParent())
	{
		UpdateItemState(pParticles);
		poEditedParticleItem = pParticles;
	}

	GetIEditorImpl()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnPlay()
{
	m_bRealtimePreviewUpdate = !m_bRealtimePreviewUpdate;
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnUpdatePlay(CCmdUI* pCmdUI)
{
	if (m_bRealtimePreviewUpdate)
		pCmdUI->SetCheck(TRUE);
	else
		pCmdUI->SetCheck(FALSE);
}

//////////////////////////////////////////////////////////////////////////
CParticleItem* CParticleDialog::GetSelected()
{
	CBaseLibraryItem* pItem = m_pCurrentItem;
	return (CParticleItem*)pItem;
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnAssignToSelection()
{
	m_bAutoAssignToSelection = !m_bAutoAssignToSelection;
	if (m_bAutoAssignToSelection)
		AssignToSelection();
}

void CParticleDialog::AssignToSelection()
{
	CParticleItem* pParticles = GetSelected();
	if (!pParticles)
		return;

	CUndo undo("Assign ParticleEffect");

	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	if (!pSel->IsEmpty())
	{
		for (int i = 0; i < pSel->GetCount(); i++)
		{
			AssignToEntity(pParticles, pSel->GetObject(i));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::Enable(bool bAll)
{
	CParticleItem* pParticles = GetSelected();
	if (!pParticles)
		return;

	CUndo undo("Enable/Disable Effect");
	if (bAll)
		pParticles->DebugEnable(!pParticles->GetEnabledState());
	else
		pParticles->GetEffect()->SetEnabled(!(pParticles->GetEnabledState() & 1));

	// Update variables.
	m_propsCtrl.EnableUpdateCallback(false);
	pParticleUI->SetFromParticles(pParticles);
	m_propsCtrl.EnableUpdateCallback(true);

	m_propsCtrl.SetUpdateCallback(functor(*this, &CParticleDialog::OnUpdateProperties));
	m_propsCtrl.EnableUpdateCallback(true);

	UpdateItemState(pParticles, bAll);
	pParticles->Update();
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnSelectAssignedObjects()
{
	//@FIXME
	/*
	   CParticleItem *pParticles = GetSelectedParticle();
	   if (!pParticles)
	   return;

	   CBaseObjectsArray objects;
	   GetIEditorImpl()->GetObjectManager()->GetObjects( objects );
	   for (int i = 0; i < objects.size(); i++)
	   {
	   CBaseObject *pObject = objects[i];
	   if (pObject->GetParticle() != pParticles)
	    continue;
	   if (pObject->IsHidden() || pObject->IsFrozen())
	    continue;
	   GetIEditorImpl()->GetObjectManager()->SelectObject( pObject );
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnReloadParticles()
{
	CUndo undo("Reload Particle");

	CParticleItem* pParticles = GetSelected();
	if (!pParticles)
		return;
	IParticleEffect* pEffect = pParticles->GetEffect();
	if (!pEffect)
		return;

	pEffect->Reload(false);
	pParticleUI->SetFromParticles(pParticles);

	// Update particles.
	pParticles->Update();

	SelectItem(pParticles, true);
	OnUpdateProperties(NULL);
}

void CParticleDialog::OnResetParticles()
{
	CUndo undo("Reset Particle");

	CParticleItem* pParticles = GetSelected();
	if (!pParticles)
		return;

	pParticleUI->ResetParticles(pParticles);

	SelectItem(pParticles, true);
	OnUpdateProperties(NULL);
}

CEntityObject* CParticleDialog::GetItemFromEntity()
{
	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		if (CEntityObject* pEntity = DYNAMIC_DOWNCAST(CEntityObject, pSel->GetObject(i)))
		{
			if (pEntity->GetProperties())
			{
				if (IVariable* pVar = pEntity->GetProperties()->FindVariable("ParticleEffect"))
				{
					string effect = pVar->GetDisplayValue();
					if (CBaseLibraryItem* pItem = (CBaseLibraryItem*)m_pItemManager->LoadItemByName(effect))
					{
						SelectItem(pItem);
						return pEntity;
					}
				}
			}
		}
	}
	return NULL;
}

void CParticleDialog::OnGetFromSelection()
{
	GetItemFromEntity();
	m_bAutoAssignToSelection = false;
}

void CParticleDialog::OnExpandAll()
{
	ExpandCollapseAll(true);
}

void CParticleDialog::OnCollapseAll()
{
	ExpandCollapseAll(false);
}

//////////////////////////////////////////////////////////////////////////
bool CParticleDialog::AssignToEntity(CParticleItem* pItem, CBaseObject* pObject, Vec3 const* pPos)
{
	// Assign ParticleEffect field if it has one.
	// Otherwise, spawn/attach an emitter to the entity
	assert(pItem);
	assert(pObject);
	if (CParticleEffectObject* pEntity = DYNAMIC_DOWNCAST(CParticleEffectObject, pObject))
	{
		string effect = pItem->GetFullName();
		pEntity->AssignEffect(effect);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CParticleDialog::CreateParticleEntity(CParticleItem* pItem, Vec3 const& pos, CBaseObject* pParent)
{
	if (!GetIEditorImpl()->GetDocument()->IsDocumentReady())
		return nullptr;

	GetIEditorImpl()->ClearSelection();
	CBaseObject* pObject = GetIEditorImpl()->NewObject("ParticleEntity");
	if (pObject)
	{
		// Set pos, offset by object size.
		AABB box;
		pObject->GetLocalBounds(box);
		pObject->SetPos(pos - Vec3(0, 0, box.min.z));
		pObject->SetRotation(Quat::CreateRotationXYZ(Ang3(DEG2RAD(90), 0, 0)));

		if (pParent)
			pParent->AttachChild(pObject);
		AssignToEntity(pItem, pObject);
		GetIEditorImpl()->SelectObject(pObject);
	}
	return pObject;
}

void CParticleDialog::UpdateItemState(CParticleItem* pItem, bool bRecursive)
{
	HTREEITEM hItem = stl::find_in_map(m_itemsToTree, pItem, (HTREEITEM)0);
	if (hItem)
	{
		// Swap icon set, depending on self & child activation.
		static int nIconStates[4] = { 6, 7, 4, 5 };
		int nIconState = nIconStates[pItem->GetEnabledState() & 3];
		m_treeCtrl.SetItemImage(hItem, nIconState, nIconState);
	}
	if (bRecursive)
		for (int i = 0; i < pItem->GetChildCount(); i++)
			UpdateItemState(pItem->GetChild(i), true);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnUpdateAssignToSelection(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetSelected() && !GetIEditorImpl()->GetSelection()->IsEmpty());
	pCmdUI->SetRadio(m_bAutoAssignToSelection);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnUpdateObjectSelected(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(!GetIEditorImpl()->GetSelection()->IsEmpty());
}

void CParticleDialog::OnKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTVKEYDOWN* nm = (NMTVKEYDOWN*)pNMHDR;
	if (GetAsyncKeyState(VK_CONTROL))
	{
		if (nm->wVKey == VK_SPACE)
			Enable(GetAsyncKeyState(VK_SHIFT));
	}

	CBaseLibraryDialog::OnKeyDownItemTree(pNMHDR, pResult);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

	HTREEITEM hItem = pNMTreeView->itemNew.hItem;

	CParticleItem* pParticles = (CParticleItem*)m_treeCtrl.GetItemData(hItem);
	if (!pParticles)
		return;

	m_pDraggedItem = pParticles;

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
void CParticleDialog::OnMouseMove(UINT nFlags, CPoint point)
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

		SetCursor(m_hCursorDefault);
		// Check if can drop here.
		{
			CPoint p;
			GetCursorPos(&p);
			CViewport* viewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint(p);
			if (viewport)
			{
				CPoint vp = p;
				viewport->ScreenToClient(&vp);
				HitContext hit;
				if (viewport->HitTest(vp, hit))
				{
					if (hit.object && DYNAMIC_DOWNCAST(CEntityObject, hit.object))
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
void CParticleDialog::OnLButtonUp(UINT nFlags, CPoint point)
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

		CPoint treepoint = p;
		m_treeCtrl.ScreenToClient(&treepoint);

		CRect treeRc;
		m_treeCtrl.GetClientRect(treeRc);

		if (treeRc.PtInRect(treepoint))
		{
			// Dropped inside tree.
			TVHITTESTINFO hit;
			ZeroStruct(hit);
			hit.pt = treepoint;
			HTREEITEM hHitItem = m_treeCtrl.HitTest(&hit);
			if (hHitItem)
			{
				DropToItem(hHitItem, m_hDraggedItem, m_pDraggedItem);
				m_hDraggedItem = 0;
				m_pDraggedItem = 0;
				return;
			}
			DropToItem(0, m_hDraggedItem, m_pDraggedItem);
		}
		else
		{
			// Not dropped inside tree.

			CWnd* wnd = WindowFromPoint(p);

			CUndo undo("Assign ParticleEffect");

			CViewport* viewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint(p);
			if (viewport)
			{
				CPoint vp = p;
				viewport->ScreenToClient(&vp);
				CParticleItem* pParticles = m_pDraggedItem;

				// Drag and drop into one of views.
				HitContext hit;
				if (viewport->HitTest(vp, hit))
				{
					Vec3 hitpos = hit.raySrc + hit.rayDir * hit.dist;
					if (hit.object && AssignToEntity(pParticles, hit.object, &hitpos))
						; // done
					else
					{
						// Place emitter at hit location.
						hitpos = viewport->SnapToGrid(hitpos);
						CreateParticleEntity(pParticles, hitpos);
					}
				}
				else
				{
					// Snap to terrain.
					bool hitTerrain;
					Vec3 pos = viewport->ViewToWorld(vp, &hitTerrain);
					if (hitTerrain)
					{
						pos.z = GetIEditorImpl()->GetTerrainElevation(pos.x, pos.y);
					}
					pos = viewport->SnapToGrid(pos);
					CreateParticleEntity(pParticles, pos);
				}
			}
		}
		m_pDraggedItem = 0;
	}
	m_pDraggedItem = 0;
	m_hDraggedItem = 0;

	CBaseLibraryDialog::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnNotifyTreeRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	// Show helper menu.
	CPoint point;

	CParticleItem* pParticles = 0;

	// Find node under mouse.
	GetCursorPos(&point);
	m_treeCtrl.ScreenToClient(&point);
	// Select the item that is at the point myPoint.
	UINT uFlags;
	HTREEITEM hItem = m_treeCtrl.HitTest(point, &uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pParticles = (CParticleItem*)m_treeCtrl.GetItemData(hItem);
	}

	if (!pParticles)
	{
		if (m_treeCtrl.GetRootItem())
		{
			CMenu expandMenu;
			expandMenu.CreatePopupMenu();

			expandMenu.AppendMenu(MF_STRING, ID_DB_EXPAND_ALL, "Expand all");
			expandMenu.AppendMenu(MF_STRING, ID_DB_COLLAPSE_ALL, "Collapse all");

			GetCursorPos(&point);
			expandMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
		}
		return;
	}

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
		menu.AppendMenu(MF_STRING, ID_DB_COPY_PATH, "Copy Path");
		menu.AppendMenu(MF_SEPARATOR, 0, "");
		menu.AppendMenu(MF_STRING, ID_DB_RENAME, "Rename");
		menu.AppendMenu(MF_STRING, ID_DB_REMOVE, "Delete");
		menu.AppendMenu(MF_STRING, IDC_RESET, "Reset");
		menu.AppendMenu(MF_STRING, ID_DB_ACTIVATE, pParticles->GetEnabledState() & 1 ? "Disable (Ctrl-Space)" : "Enable (Ctrl-Space)");
		menu.AppendMenu(MF_STRING, ID_DB_ACTIVATE_ALL, pParticles->GetEnabledState() ? "Disable All (Ctrl-Shift-Space)" : "Enable All (Ctrl-Shift-Space)");
		menu.AppendMenu(MF_SEPARATOR, 0, "");
		menu.AppendMenu(MF_STRING, ID_DB_ASSIGNTOSELECTION, "Assign to Selected Objects");
	}

	GetCursorPos(&point);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
}

void CParticleDialog::OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	CBaseLibraryDialog::OnSelChangedItemTree(pNMHDR, pResult);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnPick()
{
	if (!CParticlePickCallback::IsActive())
		GetIEditorImpl()->PickObject(new CParticlePickCallback);
	else
		GetIEditorImpl()->CancelPick();
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnUpdatePick(CCmdUI* pCmdUI)
{
	if (CParticlePickCallback::IsActive())
	{
		pCmdUI->SetCheck(1);
	}
	else
	{
		pCmdUI->SetCheck(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnCopy()
{
	CParticleItem* pParticles = GetSelected();
	if (pParticles)
	{
		XmlNodeRef node = XmlHelpers::CreateXmlNode("Particles");
		CBaseLibraryItem::SerializeContext ctx(node, false);
		ctx.bCopyPaste = true;

		CClipboard clipboard;
		pParticles->Serialize(ctx);
		clipboard.Put(node);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnPaste()
{
	if (!m_pLibrary)
		return;

	CParticleItem* pItem = GetSelected();
	if (!pItem)
		return;

	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return;
	XmlNodeRef node = clipboard.Get();
	if (!node)
		return;

	if (strcmp(node->getTag(), "Particles") == 0)
	{
		CUndo undo("Add particle library item");
		node->delAttr("Name");

		m_pPartManager->PasteToParticleItem(pItem, node, true);
		ReloadItems();
		SelectItem(pItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnClone()
{
	if (!m_pLibrary || !m_pCurrentItem)
		return;

	OnCopy();
	if (AddItem(true))
		OnPaste();
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::DropToItem(HTREEITEM hItem, HTREEITEM hSrcItem, CParticleItem* pParticles)
{
	pParticles->GetLibrary()->SetModified();

	TSmartPtr<CParticleItem> pHolder = pParticles; // Make usre its not release while we remove and add it back.

	if (!hItem)
	{
		// Detach from parent.
		pParticles->SetParent(NULL);

		ReloadItems();
		SelectItem(pParticles);
		return;
	}

	CParticleItem* pTargetItem = (CParticleItem*)m_treeCtrl.GetItemData(hItem);
	if (!pTargetItem)
	{
		// This is group.

		// Detach from parent.
		pParticles->SetParent(NULL);

		// Move item to different group.
		string groupName = GetItemFullName(hItem, ".");
		SetItemName(pParticles, groupName, pParticles->GetShortName());

		DeleteTreeItem(hSrcItem);
		InsertItemToTree(pParticles, hItem);
		return;
	}

	// Ignore itself.
	if (pTargetItem == pParticles)
		return;

	// Move to new parent.
	pParticles->SetParent(pTargetItem);

	ReloadItems();
	SelectItem(pParticles);
}

void CParticleDialog::ReleasePreviewControl()
{
	m_previewCtrl.ReleaseObject();
	m_previewCtrl.Update(true);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::DeleteAllTreeItems()
{
	HTREEITEM hVisibleItem(GetTreeCtrl().GetFirstVisibleItem());

	while (hVisibleItem)
	{
		CParticleItem* pItem = (CParticleItem*)GetTreeCtrl().GetItemData(hVisibleItem);
		if (pItem)
			pItem->Release();
		hVisibleItem = GetTreeCtrl().GetNextVisibleItem(hVisibleItem);
	}

	__super::DeleteAllTreeItems();
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::DeleteTreeItem(HTREEITEM hItem)
{
	if (hItem == NULL)
		return;
	CParticleItem* pItem = (CParticleItem*)GetTreeCtrl().GetItemData(hItem);
	if (pItem)
		pItem->Release();
	GetTreeCtrl().DeleteItem(hItem);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::ExpandCollapseAll(bool expand)
{
	HTREEITEM hItem = m_treeCtrl.GetNextItem(NULL, TVGN_ROOT);
	while (hItem)
	{
		UINT treeAction = expand ? TVE_EXPAND : TVE_COLLAPSE;
		m_treeCtrl.Expand(hItem, treeAction);
		hItem = m_treeCtrl.GetNextItem(hItem, TVGN_NEXT);
	}
}

//////////////////////////////////////////////////////////////////////////
CParticleDialog* CParticleDialog::GetCurrentInstance()
{
	return s_poCurrentInstance;
}

//////////////////////////////////////////////////////////////////////////
dll_string ParticleResourceSelector(const SResourceSelectorContext& x, const char* szPreviousValue)
{
	if (x.useLegacyPicker)
	{
		std::vector<CString> items;
		const char* openLibraryText = "[ Open Particle Database (might crash with empty DB!) ]";
		items.push_back(openLibraryText);
		{
			CParticleManager* pParticleManager = GetIEditorImpl()->GetParticleManager();

			IDataBaseItemEnumerator* pEnumerator = pParticleManager->GetItemEnumerator();
			for (IDataBaseItem* pItem = pEnumerator->GetFirst(); pItem; pItem = pEnumerator->GetNext())
			{
				items.push_back(CString(pItem->GetFullName()));
			}
			pEnumerator->Release();
		}

		CRY_ASSERT(x.parentWidget);
		CGenericSelectItemDialog dialog(CWnd::FromHandle((HWND)x.parentWidget->winId()));
		dialog.SetMode(CGenericSelectItemDialog::eMODE_TREE);
		dialog.SetItems(items);
		dialog.SetTreeSeparator(".");

		dialog.PreSelectItem(szPreviousValue);
		if (dialog.DoModal() == IDOK)
		{
			if (dialog.GetSelectedItem() == openLibraryText)
			{
				GetIEditorImpl()->OpenDataBaseLibrary(EDB_TYPE_PARTICLE);
			}
			else
			{
				return dialog.GetSelectedItem().GetBuffer();
			}
		}
	}
	else
	{
		QString relativeFilename(szPreviousValue);

		CEngineFileDialog::OpenParams dialogParams(CEngineFileDialog::OpenFile);
		dialogParams.initialDir = QtUtil::ToQString(PathUtil::GetPathWithoutFilename(relativeFilename.toLocal8Bit()));
		if (!relativeFilename.isEmpty())
		{
			dialogParams.initialFile = szPreviousValue;
		}

		dialogParams.extensionFilters = CExtensionFilter::Parse("Particle Files (*.pfx)|*.pfx||");
		CEngineFileDialog fileDialog(dialogParams);
		if (fileDialog.exec() == QDialog::Accepted)
		{
			auto files = fileDialog.GetSelectedFiles();
			CRY_ASSERT(!files.empty());
			return files.front().toLocal8Bit().constData();
		}
	}
	return szPreviousValue;
}

dll_string ParticleAssetSelector(const SResourceSelectorContext& context, const char* szPreviousValue)
{
	return SStaticAssetSelectorEntry::SelectFromAsset(context, { "Particles" }, szPreviousValue);
}

dll_string ParticleSelector(const SResourceSelectorContext& context, const char* szPreviousValue)
{
	const auto pickerState = (EAssetResourcePickerState)GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ed_enableAssetPickers")->GetIVal();
	if (!context.useLegacyPicker && pickerState == EAssetResourcePickerState::EnableRecommended || pickerState == EAssetResourcePickerState::EnableAll)
	{
		return ParticleAssetSelector(context, szPreviousValue);
	}
	else
	{
		return ParticleResourceSelector(context, szPreviousValue);
	}
}

dll_string ValidateParticlePath(const SResourceSelectorContext& context, const char* szNewValue, const char* szPreviousValue)
{
	if (!szNewValue || !*szNewValue)
		return dll_string();

	QString newPath(szNewValue);
	if (newPath.indexOf(".pfx") != -1)
	{
		const FileSystem::SnapshotPtr& snapshot = GetIEditorImpl()->GetFileSystemEnumerator()->GetCurrentSnapshot();

		if (!snapshot->GetFileByEnginePath(newPath))
		{
			if (!snapshot->GetFileByEnginePath(PathUtil::GetGameFolder() + "/" + QString(newPath)))
				return szPreviousValue;
		}
	}
	else
	{
		CParticleManager* pParticleManager = GetIEditorImpl()->GetParticleManager();
		IDataBaseItem* pDBItem = pParticleManager->FindItemByName(szPreviousValue);

		if (!pDBItem)
			return szPreviousValue;
	}

	return szNewValue;
}

void EditParticle(const SResourceSelectorContext& context, const char* szAssetPath)
{
	QString path(szAssetPath);
	if (path.indexOf(".pfx") == -1)
	{
		IDataBaseManager* pDBManager = GetIEditorImpl()->GetParticleManager();

		if (!pDBManager)
			return;

		IDataBaseItem* pDBItem = pDBManager->FindItemByName(szAssetPath);
		if (!pDBItem)
			return;

		GetIEditorImpl()->OpenDataBaseLibrary(EDB_TYPE_PARTICLE, pDBItem);
	}
	else
	{
		GetIEditorImpl()->ExecuteCommand(QString("particle.show_effect '%1'").arg(szAssetPath).toLocal8Bit());
	}
}

REGISTER_RESOURCE_EDITING_SELECTOR_WITH_LEGACY_SUPPORT("ParticleLegacy", ParticleSelector, ValidateParticlePath, EditParticle, "")
REGISTER_RESOURCE_EDITING_SELECTOR("Particle", ParticleSelector, ValidateParticlePath, EditParticle, "")

