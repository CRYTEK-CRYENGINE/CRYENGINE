// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move CQuickSearchEditCtrl inside CQuickSearchDlg?

#pragma once

#include <CrySchematyc2/IQuickSearchOptions.h>

namespace Schematyc2
{
	class CQuickSearchEditCtrl : public CEdit
	{
		DECLARE_MESSAGE_MAP()

	private:

		afx_msg UINT OnGetDlgCode();
		afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	};

	class CQuickSearchDlg : public CDialog
	{
		DECLARE_MESSAGE_MAP()

		friend class CQuickSearchEditCtrl;

	public:

		CQuickSearchDlg(CWnd* pParent, CPoint pos, const char* szCaption, const char* szDefaultFilter, const char* szDefaultOption, const IQuickSearchOptions& options);

		virtual ~CQuickSearchDlg();

		const size_t GetResult() const;

	private:

		typedef std::vector<stack_string> TStackStringVector;

		virtual BOOL OnInitDialog();
		virtual void DoDataExchange(CDataExchange* pDX);
		virtual void OnOK();

		afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
		afx_msg void OnFilterChanged();
		afx_msg void OnTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnTreeDblClk(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnViewWiki();

		void RefreshTreeCtrl(const char* szDefaultOption);
		void UpdateSelection();
		HTREEITEM FindTreeCtrlItem(HTREEITEM hRootItem, const char* name, bool recursive) const;
		HTREEITEM GetPrevTreeCtrlItem(HTREEITEM hItem);
		HTREEITEM GetNextTreeCtrlItem(HTREEITEM hItem, bool ignoreChildren);
		HTREEITEM GetLastTreeCtrlItemChild(HTREEITEM hItem);

		CPoint                     m_pos;
		string                     m_caption;
		string                     m_defaultFilter;
		string                     m_defaultOption;
		const IQuickSearchOptions& m_options;
		CQuickSearchEditCtrl       m_filterCtrl;
		CTreeCtrl                  m_treeCtrl;
		CStatic                    m_descriptionCtrl;
		CButton                    m_viewWikiButton;
		CButton                    m_okButton;
		size_t                     m_result;
	};
}
