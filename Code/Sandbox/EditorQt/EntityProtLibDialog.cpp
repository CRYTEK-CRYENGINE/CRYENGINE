// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityProtLibDialog.h"

#include "Objects\EntityScript.h"
#include "Objects\ProtEntityObject.h"
#include "Dialogs/QGroupDialog.h"
#include "Util/MFCUtil.h"
#include "EntityPrototypeManager.h"
#include "EntityPrototypeLibrary.h"
#include "EntityPrototype.h"
#include "Objects/EntityObject.h"
#include "SelectEntityClsDialog.h"
#include "Util/Clipboard.h"
#include "ViewManager.h"

#include <CryEntitySystem/IEntitySystem.h>

#include "Material/MaterialManager.h"
#include "Controls/MFCPropertyTree.h"

#include "Controls/QuestionDialog.h"

#define IDC_PROTOTYPES_TREE     AFX_IDW_PANE_FIRST
#define IDC_DESCRIPTION_EDITBOX AFX_IDW_PANE_FIRST + 1

#if (_WIN32_WINNT < 0x0600)

	#define TVM_SETEXTENDEDSTYLE (TV_FIRST + 44)
	#define TreeView_SetExtendedStyle(hwnd, dw, mask) \
	  (DWORD)SNDMSG((hwnd), TVM_SETEXTENDEDSTYLE, mask, dw)

	#define TVM_GETEXTENDEDSTYLE (TV_FIRST + 45)
	#define TreeView_GetExtendedStyle(hwnd) \
	  (DWORD)SNDMSG((hwnd), TVM_GETEXTENDEDSTYLE, 0, 0)

	#define TVS_EX_MULTISELECT         0x0002
	#define TVS_EX_DOUBLEBUFFER        0x0004
	#define TVS_EX_NOINDENTSTATE       0x0008
	#define TVS_EX_RICHTOOLTIP         0x0010
	#define TVS_EX_AUTOHSCROLL         0x0020
	#define TVS_EX_FADEINOUTEXPANDOS   0x0040
	#define TVS_EX_PARTIALCHECKBOXES   0x0080
	#define TVS_EX_EXCLUSIONCHECKBOXES 0x0100
	#define TVS_EX_DIMMEDCHECKBOXES    0x0200
	#define TVS_EX_DRAWIMAGEASYNC      0x0400

#endif

IMPLEMENT_DYNAMIC(CEntityProtLibDialog, CBaseLibraryDialog);
//////////////////////////////////////////////////////////////////////////
// CEntityProtLibDialog implementation.
//////////////////////////////////////////////////////////////////////////
CEntityProtLibDialog::CEntityProtLibDialog(CWnd* pParent)
	: CBaseLibraryDialog(IDD_DB_ENTITY, pParent)
{
	static bool bRegisterMFCPropertyTreeClass = true;
	if (bRegisterMFCPropertyTreeClass)
	{
		CMFCPropertyTree::RegisterWindowClass();
		bRegisterMFCPropertyTreeClass = false;
	}

	m_entity = 0;
	m_pEntityManager = GetIEditorImpl()->GetEntityProtManager();
	m_pItemManager = m_pEntityManager;
	m_bEntityPlaying = false;
	m_bShowDescription = false;
	m_pArchetypeExtensionPropsCtrl = nullptr;

	// Sort items using full recursion.
	m_sortRecursionType = SORT_RECURSION_FULL;

	// Immidiatly create dialog.
	Create(IDD_DATABASE, pParent);
}

CEntityProtLibDialog::~CEntityProtLibDialog()
{
	SAFE_DELETE(m_pArchetypeExtensionPropsCtrl);
}

void CEntityProtLibDialog::DoDataExchange(CDataExchange* pDX)
{
	CBaseLibraryDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CEntityProtLibDialog, CBaseLibraryDialog)
ON_COMMAND(ID_DB_ADD, OnAddPrototype)
ON_COMMAND(ID_DB_SAVE, OnSave)
ON_COMMAND(ID_DB_PLAY, OnPlay)
ON_COMMAND(ID_DB_LOADLIB, OnLoadLibrary)
ON_COMMAND(ID_DB_RELOAD, OnReloadEntityScript)
ON_COMMAND(ID_DB_DESCRIPTION, OnShowDescription)
ON_UPDATE_COMMAND_UI(ID_DB_PLAY, OnUpdatePlay)
ON_COMMAND(ID_DB_ASSIGNTOSELECTION, OnAssignToSelection)
ON_UPDATE_COMMAND_UI(ID_DB_ASSIGNTOSELECTION, OnUpdateSelected)
//ON_EN_CHANGE( IDC_DESCRIPTION,OnDescriptionChange )
ON_EN_CHANGE(IDC_DESCRIPTION_EDITBOX, OnDescriptionChange)    // Second plane

ON_COMMAND(ID_DB_SELECTASSIGNEDOBJECTS, OnSelectAssignedObjects)
ON_UPDATE_COMMAND_UI(ID_DB_SELECTASSIGNEDOBJECTS, OnUpdateSelected)

ON_NOTIFY(TVN_BEGINDRAG, IDC_PROTOTYPES_TREE, OnBeginDrag)
ON_NOTIFY(NM_RCLICK, IDC_PROTOTYPES_TREE, OnNotifyTreeRClick)

ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
ON_WM_SIZE()
ON_WM_DESTROY()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnDestroy()
{
	CBaseLibraryDialog::OnDestroy();
}

// CTVSelectKeyDialog message handlers
BOOL CEntityProtLibDialog::OnInitDialog()
{
	__super::OnInitDialog();

	m_pEntitySystem = GetIEditorImpl()->GetSystem()->GetIEntitySystem();

	InitToolbar(IDR_DB_LIBRARY_ITEM_BAR);

	CRect rc;
	GetClientRect(rc);

	// Create left panel tree control.
	m_treeCtrl.Create(WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_BORDER | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | TVS_LINESATROOT | TVS_HASLINES |
	                  TVS_FULLROWSELECT | TVS_NOHSCROLL | TVS_INFOTIP | TVS_TRACKSELECT, rc, this, IDC_LIBRARY_ITEMS_TREE);

	TreeView_SetExtendedStyle(m_treeCtrl.m_hWnd, TVS_EX_DOUBLEBUFFER | TVS_EX_NOINDENTSTATE, 0);

	int h2 = 30;

	m_wndVSplitter.CreateStatic(this, 1, 2, WS_CHILD | WS_VISIBLE);
	m_wndHSplitter.CreateStatic(&m_wndVSplitter, 2, 1, WS_CHILD | WS_VISIBLE);
	m_wndScriptPreviewSplitter.CreateStatic(&m_wndHSplitter, 1, 2, WS_CHILD | WS_VISIBLE);
	m_wndSplitter4.CreateStatic(this, 2, 1, WS_CHILD | WS_VISIBLE);
	m_wndPropsHSplitter.CreateStatic(&m_wndScriptPreviewSplitter, 2, 1, WS_CHILD | WS_VISIBLE);

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_ENTITY_TREE, 16, RGB(255, 0, 255));

	CRect scriptDlgRc = CRect(0, 0, 400, 300);

	// Attach it to the control
	m_treeCtrl.SetParent(&m_wndVSplitter);
	m_treeCtrl.SetImageList(&m_imageList, TVSIL_NORMAL);

	m_previewCtrl.Create(&m_wndScriptPreviewSplitter, rc, WS_CHILD | WS_VISIBLE);

	m_propsCtrl.Create(WS_VISIBLE | WS_CHILD | WS_BORDER, rc, &m_wndHSplitter, 2);
	m_objectPropsCtrl.Create(WS_VISIBLE | WS_CHILD | WS_BORDER, rc, this);

	m_pArchetypeExtensionPropsCtrl = new CMFCPropertyTree();
	m_pArchetypeExtensionPropsCtrl->Create(&m_wndPropsHSplitter, CRect(0, 0, 400, 100));
	m_pArchetypeExtensionPropsCtrl->SetCompact(true);
	m_pArchetypeExtensionPropsCtrl->SetHideSelection(true);
	m_pArchetypeExtensionPropsCtrl->SetExpandLevels(1);
	m_pArchetypeExtensionPropsCtrl->SetPropertyChangeHandler(functor(*this, &CEntityProtLibDialog::OnArchetypeExtensionPropsChanged));

	m_wndCaptionEntityClass.Create(this, _T(""), CPWS_EX_RAISED_EDGE, WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, CRect(0, 0, 0, 0));

	m_wndCaptionEntityClass.SetCaptionColors(
	  GetXtremeColor(COLOR_3DFACE),    // border color.
	  GetXtremeColor(COLOR_3DSHADOW),  // background color.
	  GetXtremeColor(COLOR_WINDOW));   // font color.
	m_wndCaptionEntityClass.ModifyCaptionStyle(
	  4,                     // border size in pixels.
	  &XTAuxData().fontBold, // caption font.
	  NULL,                  // caption text.
	  NULL);                 // caption icon.

	m_wndSplitter4.SetPane(0, 0, &m_objectPropsCtrl, CSize(100, 300));
	m_wndSplitter4.SetPane(1, 0, &m_previewCtrl, CSize(100, 100));

	if (m_pEntitySystem->GetEntityArchetypeManagerExtension())
	{
		m_wndPropsHSplitter.SetPane(0, 0, &m_propsCtrl, CSize(scriptDlgRc.Width(), scriptDlgRc.Height() - 10));
		m_wndPropsHSplitter.SetPane(1, 0, m_pArchetypeExtensionPropsCtrl, CSize(scriptDlgRc.Width(), 10));

		m_wndScriptPreviewSplitter.SetPane(0, 0, &m_wndPropsHSplitter, CSize(scriptDlgRc.Width() + 4, scriptDlgRc.Height() + 4));
	}
	else
	{
		m_wndScriptPreviewSplitter.SetPane(0, 0, &m_propsCtrl, CSize(scriptDlgRc.Width() + 4, scriptDlgRc.Height() + 4));
	}

	m_wndScriptPreviewSplitter.SetPane(0, 1, &m_wndSplitter4, CSize(200, 100));
	m_bShowDescription = false;

	m_wndHSplitter.SetNoBorders();
	m_wndHSplitter.SetTrackable(false);
	m_wndHSplitter.SetPane(0, 0, &m_wndCaptionEntityClass, CSize(100, h2));
	m_wndHSplitter.SetPane(1, 0, &m_wndScriptPreviewSplitter, CSize(100, h2));

	m_wndVSplitter.SetPane(0, 0, &m_treeCtrl, CSize(200, 100));
	m_wndVSplitter.SetPane(0, 1, &m_wndHSplitter, CSize(200, 100));

	RecalcLayout();

	ReloadLibs();
	ReloadItems();

	m_wndVSplitter.RecalcLayout();
	m_wndHSplitter.RecalcLayout();
	m_wndScriptPreviewSplitter.RecalcLayout();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
UINT CEntityProtLibDialog::GetDialogMenuID()
{
	return IDR_DB_ENTITY;
};

//////////////////////////////////////////////////////////////////////////
// Create the toolbar
void CEntityProtLibDialog::InitToolbar(UINT nToolbarResID)
{
	InitLibraryToolbar();
	InitItemToolbar();
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnSize(UINT nType, int cx, int cy)
{
	CBaseLibraryDialog::OnSize(nType, cx, cy);

	__super::OnSize(nType, cx, cy);

	// resize splitter window.
	if (m_wndVSplitter.m_hWnd)
	{
		m_wndVSplitter.MoveWindow(CRect(0, 0, cx, cy), FALSE);
	}
	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnAddPrototype()
{
	if (!m_pLibrary)
		return;
	string library = m_selectedLib;
	if (library.IsEmpty())
		return;
	string entityClass = SelectEntityClass();
	if (entityClass.IsEmpty())
		return;

	QGroupDialog dlg(QObject::tr("New Entity Name"));
	dlg.SetGroup(m_selectedGroup);
	dlg.SetString(entityClass);
	if (dlg.exec() != QDialog::Accepted || dlg.GetString().IsEmpty())
	{
		return;
	}

	CUndo undo("Add entity prototype library item");
	CEntityPrototype* prototype = static_cast<CEntityPrototype*>(m_pEntityManager->CreateItem(m_pLibrary));

	// Make prototype name.
	if (!SetItemName(prototype, dlg.GetGroup().GetString(), dlg.GetString().GetString()))
	{
		m_pEntityManager->DeleteItem(prototype);
		return;
	}
	// Assign entity class to prototype.
	prototype->SetEntityClassName(entityClass);

	ReloadItems();
	SelectItem(prototype);
}

//////////////////////////////////////////////////////////////////////////
string CEntityProtLibDialog::SelectEntityClass()
{
	CSelectEntityClsDialog dlg;
	if (dlg.DoModal() == IDOK)
	{
		return dlg.GetEntityClass();
	}
	return "";
}

bool CEntityProtLibDialog::MarkOrUnmarkPropertyField(IVariable* varArchetype, IVariable* varEntScript)
{
	bool bUpdated = false;

	if (varArchetype && varEntScript && varArchetype->GetName() == varEntScript->GetName())
	{
		// Determine if flags or name needs to change
		string strArchetypeValue = varArchetype->GetDisplayValue();
		string strEntScriptValue = varEntScript->GetDisplayValue();
		int nOldFlags = varArchetype->GetFlags();
		int nNewFlags = nOldFlags;
		if (strArchetypeValue == strEntScriptValue)
		{
			nNewFlags &= ~IVariable::UI_BOLD;
		}
		else
		{
			nNewFlags |= IVariable::UI_BOLD;
		}

		// Update anything that changed
		if (nOldFlags != nNewFlags)
		{
			varArchetype->SetFlags(nNewFlags);
			bUpdated = true;
		}

		// Set/Clear the script default shown in conjunction with the tooltip.
		CPropertyItem* pItem = m_propsCtrl.FindItemByVar(varArchetype);
		ASSERT(pItem);
		if (pItem)
		{
			// Clear script default if property matches; set it otherwise.
			if (strArchetypeValue == strEntScriptValue)
				pItem->ClearScriptDefault();
			else
				pItem->SetScriptDefault(strEntScriptValue);
		}
	}

	return bUpdated;
}

void CEntityProtLibDialog::MarkFieldsThatDifferFromScriptDefaults(IVariable* varArchetype, IVariable* varEntScript)
{
	if (varArchetype && varEntScript && varArchetype->GetName() == varEntScript->GetName())
	{
		if (varArchetype->GetNumVariables() > 0 && varEntScript->GetNumVariables() > 0)
		{
			for (int i = 0; i < varArchetype->GetNumVariables(); i++)
			{
				IVariable* varArchetype2 = varArchetype->GetVariable(i);
				string strNameArchetype2 = varArchetype2 ? varArchetype2->GetName() : "{unknown}";
				IVariable* varEntScript2 = NULL;
				string strNameEntScript2 = "<unknown>";
				for (int j = 0; strNameArchetype2 != strNameEntScript2 && j < varEntScript->GetNumVariables(); j++)
				{
					varEntScript2 = varEntScript->GetVariable(j);
					strNameEntScript2 = varEntScript2 ? varEntScript2->GetName() : "<unknown>";
				}
				MarkFieldsThatDifferFromScriptDefaults(varArchetype2, varEntScript2);
			}
		}
		else if (varArchetype->GetNumVariables() == 0 && varEntScript->GetNumVariables() == 0)
		{
			MarkOrUnmarkPropertyField(varArchetype, varEntScript);
		}
	}
}

void CEntityProtLibDialog::MarkFieldsThatDifferFromScriptDefaults(CVarBlock* pArchetypeVarBlock, CVarBlock* pEntScriptVarBlock)
{
	if (pArchetypeVarBlock && pEntScriptVarBlock)
	{
		for (int i = 0; i < pArchetypeVarBlock->GetNumVariables(); i++)
		{
			IVariable* varArchetype = pArchetypeVarBlock->GetVariable(i);
			IVariable* varEntScript = pEntScriptVarBlock->FindVariable(varArchetype ? varArchetype->GetName() : "");
			MarkFieldsThatDifferFromScriptDefaults(varArchetype, varEntScript);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
class CArchetypeExtensionPropsWrapper
{
public:
	CArchetypeExtensionPropsWrapper()
		: m_pArchetype()
	{}

	void SetArchetype(IEntityArchetype* pArchetype)
	{
		m_pArchetype = pArchetype;
	}

	void Serialize(Serialization::IArchive& archive)
	{
		if (m_pArchetype)
		{
			if (IEntityArchetypeManagerExtension* pExtension = gEnv->pEntitySystem->GetEntityArchetypeManagerExtension())
			{
				pExtension->Serialize(*m_pArchetype, archive);
			}
		}
	}

private:
	IEntityArchetype* m_pArchetype;
};

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::SelectItem(CBaseLibraryItem* item, bool bForceReload)
{
	bool bChanged = item != m_pCurrentItem || bForceReload;
	CBaseLibraryDialog::SelectItem(item, bForceReload);

	if (!bChanged)
		return;

	CEntityPrototype* prototype = (CEntityPrototype*)item;

	// Empty preview control.
	m_previewCtrl.SetEntity(0);
	m_previewCtrl.LoadFile("", false);

	m_wndCaptionEntityClass.UpdateCaption("", NULL);
	m_propsCtrl.DeleteAllItems();
	m_objectPropsCtrl.DeleteAllItems();

	string scriptName;
	if (prototype && prototype->GetScript())
	{
		scriptName = prototype->GetScript()->GetName();
		string caption = scriptName + "   :   " + item->GetFullName();
		if (m_wndCaptionEntityClass)
			m_wndCaptionEntityClass.UpdateCaption(caption, NULL);
	}

	if (prototype && prototype->GetProperties())
	{
		m_propsCtrl.AddVarBlock(prototype->GetProperties());
		m_propsCtrl.SetRootName("Class Properties");
		m_propsCtrl.ExpandAll();
		m_propsCtrl.SetUpdateCallback(functor(*this, &CEntityProtLibDialog::OnUpdateProperties));

		CVarBlock* pArchetypeVarBlock = prototype->GetProperties();
		CVarBlock* pEntScriptVarBlock = prototype->GetScript() ? prototype->GetScript()->GetDefaultProperties() : NULL;
		MarkFieldsThatDifferFromScriptDefaults(pArchetypeVarBlock, pEntScriptVarBlock);

		m_objectPropsCtrl.AddVarBlock(prototype->GetObjectVarBlock());
		m_objectPropsCtrl.SetRootName("Object Params");
		m_objectPropsCtrl.ExpandAll();
		m_objectPropsCtrl.SetUpdateCallback(functor(*this, &CEntityProtLibDialog::OnUpdateProperties));
	}

	if (prototype)
	{
		IEntityArchetype* pArchetype = prototype->GetIEntityArchetype();
		if (gEnv->pEntitySystem->GetEntityArchetypeManagerExtension())
		{
			static CArchetypeExtensionPropsWrapper wrapper;
			wrapper.SetArchetype(pArchetype);

			m_pArchetypeExtensionPropsCtrl->Attach(Serialization::SStruct(wrapper));
		}
		else
		{
			m_pArchetypeExtensionPropsCtrl->Detach();
		}
	}

	UpdatePreview();
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::UpdatePreview()
{
	CEntityPrototype* prototype = GetSelectedPrototype();
	if (!prototype)
		return;

	CEntityScript* script = prototype->GetScript();
	if (!script)
		return;
	// Load script if its not loaded yet.
	if (!script->IsValid())
	{
		if (!script->Load())
			return;
	}

	//////////////////////////////////////////////////////////////////////////
	// Make visual object for this entity.
	//////////////////////////////////////////////////////////////////////////
	m_visualObject = "";
	m_PrototypeMaterial = "";

	CVarBlock* pObjectVarBlock = prototype->GetProperties();
	if (pObjectVarBlock)
	{
		IVariable* pPrototypeMaterial = pObjectVarBlock->FindVariable("PrototypeMaterial");
		if (pPrototypeMaterial)
			pPrototypeMaterial->Get(m_PrototypeMaterial);
	}

	string visualObjectPath = "";

	CVarBlock* pProperties = prototype->GetProperties();
	if (pProperties)
	{
		IVariable* pVar = pProperties->FindVariable("object_Model", true);
		if (pVar)
			pVar->Get(visualObjectPath);

		if (visualObjectPath.IsEmpty())
		{
			pVar = pProperties->FindVariable("fileModel", true);
			if (pVar)
				pVar->Get(visualObjectPath);
		}
		if (visualObjectPath.IsEmpty())
		{
			pVar = pProperties->FindVariable("objModel", true);
			if (pVar)
				pVar->Get(visualObjectPath);
		}
	}

	if (m_visualObject.IsEmpty())
	{
		m_visualObject = script->GetClass()->GetEditorClassInfo().sHelper;
	}

	if (!m_visualObject.IsEmpty() && m_visualObject != m_previewCtrl.GetLoadedFile())
	{
		m_previewCtrl.LoadFile(m_visualObject, false);
		m_previewCtrl.FitToScreen();
	}

	if (!m_PrototypeMaterial.IsEmpty())
	{
		CMaterial* poPrototypeMaterial = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(m_PrototypeMaterial);
		m_previewCtrl.SetMaterial(poPrototypeMaterial);
	}

	m_previewCtrl.SetGrid(true);
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::SpawnEntity(CEntityPrototype* prototype)
{
	assert(prototype);

	ReleaseEntity();

	CEntityScript* script = prototype->GetScript();
	if (!script)
		return;
	// Load script if its not loaded yet.
	if (!script->IsValid())
	{
		if (!script->Load())
			return;
	}

	/*
	   CEntityDesc ed;
	   ed.ClassId = script->GetClsId();
	   ed.name = prototype->GetName();

	   m_entity = m_pEntitySystem->SpawnEntity( ed,false );
	   if (m_entity)
	   {
	   if (prototype->GetProperties())
	   {
	    // Assign to entity properties of prototype.
	    script->SetProperties( m_entity,prototype->GetProperties(),false );
	   }
	   // Initialize properties.
	   if (!m_pEntitySystem->InitEntity( m_entity,ed ))
	    m_entity = 0;

	   //////////////////////////////////////////////////////////////////////////
	   // Make visual object for this entity.
	   //////////////////////////////////////////////////////////////////////////
	   m_visualObject = script->GetVisualObject();

	   m_scriptDialog.SetScript( script,m_entity );
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::ReleaseEntity()
{
	m_visualObject = "";
	m_PrototypeMaterial = "";
	/*
	   if (m_entity)
	   {
	   m_entity->SetDestroyable(true);
	   m_pEntitySystem->RemoveEntity( m_entity->GetId(),true );
	   }
	 */
	m_entity = 0;
	//m_scriptDialog.SetScript( 0,0 );
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::Update()
{
	if (!m_bEntityPlaying)
		return;

	// Update preview control.
	if (m_entity)
	{
		m_previewCtrl.Update();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnUpdateProperties(IVariable* var)
{
	CEntityPrototype* prototype = GetSelectedPrototype();
	if (prototype != 0)
	{
		// Mark prototype library modified.
		prototype->GetLibrary()->SetModified();

		CEntityScript* script = prototype->GetScript();
		CVarBlock* props = prototype->GetProperties();
		if (script && props && m_entity != 0)
		{
			// Set entity properties.
			script->CopyPropertiesToScriptTable(m_entity, props, true);
		}
		prototype->Update();

		CVarBlock* pEntScriptVarBlock = script ? script->GetDefaultProperties() : NULL;
		IVariable* varEntScript = pEntScriptVarBlock ? pEntScriptVarBlock->FindVariable(var ? var->GetName() : "") : NULL;
		if (var && varEntScript && MarkOrUnmarkPropertyField(var, varEntScript))
		{
			// If flags have been changed for this field, make sure the item gets redrawn.
			CPropertyItem* pItem = m_propsCtrl.FindItemByVar(var);
			ASSERT(pItem);
			if (pItem)
			{
				m_propsCtrl.Invalidate(FALSE);
			}
		}
	}

	if (var && var->GetType() == IVariable::STRING)
	{
		// When model is updated we need to reload preview.
		UpdatePreview();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnShowDescription()
{
	/*
	   m_previewCtrl.SetDlgCtrlID(100);
	   m_wndScriptPreviewSplitter.SetPane( 0,1,&m_descriptionEditBox,CSize(200,100) );
	   m_wndScriptPreviewSplitter.RecalcLayout();
	   m_descriptionEditBox.ShowWindow( SW_SHOW );
	   m_previewCtrl.ShowWindow( SW_HIDE );
	   m_bShowDescription = true;
	 */
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnPlay()
{
	m_bEntityPlaying = !m_bEntityPlaying;

	/*
	   if (m_bShowDescription)
	   {
	   m_descriptionEditBox.SetDlgCtrlID(101);
	   m_wndScriptPreviewSplitter.SetPane( 0,1,&m_previewCtrl,CSize(200,100) );
	   m_descriptionEditBox.ShowWindow( SW_HIDE );
	   m_previewCtrl.ShowWindow( SW_SHOW );
	   m_bShowDescription = false;
	   m_wndScriptPreviewSplitter.RecalcLayout();
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnUpdatePlay(CCmdUI* pCmdUI)
{
	if (m_bEntityPlaying)
		pCmdUI->SetCheck(TRUE);
	else
		pCmdUI->SetCheck(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnArchetypeExtensionPropsChanged()
{
	if (CEntityPrototype* pPrototype = GetSelectedPrototype())
	{
		pPrototype->GetLibrary()->SetModified();
		pPrototype->Update();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnReloadEntityScript()
{
	CEntityPrototype* prototype = GetSelectedPrototype();
	if (prototype)
	{
		prototype->Reload();
		TSmartPtr<CEntityPrototype> sel = prototype;
		SelectItem(prototype);
	}
}

//////////////////////////////////////////////////////////////////////////
CEntityPrototype* CEntityProtLibDialog::GetSelectedPrototype()
{
	CBaseLibraryItem* item = m_pCurrentItem;
	return (CEntityPrototype*)item;
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnDescriptionChange()
{
	/*
	   string desc;
	   m_descriptionEditBox.GetWindowText( desc );
	   if (GetSelectedPrototype())
	   {
	   GetSelectedPrototype()->SetDescription( desc );
	   GetSelectedPrototype()->GetLibrary()->SetModified();
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnAssignToSelection()
{
	CEntityPrototype* pPrototype = GetSelectedPrototype();
	if (!pPrototype)
		return;

	CUndo undo("Assign Archetype");

	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	if (!pSel->IsEmpty())
	{
		for (int i = 0; i < pSel->GetCount(); i++)
		{
			CBaseObject* pObject = pSel->GetObject(i);
			if (pObject->IsKindOf(RUNTIME_CLASS(CProtEntityObject)))
			{
				CProtEntityObject* pProtEntity = (CProtEntityObject*)pObject;
				pProtEntity->SetPrototype(pPrototype, true);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnSelectAssignedObjects()
{
	CEntityPrototype* pItem = GetSelectedPrototype();
	if (!pItem)
		return;

	CBaseObjectsArray objects;
	GetIEditorImpl()->GetObjectManager()->GetObjects(objects);
	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];
		if (pObject->IsKindOf(RUNTIME_CLASS(CProtEntityObject)))
		{
			CProtEntityObject* protEntity = (CProtEntityObject*)pObject;
			if (protEntity->GetPrototype() != pItem)
				continue;
			if (pObject->IsHidden() || pObject->IsFrozen())
				continue;
			GetIEditorImpl()->GetObjectManager()->SelectObject(pObject);
		}
	}
}

#define ID_CMD_CHANGECLASS 2

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnNotifyTreeRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	// Show helper menu.
	CPoint point;

	CEntityPrototype* pItem = 0;

	// Find node under mouse.
	GetCursorPos(&point);
	m_treeCtrl.ScreenToClient(&point);
	// Select the item that is at the point myPoint.
	UINT uFlags;
	HTREEITEM hItem = m_treeCtrl.HitTest(point, &uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pItem = (CEntityPrototype*)m_treeCtrl.GetItemData(hItem);
	}

	if (!pItem)
		return;

	SelectItem(pItem);

	// Create pop up menu.
	CMenu menu;
	menu.CreatePopupMenu();

	if (pItem)
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
		menu.AppendMenu(MF_SEPARATOR, 0, "");
		menu.AppendMenu(MF_STRING, ID_DB_ASSIGNTOSELECTION, "Assign to Selected Objects");
		menu.AppendMenu(MF_STRING, ID_DB_SELECTASSIGNEDOBJECTS, "Select Assigned Objects");
		menu.AppendMenu(MF_STRING, ID_DB_RELOAD, "Reload");
		menu.AppendMenu(MF_STRING, ID_CMD_CHANGECLASS, "Change Entity Class");
	}

	GetCursorPos(&point);
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, point.x, point.y, this);
	switch (cmd)
	{
	case ID_CMD_CHANGECLASS:
		OnChangeEntityClass();
		break;
	default:
		SendMessage(WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

	HTREEITEM hItem = pNMTreeView->itemNew.hItem;

	CBaseLibraryItem* pItem = (CEntityPrototype*)m_treeCtrl.GetItemData(hItem);
	if (!pItem)
		return;

	m_pDraggedItem = pItem;

	m_treeCtrl.Select(hItem, TVGN_CARET);

	m_dragImage = m_treeCtrl.CreateDragImage(hItem);
	if (m_dragImage)
	{
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
void CEntityProtLibDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_dragImage)
	{
		CRect rc;
		AfxGetMainWnd()->GetWindowRect(rc);

		CPoint p = point;
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
				SetCursor(m_hCursorCreate);
				CPoint vp = p;
				viewport->ScreenToClient(&vp);
				HitContext hit;
				if (viewport->HitTest(vp, hit))
				{
					if (hit.object && hit.object->IsKindOf(RUNTIME_CLASS(CProtEntityObject)))
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
void CEntityProtLibDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
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

		CViewport* viewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint(p);
		if (viewport)
		{
			bool bHit = false;
			CPoint vp = p;
			viewport->ScreenToClient(&vp);
			// Drag and drop into one of views.
			// Start object creation.
			HitContext hit;
			if (viewport->HitTest(vp, hit))
			{
				if (hit.object)
				{
					if (hit.object->IsKindOf(RUNTIME_CLASS(CProtEntityObject)))
					{
						bHit = true;
						CUndo undo("Assign Archetype");
						((CProtEntityObject*)hit.object)->SetPrototype((CEntityPrototype*)m_pDraggedItem, false);
					}
				}
			}
			if (!bHit)
			{
				CUndo undo("Create EntityPrototype");
				string guid = m_pDraggedItem->GetGUID().ToString();
				GetIEditorImpl()->StartObjectCreation("EntityArchetype", guid);
			}
		}
		m_pDraggedItem = 0;
	}

	CBaseLibraryDialog::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnCopy()
{
	CBaseLibraryItem* pItem = m_pCurrentItem;
	if (pItem)
	{
		CClipboard clipboard;
		XmlNodeRef node = XmlHelpers::CreateXmlNode("EntityPrototype");
		CBaseLibraryItem::SerializeContext ctx(node, false);
		ctx.bCopyPaste = true;
		pItem->Serialize(ctx);
		clipboard.Put(node);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnPaste()
{
	if (!m_pLibrary)
		return;

	CClipboard clipboard;
	XmlNodeRef node = clipboard.Get();
	if (!node)
		return;

	if (strcmp(node->getTag(), "EntityPrototype") == 0)
	{
		// This is material node.
		CUndo undo("Add entity prototype library item");
		CBaseLibrary* pLib = m_pLibrary;
		CEntityPrototype* pItem = m_pEntityManager->LoadPrototype((CEntityPrototypeLibrary*)pLib, node);
		if (pItem)
		{
			ReloadItems();
			SelectItem(pItem);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnChangeEntityClass()
{
	CBaseLibraryItem* pItem = m_pCurrentItem;
	if (pItem)
	{
		CEntityPrototype* pPrototype = (CEntityPrototype*)pItem;
		string entityClass = SelectEntityClass();
		if (entityClass.IsEmpty())
			return;

		pPrototype->SetEntityClassName(entityClass);

		SelectItem(pItem, true);

		//////////////////////////////////////////////////////////////////////////
		// Reload entity prototype on all entities.
		//////////////////////////////////////////////////////////////////////////
		CBaseObjectsArray objects;
		GetIEditorImpl()->GetObjectManager()->GetObjects(objects);
		for (int i = 0; i < objects.size(); i++)
		{
			CBaseObject* pObject = objects[i];
			if (pObject->IsKindOf(RUNTIME_CLASS(CProtEntityObject)))
			{
				CProtEntityObject* pProtEntity = (CProtEntityObject*)pObject;
				if (pProtEntity->GetPrototype()->GetGUID() == pPrototype->GetGUID())
				{
					pProtEntity->SetPrototype(pPrototype, true);
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityProtLibDialog::OnRemoveLibrary()
{
	string library = m_selectedLib;
	if (library.IsEmpty())
		return;

	if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("All objects from this library will be deleted from the level.\r\nAre you sure you want to delete this library?")))
	{
		CBaseObjectsArray objects;
		GetIEditorImpl()->GetObjectManager()->GetObjects(objects);
		for (int i = 0; i < objects.size(); i++)
		{
			CBaseObject* pObject = objects[i];
			if (pObject->IsKindOf(RUNTIME_CLASS(CProtEntityObject)))
			{
				CProtEntityObject* pProtEntity = (CProtEntityObject*)pObject;
				if (pProtEntity && pProtEntity->GetPrototype() && pProtEntity->GetPrototype()->GetLibrary())
				{
					string name = pProtEntity->GetPrototype()->GetLibrary()->GetName();
					if (!strcmp(name, library))
						GetIEditorImpl()->GetObjectManager()->DeleteObject(pObject);
				}
			}
		}
		CBaseLibraryDialog::OnRemoveLibrary();
	}
}

