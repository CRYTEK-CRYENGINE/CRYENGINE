// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Script/IScriptGraph.h>

namespace Schematyc2
{
	class CAddGraphDlg : public CDialog
	{
		DECLARE_MESSAGE_MAP()

	public:

		CAddGraphDlg(CWnd* pParent, CPoint pos, TScriptFile& scriptFile, const SGUID& scopeGUID, EScriptGraphType type);

		SGUID GetGraphGUID() const;

	protected:

		virtual BOOL OnInitDialog();
		virtual void DoDataExchange(CDataExchange* pDX);
		virtual void OnNameCtrlSelChange();
		virtual void OnFilterChanged();
		virtual void OnContextCtrlSelChange();
		virtual void OnOK();

	private:

		struct SContext
		{
			SContext();

			SContext(const SGUID& _guid, const char* _name, const char* _fullName, const bool showInListBox = true);

			SGUID  guid;
			string name;
			string fullName;
			bool   showInListBox;
		};

		typedef std::vector<SContext> TContextVector;

		EVisitStatus VisitEnvSignal(const ISignalConstPtr& pSignal);
		void VisitScriptSignal(const TScriptFile& signalFile, const IScriptSignal& signal);
		EVisitStatus VisitScriptTimer(const IScriptTimer& timer);
		const SContext* GetContext() const;
		void RefreshContextCtrl();

		CPoint           m_pos;
		TScriptFile&     m_scriptFile;
		SGUID            m_scopeGUID;
		EScriptGraphType m_type;
		CEdit            m_nameCtrl;
		CEdit            m_filterCtrl;
		bool             m_nameModified;
		TContextVector   m_contexts;
		CListBox         m_contextCtrl;
		SGUID            m_graphGUID;
	};
}
