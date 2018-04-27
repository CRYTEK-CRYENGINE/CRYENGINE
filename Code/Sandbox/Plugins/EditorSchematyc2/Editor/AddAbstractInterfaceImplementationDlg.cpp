// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AddAbstractInterfaceImplementationDlg.h"

#include "Resource.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	BEGIN_MESSAGE_MAP(CAddAbstractInterfaceImplementationDlg, CDialog)
		ON_LBN_SELCHANGE(IDD_SCHEMATYC_ABSTRACT_INTERFACES, OnAbstractInterfacesCtrlSelChange)
	END_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	CAddAbstractInterfaceImplementationDlg::CAddAbstractInterfaceImplementationDlg(CWnd* pParent, CPoint pos, TScriptFile& scriptFile, const SGUID& objectGUID)
		: CDialog(IDD_SCHEMATYC_ADD_ABSTRACT_INTERFACE_IMPLEMENTATION, pParent)
		, m_pos(pos)
		, m_scriptFile(scriptFile)
		, m_objectGUID(objectGUID)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CAddAbstractInterfaceImplementationDlg::GetAbstractInterfaceImplementationGUID() const
	{
		return m_abstractInterfaceImplementationGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CAddAbstractInterfaceImplementationDlg::OnInitDialog()
	{
		SetWindowPos(NULL, m_pos.x, m_pos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		CDialog::OnInitDialog();

		m_scriptFile.VisitAbstractInterfaceImplementations(ScriptAbstractInterfaceImplementationConstVisitor::FromMemberFunction<CAddAbstractInterfaceImplementationDlg, &CAddAbstractInterfaceImplementationDlg::VisitScriptAbstractInterfaceImplementation>(*this), m_objectGUID,  false);

		GetSchematyc()->GetEnvRegistry().VisitAbstractInterfaces(EnvAbstractInterfaceVisitor::FromMemberFunction<CAddAbstractInterfaceImplementationDlg, &CAddAbstractInterfaceImplementationDlg::VisitEnvAbstractInterface>(*this));
		ScriptIncludeRecursionUtils::VisitAbstractInterfaces(m_scriptFile, ScriptIncludeRecursionUtils::AbstractInterfaceVisitor::FromMemberFunction<CAddAbstractInterfaceImplementationDlg, &CAddAbstractInterfaceImplementationDlg::VisitScriptAbstractInterface>(*this), SGUID(), true);
		
		for(AbstractInterfaceVector::const_iterator iAbstractInterface = m_abstractInterfaces.begin(), iEndAbstractInterface = m_abstractInterfaces.end(); iAbstractInterface != iEndAbstractInterface; ++ iAbstractInterface)
		{
			m_abstractInterfacesCtrl.AddString(iAbstractInterface->name.c_str());
		}
		m_abstractInterfacesCtrl.SetCurSel(0);
		OnAbstractInterfacesCtrlSelChange();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddAbstractInterfaceImplementationDlg::DoDataExchange(CDataExchange* pDX)
	{
		DDX_Control(pDX, IDD_SCHEMATYC_ABSTRACT_INTERFACES, m_abstractInterfacesCtrl);
		DDX_Control(pDX, IDD_SCHEMATYC_ABSTRACT_INTERFACE_DESCRIPTION, m_descriptionCtrl);
		CDialog::DoDataExchange(pDX);
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddAbstractInterfaceImplementationDlg::OnAbstractInterfacesCtrlSelChange()
	{
		const char*							textDesc = "";
		const AbstractInterface	abstractInterface = GetAbstractInterface();
		switch(abstractInterface.domain)
		{
		case EDomain::Env:
			{
				IAbstractInterfaceConstPtr pAbstractInterface = GetSchematyc()->GetEnvRegistry().GetAbstractInterface(abstractInterface.guid);
				if(pAbstractInterface)
				{
					textDesc = pAbstractInterface->GetDescription();
				}
				break;
			}
		case EDomain::Script:
			{
				const IScriptAbstractInterface* pAbstractInterface = ScriptIncludeRecursionUtils::GetAbstractInterface(m_scriptFile, abstractInterface.guid).second;
				if(pAbstractInterface)
				{
					textDesc = pAbstractInterface->GetDescription();
				}
				break;
			}
		}
		m_descriptionCtrl.SetWindowText(textDesc);
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddAbstractInterfaceImplementationDlg::OnOK()
	{
		const AbstractInterface                 abstractInterface = GetAbstractInterface();
		IScriptAbstractInterfaceImplementation* pAbstractInterfaceImplementation = m_scriptFile.AddAbstractInterfaceImplementation(m_objectGUID, abstractInterface.domain, abstractInterface.guid);
		if(pAbstractInterfaceImplementation)
		{
			m_abstractInterfaceImplementationGUID = pAbstractInterfaceImplementation->GetGUID();
		}
		CDialog::OnOK();
	}

	//////////////////////////////////////////////////////////////////////////
	CAddAbstractInterfaceImplementationDlg::AbstractInterface::AbstractInterface() {}

	//////////////////////////////////////////////////////////////////////////
	CAddAbstractInterfaceImplementationDlg::AbstractInterface::AbstractInterface(const char* _name, EDomain _domain, const SGUID& _guid)
		: name(_name)
		, domain(_domain)
		, guid(_guid)
	{}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CAddAbstractInterfaceImplementationDlg::VisitScriptAbstractInterfaceImplementation(const IScriptAbstractInterfaceImplementation& abstractInterfaceImplementation)
	{
		m_exclusions.push_back(abstractInterfaceImplementation.GetRefGUID());
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CAddAbstractInterfaceImplementationDlg::VisitEnvAbstractInterface(const IAbstractInterfaceConstPtr& pAbstractInterface)
	{
		const SGUID	abstractInterfaceGUID = pAbstractInterface->GetGUID();
		if(std::find(m_exclusions.begin(), m_exclusions.end(), abstractInterfaceGUID) == m_exclusions.end())
		{
			stack_string	fullName;
			EnvRegistryUtils::GetFullName(pAbstractInterface->GetName(), pAbstractInterface->GetNamespace(), fullName);
			m_abstractInterfaces.push_back(AbstractInterface(fullName.c_str(), EDomain::Env, abstractInterfaceGUID));
		}
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddAbstractInterfaceImplementationDlg::VisitScriptAbstractInterface(const TScriptFile& abstractInterfaceFile, const IScriptAbstractInterface& abstractInterface)
	{
		const SGUID abstractInterfaceGUID = abstractInterface.GetGUID();
		if(std::find(m_exclusions.begin(), m_exclusions.end(), abstractInterfaceGUID) == m_exclusions.end())
		{
			stack_string fullName;
			DocUtils::GetFullElementName(m_scriptFile, abstractInterface, fullName);
			m_abstractInterfaces.push_back(AbstractInterface(fullName.c_str(), EDomain::Script, abstractInterfaceGUID));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CAddAbstractInterfaceImplementationDlg::AbstractInterface CAddAbstractInterfaceImplementationDlg::GetAbstractInterface() const
	{
		const int	curSel = m_abstractInterfacesCtrl.GetCurSel();
		if((curSel != LB_ERR) && (curSel >= 0))
		{
			return m_abstractInterfaces[curSel];
		}
		return AbstractInterface();
	}
}
