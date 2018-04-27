// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AddContainerDlg.h"

#include <CrySchematyc2/IEnvTypeDesc.h>

#include "Resource.h"
#include "PluginUtils.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CAddContainerDlg::CAddContainerDlg(CWnd* pParent, CPoint pos, TScriptFile& scriptFile, const SGUID& scopeGUID)
		: CDialog(IDD_SCHEMATYC_ADD_CONTAINER, pParent)
		, m_pos(pos)
		, m_scriptFile(scriptFile)
		, m_scopeGUID(scopeGUID)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CAddContainerDlg::GetContainerGUID() const
	{
		return m_containerGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CAddContainerDlg::OnInitDialog()
	{
		SetWindowPos(NULL, m_pos.x, m_pos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		CDialog::OnInitDialog();

		GetSchematyc()->GetEnvRegistry().VisitTypeDescs(EnvTypeDescVisitor::FromMemberFunction<CAddContainerDlg, &CAddContainerDlg::VisitTypeDesc>(*this));
		ScriptIncludeRecursionUtils::VisitEnumerations(m_scriptFile, ScriptIncludeRecursionUtils::EnumerationVisitor::FromMemberFunction<CAddContainerDlg, &CAddContainerDlg::VisitScriptEnumeration>(*this), SGUID(), true);
		ScriptIncludeRecursionUtils::VisitStructures(m_scriptFile, ScriptIncludeRecursionUtils::StructureVisitor::FromMemberFunction<CAddContainerDlg, &CAddContainerDlg::VisitScriptStructure>(*this), SGUID(), true);
		for(TGUIDStringPairVector::const_iterator iType = m_types.begin(), iEndType = m_types.end(); iType != iEndType; ++ iType)
		{
			m_typesCtrl.AddString(iType->second.c_str());
		}
		m_typesCtrl.SetCurSel(0);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddContainerDlg::DoDataExchange(CDataExchange* pDX)
	{
		DDX_Control(pDX, IDC_SCHEMATYC_CONTAINER_TYPES, m_typesCtrl);
		DDX_Control(pDX, IDC_SCHEMATYC_CONTAINER_NAME, m_nameCtrl);
		CDialog::DoDataExchange(pDX);
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddContainerDlg::OnOK()
	{
		CString	name;
		m_nameCtrl.GetWindowText(name);
		if(PluginUtils::ValidateScriptElementName(CDialog::GetSafeHwnd(), m_scriptFile, m_scopeGUID, name.GetString(), true))
		{
			const IScriptContainer* pContainer = m_scriptFile.AddContainer(m_scopeGUID, name.GetString(), GetSelection());
			if(pContainer)
			{
				m_containerGUID = pContainer->GetGUID();
			}
			CDialog::OnOK();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CAddContainerDlg::VisitTypeDesc(const IEnvTypeDesc& typeDesc)
	{
		m_types.push_back(TGUIDStringPair(typeDesc.GetGUID(), typeDesc.GetName()));
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddContainerDlg::VisitScriptEnumeration(const TScriptFile& enumerationFile, const IScriptEnumeration& enumeration)
	{
		stack_string	fullName;
		DocUtils::GetFullElementName(enumerationFile, enumeration.GetGUID(), fullName);
		m_types.push_back(TGUIDStringPair(enumeration.GetGUID(), fullName.c_str()));
	}

	//////////////////////////////////////////////////////////////////////////
	void CAddContainerDlg::VisitScriptStructure(const TScriptFile& structureFile, const IScriptStructure& structure)
	{
		stack_string	fullName;
		DocUtils::GetFullElementName(structureFile, structure.GetGUID(), fullName);
		m_types.push_back(TGUIDStringPair(structure.GetGUID(), fullName.c_str()));
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CAddContainerDlg::GetSelection() const
	{
		const int	curSel = m_typesCtrl.GetCurSel();
		if((curSel != LB_ERR) && (curSel >= 0))
		{
			return m_types[curSel].first;
		}
		return SGUID();
	}
}
