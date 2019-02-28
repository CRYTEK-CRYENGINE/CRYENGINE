// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Config
{
struct IConfigVar;
class CConfigGroup;
}

class CConfigPanel : public CDialog
	                   , public IEditorNotifyListener
{
public:
	static const uint32 kFirstID = 10000;
	static const uint32 kLastID = 10100;

	static const uint32 kMarginTop = 10;
	static const uint32 kMarginLeft = 10;
	static const uint32 kMarginRight = 10;
	static const uint32 kControlSeparation = 3;
	static const uint32 kGroupSeparation = 5;
	static const uint32 kGroupInnerTopMargin = 20;
	static const uint32 kGroupInnerBottomMargin = 10;
	static const uint32 kGroupInnerLeftMargin = 10;
	static const uint32 kGroupInnerRightMargin = 10;

public:
	enum { IDD = IDD_CONFIG_PANEL };

	CConfigPanel(UINT idd = IDD, CWnd* pParent = NULL, uint initialY = 0);   // standard constructor
	virtual ~CConfigPanel();

	// Create edit controls for given configuration group
	void DisplayGroup(Config::CConfigGroup* pGroup, const char* szGroupName);

	// Called after value of given config variable was changed
	virtual void OnConfigValueChanged(Config::IConfigVar* pVar) {};

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	DECLARE_MESSAGE_MAP()
	virtual BOOL OnInitDialog();
	virtual void OnOK()     {};
	virtual void OnCancel() {};
	afx_msg void OnOptionChanged(UINT nID);
	afx_msg void OnTextChanged(UINT nID);
	afx_msg void OnSize(UINT nType, int cx, int cy);

protected:
	// IEditorNotifyListener interface implementation
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

	struct CItem
	{
		CWnd*               m_pWnd;
		Config::IConfigVar* m_pVar;
		DWORD               m_top;
		DWORD               m_bottom;

		CItem(Config::IConfigVar* pVar, CWnd* pWnd, const RECT& rect);
		~CItem();
	};

	struct CGroup
	{
		CWnd*               m_pGroup;
		DWORD               m_top;
		DWORD               m_bottom;
		std::vector<CItem*> m_items;

		CGroup(CWnd* pWnd, const RECT& rect);
		~CGroup();
	};

	typedef std::vector<CGroup*> TGroups;
	TGroups m_groups;

	typedef std::map<DWORD, CItem*> TItemMap;
	TItemMap m_items;

	uint     m_initialY;
	uint     m_currentPosition;
	uint     m_currentId;
};

