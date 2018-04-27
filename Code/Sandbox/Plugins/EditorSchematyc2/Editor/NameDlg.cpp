// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NameDlg.h"

#include "Resource.h"
#include "PluginUtils.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	BEGIN_MESSAGE_MAP(CNameEditCtrl, CEdit)
		ON_WM_GETDLGCODE()
		ON_WM_KEYDOWN()
	END_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	UINT CNameEditCtrl::OnGetDlgCode()
	{
		return (__super::OnGetDlgCode() | DLGC_WANTALLKEYS);
	}

	//////////////////////////////////////////////////////////////////////////
	void CNameEditCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
	{
		switch(nChar)
		{
		case VK_LEFT:
		case VK_RIGHT:
		case VK_DELETE:
		case VK_HOME:
		case VK_END:
			{
				__super::OnKeyDown(nChar, nRepCnt, nFlags);
				break;
			}
		default:
			{
				if(CNameDlg* pParent = static_cast<CNameDlg*>(GetParent()))
				{
					pParent->OnKeyDown(nChar, nRepCnt, nFlags);
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	BEGIN_MESSAGE_MAP(CNameDlg, CDialog)
		ON_WM_KEYDOWN()
		ON_EN_UPDATE(IDC_SCHEMATYC_NAME, OnNameChanged)
	END_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	CNameDlg::CNameDlg(CWnd* pParent, CPoint pos, const char* szCaption, const char* szDefaultName, ENameDlgTextCase textCase, const Validator& validator)
		: CDialog(IDD_SCHEMATYC_NAME, pParent)
		, m_pos(pos)
		, m_caption(szCaption)
		, m_defaultName(szDefaultName)
		, m_textCase(textCase)
		, m_validator(validator)
	{}

	//////////////////////////////////////////////////////////////////////////
	const char* CNameDlg::GetResult() const
	{
		return m_result.GetString();
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CNameDlg::OnInitDialog()
	{
		CDialog::SetWindowPos(nullptr, m_pos.x, m_pos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		CDialog::SetWindowText(m_caption.c_str());
		CDialog::OnInitDialog();

		switch(m_textCase)
		{
		case ENameDlgTextCase::UpperCase:
			{
				SetWindowLongPtr(m_nameCtrl.GetSafeHwnd(), GWL_STYLE, m_nameCtrl.GetStyle() | ES_UPPERCASE);
				break;
			}
		case ENameDlgTextCase::Lowercase:
			{
				SetWindowLongPtr(m_nameCtrl.GetSafeHwnd(), GWL_STYLE, m_nameCtrl.GetStyle() | ES_LOWERCASE);
				break;
			}
		}

		if(m_defaultName.empty())
		{
			m_okButton.EnableWindow(FALSE);
		}
		else
		{
			m_nameCtrl.SetWindowText(m_defaultName.c_str());
			m_okButton.EnableWindow(TRUE);
		}
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void CNameDlg::DoDataExchange(CDataExchange* pDX)
	{
		DDX_Control(pDX, IDC_SCHEMATYC_NAME, m_nameCtrl);
		DDX_Control(pDX, IDOK, m_okButton);
		CDialog::DoDataExchange(pDX);
	}

	//////////////////////////////////////////////////////////////////////////
	void CNameDlg::OnOK()
	{
		if(ValidateName(true))
		{
			m_nameCtrl.GetWindowText(m_result);
			CDialog::OnOK();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CNameDlg::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
	{
		switch(nChar)
		{
		case VK_RETURN:
			{
				if(ValidateName(true))
				{
					OnOK();
					EndDialog(TRUE);
				}
				break;
			}
		case VK_ESCAPE:
			{
				EndDialog(FALSE);
				break;
			}
		default:
			{
				__super::OnKeyDown(nChar, nRepCnt, nFlags);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CNameDlg::OnNameChanged()
	{
		RefreshOkButton();
	}

	//////////////////////////////////////////////////////////////////////////
	void CNameDlg::RefreshOkButton()
	{
		m_okButton.EnableWindow(ValidateName(false));
	}

	//////////////////////////////////////////////////////////////////////////
	bool CNameDlg::ValidateName(bool displayErrorMessages) const
	{
		if(m_validator)
		{
			CString	name;
			m_nameCtrl.GetWindowText(name);
			return m_validator(CDialog::GetSafeHwnd(), name.GetString(), displayErrorMessages);
		}
		return true;
	}
}
