// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Script/IScriptFile.h>

struct IPropertyTree;

namespace Schematyc2
{
	class CAddContainerDlg : public CDialog
	{
	public:

		CAddContainerDlg(CWnd* pParent, CPoint pos, TScriptFile& scriptFile, const SGUID& scopeGUID);

		SGUID GetContainerGUID() const;

	protected:

		virtual BOOL OnInitDialog();
		virtual void DoDataExchange(CDataExchange* pDX);
		virtual void OnOK();

	private:

		typedef std::pair<SGUID, string>			TGUIDStringPair;
		typedef std::vector<TGUIDStringPair>	TGUIDStringPairVector;

		EVisitStatus VisitTypeDesc(const IEnvTypeDesc& typeDesc);
		void VisitScriptEnumeration(const TScriptFile& enumerationFile, const IScriptEnumeration& enumeration);
		void VisitScriptStructure(const TScriptFile& structureFile, const IScriptStructure& structure);
		SGUID GetSelection() const;

		CPoint                m_pos;
		TScriptFile&          m_scriptFile;
		SGUID                 m_scopeGUID;
		TGUIDStringPairVector m_types;
		CComboBox             m_typesCtrl;
		CEdit                 m_nameCtrl;
		SGUID                 m_containerGUID;
	};
}
