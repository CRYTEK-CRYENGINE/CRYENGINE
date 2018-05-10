// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/IAbstractInterface.h>
#include <CrySchematyc2/Script/IScriptFile.h>

namespace Schematyc2
{
	typedef std::vector<SGUID> GUIDVector;

	class CAddAbstractInterfaceImplementationDlg : public CDialog
	{
		DECLARE_MESSAGE_MAP()

	public:

		CAddAbstractInterfaceImplementationDlg(CWnd* pParent, CPoint pos, TScriptFile& scriptFile, const SGUID& objectGUID);

		SGUID GetAbstractInterfaceImplementationGUID() const;

	protected:

		virtual BOOL OnInitDialog();
		virtual void DoDataExchange(CDataExchange* pDX);
		virtual void OnAbstractInterfacesCtrlSelChange();
		virtual void OnOK();

	private:

		struct AbstractInterface
		{
			AbstractInterface();
			AbstractInterface(const char* _name, EDomain _domain, const SGUID& _guid);

			string  name;
			EDomain domain;
			SGUID   guid;
		};

		typedef std::vector<AbstractInterface> AbstractInterfaceVector;

		EVisitStatus VisitScriptAbstractInterfaceImplementation(const IScriptAbstractInterfaceImplementation& abstractInterfaceImplementation);
		EVisitStatus VisitEnvAbstractInterface(const IAbstractInterfaceConstPtr& pAbstractInterface);
		void VisitScriptAbstractInterface(const TScriptFile& abstractInterfaceFile, const IScriptAbstractInterface& abstractInterface);
		AbstractInterface GetAbstractInterface() const;

		CPoint                  m_pos;
		TScriptFile&            m_scriptFile;
		SGUID                   m_objectGUID;
		GUIDVector              m_exclusions;
		AbstractInterfaceVector m_abstractInterfaces;
		CListBox                m_abstractInterfacesCtrl;
		CStatic                 m_descriptionCtrl;
		SGUID                   m_abstractInterfaceImplementationGUID;
	};
}
