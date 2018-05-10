// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ConfigPanel.h"
#include "ConfigGroup.h"

BEGIN_MESSAGE_MAP(CConfigPanel, CDialog)
ON_CONTROL_RANGE(BN_CLICKED, kFirstID, kLastID, OnOptionChanged)
ON_CONTROL_RANGE(EN_UPDATE, kFirstID, kLastID, OnTextChanged)
ON_WM_SIZE()
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------

CConfigPanel::CItem::CItem(Config::IConfigVar* pVar, CWnd* pWnd, const RECT& rect)
	: m_pVar(pVar)
	, m_pWnd(pWnd)
	, m_top(rect.top)
	, m_bottom(rect.bottom)
{
}

CConfigPanel::CItem::~CItem()
{
	delete m_pWnd;
}

//-----------------------------------------------------------------------------

CConfigPanel::CGroup::CGroup(CWnd* pWnd, const RECT& rect)
	: m_pGroup(pWnd)
	, m_top(rect.top)
	, m_bottom(rect.bottom)
{
}

CConfigPanel::CGroup::~CGroup()
{
	for (size_t i = 0; i < m_items.size(); ++i)
	{
		delete m_items[i];
	}

	m_items.clear();
}

//-----------------------------------------------------------------------------

CConfigPanel::CConfigPanel(UINT idd, CWnd* pParent /*= NULL*/, uint initialY /*= 0*/)
	: CDialog(idd, pParent)
	, m_currentPosition(kMarginTop + initialY)
	, m_currentId(kFirstID)
	, m_initialY(initialY)
{
	GetIEditor()->RegisterNotifyListener(this);
}

CConfigPanel::~CConfigPanel()
{
	GetIEditor()->UnregisterNotifyListener(this);

	for (uint32 i = 0; i < m_groups.size(); ++i)
	{
		delete m_groups[i];
	}

	m_groups.clear();
}

void CConfigPanel::DisplayGroup(Config::CConfigGroup* pGroup, const char* szGroupName)
{
	// Get current panel size
	RECT clientRect;
	GetClientRect(&clientRect);

	// Create the group container
	{
		CButton* pGroupBox = new CButton();
		RECT groupRect;
		groupRect.left = clientRect.left + kMarginLeft;
		groupRect.right = clientRect.right - kMarginRight;
		groupRect.top = clientRect.top + m_currentPosition;
		groupRect.bottom = groupRect.top + 10; // placeholder!
		pGroupBox->Create(szGroupName, WS_VISIBLE | BS_GROUPBOX, groupRect, this, -1 /*STATIC*/);
		pGroupBox->SetFont(GetFont(), FALSE);

		// create wrapper
		CGroup* pGroupInfo = new CGroup(pGroupBox, groupRect);
		m_groups.push_back(pGroupInfo);

		// Create the items
		uint32 innerOffset = kGroupInnerTopMargin;
		const uint32 count = pGroup->GetVarCount();
		for (uint32 i = 0; i < count; ++i)
		{
			Config::IConfigVar* pVar = pGroup->GetVar(i);

			// allocate ID
			const uint32 id = m_currentId++;

			// create control
			switch (pVar->GetType())
			{
			case Config::IConfigVar::eType_BOOL:
				{
					RECT itemRect;
					itemRect.left = groupRect.left + kGroupInnerLeftMargin;
					itemRect.right = groupRect.right - kGroupInnerRightMargin;
					itemRect.top = groupRect.top + innerOffset;
					itemRect.bottom = itemRect.top + 15; // CHECKBOX SIZE

					CButton* pButton = new CButton();
					pButton->Create(pVar->GetDescription().c_str(), BS_AUTOCHECKBOX | WS_TABSTOP | WS_VISIBLE, itemRect, this, id);
					pButton->SetFont(GetFont(), FALSE);

					// get value
					bool bIsChecked = false;
					pVar->Get(&bIsChecked);
					pButton->SetCheck(bIsChecked ? BST_CHECKED : BST_UNCHECKED);

					CItem* pItem = new CItem(pVar, pButton, itemRect);
					pGroupInfo->m_items.push_back(pItem);
					m_items[id] = pItem;

					innerOffset += (itemRect.bottom - itemRect.top) + kControlSeparation;
					break;
				}

			case Config::IConfigVar::eType_INT:
			case Config::IConfigVar::eType_STRING:
			case Config::IConfigVar::eType_FLOAT:
				{
					RECT captionRect;
					captionRect.left = groupRect.left + kGroupInnerLeftMargin;
					captionRect.right = groupRect.right - kGroupInnerRightMargin;
					captionRect.top = groupRect.top + innerOffset + 5;
					captionRect.bottom = captionRect.top + 12; // Caption size

					CStatic* pStatic = new CStatic();
					const string caption = string(pVar->GetDescription()) + ":";
					pStatic->Create(caption.c_str(), SS_LEFT | WS_VISIBLE, captionRect, this, id);
					pStatic->SetFont(GetFont(), FALSE);

					RECT itemRect;
					itemRect.left = captionRect.left;
					itemRect.right = captionRect.right;
					itemRect.top = captionRect.bottom + 4;
					itemRect.bottom = itemRect.top + 18;

					CEdit* pEdit = new CEdit();
					pEdit->Create(ES_LEFT | WS_TABSTOP | WS_VISIBLE, itemRect, this, id);
					pEdit->SetFont(GetFont(), FALSE);

					// get value
					if (pVar->GetType() == Config::IConfigVar::eType_INT)
					{
						int iValue = 0.0f;
						pVar->Get(&iValue);
						char buf[32];
						cry_sprintf(buf, "%d", iValue);
						pEdit->SetWindowText(buf);
					}
					else if (pVar->GetType() == Config::IConfigVar::eType_FLOAT)
					{
						float fValue = 0.0f;
						pVar->Get(&fValue);
						char buf[32];
						cry_sprintf(buf, "%f", fValue);
						pEdit->SetWindowText(buf);
					}
					else if (pVar->GetType() == Config::IConfigVar::eType_STRING)
					{
						string sValue;
						pVar->Get(&sValue);
						pEdit->SetWindowText(sValue.c_str());
					}

					CItem* pItem = new CItem(pVar, pEdit, itemRect);
					pGroupInfo->m_items.push_back(pItem);
					m_items[id] = pItem;

					innerOffset += (itemRect.bottom - captionRect.top) + kControlSeparation;
					break;
				}
			}
		}

		// adjust the group box size
		groupRect.bottom = groupRect.top + innerOffset + kGroupInnerBottomMargin;
		pGroupBox->MoveWindow(&groupRect, FALSE);
		pGroupInfo->m_bottom = groupRect.bottom;
		m_currentPosition = groupRect.bottom + kGroupSeparation;
	}

	// Resize the whole window
	SetWindowPos(NULL, 0, 0, clientRect.right, m_currentPosition - kGroupSeparation + kMarginTop,
	             SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);

	// Redraw interface
	Invalidate(TRUE);
}

void CConfigPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CConfigPanel::OnInitDialog()
{
	return TRUE;
}

void CConfigPanel::OnOptionChanged(UINT nID)
{
	TItemMap::const_iterator it = m_items.find(nID);
	if (it != m_items.end())
	{
		CItem* pItem = it->second;
		if (pItem->m_pVar->GetType() == Config::IConfigVar::eType_BOOL)
		{
			// get current selection state
			const bool bIsChecked = (static_cast<CButton*>(pItem->m_pWnd)->GetCheck() == BST_CHECKED);
			pItem->m_pVar->Set(&bIsChecked);
			OnConfigValueChanged(pItem->m_pVar);
		}
	}
}

void CConfigPanel::OnTextChanged(UINT nID)
{
	TItemMap::const_iterator it = m_items.find(nID);
	if (it != m_items.end())
	{
		CItem* pItem = it->second;
		switch (pItem->m_pVar->GetType())
		{
		case Config::IConfigVar::eType_FLOAT:
			{
				// get text from the window
				CString text;
				static_cast<CEdit*>(pItem->m_pWnd)->GetWindowText(text);
				float newValue = 0.0f;
				if (FromString(newValue, (const char*)text))
				{
					pItem->m_pVar->Set(&newValue);
					OnConfigValueChanged(pItem->m_pVar);
				}

				break;
			}

		case Config::IConfigVar::eType_INT:
			{
				// get text from the window
				CString text;
				static_cast<CEdit*>(pItem->m_pWnd)->GetWindowText(text);
				int newValue = 0.0f;
				if (FromString(newValue, (const char*)text))
				{
					pItem->m_pVar->Set(&newValue);
					OnConfigValueChanged(pItem->m_pVar);
				}

				break;
			}

		case Config::IConfigVar::eType_STRING:
			{
				// get text from the window
				CString text;
				static_cast<CEdit*>(pItem->m_pWnd)->GetWindowText(text);

				const string newValue((const char*)text);
				pItem->m_pVar->Set(&newValue);
				OnConfigValueChanged(pItem->m_pVar);

				break;
			}
		}
	}
}

void CConfigPanel::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	// start batched update, we need a window count for this
	uint numWindows = 0;
	for (uint32 i = 0; i < m_groups.size(); ++i)
	{
		CGroup* pGroup = m_groups[i];
		numWindows += 1 + pGroup->m_items.size();
	}
	HDWP hDP = BeginDeferWindowPos(numWindows);

	// update the size of the controls
	for (uint32 i = 0; i < m_groups.size(); ++i)
	{
		CGroup* pGroup = m_groups[i];

		const DWORD newWidth = cx - (kMarginRight + kMarginLeft);
		const DWORD newHeight = pGroup->m_bottom - pGroup->m_top;

		// move the group window
		DeferWindowPos(hDP, pGroup->m_pGroup->GetSafeHwnd(), NULL,
		               0, 0, newWidth, newHeight, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);

		// move the item windows
		for (uint32 i = 0; i < pGroup->m_items.size(); ++i)
		{
			CItem* pItem = pGroup->m_items[i];

			const DWORD newWidth = cx - (kMarginRight + kMarginLeft + kGroupInnerLeftMargin + kGroupInnerRightMargin);
			const DWORD newHeight = pItem->m_bottom - pItem->m_top;

			// move the group window
			DeferWindowPos(hDP, pItem->m_pWnd->GetSafeHwnd(), NULL,
			               0, 0, newWidth, newHeight, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
		}
	}

	EndDeferWindowPos(hDP);
}

void CConfigPanel::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
}

