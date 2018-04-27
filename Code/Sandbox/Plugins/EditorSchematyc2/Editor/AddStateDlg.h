// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

namespace Schematyc2
{
	class CAddStateDlg : public CDialog
	{
	public:

		CAddStateDlg(CWnd* pParent, CPoint pos, TScriptFile& scriptFile, const SGUID& scopeGUID);

		SGUID GetStateGUID() const;

	protected:

		virtual BOOL OnInitDialog();
		virtual void DoDataExchange(CDataExchange* pDX);
		virtual void OnOK();

	private:

		struct SPartner
		{
			SPartner();

			SPartner(const SGUID& _guid, const char* _name);

			SGUID		guid;
			string	name;
		};

		typedef std::vector<SPartner> TPartnerVector;

		EVisitStatus VisitScriptState(const IScriptState& state);
		size_t GetStateCount() const;
		SGUID GetPartnerGUID() const;

		CPoint         m_pos;
		TScriptFile&   m_scriptFile;
		SGUID          m_scopeGUID;
		CEdit          m_nameCtrl;
		TPartnerVector m_partners;
		CListBox       m_partnerCtrl;
		SGUID          m_stateGUID;
	};
}
