// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehiclePartsPanel.h"

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IStatObj.h>
#include <CryAnimation/ICryAnimation.h>
#include "Controls\PropertyItem.h"

#include "VehicleEditorDialog.h"
#include "VehiclePrototype.h"
#include "VehicleHelperObject.h"
#include "VehiclePart.h"
#include "VehicleSeat.h"
#include "VehicleData.h"
#include "VehicleWeapon.h"
#include "VehicleComp.h"

#include <IVehicleSystem.h>

#include "Util/MFCUtil.h"
#include "IUndoObject.h"

const static bool SELECTED = true;
const static bool NOT_SELECTED = false;
const static bool UNIQUE = true;
const static bool NOT_UNIQUE = false;
const static bool EDIT_LABEL = true;
const static bool NOT_EDIT_LABEL = false;
const static bool FROM_ASSET = true;
const static bool FROM_EDITOR = false;

//////////////////////////////////////////////////////////////////////////////
// Tool Panel implementation
//////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CPartsToolsPanel, CDialog)
ON_BN_CLICKED(IDC_VEED_SHOWSEATS, OnDisplaySeats)
ON_BN_CLICKED(IDC_VEED_SHOWASSETHELPERS, OnDisplayAssetHelpers)
ON_BN_CLICKED(IDC_VEED_SHOWVEEDHELPERS, OnDisplayVeedHelpers)
ON_BN_CLICKED(IDC_VEED_SHOWWHEELS, OnDisplayWheels)
ON_BN_CLICKED(IDC_VEED_SHOWCOMPONENTS, OnDisplayComps)
END_MESSAGE_MAP()

BOOL CPartsToolsPanel::Create(UINT nIDTemplate, CWnd* pParentWnd /* = NULL */)
{
	m_bSeats = true;
	m_bWheels = false;
	m_bVeedHelpers = false;
	m_bAssetHelpers = false;
	m_bComps = false;

	return CDialog::Create(nIDTemplate, pParentWnd);
}

void CPartsToolsPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Check(pDX, IDC_VEED_SHOWSEATS, m_bSeats);
	DDX_Check(pDX, IDC_VEED_SHOWWHEELS, m_bWheels);
	DDX_Check(pDX, IDC_VEED_SHOWASSETHELPERS, m_bAssetHelpers);
	DDX_Check(pDX, IDC_VEED_SHOWVEEDHELPERS, m_bVeedHelpers);
	DDX_Check(pDX, IDC_VEED_SHOWCOMPONENTS, m_bComps);
}

void CPartsToolsPanel::OnDisplaySeats()
{
	UpdateData(TRUE);
	pPartsPanel->ShowSeats(m_bSeats);
}

void CPartsToolsPanel::OnDisplayWheels()
{
	UpdateData(TRUE);
	pPartsPanel->ShowWheels(m_bWheels);
}

void CPartsToolsPanel::OnDisplayComps()
{
	UpdateData(TRUE);
	pPartsPanel->ShowComps(m_bComps);
}

void CPartsToolsPanel::OnDisplayVeedHelpers()
{
	UpdateData(TRUE);
	pPartsPanel->ShowVeedHelpers(m_bVeedHelpers);
}

void CPartsToolsPanel::OnDisplayAssetHelpers()
{
	UpdateData(TRUE);
	pPartsPanel->ShowAssetHelpers(m_bAssetHelpers);
}

//////////////////////////////////////////////////////////////////////////////
// CWheelMasterDialog implementation
//////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CWheelMasterDialog, CDialog)

CWheelMasterDialog::CWheelMasterDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CWheelMasterDialog::IDD, pParent)
{
	m_pVar = 0;
	m_pWheels = 0;

	m_boxes[0] = FALSE; // uncheck axle

	for (int i = 1; i < NUM_BOXES; ++i)
	{
		m_boxes[i] = TRUE;  // check the rest
	}

	m_toggleAll = TRUE;
}

void CWheelMasterDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_WHEELMASTER_LIST, m_wheelList);
	DDX_Check(pDX, IDC_WHEELMASTER_TOGGLE_ALL, m_toggleAll);

	DDX_Check(pDX, IDC_CHECK1, m_boxes[0]);
	DDX_Check(pDX, IDC_CHECK2, m_boxes[1]);
	DDX_Check(pDX, IDC_CHECK3, m_boxes[2]);
	DDX_Check(pDX, IDC_CHECK4, m_boxes[3]);
	DDX_Check(pDX, IDC_CHECK5, m_boxes[4]);
	DDX_Check(pDX, IDC_CHECK6, m_boxes[5]);
	DDX_Check(pDX, IDC_CHECK7, m_boxes[6]);
	DDX_Check(pDX, IDC_CHECK8, m_boxes[7]);
	DDX_Check(pDX, IDC_CHECK9, m_boxes[8]);
}

BEGIN_MESSAGE_MAP(CWheelMasterDialog, CDialog)
ON_BN_CLICKED(IDC_CHECK1, OnCheckBoxClicked)
ON_BN_CLICKED(IDC_CHECK2, OnCheckBoxClicked)
ON_BN_CLICKED(IDC_CHECK3, OnCheckBoxClicked)
ON_BN_CLICKED(IDC_CHECK4, OnCheckBoxClicked)
ON_BN_CLICKED(IDC_CHECK5, OnCheckBoxClicked)
ON_BN_CLICKED(IDC_CHECK6, OnCheckBoxClicked)
ON_BN_CLICKED(IDC_CHECK7, OnCheckBoxClicked)
ON_BN_CLICKED(IDC_CHECK8, OnCheckBoxClicked)
ON_BN_CLICKED(IDC_CHECK9, OnCheckBoxClicked)
ON_BN_CLICKED(IDC_WHEELMASTER_CLOSE, OnOKClicked)
ON_BN_CLICKED(IDC_APPLYWHEELS, OnApplyWheels)
ON_BN_CLICKED(IDC_WHEELMASTER_TOGGLE_ALL, OnToggleAll)
END_MESSAGE_MAP()

void CWheelMasterDialog::OnOKClicked()
{
	EndDialog(IDOK);
}

BOOL CWheelMasterDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	CRect rc(25, 20, 240, 176);
	m_propsCtrl.Create(WS_CHILD | WS_VISIBLE | WS_BORDER, rc, this);

	CVarBlock block;
	block.AddRef();
	block.AddVariable(m_pVar);

	m_propsCtrl.AddVarBlock(&block);

	CPropertyItem* pItem = m_propsCtrl.FindItemByVar(m_pVar);
	m_propsCtrl.Expand(pItem, true);
	m_propsCtrl.Expand(pItem->GetChild(0), true);

	ApplyCheckBoxes();

	// fill wheel listbox
	m_wheelList.ResetContent();

	for (std::vector<CVehiclePart*>::iterator it = m_pWheels->begin(); it != m_pWheels->end(); ++it)
	{
		int idx = m_wheelList.AddString((*it)->GetName());
		m_wheelList.SetItemDataPtr(idx, *it);
	}

	return TRUE;
}

void CWheelMasterDialog::SetVariable(IVariable* pVar)
{
	m_pVar = pVar->Clone(true);
}

void CWheelMasterDialog::OnCheckBoxClicked()
{
	UpdateData(TRUE);
	ApplyCheckBoxes();
}

void CWheelMasterDialog::ApplyCheckBoxes()
{
	CPropertyItem* pItem = m_propsCtrl.FindItemByVar(m_pVar->GetVariable(0));
	assert(pItem);

	for (int i = 0; i < NUM_BOXES && i < pItem->GetChildCount(); ++i)
	{
		IVariable* pVar = pItem->GetChild(i)->GetVariable();

		if (m_boxes[i] == FALSE)
		{
			pVar->SetFlags(pVar->GetFlags() | IVariable::UI_DISABLED);
		}
		else
		{
			pVar->SetFlags(pVar->GetFlags() & ~IVariable::UI_DISABLED);
		}
	}

	m_propsCtrl.Invalidate();
}

void CWheelMasterDialog::OnApplyWheels()
{
	// apply (active) master values to all selected wheels
	int count = m_wheelList.GetSelCount();

	if (count > 0)
	{
		int* items = new int[count];
		m_wheelList.GetSelItems(count, items);

		for (int i = 0; i < count; ++i)
		{
			CVehiclePart* pWheel = (CVehiclePart*)m_wheelList.GetItemDataPtr(items[i]);

			IVariable* pPartVar = IVeedObject::GetVeedObject(pWheel)->GetVariable();
			IVariable* pSubVar = GetChildVar(pPartVar, "SubPartWheel");

			if (!pSubVar)
			{
				CryLog("Warning, SubPartWheel not found!");
				continue;
			}

			IVariable* pEditMaster = m_pVar->GetVariable(0);

			for (int i = 0; i < pEditMaster->GetNumVariables() && i < NUM_BOXES; ++i)
			{
				if (m_boxes[i] == TRUE)
				{
					CString val;
					pEditMaster->GetVariable(i)->Get(val);

					// get variable from wheel to update
					const char* varName = pEditMaster->GetVariable(i)->GetName();
					IVariable* pVar = GetChildVar(pSubVar, varName);

					if (pVar)
					{
						pVar->Set(val);
						VeedLog("%s: applying %s [%s]", pWheel->GetName(), varName, val);
					}
					else
					{
						CryLog("%s: Variable %s not found", pWheel->GetName(), varName);
					}
				}
			}
		}

		delete[] items;
	}

}

void CWheelMasterDialog::OnToggleAll()
{
	UpdateData(TRUE);

	for (int i = 1; i < NUM_BOXES; ++i) // don't toggle axle box
	{
		m_boxes[i] = m_toggleAll;
	}

	UpdateData(FALSE);

	ApplyCheckBoxes();
}

//////////////////////////////////////////////////////////////////////////////
// VehicleScaleDlg implementation
//////////////////////////////////////////////////////////////////////////////
class CVehicleScaleDialog : public CDialog
{
	DECLARE_DYNAMIC(CVehicleScaleDialog)

public:
	CVehicleScaleDialog(CWnd* pParent = NULL)
		: CDialog(CVehicleScaleDialog::IDD, pParent)
		, m_scale(1.f)
	{}
	virtual BOOL OnInitDialog();
	float        GetScale() { return m_scale; }

	// Dialog Data
	enum { IDD = IDD_VEED_SCALE_HELPERS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);      // DDX/DDV support
	DECLARE_MESSAGE_MAP()
	afx_msg void OnOKClicked();

	CEdit m_editScale;
	float m_scale;
};

IMPLEMENT_DYNAMIC(CVehicleScaleDialog, CDialog)

BEGIN_MESSAGE_MAP(CVehicleScaleDialog, CDialog)
ON_BN_CLICKED(IDOK, OnOKClicked)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////////
BOOL CVehicleScaleDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_editScale.SetWindowText("1.0");
	m_editScale.SetFocus();

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleScaleDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_SCALE, m_editScale);
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleScaleDialog::OnOKClicked()
{
	UpdateData(TRUE);

	CString str;
	m_editScale.GetWindowText(str);

	m_scale = (float)atof((const char*)str);

	EndDialog(IDOK);
}

//////////////////////////////////////////////////////////////////////////////
// VehiclePartsPanel implementation
//////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CVehiclePartsPanel, CWnd)

CVehiclePartsPanel::CVehiclePartsPanel(CVehicleEditorDialog* pDialog)
	: m_pVehicle(0)
	, m_pSelItem(0)
	, m_partToTree(pDialog->m_partToTree)
{
	assert(pDialog); // dialog is needed
	m_pDialog = pDialog;

	m_pObjMan = GetIEditor()->GetObjectManager();
	m_pMainPart = 0;
	m_pRootVar = 0;
	m_classBlock.AddRef(); // don't release
	m_bSeats = true;
}

CVehiclePartsPanel::~CVehiclePartsPanel()
{
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::DoDataExchange(CDataExchange* pDX)
{
	CWnd::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CVehiclePartsPanel, CWnd)
ON_WM_CREATE()
ON_WM_SIZE()
ON_WM_DESTROY()
ON_WM_CLOSE()
ON_NOTIFY(NM_RCLICK, AFX_IDW_PANE_FIRST, OnItemRClick)     // splitter
ON_NOTIFY(NM_DBLCLK, AFX_IDW_PANE_FIRST, OnItemDblClick)
ON_COMMAND(ID_VEED_HELPER_NEW, OnHelperNew)
ON_COMMAND(ID_VEED_HELPER_DELETE, OnDeleteItem)
ON_COMMAND(ID_VEED_HELPER_RENAME, OnHelperRename)
ON_COMMAND(ID_VEED_PART_SELECT, OnPartSelect)
ON_COMMAND(ID_VEED_PART_NEW, OnPartNew)
ON_COMMAND(ID_VEED_PART_DELETE, OnDeleteItem)
ON_COMMAND(ID_VEED_SEAT_NEW, OnSeatNew)
ON_COMMAND(ID_VEED_SEAT_DELETE, OnDeleteItem)
ON_COMMAND(ID_VEED_WEAPON_NEW, OnPrimaryWeaponNew)
ON_COMMAND(ID_VEED_COMPONENT_NEW, OnComponentNew)
ON_COMMAND(ID_VEED_COMPONENT_DELETE, OnDeleteItem)
ON_COMMAND(ID_VEED_EDIT_WHEELMASTER, OnEditWheelMaster)
ON_COMMAND(ID_VEED_HELPERS_SCALE, OnScaleHelpers)

ON_NOTIFY(TVN_ENDLABELEDIT, AFX_IDW_PANE_FIRST, OnHelperRenameDone)
ON_NOTIFY(TVN_SELCHANGED, AFX_IDW_PANE_FIRST, OnSelect)
ON_NOTIFY(TVN_KEYDOWN, AFX_IDW_PANE_FIRST, OnTreeKeyDown)

//ON_NOTIFY_REFLECT_EX( TVN_ENDLABELEDIT, OnHelperRenameDone )
//ON_NOTIFY( NM_RETURN, IDC_VEED_PARTS_TREE, OnHelperRenameDone )

END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnTreeKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	// Key press in items tree view.

	bool bCtrl = GetAsyncKeyState(VK_CONTROL) != 0;
	NMTVKEYDOWN* nm = (NMTVKEYDOWN*)pNMHDR;

	//if (bCtrl && (nm->wVKey == 'c' || nm->wVKey == 'C'))
	//{
	//  OnCopy();	// Ctrl+C
	//}
	//if (bCtrl && (nm->wVKey == 'v' || nm->wVKey == 'V'))
	//{
	//  OnPaste(); // Ctrl+V
	//}

	if (nm->wVKey == VK_DELETE)
	{
		OnDeleteItem();
	}

	if (nm->wVKey == VK_F2)
	{
		//OnRenameItem();
	}
}

//////////////////////////////////////////////////////////////////////////////
// called when selection in tree control changes
void CVehiclePartsPanel::OnSelect(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTREEVIEW* tv = (NMTREEVIEW*)pNMHDR;
	TVITEM tvItem = tv->itemNew;

	if (!tvItem.hItem)
	{
		return;
	}

	m_propsCtrl.DeleteAllItems();
	m_propsCtrl.SetCallbackOnNonModified(false);
	m_propsCtrl.SetUpdateCallback(functor(*this, &CVehiclePartsPanel::OnApplyChanges));

	CBaseObject* pObj = (CBaseObject*)m_treeCtrl.GetItemData(tvItem.hItem);

	if (!pObj || pObj->IsKindOf(RUNTIME_CLASS(CVehiclePrototype)))
	{
		return;
	}

	IVeedObject* pVO = IVeedObject::GetVeedObject(pObj);

	// open properties of this obj
	IVariable* pVar = pVO->GetVariable();

	if (pVar)
	{
		CVarBlock* block = new CVarBlock();
		block->AddVariable(pVar);
		m_propsCtrl.AddVarBlock(block);
		m_propsCtrl.ExpandAll();
		m_propsCtrl.EnableUpdateCallback(false);

		for (int i = 0; i < pVar->GetNumVariables(); ++i)
		{
			IVariable* pSubVar = pVar->GetVariable(i);
			CPropertyItem* pItem = m_propsCtrl.FindItemByVar(pSubVar);

			m_pDialog->OnPropsSelChanged(pItem);
		}

		m_propsCtrl.EnableUpdateCallback(true);

		// if its a Part, also expand SubVariable
		if (pObj->IsKindOf(RUNTIME_CLASS(CVehiclePart)))
		{
			if (IVariable* pSubVar = GetChildVar(pVar, ((CVehiclePart*)pObj)->GetPartClass()))
			{
				CPropertyItem* pItem = m_propsCtrl.FindItemByVar(pSubVar);

				if (pItem)
				{
					m_propsCtrl.Expand(pItem, true);
				}
			}

			pVO->OnTreeSelection();
		}
	}

	if (!pObj->IsSelected())
	{
		GetIEditor()->ClearSelection();
		GetIEditor()->SelectObject(pObj);
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnItemDblClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	CPoint point;

	// Find node under mouse.
	GetCursorPos(&point);
	m_treeCtrl.ScreenToClient(&point);

	// Select the item that is at myPoint.
	UINT uFlags;
	HTREEITEM hItem = m_treeCtrl.HitTest(point, &uFlags);

	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		m_treeCtrl.SelectItem(hItem);
		m_treeCtrl.EditLabel(hItem);
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnItemRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (!m_pVehicle)
	{
		return;
	}

	// Show helper menu.
	CPoint point;

	CVehiclePart* pPart = 0;

	// Find node under mouse.
	GetCursorPos(&point);
	m_treeCtrl.ScreenToClient(&point);

	// Select the item that is at the point myPoint.
	UINT uFlags;
	HTREEITEM hItem = m_treeCtrl.HitTest(point, &uFlags);

	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		m_treeCtrl.SelectItem(hItem);
		CBaseObject* pObj = (CBaseObject*)m_treeCtrl.GetItemData(hItem);

		if (!pObj)
		{
			return;
		}

		m_pSelItem = pObj;

		// Create pop up menu.
		CMenu menu;
		menu.CreatePopupMenu();

		// insert common items
		//menu.AppendMenu( MF_STRING, ID_VEED_PART_SELECT, "Select" );

		// check what kind of item we're clicking on
		if (pObj->IsKindOf(RUNTIME_CLASS(CVehiclePrototype)))
		{
			menu.AppendMenu(MF_STRING, ID_VEED_PART_NEW, "Add Part");
			menu.AppendMenu(MF_STRING, ID_VEED_SEAT_NEW, "Add Seat");
			menu.AppendMenu(MF_STRING, ID_VEED_COMPONENT_NEW, "Add Component");
			menu.AppendMenu(MF_STRING, ID_VEED_HELPER_NEW, "Add Helper");
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING, ID_VEED_HELPERS_SCALE, "Update Scale");
		}
		else if (pObj->IsKindOf(RUNTIME_CLASS(CVehiclePart)))
		{
			menu.AppendMenu(MF_STRING, ID_VEED_HELPER_RENAME, "Rename");
			menu.AppendMenu(MF_STRING, ID_VEED_PART_DELETE, "Delete");

			CVehiclePart* pPart = (CVehiclePart*)pObj;

			if (!pPart->IsLeaf())
			{
				menu.AppendMenu(MF_SEPARATOR);
				menu.AppendMenu(MF_STRING, ID_VEED_PART_NEW, "Add Part");
				menu.AppendMenu(MF_STRING, ID_VEED_SEAT_NEW, "Add Seat");
			}
			else
			{
				if (pPart->GetPartClass() == PARTCLASS_WHEEL)
				{
					menu.AppendMenu(MF_SEPARATOR);
					menu.AppendMenu(MF_STRING, ID_VEED_EDIT_WHEELMASTER, "Edit Wheel Master");
				}
			}
		}
		else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleSeat)))
		{
			menu.AppendMenu(MF_STRING, ID_VEED_HELPER_RENAME, "Rename");
			menu.AppendMenu(MF_STRING, ID_VEED_SEAT_DELETE, "Delete");
			menu.AppendMenu(MF_SEPARATOR);
			menu.AppendMenu(MF_STRING, ID_VEED_WEAPON_NEW, "Add Primary Weapon");
		}
		else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleHelper)))
		{
			if (!((CVehicleHelper*)pObj)->IsFromGeometry())
			{
				menu.AppendMenu(MF_STRING, ID_VEED_HELPER_RENAME, "Rename");
				menu.AppendMenu(MF_STRING, ID_VEED_HELPER_DELETE, "Delete");
			}
		}
		else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleComponent)))
		{
			menu.AppendMenu(MF_STRING, ID_VEED_HELPER_RENAME, "Rename");
			menu.AppendMenu(MF_STRING, ID_VEED_COMPONENT_DELETE, "Delete");
		}
		else
		{
			CryLog("unknown item clicked");
		}

		GetCursorPos(&point);
		menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
	}

	/*CClipboard clipboard;
	   bool bNoPaste = clipboard.IsEmpty();
	   int pasteFlags = 0;
	   if (bNoPaste)
	   pasteFlags |= MF_GRAYED;*/

}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnPartSelect()
{
	HTREEITEM hSelItem = m_treeCtrl.GetSelectedItem();
	CBaseObject* pSelPart = (CBaseObject*)m_treeCtrl.GetItemData(hSelItem);

	if (pSelPart)
	{
		GetIEditor()->ClearSelection();
		GetIEditor()->SelectObject(pSelPart);
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnPartNew()
{
	CScopedSuspendUndo susp;
	assert(m_pSelItem);

	// part creation

	HTREEITEM hSelItem = m_treeCtrl.GetSelectedItem();
	CBaseObject* pParent = (CBaseObject*)m_treeCtrl.GetItemData(hSelItem);

	// create obj
	CVehiclePart* pObj = (CVehiclePart*)GetIEditor()->GetObjectManager()->NewObject("VehiclePart");

	if (pObj)
	{
		//pObj->SetHidden(true);
		pObj->SetName("NewPart");
		pObj->SetWorldTM(m_pVehicle->GetWorldTM());
		pObj->SetVehicle(m_pVehicle);
		pObj->UpdateObjectFromVar();

		if (pParent)
		{
			if (pParent == m_pVehicle)
			{
				((CVehiclePrototype*)pParent)->AddPart(pObj);
			}
			else if (pParent->IsKindOf(RUNTIME_CLASS(CVehiclePart)))
			{
				((CVehiclePart*)pParent)->AddPart(pObj);
			}

			HTREEITEM hItem = InsertTreeItem(pObj, hSelItem, true);
			m_treeCtrl.EditLabel(hItem);
		}

		pObj->AddEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));

		GetIEditor()->SetModifiedFlag();
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnComponentNew()
{
	CScopedSuspendUndo susp;
	assert(m_pSelItem);

	HTREEITEM hSelItem = m_treeCtrl.GetSelectedItem();
	CVehiclePrototype* pParent = (CVehiclePrototype*)m_treeCtrl.GetItemData(hSelItem);

	// create obj
	CVehicleComponent* pComp = (CVehicleComponent*)GetIEditor()->GetObjectManager()->NewObject("VehicleComponent");

	if (pComp)
	{
		pComp->SetVehicle(m_pVehicle);
		pComp->CreateVariable();

		if (pParent)
		{
			pParent->AddComponent(pComp);
			pComp->ResetPosition();

			HTREEITEM hItem = InsertTreeItem(pComp, hSelItem, true);
			m_treeCtrl.EditLabel(hItem);
		}

		pComp->UpdateObjectFromVar();

		pComp->AddEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));

		GetIEditor()->SetModifiedFlag();
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnSeatNew()
{
	CScopedSuspendUndo susp;
	assert(m_pSelItem);

	HTREEITEM hSelItem = m_treeCtrl.GetSelectedItem();
	CBaseObject* pParent = (CBaseObject*)m_treeCtrl.GetItemData(hSelItem);

	// create obj
	CVehicleSeat* pSeat = (CVehicleSeat*)GetIEditor()->GetObjectManager()->NewObject("VehicleSeat");

	if (pSeat)
	{
		pSeat->SetHidden(true);
		pSeat->SetVehicle(m_pVehicle);
		pSeat->UpdateObjectFromVar();

		if (pParent)
		{
			pParent->AttachChild(pSeat);
			pSeat->UpdateVarFromObject();

			HTREEITEM hItem = InsertTreeItem(pSeat, hSelItem, true);
			m_treeCtrl.EditLabel(hItem);
		}

		pSeat->AddEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));

		GetIEditor()->SetModifiedFlag();
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnPrimaryWeaponNew()
{
	CVehicleSeat* pSeat = (CVehicleSeat*)m_treeCtrl.GetItemData(m_treeCtrl.GetSelectedItem());

	if (pSeat)
	{
		CreateWeapon(WEAPON_PRIMARY, pSeat);
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnSecondaryWeaponNew()
{
	CVehicleSeat* pSeat = (CVehicleSeat*)m_treeCtrl.GetItemData(m_treeCtrl.GetSelectedItem());

	if (pSeat)
	{
		CreateWeapon(WEAPON_SECONDARY, pSeat);
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnHelperNew()
{
	CScopedSuspendUndo susp;
	assert(m_pSelItem);

	// helper creation:
	// - create Helper object in editor
	// - create tree item

	// place helper above vehicle
	AABB bbox;
	m_pVehicle->GetBoundBox(bbox);
	Vec3 pos = m_pVehicle->GetWorldTM().GetTranslation() + Vec3(0, 0, bbox.GetSize().z + 0.25f);

	// create obj
	CVehicleHelper* pHelper = CreateHelperObject(pos, Vec3(FORWARD_DIRECTION), "NewHelper", UNIQUE, SELECTED, EDIT_LABEL, FROM_EDITOR);

	GetIEditor()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////////
HTREEITEM CVehiclePartsPanel::InsertTreeItem(CBaseObject* pObj, CBaseObject* pParent, bool select /*=false*/)
{
	HTREEITEM hParentItem = (stl::find_in_map(m_partToTree, pParent, STreeItem())).item;

	if (hParentItem)
	{
		return InsertTreeItem(pObj, hParentItem, select);
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////
HTREEITEM CVehiclePartsPanel::InsertTreeItem(CBaseObject* pObj, HTREEITEM hParent, bool select /*=false*/)
{
	IVeedObject* pVO = IVeedObject::GetVeedObject(pObj);

	if (!pVO)
	{
		return NULL;
	}

	int icon = pVO->GetIconIndex();
	HTREEITEM hItem = m_treeCtrl.InsertItem(pObj->GetName(), icon, icon, hParent);

	m_treeCtrl.SetItemData(hItem, (DWORD_PTR)pObj);
	m_treeCtrl.Expand(hParent, TVE_EXPAND);

	if (select)
	{
		m_treeCtrl.SelectItem(hItem);
		m_pSelItem = pObj;
	}

	//m_treeCtrl.EditLabel(hItem);

	m_partToTree[pObj] = STreeItem(hItem, hParent);

	return hItem;
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnDeleteItem()
{
	CScopedSuspendUndo susp;

	HTREEITEM hItem = m_treeCtrl.GetSelectedItem();

	if (!hItem)
	{
		return;
	}

	CBaseObject* pObj = (CBaseObject*)m_treeCtrl.GetItemData(hItem);

	if (pObj == m_pVehicle) // don't delete vehicle itself
	{
		return;
	}

	DeleteObject(pObj);
	DeleteTreeItem(m_partToTree.find(pObj));

	m_pSelItem = 0;
}

//////////////////////////////////////////////////////////////////////////////
CVehiclePart* CVehiclePartsPanel::GetPartForHelper(CVehicleHelper* pHelper)
{
	TPartToTreeMap::iterator it = m_partToTree.find((CBaseObject*)pHelper);

	if (it != m_partToTree.end())
	{
		HTREEITEM hParent = m_treeCtrl.GetParentItem(it->second.item);
		CVehiclePart* pPart = (CVehiclePart*)m_treeCtrl.GetItemData(hParent);
		return pPart;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::DeleteObject(CBaseObject* pObj)
{
	CScopedSuspendUndo susp;
	// delete editor object
	pObj->RemoveEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));
	GetIEditor()->DeleteObject(pObj);
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnHelperRename()
{
	HTREEITEM hItem = m_treeCtrl.GetSelectedItem();
	assert(hItem);

	CEdit* pEdit = m_treeCtrl.EditLabel(hItem);

	if (!pEdit)
	{
		CryLog("[CVehiclePartsPanel] CEdit for treeCtrl is NULL");
		return;
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnHelperRenameDone(NMHDR* pNMHDR, LRESULT* pResult)
{
	CScopedSuspendUndo susp;
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
	TVITEM tvItem = pTVDispInfo->item;

	if (tvItem.hItem && tvItem.pszText != 0)
	{
		m_treeCtrl.SetItemText(tvItem.hItem, tvItem.pszText);

		if (CBaseObject* pObj = (CBaseObject*)m_treeCtrl.GetItemData(tvItem.hItem))
		{
			pObj->SetName(tvItem.pszText);

			if (IVeedObject* pVO = IVeedObject::GetVeedObject(pObj))
			{
				pVO->UpdateVarFromObject();
			}

		}

		*pResult = TRUE;
	}

	//m_treeCtrl.SortChildren(m_treeCtrl.GetParentItem(tvItem.hItem));

	/*NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	   TVITEM tvitem = (TVITEM)pNMTreeView->itemNew;
	   HTREEITEM hItem = tvitem.hItem;*/

}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::ExpandProps(CPropertyItem* pItem, bool expand /*=true*/)
{
	// expand all children and their children
	for (int i = 0; i < pItem->GetChildCount(); ++i)
	{
		CPropertyItem* pChild = pItem->GetChild(i);
		m_propsCtrl.Expand(pChild, true);

		for (int j = 0; j < pChild->GetChildCount(); ++j)
		{
			m_propsCtrl.Expand(pChild->GetChild(j), true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
IVariable* CVehiclePartsPanel::GetVariable()
{
	return m_pRootVar;
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::UpdateVehiclePrototype(CVehiclePrototype* pProt)
{
	CScopedSuspendUndo susp;

	// delete parts of current vehicle, if present
	m_pDialog->DeleteEditorObjects();

	assert(pProt && pProt->GetVariable());
	m_pVehicle = pProt;
	m_pRootVar = pProt->GetVehicleData()->GetRoot()->Clone(true);

	m_propsCtrl.SetVehicleVar(m_pRootVar);

	ReloadTreeCtrl();

	ShowAssetHelpers(m_toolsPanel.m_bAssetHelpers);

	m_pVehicle->AddEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));

	GetIEditor()->SetModifiedFlag();

#ifdef _DEBUG
	DumpPartTreeMap();
#endif
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::ReloadTreeCtrl()
{
	if (m_pVehicle)
	{
		m_treeCtrl.DeleteAllItems();

		int icon = IVeedObject::GetVeedObject(m_pVehicle)->GetIconIndex();
		m_hVehicle = m_treeCtrl.InsertItem(m_pVehicle->GetCEntity()->GetEntityClass(), icon, icon, TVI_ROOT);
		m_treeCtrl.SetItemData(m_hVehicle, (DWORD_PTR)m_pVehicle);
		m_treeCtrl.Expand(m_hVehicle, TVE_EXPAND);

		FillParts(GetVariable());   // parse parts in vehicle xml
		FillHelpers(GetVariable()); // add all helpers
		FillAssetHelpers();
		FillComps(GetVariable()); // add components
		FillSeats();              // add seats to tree
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnApplyChanges(IVariable* pVar)
{
	bool bReloadTree = false;

	if (pVar)
	{
		if (stricmp(pVar->GetName(), "name") == 0)
		{
			HTREEITEM hItem = m_treeCtrl.GetSelectedItem();
			assert(hItem);

			m_treeCtrl.SetItemText(hItem, pVar->GetDisplayValue());
		}
		else if (stricmp(pVar->GetName(), "part") == 0)
		{
			bReloadTree = true;
		}
	}

	UpdateVariables();

	if (bReloadTree)
	{
		ReloadTreeCtrl();
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::UpdateVariables()
{
	// push workspace data into main tree
	ReplaceChildVars(m_pRootVar, m_pVehicle->GetVariable());
}

//////////////////////////////////////////////////////////////////////////////
int CVehiclePartsPanel::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int nRes = __super::OnCreate(lpCreateStruct);

	CRect rc;
	GetClientRect(rc);

	CRect rcTree(0, 0, 200, 300);
	CRect rcProp(0, rcTree.bottom, 200, rcTree.bottom + 200);

	// Create left panel tree control.
	m_treeCtrl.Create(WS_VISIBLE | WS_CHILD | WS_TABSTOP | TVS_HASBUTTONS | TVS_SHOWSELALWAYS |
	                  TVS_LINESATROOT | TVS_HASLINES | TVS_FULLROWSELECT | TVS_EDITLABELS, rc, this, IDC_VEED_PARTS_TREE);

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_VEED_TREE, 16, RGB(255, 0, 255));
	m_treeCtrl.SetImageList(&m_imageList, TVSIL_NORMAL);

	m_treeCtrl.SetNoDrag(true);

	// create tools panel
	m_toolsPanel.Create(CPartsToolsPanel::IDD, this);
	m_toolsPanel.ModifyStyle(0, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_toolsPanel.pPartsPanel = this;

	// create property control
	m_propsCtrl.Create(WS_CHILD | WS_VISIBLE, rc, this);

	////////////////////////////////////////////////////////////////////////////
	// Splitter stuff
	////////////////////////////////////////////////////////////////////////////
	m_vSplitter.CreateStatic(this, 1, 2, WS_CHILD | WS_VISIBLE, AFX_IDW_PANE_FIRST);
	m_hSplitter.CreateStatic(&m_vSplitter, 2, 1, WS_CHILD | WS_VISIBLE, AFX_IDW_PANE_FIRST + 1);
	m_vSplitter.SetPane(0, 0, &m_treeCtrl, CSize(250, rc.Height()));
	m_vSplitter.SetPane(0, 1, &m_hSplitter, CSize(150, rc.Height()));
	m_hSplitter.SetPane(0, 0, &m_toolsPanel, CSize(200, 180));
	m_hSplitter.SetPane(1, 0, &m_propsCtrl, CSize(200, 200));
	m_vSplitter.RecalcLayout();
	m_hSplitter.RecalcLayout();
	return nRes;
}

//////////////////////////////////////////////////////////////////////////////
CVehicleHelper* CVehiclePartsPanel::CreateHelperObject(const Vec3& pos, const Vec3& dir, const CString& name, bool unique, bool select, bool editLabel, bool isFromAsset, IVariable* pHelperVar /*=0*/)
{
	CScopedSuspendUndo susp;

	if (name.GetLength() == 0) // don't allow to create nameless helpers
	{
		return 0;
	}

	static IObjectManager* pObjMan = GetIEditor()->GetObjectManager();
	CVehicleHelper* pObj = (CVehicleHelper*)pObjMan->NewObject("VehicleHelper");

	if (pObj && pObj->IsKindOf(RUNTIME_CLASS(CVehicleHelper)))
	{
		pObj->SetHidden(false);

		if (unique)
		{
			pObj->SetUniqName(name);
		}
		else
		{
			pObj->SetName(name);
		}

		if (select)
		{
			//GetIEditor()->ClearSelection();
			//GetIEditor()->SelectObject(pObj);
		}

		Vec3 direction = (dir.GetLengthSquared() > 0) ? dir.GetNormalized() : Vec3(0, 1, 0);
		Matrix34 tm = Matrix33::CreateRotationVDir(m_pVehicle->GetWorldTM().TransformVector(direction));

		pObj->SetIsFromGeometry(isFromAsset);

		tm.SetTranslation(pos);
		pObj->SetWorldTM(tm);

		HTREEITEM hItem = InsertTreeItem(pObj, m_hVehicle, select);

		if (editLabel)
		{
			m_treeCtrl.EditLabel(hItem);
		}

		m_pVehicle->AddHelper(pObj, pHelperVar);

		pObj->AddEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));

		return pObj;
	}

	if (pObj)
	{
		delete pObj;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::CreateHelpersFromStatObj(IStatObj* pObj, CVehiclePart* pParent)
{
	CScopedSuspendUndo susp;

	if (!pObj)
	{
		return;
	}

	// parse all subobjects, check if helper, and create
	for (int j = 0; j < pObj->GetSubObjectCount(); ++j)
	{
		IStatObj::SSubObject* pSubObj = pObj->GetSubObject(j);

		//CryLog("sub <%s> type %i", pSubObj->name, pSubObj->nType);
		if (pSubObj->nType == STATIC_SUB_OBJECT_DUMMY)
		{
			// check if there's already a helper with that name on this part -> skip
			if (m_pVehicle->GetHelper(pSubObj->name))
			{
				//CryLog("helper %s already loaded from xml, skipping", pSubObj->name);
				continue;
			}

			Vec3 vPos = pObj->GetHelperPos(pSubObj->name);

			if (vPos.IsZero())
			{
				continue;
			}

			vPos = m_pVehicle->GetCEntity()->GetIEntity()->GetWorldTM().TransformPoint(vPos);
			CString name(pSubObj->name);
			CVehicleHelper* pHelper = CreateHelperObject(vPos, Vec3(FORWARD_DIRECTION), name, NOT_UNIQUE, NOT_SELECTED, NOT_EDIT_LABEL, FROM_ASSET);
		}
	}

	GetIEditor()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::ShowObject(TPartToTreeMap::iterator it, bool bShow)
{
	// hide/show object and remove/insert tree item
	CBaseObject* pObj = it->first;
	pObj->SetHidden(!bShow);

	if (!bShow)
	{
		// delete item from tree and set to 0 in map
		m_treeCtrl.DeleteItem(it->second.item);
		it->second.item = 0;
	}
	else if (it->second.item == 0)
	{
		// if currently hidden, insert beneath parent item
		int icon = IVeedObject::GetVeedObject(pObj)->GetIconIndex();
		it->second.item = m_treeCtrl.InsertItem(pObj->GetName(), icon, icon, it->second.parent);
		m_treeCtrl.SetItemData(it->second.item, (DWORD_PTR)pObj);

		if (!it->second.item)
		{
			CryLog("InsertItem for %s failed!", pObj->GetName());
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::ShowVeedHelpers(bool bShow)
{
	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		if ((*it).first->IsKindOf(RUNTIME_CLASS(CVehicleHelper)))
		{
			CVehicleHelper* pHelper = (CVehicleHelper*)it->first;

			if (!pHelper->IsFromGeometry())
			{
				ShowObject(it, bShow);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::ShowSeats(bool bShow)
{
	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		if ((*it).first->IsKindOf(RUNTIME_CLASS(CVehicleSeat)))
		{
			ShowObject(it, bShow);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::ShowComps(bool bShow)
{
	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		if ((*it).first->IsKindOf(RUNTIME_CLASS(CVehicleComponent)))
		{
			ShowObject(it, bShow);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::ShowWheels(bool bShow)
{
	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		if ((*it).first->IsKindOf(RUNTIME_CLASS(CVehiclePart)))
		{
			CVehiclePart* pPart = (CVehiclePart*)it->first;

			if (pPart->GetPartClass() == PARTCLASS_WHEEL)
			{
				ShowObject(it, bShow);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::ShowAssetHelpers(bool bShow)
{
	CScopedSuspendUndo susp;

	if (bShow)
	{
		// TODO: Automatically recreating list asset helpers when vehicle geometry changes.
		// Need callback when an animated part is created.
		// Currently will need to check/uncheck the box for the update to happen.
		RemoveAssetHelpers();
		FillAssetHelpers();
	}
	else
	{
		HideAssetHelpers();
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::DeleteTreeItem(TPartToTreeMap::iterator it)
{
	// delete tree item, erase from map
	m_treeCtrl.DeleteItem((*it).second.item);
	m_partToTree.erase(it);
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::FillAssetHelpers()
{
	CScopedSuspendUndo susp;

	if (m_pVehicle == NULL)
	{
		return;
	}

	IEntity* pEntity = m_pVehicle->GetCEntity()->GetIEntity();

	IVehicleSystem* pVehicleSystem = NULL;

	if (gEnv->pGameFramework)
	{
		pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
	}

	if (pVehicleSystem == NULL)
	{
		return;
	}

	IVehicle* pVehicle = pVehicleSystem->GetVehicle(pEntity->GetId());
	assert(pVehicle != NULL);

	if (pVehicle == NULL)
	{
		return;
	}

	for (int partIndex = 0; partIndex < pVehicle->GetPartCount(); partIndex++)
	{
		IVehiclePart* pPart = pVehicle->GetPart(partIndex);

		const TVehicleObjectId ANIMATED_PART_ID = pVehicleSystem->GetVehicleObjectId("Animated");
		TVehicleObjectId partId = pPart->GetId();
		bool isAnimatedPart = (partId == ANIMATED_PART_ID);

		if (isAnimatedPart)
		{
			IEntity* pPartEntity = pPart->GetEntity();

			if (pPartEntity != NULL)
			{
				int slot = pPart->GetSlot();
				ICharacterInstance* pCharacter = pPartEntity->GetCharacter(slot);

				if (pCharacter != NULL)
				{
					ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
					IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
					assert(pSkeletonPose != NULL);

					int32 numJoints = rIDefaultSkeleton.GetJointCount();

					for (int32 jointId = 0; jointId < numJoints; jointId++)
					{
						CString jointName = rIDefaultSkeleton.GetJointNameByID(jointId);

						const QuatT& joint = pSkeletonPose->GetRelJointByID(jointId);

						const Vec3& localPosition = joint.t;
						const Vec3 position = m_pVehicle->GetWorldTM().TransformPoint(localPosition);

						const Vec3& direction = joint.GetColumn1();

						CVehicleHelper* pHelper = CreateHelperObject(position, direction, jointName, NOT_UNIQUE, NOT_SELECTED, NOT_EDIT_LABEL, FROM_ASSET);
						pHelper->UpdateVarFromObject();
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::RemoveAssetHelpers()
{
	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); )
	{
		bool found = false;

		if ((*it).first->IsKindOf(RUNTIME_CLASS(CVehicleHelper)))
		{
			CVehicleHelper* pHelper = (CVehicleHelper*)(it->first);

			if (pHelper->IsFromGeometry())
			{
				DeleteObject(pHelper);
				DeleteTreeItem(it++);
				found = true;
			}
		}

		if (!found)
		{
			++it;
		}
	}

	GetIEditor()->SetModifiedFlag();
}

void CVehiclePartsPanel::HideAssetHelpers()
{
	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); )
	{
		bool found = false;

		if ((*it).first->IsKindOf(RUNTIME_CLASS(CVehicleHelper)))
		{
			CVehicleHelper* pHelper = (CVehicleHelper*)(it->first);

			if (pHelper->IsFromGeometry())
			{
				DeleteTreeItem(it++);
				pHelper->SetHidden(true);
				found = true;
			}
		}

		if (!found)
		{
			++it;
		}
	}

	GetIEditor()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::FillSeats()
{
	CScopedSuspendUndo susp;
	// get seats, create objs, add to treeview
	HTREEITEM hRoot = m_hVehicle;

	IVariable* pData = GetVariable();

	IVariable* pSeatsVar = GetChildVar(pData, "Seats", false);

	if (pSeatsVar)
	{
		for (int i = 0; i < pSeatsVar->GetNumVariables(); ++i)
		{
			IVariable* pSeatVar = pSeatsVar->GetVariable(i);

			// adds missing child vars
			CVehicleData::FillDefaults(pSeatVar, "Seat", GetChildVar(CVehicleData::GetDefaultVar(), "Seats"));

			// create new seat object
			CVehicleSeat* pSeatObj = (CVehicleSeat*)GetIEditor()->GetObjectManager()->NewObject("VehicleSeat");

			if (!pSeatObj)
			{
				continue;
			}

			pSeatObj->SetHidden(true);
			pSeatObj->SetVehicle(m_pVehicle);
			pSeatObj->SetVariable(pSeatVar);

			HTREEITEM hParentItem;

			// attach to vehicle or parent part, if present
			bool bPart = false;

			if (IVariable* pPartVar = GetChildVar(pSeatVar, "part"))
			{
				CString sPart;
				pPartVar->Get(sPart);
				CVehiclePart* pPartObj = FindPart(sPart);

				if (pPartObj)
				{
					VeedLog("Attaching Seat to part %s", sPart);
					pPartObj->AttachChild(pSeatObj);
					hParentItem = (stl::find_in_map(m_partToTree, pPartObj, STreeItem())).item;
					bPart = true;
				}
			}

			if (!bPart)
			{
				// fallback
				VeedLog("no part found for seat, attaching to vehicle");
				m_pVehicle->AttachChild(pSeatObj);
				hParentItem = m_hVehicle;
			}

			HTREEITEM hItem = InsertTreeItem(pSeatObj, hParentItem);
			pSeatObj->AddEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));

			// create weapon objects
			if (IVariable* pWeapons = GetChildVar(pSeatVar, "Weapons", true))
			{
				if (IVariable* pPrim = GetChildVar(pWeapons, "Primary"))
					for (int i = 0; i < pPrim->GetNumVariables(); ++i)
					{
						CreateWeapon(WEAPON_PRIMARY, pSeatObj, pPrim->GetVariable(i));
					}

				if (IVariable* pSec = GetChildVar(pWeapons, "Secondary"))
					for (int i = 0; i < pSec->GetNumVariables(); ++i)
					{
						CreateWeapon(WEAPON_SECONDARY, pSeatObj, pSec->GetVariable(i));
					}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
CVehicleWeapon* CVehiclePartsPanel::CreateWeapon(int weaponType, CVehicleSeat* pSeat, IVariable* pVar /*=0*/)
{
	CScopedSuspendUndo susp;

	IObjectManager* pObjMan = GetIEditor()->GetObjectManager();
	CVehicleWeapon* pObj = (CVehicleWeapon*)pObjMan->NewObject("VehicleWeapon");

	if (pObj)
	{
		pSeat->AddWeapon(weaponType, pObj, pVar);
		pObj->SetHidden(true);

		InsertTreeItem(pObj, pSeat);

		ReloadPropsCtrl();

		pObj->AddEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));
		return pObj;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::DeleteSeats()
{
	DeleteTreeObjects(RUNTIME_CLASS(CVehicleSeat));
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::DumpPartTreeMap()
{
	int i = 0;
	VeedLog("PartToTreeMap is:");

	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		VeedLog("[%i] %s", i++, (*it).first->GetName());
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::DeleteTreeObjects(const CRuntimeClass* pClass)
{
	CScopedSuspendUndo susp;

#ifdef _DEBUG
	DumpPartTreeMap();
#endif

	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); )
	{
		if ((*it).first->IsKindOf(pClass))
		{
			// delete tree item, delete editor object, erase from map
			m_treeCtrl.DeleteItem((*it).second.item);
			(*it).first->RemoveEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));
			m_pObjMan->DeleteObject(it->first);
			m_partToTree.erase(it++);
		}
		else
		{
			++it;
		}
	}

	GetIEditor()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////////
CVehiclePart* CVehiclePartsPanel::FindPart(CString name)
{
	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		CBaseObject* pObj = (*it).first;

		if (pObj->IsKindOf(RUNTIME_CLASS(CVehiclePart)) && pObj->GetName() == name.GetString())
		{
			return (CVehiclePart*)pObj;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////
CVehiclePart* CVehiclePartsPanel::GetMainPart()
{
	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		if ((*it).first->IsKindOf(RUNTIME_CLASS(CVehiclePart)) && ((CVehiclePart*)it->first)->IsMainPart())
		{
			return (CVehiclePart*)it->first;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::FillComps(IVariablePtr pData)
{
	if (!m_pVehicle)
	{
		return;
	}

	CScopedSuspendUndo susp;

	HTREEITEM hRoot = m_hVehicle;

	IVariable* pComps = GetChildVar(pData, "Components");

	if (pComps)
	{
		int numChildren = pComps->GetNumVariables();

		for (int i = 0; i < numChildren; ++i)
		{
			IVariable* pVar = pComps->GetVariable(i);

			// adds missing child vars
			CVehicleData::FillDefaults(pVar, "Component", GetChildVar(CVehicleData::GetDefaultVar(), "Components"));

			// create new comp object
			CVehicleComponent* pComp = (CVehicleComponent*)GetIEditor()->GetObjectManager()->NewObject("VehicleComponent");

			if (!pComp)
			{
				continue;
			}

			pComp->SetVehicle(m_pVehicle);

			m_pVehicle->AttachChild(pComp);
			pComp->SetVariable(pVar);

			HTREEITEM hItem = InsertTreeItem(pComp, hRoot);
			pComp->AddEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));
		}
	}

	ShowComps(m_toolsPanel.m_bComps);

}

//////////////////////////////////////////////////////////////////////////////
int CVehiclePartsPanel::FillParts(IVariablePtr pData)
{
	CScopedSuspendUndo susp;

	if (!m_pVehicle)
	{
		return 0;
	}

	HTREEITEM hRoot = m_hVehicle;

	IVariable* pParts = GetChildVar(pData, "Parts", false);

	if (pParts && pParts->GetNumVariables() > 0)
	{
		if (IVariable* pFirstPart = pParts->GetVariable(0))
		{
			// mark first part as main
			IVariable* pVar = GetChildVar(pFirstPart, "mainPart", false);

			if (!pVar)
			{
				pVar = new CVariable<bool>;
				pVar->SetName("mainPart");
				pFirstPart->AddVariable(pVar);
			}

			pVar->Set(true);
		}

		AddParts(pParts, m_pVehicle);
	}

	// show/hide stuff according to display settings
	ShowWheels(m_toolsPanel.m_bWheels);

	//GetIEditor()->SetModifiedFlag();
	m_treeCtrl.SortChildren(hRoot);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::AddParts(IVariable* pParts, CBaseObject* pParent)
{
	assert(pParts);

	if (!pParts || !pParent)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[CVehiclePartsPanel::AddParts] ERROR: called with NULL pointer!");
		return;
	}

	for (int i = 0; i < pParts->GetNumVariables(); ++i)
	{
		IVariable* pPartVar = pParts->GetVariable(i);

		if (!pPartVar)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[CVehiclePartsPanel::AddParts] ERROR: pPartVar is NULL!");
		}

		// skip unnamed parts
		if (0 == GetChildVar(pPartVar, "name", false))
		{
			continue;
		}

		// create new part object
		CVehiclePart* pPartObj = (CVehiclePart*)GetIEditor()->GetObjectManager()->NewObject("VehiclePart");

		if (!pPartObj)
		{
			continue;
		}

		pPartObj->SetWorldTM(m_pVehicle->GetWorldTM());
		//pPartObj->SetHidden(true);
		pPartObj->SetVehicle(m_pVehicle);
		pPartObj->SetVariable(pPartVar);

		HTREEITEM hParentItem;

		// attach to vehicle or parent part, if present
		if (pParent == m_pVehicle)
		{
			m_pVehicle->AttachChild(pPartObj);
			hParentItem = m_hVehicle;
		}
		else
		{
			VeedLog("Attaching part %s to parent %s", pPartObj->GetName(), pParent->GetName());
			pParent->AttachChild(pPartObj);
			hParentItem = (stl::find_in_map(m_partToTree, pParent, STreeItem())).item;
			assert(hParentItem);
		}

		if (IVariable* pMainVar = GetChildVar(pPartVar, "mainPart", false))
		{
			pPartObj->SetMainPart(true);
			m_pMainPart = pPartObj;
		}

		int icon = IVeedObject::GetVeedObject(pPartObj)->GetIconIndex();
		HTREEITEM hNewItem = m_treeCtrl.InsertItem(pPartObj->GetName(), icon, icon, hParentItem);
		m_treeCtrl.SetItemData(hNewItem, (DWORD_PTR)pPartObj);
		m_partToTree[pPartObj] = STreeItem(hNewItem, hParentItem);

		pPartObj->AddEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));

		// check if the part has a child parts table
		if (IVariable* pChildParts = GetChildVar(pPartVar, pParts->GetName(), false))
		{
			AddParts(pChildParts, pPartObj); // add children
		}

		// create the helper objects for this part
		if (IVariable* pHelpers = GetChildVar(pPartVar, "Helpers"))
		{
			int nOrigHelpers = pHelpers->GetNumVariables();

			for (int h = 0; h < nOrigHelpers; ++h)
			{
				IVariable* pHelper = pHelpers->GetVariable(h);
				IVariable* pName = GetChildVar(pHelper, "name");
				IVariable* pPos = GetChildVar(pHelper, "position");

				if (pName && pPos)
				{
					CString name("UNKNOWN");
					Vec3 pos;
					pName->Get(name);
					pPos->Get(pos);

					if (IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity())
					{
						pos = pEnt->GetWorldTM().TransformPoint(pos);
					}

					Vec3 dir(FORWARD_DIRECTION);

					if (IVariable* pDir = GetChildVar(pHelper, "direction"))
					{
						pDir->Get(dir);
					}

					CreateHelperObject(pos, dir, name, NOT_UNIQUE, NOT_SELECTED, NOT_EDIT_LABEL, FROM_EDITOR, pHelper);
				}
			}
		}

		// sort children of new part
		m_treeCtrl.SortChildren(hNewItem);
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::FillHelpers(IVariablePtr pData)
{
	IVariable* pHelpers = GetChildVar(pData, "Helpers", false);

	if (pHelpers && pHelpers->GetNumVariables() > 0)
	{
		for (int i = 0; i < pHelpers->GetNumVariables(); ++i)
		{
			IVariable* pHelperVar = pHelpers->GetVariable(i);

			if (!pHelperVar)
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[CVehiclePartsPanel::AddHelpers] ERROR: pHelperVar is NULL!");
			}

			IVariable* pName = GetChildVar(pHelperVar, "name");
			IVariable* pPos = GetChildVar(pHelperVar, "position");

			if (pName && pPos)
			{
				CString name("UNKNOWN");
				Vec3 pos;
				pName->Get(name);
				pPos->Get(pos);

				if (IEntity* pEnt = m_pVehicle->GetCEntity()->GetIEntity())
				{
					pos = pEnt->GetWorldTM().TransformPoint(pos);
				}

				Vec3 dir(FORWARD_DIRECTION);

				if (IVariable* pDir = GetChildVar(pHelperVar, "direction"))
				{
					pDir->Get(dir);
				}

				CreateHelperObject(pos, dir, name, NOT_UNIQUE, NOT_SELECTED, NOT_EDIT_LABEL, FROM_EDITOR, pHelperVar);
			}
		}
	}

	ShowVeedHelpers(m_toolsPanel.m_bVeedHelpers);
	ShowAssetHelpers(m_toolsPanel.m_bAssetHelpers);
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnPaneClose()
{
	ShowAssetHelpers(false);
	//DeleteParts();
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnObjectEvent(CBaseObject* object, int event)
{
	if (object == m_pVehicle)
	{
		if (event == OBJECT_ON_DELETE)
		{
			m_treeCtrl.DeleteItem(TVI_ROOT);
			m_propsCtrl.DeleteAllItems();
		}

		return;
	}

	if (event == OBJECT_ON_RENAME)
	{
		if (HTREEITEM hItem = (stl::find_in_map(m_partToTree, object, STreeItem())).item)
		{
			m_treeCtrl.SetItemText(hItem, object->GetName());
		}
	}
	else if (event == OBJECT_ON_DELETE)
	{
		// remove from tree
		if (HTREEITEM hItem = (stl::find_in_map(m_partToTree, object, STreeItem())).item)
		{
			m_treeCtrl.DeleteItem(hItem);
		}

		m_partToTree.erase(object);
	}
	else if (event == OBJECT_ON_SELECT)
	{
		if (HTREEITEM hItem = (stl::find_in_map(m_partToTree, object, STreeItem())).item)
		{
			m_treeCtrl.SelectItem(hItem);
			m_pSelItem = object;
		}
	}
	else if (event == OBJECT_ON_CHILDATTACHED)
	{
		// check if we created this child ourself (then we know it already)
		CBaseObject* pChild = object->GetChild(object->GetChildCount() - 1);

		if (m_partToTree.find(pChild) == m_partToTree.end())
		{
			// currently only helpers need to be handled
			if (object->IsKindOf(RUNTIME_CLASS(CVehiclePart)) && pChild->IsKindOf(RUNTIME_CLASS(CVehicleHelper)))
			{
				HTREEITEM hItem = InsertTreeItem(pChild, object, false);
				pChild->AddEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));
			}

			UpdateVariables();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	CRect rc;
	GetClientRect(rc);

	// resize splitter window.
	if (m_vSplitter.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_vSplitter.MoveWindow(rc, FALSE);
	}

	m_toolsPanel.RedrawWindow();

	//if (m_treeCtrl.m_hWnd)
	//m_treeCtrl.MoveWindow(0, 0, rc.right, rc.bottom, true);

	//if (m_propsCtrl.m_hWnd)
	// m_propsCtrl.MoveWindow( 0, 300, rc.right, rc.bottom, true );
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::DeleteParts()
{
	CScopedSuspendUndo susp;

	// this function is used for cleanup only and therefore must not delete variables
	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		if (IVeedObject* pVO = IVeedObject::GetVeedObject(it->first))
		{
			pVO->DeleteVar(false);
		}
	}

	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		// only delete top-level parts, they take care of children
		if ((*it).first->GetParent() == (CBaseObject*)m_pVehicle)
		{
			VeedLog("[CVehiclePartsPanel]: calling delete for %s", it->first->GetName());
			(*it).first->RemoveEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));
			m_pObjMan->DeleteObject(it->first);
		}
	}

	m_partToTree.clear();
	GetIEditor()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnClose()
{
	OnPaneClose();
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnDestroy()
{
	OnPaneClose();
}

//////////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::ReloadPropsCtrl()
{
	if (m_propsCtrl.GetRootItem() && m_propsCtrl.GetRootItem()->GetChildCount() > 0)
	{
		if (IVariablePtr pVar = m_propsCtrl.GetRootItem()->GetChild(0)->GetVariable())
		{
			m_propsCtrl.DeleteAllItems();
			CVarBlock* block = new CVarBlock();
			block->AddVariable(pVar);
			m_propsCtrl.AddVarBlock(block);
			m_propsCtrl.ExpandAll();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnSetPartClass(IVariable* pVar)
{
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnEditWheelMaster()
{
	CWheelMasterDialog dlg(this);

	IVariable* pMaster = GetOrCreateChildVar(GetVariable(), "WheelMaster");
	GetOrCreateChildVar(pMaster, "SubPartWheel", true, true);

	dlg.SetVariable(pMaster);

	// gather wheel variables
	std::vector<CVehiclePart*> wheels;
	GetWheels(wheels);
	dlg.SetWheels(&wheels);

	if (dlg.DoModal() == IDOK)
	{
		IVariable* pOrigMaster = pMaster->GetVariable(0);
		IVariable* pEditMaster = dlg.GetVariable()->GetVariable(0);

		// save master
		ReplaceChildVars(pEditMaster, pOrigMaster);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::GetWheels(std::vector<CVehiclePart*>& wheels)
{
	wheels.clear();

	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		if (it->first->IsKindOf(RUNTIME_CLASS(CVehiclePart)))
		{
			CVehiclePart* pPart = (CVehiclePart*)it->first;

			if (pPart->GetPartClass() == PARTCLASS_WHEEL)
			{
				wheels.push_back(pPart);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::NotifyObjectsDeletion(CVehiclePrototype* pProt)
{
	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		(*it).first->RemoveEventListener(functor(*this, &CVehiclePartsPanel::OnObjectEvent));
	}
}

//////////////////////////////////////////////////////////////////////////
void CVehiclePartsPanel::OnScaleHelpers()
{
	CVehicleScaleDialog dlg;
	float scale = 0.f;

	if (dlg.DoModal() != IDOK)
	{
		return;
	}

	scale = dlg.GetScale();

	if (scale <= 0.f)
	{
		CryLog("[VehiclePartsPanel]: scale %f invalid", scale);
		return;
	}

	for (TPartToTreeMap::iterator it = m_partToTree.begin(); it != m_partToTree.end(); ++it)
	{
		IVeedObject* pObj = IVeedObject::GetVeedObject(it->first);

		if (pObj)
		{
			pObj->UpdateScale(scale);
		}
	}
}

