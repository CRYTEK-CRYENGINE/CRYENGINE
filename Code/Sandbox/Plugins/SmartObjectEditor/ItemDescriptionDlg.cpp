// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ItemDescriptionDlg.h"
#include "SmartObjectsEditorDialog.h"

// CItemDescriptionDlg dialog

IMPLEMENT_DYNAMIC(CItemDescriptionDlg, CDialog)
CItemDescriptionDlg::CItemDescriptionDlg(CWnd* pParent /*=NULL*/, bool bEditName /*=true*/,
                                         bool bAllowAnyName /*=false*/, bool bLocation /*=false*/, bool bTemplate /*=false*/)
	: CDialog(bLocation ? (bTemplate ? CItemDescriptionDlg::IDD_TEMPLATE : CItemDescriptionDlg::IDD_LOCATION) : CItemDescriptionDlg::IDD, pParent)
	, m_sItemCaption(_T("&Item:"))
	, m_sItemEdit(_T(""))
	, m_sDescription(_T(""))
	, m_sCaption(_T("Item/Description Editor"))
	, m_bEditName(bEditName)
	, m_bAllowAnyName(bAllowAnyName)
	, m_bLocation(bLocation)
	, m_bTemplate(bTemplate)
{
}

CItemDescriptionDlg::~CItemDescriptionDlg()
{
}

void CItemDescriptionDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_ITEMTEXT, m_sItemCaption);
	DDX_Text(pDX, IDC_ITEMEDIT, m_sItemEdit);
	if (m_bLocation)
		DDX_Text(pDX, IDC_LOCATIONEDIT, m_sLocation);
	DDX_Text(pDX, IDC_DESCRIPTIONEDIT, m_sDescription);
	if (m_bTemplate)
		DDX_CBString(pDX, IDC_COMBO1, m_sTemplate);
	DDX_Control(pDX, IDOK, m_btnOk);
}

BEGIN_MESSAGE_MAP(CItemDescriptionDlg, CDialog)
ON_EN_CHANGE(IDC_ITEMEDIT, OnEnChangeItemedit)
ON_EN_CHANGE(IDC_LOCATIONEDIT, OnEnChangeItemedit)
ON_CBN_SELCHANGE(IDC_COMBO1, OnEnChangeItemedit)
ON_EN_CHANGE(IDC_DESCRIPTIONEDIT, OnEnChangeItemedit)
END_MESSAGE_MAP()

// CItemDescriptionDlg message handlers

void CItemDescriptionDlg::OnEnChangeItemedit()
{
	SetDlgItemText(IDCANCEL, "Cancel");
	UpdateData();
	m_btnOk.EnableWindow((m_bAllowAnyName || ValidateItem(m_sItemEdit)) && (!m_bLocation || ValidateLocation(m_sLocation)));
}

bool CItemDescriptionDlg::ValidateItem(const CString& item)
{
	if (item.IsEmpty())
		return false;

	char c = item.GetAt(0);
	if (c < 'A' || c > 'Z' && c < 'a' && c != '_' || c > 'z')
		return false;

	for (int i = 1; i < item.GetLength(); ++i)
	{
		c = item.GetAt(i);
		if (c < '0' || c > '9' && c < 'A' || c > 'Z' && c < 'a' && c != '_' || c > 'z')
			return false;
	}

	return true;
}

bool CItemDescriptionDlg::ValidateLocation(CString& location)
{
	location.Trim(" /\\");
	if (location.IsEmpty())
		return true;
	location.Replace('\\', '/');

	CString token;
	int i = 0;
	while (!(token = location.Tokenize("/", i)).IsEmpty())
	{
		char c = token.GetAt(0);
		if (c < '0' || c > '0' && c < 'A' || c > 'Z' && c < 'a' && c != '_' || c > 'z')
			return false;

		for (int i = 1; i < token.GetLength(); ++i)
		{
			c = token.GetAt(i);
			if (c != ' ' && c < '0' || c > '9' && c < 'A' || c > 'Z' && c < 'a' && c != '_' || c > 'z')
				return false;
		}

		// the last char is not allowed to be a space char
		if (c == ' ')
			return false;
	}

	return true;
}

BOOL CItemDescriptionDlg::OnInitDialog()
{
	if (m_bTemplate)
	{
		CComboBox* pComboBox = (CComboBox*) GetDlgItem(IDC_COMBO1);
		if (pComboBox)
		{
			bool templateNameExists = false;
			pComboBox->AddString("");
			CSOLibrary::VectorClassTemplateData& templates = CSOLibrary::GetClassTemplates();
			CSOLibrary::VectorClassTemplateData::iterator it, itEnd = templates.end();
			for (it = templates.begin(); it != itEnd; ++it)
			{
				if (stricmp(it->name, m_sTemplate) == 0)
				{
					templateNameExists = true;
					pComboBox->AddString(m_sTemplate);
				}
				else
					pComboBox->AddString(it->name);
			}
			if (!templateNameExists)
				pComboBox->AddString(m_sTemplate);
			pComboBox->SetItemHeight(0, int(pComboBox->GetItemHeight(0) * 1.25f));
		}
	}
	CDialog::OnInitDialog();

	GetDlgItem(IDC_ITEMEDIT)->EnableWindow(m_bEditName);
	SetWindowText(m_sCaption);

	if (m_bAllowAnyName)
	{
		SetDlgItemText(IDCANCEL, "Cancel");
		m_btnOk.EnableWindow(TRUE);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

