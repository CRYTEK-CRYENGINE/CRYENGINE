// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EquipPackDialog.h"
#include "EquipPackLib.h"
#include "EquipPack.h"
#include "Dialogs/QStringDialog.h"
#include "GameEngine.h"
#include <CrySandbox/IEditorGame.h>
#include "UserMessageDefines.h"
#include "Controls/QuestionDialog.h"
#include "FileDialogs/SystemFileDialog.h"
#include "Dialogs/BaseFrameWnd.h"
#include "Dialogs/QStringDialog.h"
#include "QtViewPane.h"

class CEquipPackEditor : public CBaseFrameWnd
{
	DECLARE_DYNCREATE(CEquipPackEditor)

	DECLARE_MESSAGE_MAP()

	CEquipPackEditor()
		: m_equipDialog(this)
	{
		CRect rc(0, 0, 0, 0);
		Create(WS_CHILD | WS_VISIBLE, rc, AfxGetMainWnd());
	}

	BOOL virtual OnInitDialog() override
	{
		CBaseFrameWnd::OnInitDialog();

		m_equipDialog.Create(CEquipPackDialog::IDD, this);
		m_equipDialog.ModifyStyle(WS_THICKFRAME | WS_DLGFRAME | WS_BORDER, WS_CHILD | DS_CONTROL);
		m_equipDialog.ModifyStyleEx(WS_EX_DLGMODALFRAME, 0);
		m_equipDialog.SetParent(this);
		m_equipDialog.ShowWindow(SW_SHOW);

		CRect rc;
		GetWindowRect(rc);
		m_equipDialog.MoveWindow(rc);
		return TRUE;
	}

	void OnSize(UINT nType, int cx, int cy)
	{
		CBaseFrameWnd::OnSize(nType, cx, cy);
		CRect rc(0, 0, cx, cy);
		m_equipDialog.MoveWindow(rc);
	}
private:
	CEquipPackDialog m_equipDialog;
};

BEGIN_MESSAGE_MAP(CEquipPackEditor, CBaseFrameWnd)
	ON_WM_SIZE()
END_MESSAGE_MAP()

class CEquipPackEditorClass : public IViewPaneClass
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID()   override { return ESYSTEM_CLASS_VIEWPANE; };
	virtual const char*    ClassName()       override { return "EquipPackEditor"; };
	virtual const char*    Category()        override { return "Editor"; };
	virtual const char*    GetMenuPath()     override { return "Deprecated"; }
	virtual CRuntimeClass* GetRuntimeClass() override { return RUNTIME_CLASS(CEquipPackEditor); };
	virtual const char*    GetPaneTitle()    override { return _T("Equip Pack Editor"); };
	virtual bool           SinglePane()      override { return true; };
};

REGISTER_CLASS_DESC(CEquipPackEditorClass);

IMPLEMENT_DYNCREATE(CEquipPackEditor, CBaseFrameWnd);

// CEquipPackDialog dialog
IMPLEMENT_DYNAMIC(CEquipPackDialog, CXTResizeDialog)
CEquipPackDialog::CEquipPackDialog(CWnd* pParent /*=NULL*/)
	: CXTResizeDialog(CEquipPackDialog::IDD, pParent), m_pEquipPacks(nullptr)
{
	m_pEquipPacks = &CEquipPackLib::GetRootEquipPack();
	m_sCurrEquipPack = "";
}

CEquipPackDialog::~CEquipPackDialog()
{
}

void CEquipPackDialog::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROPERTIES, m_AmmoPropWnd);
	DDX_Control(pDX, IDOK, m_OkBtn);
	DDX_Control(pDX, IDC_EXPORT, m_ExportBtn);
	DDX_Control(pDX, IDC_ADD, m_AddBtn);
	DDX_Control(pDX, IDC_DELETE, m_DeleteBtn);
	DDX_Control(pDX, IDC_RENAME, m_RenameBtn);
	DDX_Control(pDX, IDC_EQUIPPACK, m_EquipPacksList);
	DDX_Control(pDX, IDC_EQUIPAVAILLST, m_AvailEquipList);
	DDX_Control(pDX, IDC_EQUIPUSEDLST, m_EquipList);
	DDX_Control(pDX, IDC_INSERT, m_InsertBtn);
	DDX_Control(pDX, IDC_REMOVE, m_RemoveBtn);
	DDX_Control(pDX, IDC_PRIMARY, m_PrimaryEdit);
}

void CEquipPackDialog::UpdateEquipPacksList()
{
	m_EquipPacksList.ResetContent();
	const TEquipPackMap& mapEquipPacks = m_pEquipPacks->GetEquipmentPacks();
	for (TEquipPackMap::const_iterator It = mapEquipPacks.begin(); It != mapEquipPacks.end(); ++It)
	{
		m_EquipPacksList.AddString(It->first);
	}

	int nCurSel = m_EquipPacksList.FindStringExact(0, m_sCurrEquipPack);
	const int nEquipPacks = m_pEquipPacks->Count();
	if (!nEquipPacks)
		nCurSel = -1;
	else if (nEquipPacks <= nCurSel)
		nCurSel = 0;
	else if (nCurSel == -1)
		nCurSel = 0;
	m_EquipPacksList.SetCurSel(nCurSel);
	m_DeleteBtn.EnableWindow(nCurSel != -1);
	m_RenameBtn.EnableWindow(nCurSel != -1);
	UpdateEquipPackParams();
}

void CEquipPackDialog::UpdateEquipPackParams()
{
	m_AvailEquipList.ResetContent();
	m_EquipList.DeleteAllItems();
	m_AmmoPropWnd.DeleteAllItems();

	int nItem = m_EquipPacksList.GetCurSel();
	m_ExportBtn.EnableWindow(nItem != -1);
	if (nItem == -1)
	{
		m_sCurrEquipPack = "";
		return;
	}
	m_EquipPacksList.GetLBText(nItem, m_sCurrEquipPack);
	CString sName;
	m_EquipPacksList.GetLBText(nItem, sName);
	CEquipPack* pEquip = m_pEquipPacks->FindEquipPack(sName.GetString());
	if (!pEquip)
	{
		return;
	}
	TEquipmentVec& equipVec = pEquip->GetEquip();

	// add in the order equipments appear in the pack
	for (TEquipmentVec::iterator iter = equipVec.begin(), iterEnd = equipVec.end(); iter != iterEnd; ++iter)
	{
		const SEquipment& equip = *iter;

		AddEquipmentListItem(equip);
	}

	// add all other items to available list
	for (TLstStringIt iter = m_lstAvailEquip.begin(), iterEnd = m_lstAvailEquip.end(); iter != iterEnd; ++iter)
	{
		const SEquipment& equip = *iter;
		if (std::find(equipVec.begin(), equipVec.end(), equip) == equipVec.end())
			m_AvailEquipList.AddString(equip.sName);
	}

	UpdatePrimary();

	m_AmmoPropWnd.AddVarBlock(CreateVarBlock(pEquip));
	m_AmmoPropWnd.SetUpdateCallback(functor(*this, &CEquipPackDialog::AmmoUpdateCallback));
	m_AmmoPropWnd.EnableUpdateCallback(true);
	m_AmmoPropWnd.ExpandAllChilds(m_AmmoPropWnd.GetRootItem(), true);

	// update lists
	m_AvailEquipList.SetCurSel(0);
	OnLbnSelchangeEquipavaillst();
	OnTvnSelchangedEquipusedlst(NULL, NULL);
}

void CEquipPackDialog::AddEquipmentListItem(const SEquipment& equip)
{
	CString sSetup = equip.sSetup;
	HTREEITEM parent = m_EquipList.InsertItem(equip.sName);

	IEquipmentSystemInterface::IEquipmentItemIteratorPtr accessoryIter = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface()->CreateEquipmentAccessoryIterator(equip.sName);

	if (accessoryIter)
	{
		IEquipmentSystemInterface::IEquipmentItemIterator::SEquipmentItem accessoryItem;
		while (accessoryIter->Next(accessoryItem))
		{
			HTREEITEM categoryItem = NULL;

			if (accessoryItem.type && accessoryItem.type[0])
			{
				categoryItem = m_EquipList.GetChildItem(parent);

				while (categoryItem != NULL)
				{
					if (stricmp(m_EquipList.GetItemText(categoryItem), accessoryItem.type) == 0)
					{
						break;
					}

					categoryItem = m_EquipList.GetNextSiblingItem(categoryItem);
				}

				if (categoryItem == NULL)
				{
					categoryItem = m_EquipList.InsertItem(accessoryItem.type, parent);
				}
			}

			HTREEITEM accessory = m_EquipList.InsertItem(accessoryItem.name, categoryItem);

			CString formattedName;
			formattedName.Format("|%s|", accessoryItem.name);

			if (strstr(sSetup, formattedName) != NULL)
			{
				m_EquipList.SetCheck(accessory, true);
				m_EquipList.SetCheck(categoryItem, true);
				m_EquipList.SetCheck(parent, true);
			}
		}
	}
}

CVarBlockPtr CEquipPackDialog::CreateVarBlock(CEquipPack* pEquip)
{
	CVarBlock* pVarBlock = m_pVarBlock ? m_pVarBlock->Clone(true) : new CVarBlock();

	TAmmoVec notFoundAmmoVec;
	TAmmoVec& ammoVec = pEquip->GetAmmoVec();
	for (TAmmoVec::iterator iter = ammoVec.begin(); iter != ammoVec.end(); ++iter)
	{
		// check if var block contains all variables
		IVariablePtr pVar = pVarBlock->FindVariable(iter->sName);
		if (pVar == 0)
		{
			// pVar = new CVariable<int>;
			// pVar->SetFlags(IVariable::UI_DISABLED);
			// pVarBlock->AddVariable(pVar);
			notFoundAmmoVec.push_back(*iter);
		}
		else
		{
			pVar->SetName(iter->sName);
			pVar->Set(iter->nAmount);
		}
	}

	if (notFoundAmmoVec.empty() == false)
	{
		CString notFoundString;
		notFoundString.Format("The following ammo items in pack '%s' could not be found.\r\nDo you want to permanently remove them?\r\n\r\n", pEquip->GetName());
		TAmmoVec::iterator iter = notFoundAmmoVec.begin();
		TAmmoVec::iterator iterEnd = notFoundAmmoVec.end();
		while (iter != iterEnd)
		{
			notFoundString.AppendFormat("'%s'\r\n", iter->sName);
			++iter;
		}

		if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(notFoundString)))
		{
			iter = notFoundAmmoVec.begin();
			while (iter != iterEnd)
			{
				const SAmmo& ammo = *iter;
				stl::find_and_erase(ammoVec, ammo);
				++iter;
			}
			pEquip->SetModified(true);
		}
		else
		{
			// add missing ammos, but disabled
			iter = notFoundAmmoVec.begin();
			while (iter != iterEnd)
			{
				IVariablePtr pVar = pVarBlock->FindVariable(iter->sName);
				if (pVar == 0)
				{
					pVar = new CVariable<int>;
					pVar->SetFlags(IVariable::UI_DISABLED);
					pVarBlock->AddVariable(pVar);
				}
				pVar->SetName(iter->sName);
				pVar->Set(iter->nAmount);
				++iter;
			}
		}
		notFoundAmmoVec.resize(0);
	}

	int nCount = pVarBlock->GetNumVariables();
	for (int i = 0; i < nCount; ++i)
	{
		IVariable* pVar = pVarBlock->GetVariable(i);
		SAmmo ammo;
		ammo.sName = pVar->GetName();
		if (std::find(ammoVec.begin(), ammoVec.end(), ammo) == ammoVec.end())
		{
			pVar->Get(ammo.nAmount);
			pEquip->AddAmmo(ammo);
			notFoundAmmoVec.push_back(ammo);
		}
	}

	if (notFoundAmmoVec.empty() == false)
	{
		CString notFoundString;
		notFoundString.Format("The following ammo items have been automatically added to pack '%s':\r\n\r\n", pEquip->GetName());
		TAmmoVec::iterator iter = notFoundAmmoVec.begin();
		TAmmoVec::iterator iterEnd = notFoundAmmoVec.end();
		while (iter != iterEnd)
		{
			notFoundString.AppendFormat("'%s'\r\n", iter->sName);
			++iter;
		}

		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr(notFoundString));
	}
	return pVarBlock;
}

void CEquipPackDialog::UpdatePrimary()
{
	CString name;
	if (m_EquipList.GetCount() > 0)
	{
		name = m_EquipList.GetItemText(m_EquipList.GetRootItem());
	}
	m_PrimaryEdit.SetWindowText(name);
}

void CEquipPackDialog::AmmoUpdateCallback(IVariable* pVar)
{
	CEquipPack* pEquip = m_pEquipPacks->FindEquipPack(m_sCurrEquipPack.GetString());
	if (!pEquip)
		return;
	SAmmo ammo;
	ammo.sName = pVar->GetName();
	pVar->Get(ammo.nAmount);
	pEquip->AddAmmo(ammo);
}

SEquipment* CEquipPackDialog::GetEquipment(CString sDesc)
{
	SEquipment Equip;
	Equip.sName = sDesc;
	TLstStringIt It = std::find(m_lstAvailEquip.begin(), m_lstAvailEquip.end(), Equip);
	if (It == m_lstAvailEquip.end())
		return NULL;
	return &(*It);
}

BEGIN_MESSAGE_MAP(CEquipPackDialog, CXTResizeDialog)
ON_CBN_SELCHANGE(IDC_EQUIPPACK, OnCbnSelchangeEquippack)
ON_BN_CLICKED(IDC_ADD, OnBnClickedAdd)
//	ON_BN_CLICKED(IDC_REFRESH, OnBnClickedRefresh)
ON_BN_CLICKED(IDC_DELETE, OnBnClickedDelete)
ON_BN_CLICKED(IDC_RENAME, OnBnClickedRename)
ON_BN_CLICKED(IDC_INSERT, OnBnClickedInsert)
ON_BN_CLICKED(IDC_REMOVE, OnBnClickedRemove)
ON_BN_CLICKED(IDC_MOVEUP, OnBnClickedMoveUp)
ON_BN_CLICKED(IDC_MOVEDOWN, OnBnClickedMoveDown)
ON_LBN_SELCHANGE(IDC_EQUIPAVAILLST, OnLbnSelchangeEquipavaillst)
//	ON_WM_DESTROY()
ON_BN_CLICKED(IDC_IMPORT, OnBnClickedImport)
ON_BN_CLICKED(IDC_EXPORT, OnBnClickedExport)
ON_NOTIFY(TVN_SELCHANGED, IDC_EQUIPUSEDLST, &CEquipPackDialog::OnTvnSelchangedEquipusedlst)
ON_NOTIFY(NM_CLICK, IDC_EQUIPUSEDLST, &CEquipPackDialog::OnNMClickEquipusedlst)
ON_NOTIFY(TVN_KEYDOWN, IDC_EQUIPUSEDLST, &CEquipPackDialog::OnTVNKeyDownEquipusedlst)
ON_MESSAGE(UM_EQUIPLIST_CHECKSTATECHANGE, &CEquipPackDialog::OnCheckStateChange)
ON_BN_CLICKED(IDCANCEL, &CEquipPackDialog::OnBnClickedCancel)
ON_BN_CLICKED(IDOK, &CEquipPackDialog::OnBnClickedOk)
END_MESSAGE_MAP()

// CEquipPackDialog message handlers

BOOL CEquipPackDialog::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	SetResize(IDOK, SZ_REPOS(1));
	SetResize(IDCANCEL, SZ_REPOS(1));
	SetResize(IDC_EXPORT, SZ_HORREPOS(1));
	SetResize(IDC_IMPORT, SZ_HORREPOS(1));
	SetResize(IDC_ADD, SZ_HORREPOS(1));
	SetResize(IDC_DELETE, SZ_HORREPOS(1));
	SetResize(IDC_RENAME, SZ_HORREPOS(1));

	SetResize(IDC_EQUIPPACK, CXTResizeRect(0, 0, 1, 0));
	SetResize(IDC_EQUIPAVAILLST, CXTResizeRect(0, 0, 0, 1));
	SetResize(IDC_EQUIPUSEDLST, CXTResizeRect(0, 0, 0, 1));
	SetResize(IDC_PROPERTIES, CXTResizeRect(0, 0, 1, 1));
	SetResize(IDC_INSERT, CXTResizeRect(0, 1, 0, 1));
	SetResize(IDC_REMOVE, CXTResizeRect(0, 1, 0, 1));
	SetResize(IDC_STATIC_NOTE, CXTResizeRect(0, 1, 0, 1));
	SetResize(IDC_PRIMARY_STATIC, CXTResizeRect(0, 1, 0, 1));
	SetResize(IDC_PRIMARY, CXTResizeRect(0, 1, 0, 1));
	SetResize(IDC_MOVEUP, CXTResizeRect(0, 1, 0, 1));
	SetResize(IDC_MOVEDOWN, CXTResizeRect(0, 1, 0, 1));

	m_sCurrEquipPack = "Player_Default";

	m_lstAvailEquip.clear();

	if (!GetIEditor()->GetGameEngine() || !GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface())
		return TRUE;

	// Fill available weapons ListBox with enumerated weapons from game.
	IEquipmentSystemInterface::IEquipmentItemIteratorPtr iter = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface()->CreateEquipmentItemIterator("ItemGivable");
	IEquipmentSystemInterface::IEquipmentItemIterator::SEquipmentItem item;

	if (iter)
	{
		SEquipment Equip;
		while (iter->Next(item))
		{
			Equip.sName = item.name;
			Equip.sType = item.type;
			m_lstAvailEquip.push_back(Equip);
		}
	}
	else
		CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr("GetAvailableWeaponNames() returned NULL. Unable to retrieve weapons."));

	m_pVarBlock = new CVarBlock();
	iter = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface()->CreateEquipmentItemIterator("Ammo");
	if (iter)
	{
		while (iter->Next(item))
		{
			static const int defaultAmount = 0;
			IVariablePtr pVar = new CVariable<int>;
			pVar->SetName(item.name);
			pVar->Set(defaultAmount);
			m_pVarBlock->AddVariable(pVar);
		}
	}
	else
		CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr("GetAvailableWeaponNames() returned NULL. Unable to retrieve weapons."));

	m_EquipList.ModifyStyle(0, TVS_CHECKBOXES);

	UpdateEquipPacksList();

	// Simulate resize of the properties window, to force redraw scrollbar.
	CRect rc;
	m_AmmoPropWnd.GetClientRect(rc);
	m_AmmoPropWnd.SendMessage(WM_SIZE, (WPARAM)SIZE_RESTORED, MAKELPARAM(rc.Width(), rc.Height()));

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CEquipPackDialog::OnCbnSelchangeEquippack()
{
	UpdateEquipPackParams();
}

void CEquipPackDialog::OnBnClickedAdd()
{
	QStringDialog Dlg("Enter name for new Equipment-Pack");
	if (Dlg.exec() != QDialog::Accepted)
		return;
	if (Dlg.GetString().IsEmpty())
		return;
	CString packName = Dlg.GetString();
	if (!m_pEquipPacks->CreateEquipPack(packName.GetString()))
	{
		CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr("An Equipment-Pack with this name exists already !"));
	}
	else
	{
		m_sCurrEquipPack = packName;
		UpdateEquipPacksList();
	}
}

void CEquipPackDialog::OnBnClickedDelete()
{
	if (QDialogButtonBox::StandardButton::No == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Are you sure ?")))
	{
		return;
	}

	CString sName;
	m_EquipPacksList.GetLBText(m_EquipPacksList.GetCurSel(), sName);
	if (!m_pEquipPacks->RemoveEquipPack(sName.GetString(), true))
	{
		CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr("Unable to delete Equipment-Pack !"));
	}
	else
	{
		UpdateEquipPacksList();
	}
}

void CEquipPackDialog::OnBnClickedRename()
{
	CString sName;
	m_EquipPacksList.GetLBText(m_EquipPacksList.GetCurSel(), sName);
	QStringDialog Dlg("Enter new name for Equipment-Pack");
	if (Dlg.exec() != QDialog::Accepted)
		return;
	if (Dlg.GetString().IsEmpty())
		return;
	if (!m_pEquipPacks->RenameEquipPack(sName.GetString(), Dlg.GetString().GetString()))
	{
		CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr("Unable to rename Equipment-Pack ! Probably the new name is already used."));
	}
	else
	{
		UpdateEquipPacksList();
	}
}

void CEquipPackDialog::OnBnClickedInsert()
{
	int nItem = m_EquipPacksList.GetCurSel();
	if (nItem == -1)
		return;
	CString sName;
	m_EquipPacksList.GetLBText(nItem, sName);
	CEquipPack* pEquipPack = m_pEquipPacks->FindEquipPack(sName.GetString());
	if (!pEquipPack)
		return;

	int currSelected = m_AvailEquipList.GetCurSel();
	m_AvailEquipList.GetText(currSelected, sName);
	SEquipment* pEquip = GetEquipment(sName);
	if (!pEquip)
		return;

	pEquipPack->AddEquip(*pEquip);
	AddEquipmentListItem(*pEquip);
	m_AvailEquipList.DeleteString(m_AvailEquipList.GetCurSel());

	currSelected = min(currSelected, m_AvailEquipList.GetCount() - 1);
	if (currSelected >= 0)
	{
		m_AvailEquipList.SetCurSel(currSelected);
	}

	OnLbnSelchangeEquipavaillst();
}

void CEquipPackDialog::OnBnClickedRemove()
{
	int nItem = m_EquipPacksList.GetCurSel();
	if (nItem == -1)
		return;
	CString sName;
	m_EquipPacksList.GetLBText(nItem, sName);
	CEquipPack* pEquipPack = m_pEquipPacks->FindEquipPack(sName.GetString());
	if (!pEquipPack)
		return;

	HTREEITEM selectedItem = m_EquipList.GetSelectedItem();

	if (selectedItem != NULL)
	{
		sName = m_EquipList.GetItemText(selectedItem);
		SEquipment* pEquip = GetEquipment(sName);

		if (!pEquip)
			return;

		pEquipPack->RemoveEquip(*pEquip);

		m_AvailEquipList.AddString(sName);
		m_EquipList.DeleteItem(selectedItem);
	}

	OnLbnSelchangeEquipavaillst();
}

void CEquipPackDialog::StoreUsedEquipmentList()
{
	int nItem = m_EquipPacksList.GetCurSel();
	if (nItem == -1)
		return;

	CString sName;
	m_EquipPacksList.GetLBText(nItem, sName);

	CEquipPack* pEquipPack = m_pEquipPacks->FindEquipPack(sName.GetString());
	if (!pEquipPack)
		return;

	pEquipPack->Clear();

	HTREEITEM item = m_EquipList.GetRootItem();

	while (item != NULL)
	{
		sName = m_EquipList.GetItemText(item);
		SEquipment* pEquip = GetEquipment(sName);
		if (pEquip)
		{
			SEquipment newEquip;
			newEquip.sName = pEquip->sName;
			newEquip.sType = pEquip->sType;
			CString setupString = "|";

			if (m_EquipList.GetCheck(item))
			{
				HTREEITEM child = m_EquipList.GetChildItem(item);

				while (child != NULL)
				{
					bool isChecked = m_EquipList.GetCheck(child);

					if (isChecked)
					{
						if (m_EquipList.ItemHasChildren(child))
						{
							HTREEITEM childsChild = m_EquipList.GetChildItem(child);

							while (childsChild != NULL)
							{
								if (m_EquipList.GetCheck(childsChild))
								{
									setupString.AppendFormat("%s%s", m_EquipList.GetItemText(childsChild), "|");
									childsChild = NULL;
								}
								else
								{
									childsChild = m_EquipList.GetNextSiblingItem(childsChild);
								}
							}
						}
						else
						{
							setupString.AppendFormat("%s%s", m_EquipList.GetItemText(child), "|");
						}
					}

					child = m_EquipList.GetNextSiblingItem(child);
				}
			}

			if (setupString != "|")
			{
				newEquip.sSetup = setupString;
			}

			pEquipPack->AddEquip(newEquip);
		}

		item = m_EquipList.GetNextSiblingItem(item);
	}
}

void CEquipPackDialog::CopyChildren(HTREEITEM copyToItem, HTREEITEM copyFromItem)
{
	HTREEITEM copyFromChild = m_EquipList.GetChildItem(copyFromItem);

	while (copyFromChild != NULL)
	{
		HTREEITEM newChild = m_EquipList.InsertItem(m_EquipList.GetItemText(copyFromChild), copyToItem);
		m_EquipList.SetCheck(newChild, m_EquipList.GetCheck(copyFromChild));

		if (m_EquipList.ItemHasChildren(copyFromChild))
		{
			CopyChildren(newChild, copyFromChild);
		}

		m_EquipList.Expand(newChild, (m_EquipList.GetItemState(copyFromChild, TVIS_EXPANDED) & TVIS_EXPANDED) ? TVE_EXPAND : TVE_COLLAPSE);

		copyFromChild = m_EquipList.GetNextSiblingItem(copyFromChild);
	}
}

HTREEITEM CEquipPackDialog::MoveEquipmentListItem(HTREEITEM existingItem, HTREEITEM insertAfter)
{
	HTREEITEM newItem = m_EquipList.InsertItem(m_EquipList.GetItemText(existingItem), NULL, insertAfter);
	m_EquipList.SetCheck(newItem, m_EquipList.GetCheck(existingItem));

	CopyChildren(newItem, existingItem);

	m_EquipList.Expand(newItem, (m_EquipList.GetItemState(existingItem, TVIS_EXPANDED) & TVIS_EXPANDED) ? TVE_EXPAND : TVE_COLLAPSE);

	m_EquipList.DeleteItem(existingItem);

	return newItem;
}

void CEquipPackDialog::OnBnClickedMoveUp()
{
	HTREEITEM selectedItem = m_EquipList.GetSelectedItem();
	if (selectedItem == m_EquipList.GetRootItem())
		return; // can't move up first item

	if (m_EquipList.GetParentItem(selectedItem) != NULL)
		return; // move should only be available for root level items

	HTREEITEM prevItem = m_EquipList.GetPrevSiblingItem(selectedItem);

	MoveEquipmentListItem(prevItem, selectedItem);

	StoreUsedEquipmentList();
	m_EquipList.SelectItem(selectedItem);
	OnTvnSelchangedEquipusedlst(NULL, NULL);
	UpdatePrimary();
}

void CEquipPackDialog::OnBnClickedMoveDown()
{
	HTREEITEM selectedItem = m_EquipList.GetSelectedItem();

	HTREEITEM nextItem = m_EquipList.GetNextSiblingItem(selectedItem);

	if (nextItem == NULL)
		return; // can't move down last item

	if (m_EquipList.GetParentItem(selectedItem) != NULL)
		return; // move should only be available for root level items

	HTREEITEM newItem = MoveEquipmentListItem(selectedItem, nextItem);

	StoreUsedEquipmentList();
	m_EquipList.SelectItem(newItem);
	OnTvnSelchangedEquipusedlst(NULL, NULL);
	UpdatePrimary();
}

void CEquipPackDialog::OnLbnSelchangeEquipavaillst()
{
	m_InsertBtn.EnableWindow(m_AvailEquipList.GetCount() != 0);
}

void CEquipPackDialog::OnBnClickedImport()
{
	CSystemFileDialog::RunParams params;
	params.title = ("Import Equipment Pack");
	params.extensionFilters << CExtensionFilter(QObject::tr("Equipment Pack Files"), "eqp");

	auto filepath = CSystemFileDialog::RunImportFile(params);
	if (!filepath.isEmpty())
	{
		XmlNodeRef root = XmlHelpers::LoadXmlFromFile(filepath.toStdString().c_str());
		if (root)
		{
			m_pEquipPacks->Serialize(root, true);
			UpdateEquipPacksList();
		}
	}
}

void CEquipPackDialog::OnBnClickedExport()
{
	CSystemFileDialog::RunParams params;
	params.title = ("Export Equipment Pack");
	params.extensionFilters << CExtensionFilter(QObject::tr("Equipment Pack Files"), "eqp");

	auto filepath = CSystemFileDialog::RunExportFile(params);
	if (!filepath.isEmpty())
	{
		XmlNodeRef root = XmlHelpers::CreateXmlNode("EquipPackDB");
		m_pEquipPacks->Serialize(root, false);
		XmlHelpers::SaveXmlNode(root, filepath.toStdString().c_str());

	}
}

void CEquipPackDialog::OnTvnSelchangedEquipusedlst(NMHDR* pNMHDR, LRESULT* pResult)
{
	HTREEITEM selectedItem = m_EquipList.GetSelectedItem();

	bool showButtons = (selectedItem != NULL) && !m_EquipList.GetParentItem(selectedItem);

	m_RemoveBtn.EnableWindow(showButtons);
	GetDlgItem(IDC_MOVEUP)->EnableWindow(showButtons && selectedItem != m_EquipList.GetRootItem());
	GetDlgItem(IDC_MOVEDOWN)->EnableWindow(showButtons && m_EquipList.GetNextSiblingItem(selectedItem) != NULL);

	if (pResult)
	{
		*pResult = 0;
	}
}

void CEquipPackDialog::OnNMClickEquipusedlst(NMHDR* pNMHDR, LRESULT* pResult)
{
	DWORD dwpos = GetMessagePos();

	TVHITTESTINFO ht;
	ht.pt.x = GET_X_LPARAM(dwpos);
	ht.pt.y = GET_Y_LPARAM(dwpos);
	ht.flags = 0;
	ht.hItem = NULL;
	::MapWindowPoints(HWND_DESKTOP, pNMHDR->hwndFrom, &ht.pt, 1);

	m_EquipList.HitTest(&ht);

	if (TVHT_ONITEMSTATEICON & ht.flags)
	{
		PostMessage(UM_EQUIPLIST_CHECKSTATECHANGE, 0, (LPARAM)ht.hItem);
	}

	*pResult = 0;
}

void CEquipPackDialog::OnTVNKeyDownEquipusedlst(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTVKEYDOWN lpNMKey = (LPNMTVKEYDOWN) pNMHDR;

	if (lpNMKey->wVKey == VK_SPACE)
	{
		HTREEITEM selectedItem = m_EquipList.GetSelectedItem();
		if (selectedItem)
		{
			PostMessage(UM_EQUIPLIST_CHECKSTATECHANGE, 0, (LPARAM)selectedItem);
		}
	}
}

LRESULT CEquipPackDialog::OnCheckStateChange(WPARAM wparam, LPARAM lparam)
{
	HTREEITEM checkItem = (HTREEITEM)(lparam);

	bool itemHasChildren = m_EquipList.ItemHasChildren(checkItem);

	if (!m_EquipList.GetCheck(checkItem))
	{
		// if unchecking a parent, then uncheck all children too
		if (itemHasChildren)
		{
			SetChildCheckState(checkItem, false);
		}
	}
	else if (!itemHasChildren)
	{
		//check it has a category and item parent, and uncheck siblings if true
		HTREEITEM categoryParent = m_EquipList.GetParentItem(checkItem);

		if (categoryParent)
		{
			HTREEITEM itemParent = m_EquipList.GetParentItem(categoryParent);

			if (itemParent)
			{
				SetChildCheckState(categoryParent, false, checkItem);
			}
		}
		else
		{
			//if item has no parent and no children force unchecked
			m_EquipList.SetCheck(checkItem, false);
		}
	}

	m_EquipList.SelectItem(checkItem);

	UpdateCheckState(checkItem);
	StoreUsedEquipmentList();

	return 0;
}

void CEquipPackDialog::SetChildCheckState(HTREEITEM checkItem, bool checkState, HTREEITEM ignoreItem)
{
	HTREEITEM childItem = m_EquipList.GetChildItem(checkItem);

	while (childItem != NULL)
	{
		if (childItem != ignoreItem)
		{
			m_EquipList.SetCheck(childItem, checkState);

			if (m_EquipList.ItemHasChildren(childItem))
			{
				SetChildCheckState(childItem, checkState);
			}
		}

		childItem = m_EquipList.GetNextSiblingItem(childItem);
	}
}

void CEquipPackDialog::UpdateCheckState(HTREEITEM checkItem)
{
	if (checkItem != NULL)
	{
		if (m_EquipList.ItemHasChildren(checkItem))
		{
			m_EquipList.SetCheck(checkItem, HasCheckedChild(checkItem));
		}

		UpdateCheckState(m_EquipList.GetParentItem(checkItem));
	}
}

bool CEquipPackDialog::HasCheckedChild(HTREEITEM parentItem)
{
	HTREEITEM childItem = m_EquipList.GetChildItem(parentItem);

	while (childItem != NULL)
	{
		if (m_EquipList.GetCheck(childItem))
		{
			return true;
		}

		childItem = m_EquipList.GetNextSiblingItem(childItem);
	}

	return false;
}

void CEquipPackDialog::OnBnClickedCancel()
{
	//reload from files to overwrite local changes
	m_pEquipPacks->Reset();
	m_pEquipPacks->LoadLibs(true);
	OnCancel();
}

void CEquipPackDialog::OnBnClickedOk()
{
	StoreUsedEquipmentList();
	m_pEquipPacks->SaveLibs(true);
	OnOK();
}

