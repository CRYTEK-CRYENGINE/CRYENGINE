// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move CNameEditCtrl inside CNameDlg?

#pragma once

#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Script/IScriptFile.h>

namespace Schematyc2
{
	enum class ENameDlgTextCase
	{
		MixedCase,
		UpperCase,
		Lowercase
	};

	class CNameEditCtrl : public CEdit
	{
		DECLARE_MESSAGE_MAP()

	private:

		afx_msg UINT OnGetDlgCode();
		afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	};

	class CNameDlg : public CDialog
	{
		DECLARE_MESSAGE_MAP()

		friend class CNameEditCtrl;

	public:

		typedef std::function<bool (HWND, const char*, bool)> Validator;

		CNameDlg(CWnd* pParent, CPoint pos, const char* szCaption, const char* szDefaultName = "", ENameDlgTextCase textCase = ENameDlgTextCase::MixedCase, const Validator& validator = Validator());

		const char* GetResult() const;

	protected:

		virtual BOOL OnInitDialog();
		virtual void DoDataExchange(CDataExchange* pDX);
		virtual void OnOK();

	private:

		afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
		afx_msg void OnNameChanged();

		void RefreshOkButton();
		bool ValidateName(bool displayErrorMessages) const;

		CPoint						m_pos;
		string						m_caption;
		string						m_defaultName;
		ENameDlgTextCase	m_textCase;
		Validator					m_validator;
		CNameEditCtrl			m_nameCtrl;
		CButton						m_okButton;
		CString						m_result;
	};
}
