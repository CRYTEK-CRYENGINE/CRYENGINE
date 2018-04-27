// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "AddGraphDlg.h"

#include <CrySchematyc2/ISignal.h>

#include "Resource.h"
#include "PluginUtils.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	BEGIN_MESSAGE_MAP(CAddGraphDlg, CDialog)
		ON_EN_UPDATE(IDC_SCHEMATYC_GRAPH_FILTER, OnFilterChanged)
		ON_EN_CHANGE(IDC_SCHEMATYC_GRAPH_NAME, OnNameCtrlSelChange)
		ON_LBN_SELCHANGE(IDD_SCHEMATYC_GRAPH_CONTEXT, OnContextCtrlSelChange)
	END_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	CAddGraphDlg::CAddGraphDlg(CWnd* pParent, CPoint pos, TScriptFile& scriptFile, const SGUID& scopeGUID, EScriptGraphType type)
		: CDialog(IDD_SCHEMATYC_ADD_GRAPH, pParent)
		, m_pos(pos)
		, m_scriptFile(scriptFile)
		, m_scopeGUID(scopeGUID)
		, m_type(type)
		, m_nameModified(false)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CAddGraphDlg::GetGraphGUID() const
	{
		return m_graphGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CAddGraphDlg::OnInitDialog()
	{
		SetWindowPos(NULL, m_pos.x, m_pos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		CDialog::OnInitDialog();

		switch(m_type)
		{
		case EScriptGraphType::SignalReceiver:
			{
				GetSchematyc()->GetEnvRegistry().VisitSignals(EnvSignalVisitor::FromMemberFunction<CAddGraphDlg, &CAddGraphDlg::VisitEnvSignal>(*this));
				ScriptIncludeRecursionUtils::VisitSignals(m_scriptFile, ScriptIncludeRecursionUtils::SignalVisitor::FromMemberFunction<CAddGraphDlg, &CAddGraphDlg::VisitScriptSignal>(*this), SGUID(), true);
				m_scriptFile.VisitTimers(ScriptTimerConstVisitor::FromMemberFunction<CAddGraphDlg, &CAddGraphDlg::VisitScriptTimer>(*this), SGUID(), true);
				std::sort(m_contexts.begin(), m_contexts.end(), [](const SContext& a, const SContext& b) { return a.fullName < b.fullName; });
				break;
			}
		default:
			{
				m_filterCtrl.EnableWindow(FALSE);
				m_contextCtrl.EnableWindow(FALSE);
			}
		}

		RefreshContextCtrl();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddGraphDlg::DoDataExchange(CDataExchange* pDX)
	{
		DDX_Control(pDX, IDC_SCHEMATYC_GRAPH_NAME, m_nameCtrl);
		DDX_Control(pDX, IDC_SCHEMATYC_GRAPH_FILTER, m_filterCtrl);
		DDX_Control(pDX, IDD_SCHEMATYC_GRAPH_CONTEXT, m_contextCtrl);
		CDialog::DoDataExchange(pDX);
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddGraphDlg::OnNameCtrlSelChange()
	{
		m_nameModified = true;
	}

	void CAddGraphDlg::OnFilterChanged()
	{
		RefreshContextCtrl();
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddGraphDlg::OnContextCtrlSelChange()
	{
		CString	name;
		m_nameCtrl.GetWindowText(name);
		if((m_nameModified == false) || (name.IsEmpty() == true))
		{
			stack_string	autoName;
			switch(m_type)
			{
			case EScriptGraphType::Constructor:
				{
					autoName = "Constructor";
					break;
				}
			case EScriptGraphType::Destructor:
				{
					autoName = "Destructor";
					break;
				}
			case EScriptGraphType::SignalReceiver:
				{
					if(const SContext* pContext = GetContext())
					{
						autoName = "On";
						autoName.append(pContext->name.c_str());
						autoName.replace(":", "");
					}
					break;
				}
			}
			if(autoName.empty() == false)
			{
				m_nameCtrl.SetWindowText(autoName.c_str());
				m_nameModified = false;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddGraphDlg::OnOK()
	{
		CString name;
		m_nameCtrl.GetWindowText(name);
		if(PluginUtils::ValidateScriptElementName(CDialog::GetSafeHwnd(), m_scriptFile, m_scopeGUID, name.GetString(), true))
		{
			IDocGraph* pGraph = nullptr;
			switch(m_type)
			{
			case EScriptGraphType::Function:
				{
					pGraph = m_scriptFile.AddGraph(SScriptGraphParams(m_scopeGUID, name.GetString(), m_type, SGUID()));
					if(pGraph)
					{
						// #SchematycTODO : Should really do this somewhere in the script graph code!!!
						pGraph->AddNode(EScriptGraphNodeType::Begin, SGUID(), SGUID(), Vec2(0.0f, 0.0f));
					}
					break;
				}
			case EScriptGraphType::Condition:
				{
					pGraph = m_scriptFile.AddGraph(SScriptGraphParams(m_scopeGUID, name.GetString(), m_type, SGUID()));
					if(pGraph)
					{
						// #SchematycTODO : Should really do this somewhere in the script graph code!!!
						pGraph->AddNode(EScriptGraphNodeType::Begin, SGUID(), SGUID(), Vec2(0.0f, 0.0f));
					}
					break;
				}
			case EScriptGraphType::Constructor:
				{
					pGraph = m_scriptFile.AddGraph(SScriptGraphParams(m_scopeGUID, name.GetString(), m_type, SGUID()));
					if(pGraph)
					{
						// #SchematycTODO : Should really do this somewhere in the script graph code!!!
						pGraph->AddNode(EScriptGraphNodeType::BeginConstructor, SGUID(), SGUID(), Vec2(0.0f, 0.0f));
					}
					break;
				}
			case EScriptGraphType::Destructor:
				{
					pGraph = m_scriptFile.AddGraph(SScriptGraphParams(m_scopeGUID, name.GetString(), m_type, SGUID()));
					if(pGraph)
					{
						// #SchematycTODO : Should really do this somewhere in the script graph code!!!
						pGraph->AddNode(EScriptGraphNodeType::BeginDestructor, SGUID(), SGUID(), Vec2(0.0f, 0.0f));
					}
					break;
				}
			case EScriptGraphType::SignalReceiver:
				{
					const SContext* pContext = GetContext();
					if(pContext)
					{
						pGraph = m_scriptFile.AddGraph(SScriptGraphParams(m_scopeGUID, name.GetString(), m_type, pContext->guid));
						if(pGraph)
						{
							// #SchematycTODO : Should really do this somewhere in the script graph code!!!
							pGraph->AddNode(EScriptGraphNodeType::BeginSignalReceiver, SGUID(), SGUID(), Vec2(0.0f, 0.0f));
						}
					}
					break;
				}
			case EScriptGraphType::Transition:
				{
					pGraph = m_scriptFile.AddGraph(SScriptGraphParams(m_scopeGUID, name.GetString(), m_type, SGUID()));
					break;
				}
			}
			if(pGraph)
			{
				m_graphGUID = pGraph->GetGUID();
			}
			CDialog::OnOK();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CAddGraphDlg::SContext::SContext() : showInListBox(true) {}

	//////////////////////////////////////////////////////////////////////////
	CAddGraphDlg::SContext::SContext(const SGUID& _guid, const char* _name, const char* _fullName, const bool showInListBox)
		: guid(_guid)
		, name(_name)
		, fullName(_fullName)
		, showInListBox(showInListBox)
	{}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CAddGraphDlg::VisitEnvSignal(const ISignalConstPtr& pSignal)
	{
		if(DocUtils::IsSignalAvailableInScope(m_scriptFile, m_scopeGUID, pSignal->GetSenderGUID(), pSignal->GetNamespace()))
		{
			stack_string fullName;
			EnvRegistryUtils::GetFullName(pSignal->GetName(), pSignal->GetNamespace(), pSignal->GetSenderGUID(), fullName);
			m_contexts.push_back(SContext(pSignal->GetGUID(), pSignal->GetName(), fullName.c_str()));
		}
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddGraphDlg::VisitScriptSignal(const TScriptFile& signalFile, const IScriptSignal& signal)
	{
		if(DocUtils::IsElementInScope(signalFile, m_scopeGUID, signal.GetScopeGUID()))
		{
			stack_string fullName;
			DocUtils::GetFullElementName(signalFile, signal, fullName, EScriptElementType::Class);
			m_contexts.push_back(SContext(signal.GetGUID(), signal.GetName(), fullName.c_str()));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CAddGraphDlg::VisitScriptTimer(const IScriptTimer& timer)
	{
		if(DocUtils::IsElementInScope(m_scriptFile, m_scopeGUID, timer.GetScopeGUID()))
		{
			stack_string fullName;
			DocUtils::GetFullElementName(m_scriptFile, timer, fullName, EScriptElementType::Class);
			m_contexts.push_back(SContext(timer.GetGUID(), timer.GetName(), fullName.c_str()));
		}
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	const CAddGraphDlg::SContext* CAddGraphDlg::GetContext() const
	{
		const int	curSel = m_contextCtrl.GetCurSel();
		if ((curSel != LB_ERR) && (curSel >= 0))
		{
			DWORD contextIdx = (DWORD)m_contextCtrl.GetItemData(curSel);
			return &m_contexts[contextIdx];
		}

		return nullptr;
	}

	void CAddGraphDlg::RefreshContextCtrl()
	{
		CString filter;
		m_filterCtrl.GetWindowText(filter);
		const bool isEmptyFilter = filter.IsEmpty();

		for (SContext& context : m_contexts)
		{
			if (isEmptyFilter || StringUtils::FilterString(context.fullName, filter.GetString()))
			{
				context.showInListBox = true;
			}
			else
			{
				context.showInListBox = false;
			}
		}

		m_contextCtrl.ResetContent();

		int contextIndex(0);
		for (TContextVector::const_iterator iContext = m_contexts.begin(), iEndContext = m_contexts.end(); iContext != iEndContext; ++iContext, ++contextIndex)
		{
			if (iContext->showInListBox)
			{
				int listIndex = m_contextCtrl.AddString(iContext->fullName.c_str());
				m_contextCtrl.SetItemData(listIndex, contextIndex);
			}
		}

		m_contextCtrl.SetCurSel(0);
		OnContextCtrlSelChange();
	}

}
