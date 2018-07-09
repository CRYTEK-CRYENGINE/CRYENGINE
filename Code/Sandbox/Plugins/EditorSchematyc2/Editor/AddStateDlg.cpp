// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AddStateDlg.h"

#include "Resource.h"
#include "PluginUtils.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CAddStateDlg::CAddStateDlg(CWnd* pParent, CPoint pos, TScriptFile& scriptFile, const SGUID& scopeGUID)
		: CDialog(IDD_SCHEMATYC_ADD_STATE, pParent)
		, m_pos(pos)
		, m_scriptFile(scriptFile)
		, m_scopeGUID(scopeGUID)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CAddStateDlg::GetStateGUID() const
	{
		return m_stateGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CAddStateDlg::OnInitDialog()
	{
		SetWindowPos(NULL, m_pos.x, m_pos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		CDialog::OnInitDialog();

		SGUID partnerStateMachineGUID;
		if(const IScriptStateMachine* pStateMachine = DocUtils::FindOwnerStateMachine(m_scriptFile, m_scopeGUID))
		{
			partnerStateMachineGUID = pStateMachine->GetPartnerGUID();
		}
		if(const IScriptStateMachine* pPartnerStateMachine = m_scriptFile.GetStateMachine(partnerStateMachineGUID))
		{
			m_partners.reserve(10);
			m_partners.push_back(SPartner(SGUID(), "None"));
			m_scriptFile.VisitStates(ScriptStateConstVisitor::FromMemberFunction<CAddStateDlg, &CAddStateDlg::VisitScriptState>(*this), partnerStateMachineGUID, true);
			for(TPartnerVector::const_iterator iPartner = m_partners.begin(), iEndPartner = m_partners.end(); iPartner != iEndPartner; ++ iPartner)
			{
				m_partnerCtrl.AddString(iPartner->name.c_str());
			}
			m_partnerCtrl.SetCurSel(0);
		}
		else
		{
			m_partnerCtrl.EnableWindow(FALSE);
		}

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddStateDlg::DoDataExchange(CDataExchange* pDX)
	{
		DDX_Control(pDX, IDC_SCHEMATYC_STATE_NAME, m_nameCtrl);
		DDX_Control(pDX, IDC_SCHEMATYC_STATE_PARTNER, m_partnerCtrl);
		CDialog::DoDataExchange(pDX);
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddStateDlg::OnOK()
	{
		CString	name;
		m_nameCtrl.GetWindowText(name);
		if(PluginUtils::ValidateScriptElementName(CDialog::GetSafeHwnd(), m_scriptFile, m_scopeGUID, name.GetString(), true))
		{
			IScriptState* pState = m_scriptFile.AddState(m_scopeGUID, name.GetString(), GetPartnerGUID());
			if(pState)
			{
				m_stateGUID = pState->GetGUID();
			}
			CDialog::OnOK();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CAddStateDlg::SPartner::SPartner() {}

	//////////////////////////////////////////////////////////////////////////
	CAddStateDlg::SPartner::SPartner(const SGUID& _guid, const char* _name)
		: guid(_guid)
		, name(_name)
	{}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CAddStateDlg::VisitScriptState(const IScriptState& state)
	{
		m_partners.push_back(SPartner(state.GetGUID(), state.GetName()));
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CAddStateDlg::GetStateCount() const
	{
		struct SStateVisitor
		{
			inline SStateVisitor()
				: stateCount(0)
			{}

			EVisitStatus VisitState(const IScriptState& state)
			{
				++ stateCount;
				return EVisitStatus::Continue;
			}

			size_t	stateCount;
		};

		SStateVisitor	stateVisitor;
		m_scriptFile.VisitStates(ScriptStateConstVisitor::FromMemberFunction<SStateVisitor, &SStateVisitor::VisitState>(stateVisitor), m_scopeGUID, false);
		return stateVisitor.stateCount;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CAddStateDlg::GetPartnerGUID() const
	{
		const int	curSel = m_partnerCtrl.GetCurSel();
		return (curSel != LB_ERR) && (curSel >= 0) ? m_partners[curSel].guid : SGUID();
	}
}
