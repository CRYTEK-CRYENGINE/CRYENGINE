// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// CCheckOutDialog dialog

class PLUGIN_API CCheckOutDialog : public CDialog
{
	DECLARE_DYNAMIC(CCheckOutDialog)

	// Checkout dialog result.
	enum EResult
	{
		CHECKOUT,
		OVERWRITE,
		OVERWRITE_ALL,
		CANCEL,
	};

public:
	CCheckOutDialog(const CString& file, CWnd* pParent = NULL);   // standard constructor
	virtual ~CCheckOutDialog();

	virtual BOOL OnInitDialog();
	EResult      GetResult() const { return m_result; };

	// Enable functionality For All. In the end call with false to return it in init state.
	// Returns previous enable state
	static bool EnableForAll(bool isEnable);
	static bool IsForAll() { return InstanceIsForAll(); }

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedCheckout();
	afx_msg void OnBnClickedOverwriteAll();
	afx_msg void OnBnClickedOk();

private:
	static bool& InstanceEnableForAll();
	static bool& InstanceIsForAll();

	CString m_file;
	CString m_text;
	EResult m_result;
};

class CAutoCheckOutDialogEnableForAll
{
public:
	CAutoCheckOutDialogEnableForAll()
	{
		m_bPrevState = CCheckOutDialog::EnableForAll(true);
	}

	~CAutoCheckOutDialogEnableForAll()
	{
		CCheckOutDialog::EnableForAll(m_bPrevState);
	}

private:
	bool m_bPrevState;
};

