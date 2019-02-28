// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequencerKeyPropertiesDlg.h"
#include "SequencerSequence.h"

IMPLEMENT_DYNAMIC(CSequencerKeyUIControls, CObject)

//////////////////////////////////////////////////////////////////////////
void CSequencerKeyUIControls::OnInternalVariableChange(IVariable* pVar)
{
	SelectedKeys keys;
	CSequencerUtils::GetSelectedKeys(m_pKeyPropertiesDlg->GetSequence(), keys);

	OnUIChange(pVar, keys);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerKeyUIControls::RefreshSequencerKeys()
{
	GetIEditorImpl()->Notify(eNotify_OnUpdateSequencerKeys);
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CSequencerKeyPropertiesDlg, CDialog)
ON_WM_SIZE()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CSequencerKeyPropertiesDlg::CSequencerKeyPropertiesDlg(bool overrideConstruct)
	: m_pLastTrackSelected(NULL)
{
	m_pVarBlock = new CVarBlock;

	if (!overrideConstruct)
	{
		// Add key UI classes
		/*std::vector<IClassDesc*> classes;
		   GetIEditorImpl()->GetClassFactory()->GetClassesBySystemID( ESYSTEM_CLASS_SEQUENCER_KEYUI,classes );
		   for (int i = 0; i < (int)classes.size(); i++)
		   {
		   CObject *pObj = classes[i]->GetRuntimeClass()->CreateObject();
		   if (pObj->IsKindOf(RUNTIME_CLASS(CSequencerKeyUIControls)))
		   {
		    m_keyControls.push_back( (CSequencerKeyUIControls*)pObj );
		   }
		   }*/

		CreateAllVars();
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CSequencerKeyPropertiesDlg::OnInitDialog()
{
	BOOL bRes = __super::OnInitDialog();

	if (bRes)
	{
		CRect rc;
		GetClientRect(rc);
		m_wndProps.Create(WS_CHILD | WS_VISIBLE, rc, this);
		m_wndProps.ModifyStyleEx(0, WS_EX_CLIENTEDGE);

		m_wndTrackProps.Create(CSequencerTrackPropsDlg::IDD, this);

		/*{
		   CBrush m_bkgBrush;
		   m_bkgBrush.CreateSolidBrush(RGB(0xE0,0xE0,0xE0));
		   WNDCLASS wndcls;
		   ZeroStruct(wndcls);
		   //you can specify your own window procedure
		   wndcls.lpfnWndProc = ::DefWindowProc;
		   wndcls.hInstance = AfxGetInstanceHandle();
		   wndcls.hbrBackground = (HBRUSH) (COLOR_3DFACE + 1);
		   wndcls.lpszClassName = _T("SequencerTcbPreviewClass");
		   AfxRegisterClass(&wndcls);
		   m_wndTcbPreview.Create( _T("SequencerTcbPreviewClass"),NULL,WS_CHILD|WS_VISIBLE,CRect(0,0,60,60),this,0 );
		   }*/
	}

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerKeyPropertiesDlg::OnSize(UINT nType, int cx, int cy)
{
	if (m_wndProps.m_hWnd)
	{
		CRect rcClient;
		GetClientRect(rcClient);

		CRect rcTrackWnd;
		m_wndTrackProps.GetWindowRect(rcTrackWnd);
		CRect rcTrack(rcClient.left, rcClient.top, rcClient.right, rcClient.top + rcTrackWnd.Height());
		m_wndTrackProps.MoveWindow(rcTrack);

		int tcbPreview = 0;

		CRect rcProps(2, rcTrack.bottom + 4, rcClient.right - 2, rcClient.bottom - 4 - tcbPreview);
		m_wndProps.MoveWindow(rcProps);

		//CRect rcPreview( 2,rcProps.bottom+4,rcClient.left + tcbPreview + 20,rcClient.bottom - 4 );
		//m_wndTcbPreview.MoveWindow( rcPreview );
	}
}

//////////////////////////////////////////////////////////////////////////
//BOOL CSequencerKeyPropertiesDlg::PreTranslateMessage( MSG* pMsg )
//{
//if (pMsg->message == WM_KEYDOWN)
//{
//	// In case of arrow keys, pass the message to keys window so that it can handle
//	// a key selection change by arrow keys.
//	int nVirtKey = (int) pMsg->wParam;
//	if (nVirtKey == VK_UP || nVirtKey == VK_DOWN || nVirtKey == VK_RIGHT || nVirtKey == VK_LEFT)
//	{
//		if (m_keysCtrl)
//			m_keysCtrl->SendMessage(WM_KEYDOWN, pMsg->wParam, pMsg->lParam);
//	}
//}

//return __super::PreTranslateMessage(pMsg);
//return false;
//}

//////////////////////////////////////////////////////////////////////////
void CSequencerKeyPropertiesDlg::OnVarChange(IVariable* pVar)
{

}

//////////////////////////////////////////////////////////////////////////
void CSequencerKeyPropertiesDlg::CreateAllVars()
{
	for (int i = 0; i < (int)m_keyControls.size(); i++)
	{
		m_keyControls[i]->SetKeyPropertiesDlg(this);
		m_keyControls[i]->CreateVars();
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerKeyPropertiesDlg::PopulateVariables()
{
	//SetVarBlock( m_pVarBlock,functor(*this,&CSequencerKeyPropertiesDlg::OnVarChange) );

	// Must first clear any selection in properties window.
	m_wndProps.ClearSelection();
	m_wndProps.DeleteAllItems();
	m_wndProps.AddVarBlock(m_pVarBlock);

	m_wndProps.SetUpdateCallback(functor(*this, &CSequencerKeyPropertiesDlg::OnVarChange));
	//m_wndProps.ExpandAll();

	ReloadValues();
}

//////////////////////////////////////////////////////////////////////////
void CSequencerKeyPropertiesDlg::PopulateVariables(CPropertyCtrl& propCtrl)
{
	propCtrl.ClearSelection();
	propCtrl.DeleteAllItems();
	propCtrl.AddVarBlock(m_pVarBlock);

	propCtrl.ReloadValues();
}

//////////////////////////////////////////////////////////////////////////
void CSequencerKeyPropertiesDlg::OnKeySelectionChange()
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(GetSequence(), selectedKeys);

	if (m_wndTrackProps.m_hWnd)
		m_wndTrackProps.OnKeySelectionChange(selectedKeys);

	bool bSelectChangedInSameTrack
	  = m_pLastTrackSelected
	    && selectedKeys.keys.size() == 1
	    && selectedKeys.keys[0].pTrack == m_pLastTrackSelected;

	if (selectedKeys.keys.size() == 1)
		m_pLastTrackSelected = selectedKeys.keys[0].pTrack;
	else
		m_pLastTrackSelected = NULL;

	if (bSelectChangedInSameTrack == false)
		m_pVarBlock->DeleteAllVariables();

	bool bAssigned = false;
	if (selectedKeys.keys.size() > 0)
	{
		for (int i = 0; i < (int)m_keyControls.size(); i++)
		{
			const ESequencerParamType valueType = selectedKeys.keys[0].pTrack->GetParameterType();
			if (m_keyControls[i]->SupportTrackType(valueType))
			{
				if (bSelectChangedInSameTrack == false)
					AddVars(m_keyControls[i]);
				if (m_keyControls[i]->OnKeySelectionChange(selectedKeys))
					bAssigned = true;

				break;
			}
		}
		m_wndProps.EnableWindow(TRUE);
	}
	else
	{
		m_wndProps.EnableWindow(FALSE);
	}

	if (bSelectChangedInSameTrack)
	{
		m_wndProps.ClearSelection();
		ReloadValues();
	}
	else
	{
		PopulateVariables();
	}

	if (selectedKeys.keys.size() > 1 || !bAssigned)
		m_wndProps.SetDisplayOnlyModified(true);
	else
		m_wndProps.SetDisplayOnlyModified(false);
}

//////////////////////////////////////////////////////////////////////////
void CSequencerKeyPropertiesDlg::AddVars(CSequencerKeyUIControls* pUI)
{
	CVarBlock* pVB = pUI->GetVarBlock();
	for (int i = 0, num = pVB->GetNumVariables(); i < num; i++)
	{
		IVariable* pVar = pVB->GetVariable(i);
		m_pVarBlock->AddVariable(pVar);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequencerKeyPropertiesDlg::ReloadValues()
{
	if (m_wndProps.m_hWnd)
	{
		m_wndProps.ReloadValues();
	}
}

void CSequencerKeyPropertiesDlg::SetSequence(CSequencerSequence* pSequence)
{
	m_pSequence = pSequence;
	m_wndTrackProps.SetSequence(pSequence);
}

void CSequencerKeyPropertiesDlg::SetVarValue(const char* varName, const char* varValue)
{
	if (varName == NULL || varName[0] == 0)
	{
		return;
	}

	if (varValue == NULL || varValue[0] == 0)
	{
		return;
	}

	if (!m_pVarBlock)
	{
		return;
	}

	IVariable* pVar = NULL;

	string varNameList = varName;
	int start = 0;
	string token = varNameList.Tokenize(".", start);
	CRY_ASSERT(!token.empty());

	while (!token.empty())
	{
		if (pVar == NULL)
		{
			const bool recursiveSearch = true;
			pVar = m_pVarBlock->FindVariable(token.c_str(), recursiveSearch);
		}
		else
		{
			const bool recursiveSearch = false;
			pVar = pVar->FindVariable(token.c_str(), recursiveSearch);
		}

		if (pVar == NULL)
		{
			return;
		}

		token = varNameList.Tokenize(".", start);
	}

	pVar->Set(varValue);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CSequencerTrackPropsDlg::CSequencerTrackPropsDlg()
{
	m_track = 0;
	m_key = -1;
}

void CSequencerTrackPropsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PREVNEXT, m_keySpinBtn);
	DDX_Control(pDX, IDC_KEYNUM, m_keynum);
}

BEGIN_MESSAGE_MAP(CSequencerTrackPropsDlg, CDialog)
ON_NOTIFY(UDN_DELTAPOS, IDC_PREVNEXT, OnDeltaposPrevnext)
ON_EN_UPDATE(IDC_TIME, OnUpdateTime)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CSequencerTrackPropsDlg::OnInitDialog()
{
	BOOL bRes = CDialog::OnInitDialog();

	m_time.Create(this, IDC_TIME);
	m_time.SetRange(0, 0);

	m_keySpinBtn.SetPos(0);
	m_keySpinBtn.SetBuddy(&m_keynum);
	m_keySpinBtn.SetRange(-10000, 10000);

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerTrackPropsDlg::SetSequence(CSequencerSequence* pSequence)
{
	if (pSequence)
	{
		Range range = pSequence->GetTimeRange();
		m_time.SetRange(range.start, range.end);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerTrackPropsDlg::OnKeySelectionChange(CSequencerUtils::SelectedKeys& selectedKeys)
{
	if (!IsWindow(m_hWnd))
		return false;

	m_track = 0;
	m_key = 0;

	if (selectedKeys.keys.size() == 1)
	{
		m_track = selectedKeys.keys[0].pTrack;
		m_key = selectedKeys.keys[0].nKey;
	}

	if (m_track != NULL)
	{
		m_time.SetValue(m_track->GetKeyTime(m_key));
		m_keySpinBtn.SetRange(1, m_track->GetNumKeys());
		m_keySpinBtn.SetPos(m_key + 1);

		m_keySpinBtn.EnableWindow(TRUE);
		m_time.EnableWindow(TRUE);
	}
	else
	{
		m_keySpinBtn.EnableWindow(FALSE);
		m_time.EnableWindow(FALSE);
	}
	return true;
}

void CSequencerTrackPropsDlg::OnDeltaposPrevnext(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

	if (!m_track)
		return;

	int nkey = m_key + pNMUpDown->iDelta;
	if (nkey < 0)
		nkey = m_track->GetNumKeys() - 1;
	if (nkey > m_track->GetNumKeys() - 1)
		nkey = 0;

	SetCurrKey(nkey);

	*pResult = 1;
}

void CSequencerTrackPropsDlg::OnUpdateTime()
{
	if (!m_track)
		return;

	if (m_key < 0 || m_key >= m_track->GetNumKeys())
		return;

	float time = m_time.GetValue();
	m_track->SetKeyTime(m_key, time);
	m_track->SortKeys();

	int k = m_track->FindKey(time);
	if (k != m_key)
	{
		SetCurrKey(k);
	}
}

void CSequencerTrackPropsDlg::SetCurrKey(int nkey)
{
	if (m_key >= 0 && m_key < m_track->GetNumKeys())
	{
		m_track->SelectKey(m_key, false);
	}
	if (nkey >= 0 && nkey < m_track->GetNumKeys())
	{
		m_track->SelectKey(nkey, true);
	}
	GetIEditorImpl()->Notify(eNotify_OnUpdateSequencerKeySelection);
}

