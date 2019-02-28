// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

class CVehicleModificationDialog : public CDialog
{
	DECLARE_DYNAMIC(CVehicleModificationDialog)

public:
	CVehicleModificationDialog(CWnd* pParent = NULL);     // standard constructor
	virtual ~CVehicleModificationDialog(){}

	virtual BOOL OnInitDialog();

	void         SetVariable(IVariable* pVar);
	IVariable*   GetVariable() { return m_pVar; }

	// Dialog Data
	enum { IDD = IDD_VEED_MODS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);      // DDX/DDV support
	DECLARE_MESSAGE_MAP()

	afx_msg void OnSize(UINT nType, int cx, int cy);

	afx_msg void OnCancelClicked();
	afx_msg void OnOkClicked();
	afx_msg void OnSelChanged();
	afx_msg void OnModNew();
	afx_msg void OnModClone();
	afx_msg void OnModDelete();

	void         ReloadModList();
	void         SelectMod(IVariable* pMod);
	IVariable*   GetSelectedVar();

	CPropertyCtrl m_propsCtrl;
	CListBox      m_modList;
	IVariable*    m_pVar;

	int           m_propsLeft, m_propsTop;

};

//////////////////////////////////////////////////////////////////////////////
// CVehicleModificationDialog implementation
//////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CVehicleModificationDialog, CDialog)

CVehicleModificationDialog::CVehicleModificationDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CVehicleModificationDialog::IDD, pParent)
{
	m_pVar = 0;
}

BEGIN_MESSAGE_MAP(CVehicleModificationDialog, CDialog)
ON_WM_SIZE()
ON_BN_CLICKED(IDC_VEEDMOD_CLOSE, OnCancelClicked)
ON_BN_CLICKED(IDC_VEEDMOD_SAVE, OnOkClicked)
ON_LBN_SELCHANGE(IDC_VEEDMOD_LIST, OnSelChanged)
ON_BN_CLICKED(IDC_VEEDMOD_NEW, OnModNew)
ON_BN_CLICKED(IDC_VEEDMOD_CLONE, OnModClone)
ON_BN_CLICKED(IDC_VEEDMOD_DELETE, OnModDelete)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_VEEDMOD_LIST, m_modList);
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::OnCancelClicked()
{
	EndDialog(IDCANCEL);
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::OnOkClicked()
{
	EndDialog(IDOK);
}

#define WND_PROPS_LEFT 282
#define WND_PROPS_TOP  27

//////////////////////////////////////////////////////////////////////////////
BOOL CVehicleModificationDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	WINDOWPLACEMENT wp;
	GetWindowPlacement(&wp);

	int width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
	int height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;

	CWnd* wnd;

	if (wnd = GetDlgItem(IDC_VEEDMOD_NEW))
	{
		WINDOWPLACEMENT wpNew;
		wnd->GetWindowPlacement(&wpNew);

		//m_propsLeft = width - wpNew.rcNormalPosition.right + 20;
		// fixme: the above delivers obviously oversized values, dunno why
		m_propsLeft = 280;

		m_propsTop = wpNew.rcNormalPosition.top;
	}

	CRect clientRect;
	GetClientRect(clientRect);

	CRect rc(m_propsLeft, m_propsTop, clientRect.right - 20, clientRect.bottom - 15);
	m_propsCtrl.Create(WS_CHILD | WS_VISIBLE | WS_BORDER, rc, this);

	ReloadModList();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::SetVariable(IVariable* pVar)
{
	if (pVar)
		m_pVar = pVar->Clone(true);
	else
	{
		m_pVar = new CVariableArray();
		m_pVar->SetName("Modifications");
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::OnSelChanged()
{
	UpdateData(TRUE);
	int idx = m_modList.GetCurSel();
	if (idx != LB_ERR)
	{
		IVariable* pModVar = (IVariable*)m_modList.GetItemDataPtr(idx);

		// update props panel
		_smart_ptr<CVarBlock> block = new CVarBlock();
		block->AddVariable(pModVar);

		m_propsCtrl.DeleteAllItems();
		m_propsCtrl.AddVarBlock(block);

		m_propsCtrl.ExpandAll();

		if (IVariable* pElems = GetChildVar(pModVar, "Elems"))
		{
			m_propsCtrl.ExpandVariableAndChilds(pElems, false);

			CPropertyItem* pItem = m_propsCtrl.FindItemByVar(pElems);

			// expand Elems children
			for (int i = 0; i < pItem->GetChildCount(); ++i)
			{
				CPropertyItem* pElem = pItem->GetChild(i);
				m_propsCtrl.Expand(pElem, true);

				if (IVariable* pId = GetChildVar(pElem->GetVariable(), "idRef"))
				{
					pId->SetFlags(pId->GetFlags() | IVariable::UI_DISABLED);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::OnModNew()
{
	IVariable* pMod = new CVariableArray();
	pMod->SetName("Modification");

	IVariable* pName = new CVariable<string>;
	pName->SetName("name");
	pName->Set("new modification");

	pMod->AddVariable(pName);
	m_pVar->AddVariable(pMod);

	ReloadModList();
	SelectMod(pMod);
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::OnModClone()
{
	IVariable* pMod = GetSelectedVar();
	if (!pMod)
		return;

	IVariable* pClone = pMod->Clone(true);
	m_pVar->AddVariable(pClone);

	ReloadModList();
	SelectMod(pClone);
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::OnModDelete()
{
	int idx = m_modList.GetCurSel();

	IVariable* pMod = GetSelectedVar();
	if (pMod)
	{
		m_modList.DeleteString(m_modList.GetCurSel());
		m_pVar->DeleteVariable(pMod);

	}
	m_propsCtrl.DeleteAllItems();

	if (idx > 0)
	{
		m_modList.SetCurSel(idx - 1);
		OnSelChanged();
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::ReloadModList()
{
	if (m_pVar)
	{
		// fill mods into listbox
		m_modList.ResetContent();

		for (int i = 0; i < m_pVar->GetNumVariables(); ++i)
		{
			IVariable* pMod = m_pVar->GetVariable(i);
			if (IVariable* pName = GetChildVar(pMod, "name"))
			{
				string name;
				pName->Get(name);
				int idx = m_modList.AddString(name);
				m_modList.SetItemDataPtr(idx, pMod);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::SelectMod(IVariable* pMod)
{
	for (int i = 0; i < m_modList.GetCount(); ++i)
	{
		if (m_modList.GetItemDataPtr(i) == pMod)
		{
			m_modList.SetCurSel(i);
			OnSelChanged();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
IVariable* CVehicleModificationDialog::GetSelectedVar()
{
	int idx = m_modList.GetCurSel();
	if (idx != LB_ERR)
		return (IVariable*)m_modList.GetItemDataPtr(idx);

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleModificationDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (m_propsCtrl.GetSafeHwnd())
	{
		CRect rect;
		rect.left = m_propsLeft;
		rect.top = m_propsTop;
		rect.right = cx - 20;
		rect.bottom = cy - 20;
		m_propsCtrl.MoveWindow(rect, TRUE);
	}

	int Buttons[] = { IDC_VEEDMOD_CLOSE, IDC_VEEDMOD_SAVE };

	CWnd* wnd;
	WINDOWPLACEMENT wp;

	GetWindowPlacement(&wp);

	for (int i = 0; i < CRY_ARRAY_COUNT(Buttons); ++i)
	{
		if (wnd = GetDlgItem(Buttons[i]))
		{
			WINDOWPLACEMENT wp2;
			wnd->GetWindowPlacement(&wp2);
			int dy = wp2.rcNormalPosition.bottom - wp2.rcNormalPosition.top;

			CRect rect;
			wnd->GetWindowRect(rect);  // rel to parent
			ScreenToClient(rect);

			//rect.left = wp2.rcNormalPosition.left - wp.rcNormalPosition.left;
			//rect.right = wp2.rcNormalPosition.right - wp.rcNormalPosition.left;
			rect.top = cy - 15 - dy;
			rect.bottom = cy - 15;
			wnd->MoveWindow(rect, TRUE);
		}
	}
}

