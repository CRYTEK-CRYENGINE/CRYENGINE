// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Serialization/SerializationUtils.h>
#include <CrySchematyc2/Services/ILog.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ScopedConnection.h>

#include "BrowserCtrl.h"
#include "DetailWidget.h"
#include "DocGraphView.h"
#include "EnvBrowserCtrl.h"
#include "RichEditCtrl.h"

namespace Schematyc2
{
	class CLogWnd;
	class CPreviewWnd;

	class CMainFrameWnd : public CXTPFrameWnd, public IEditorNotifyListener
	{
		DECLARE_DYNCREATE(CMainFrameWnd)	

		struct SGotoHelper
		{
			SGotoHelper();

			void GoForward();
			void GoBack();
			void SetCurrentSelection(SGUID gotoNode, SGUID gobackNode);
			void ClearCurrentSelection();
			void GetCanGoForwardOrBack(bool& forward, bool& back) const;

		private:
			std::vector<SGUID> m_gotoHistory;
			SGUID              m_goto;
			SGUID              m_goBack;
			int                m_currentGoto;
		};

		public:
	
			CMainFrameWnd();

			~CMainFrameWnd();

			virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
			virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd);

			// IEditorNotifyListener
			virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
			// ~IEditorNotifyListener

			static void RegisterViewClass();

			void SelectItem(SGUID& itemGuid, SGUID& childGuid);
			void GotoItem(SGUID& itemGuid, SGUID& lastItemGuid);
			void DiffSelectedItem();
			void UpdatePreview();

		protected:
	
			DECLARE_MESSAGE_MAP()

			virtual BOOL OnInitDialog();
			virtual void DoDataExchange(CDataExchange* pDataExchange);
			virtual void PostNcDestroy();
			virtual BOOL PreTranslateMessage(MSG* pMsg);

			afx_msg BOOL OnViewMenuCommand(UINT nID);
			afx_msg void OnUpdateViewMenuItem(CCmdUI* pCmdUI);
			afx_msg BOOL OnWorldDebugMenuCommand(UINT nID);
			afx_msg void OnUpdateWorldDebugMenuItem(CCmdUI* pCmdUI);
			afx_msg void OnDebugSelectedEntity();
			afx_msg void OnUpdateDebugSelectedEntity(CCmdUI* pCmdUI);
			afx_msg BOOL OnEntityDebugMenuCommand(UINT nID);
			afx_msg void OnUpdateEntityDebugMenuItem(CCmdUI* pCmdUI);
			afx_msg void OnHelpViewWiki();
			afx_msg void OnGotoOrFromTransitionGraph();
			afx_msg void OnGoto();
			afx_msg void OnGoBack();
			afx_msg void OnGotoSelectionChanged(const SGotoSelection& gotoSelection);
			afx_msg void OnHelpViewShortcutKeys();
			afx_msg void OnSave();
			afx_msg void OnSaveSettings();
			afx_msg void OnRefreshEnv();
			afx_msg void OnUpdateRefreshEnv(CCmdUI* pCmdUI);
			afx_msg LRESULT OnDockingPaneNotify(WPARAM wParam, LPARAM lParam);
			afx_msg void OnDestroy();

		private:

			bool CreateMenuAndToolbar();
			void CreatePanes();
			void LoadSettings();
			void SaveSettings();
			void InitPaneTitles();
			void SelectGraph(TScriptFile& scriptFile, IScriptGraph& scriptGraph);
			void ClearGraph();
			void UpdateCompilerOutputCtrl(bool recompileGraph);
			ILibPtr CompileScriptFile(TScriptFile& scriptFile);
			void OnBrowserSelection(const SBrowserSelectionParams& params);
			void OnBrowserItemRemoved(TScriptFile& scriptFile, IScriptElement& scriptElement);
			void OnBrowserDocModified(TScriptFile& scriptFile);
			void OnDetailModification(IDetailItem& detailItem);
			void OnGraphViewSelection(const SDocGraphViewSelection& selection);
			void OnGraphViewModification(TScriptFile& scriptFile);
			void UpdateGotoToolbarButtons();
			CXTPControl* GetToolbarControl(CXTPToolBar* pToolbar, int id);
			
			CXTPDockingPaneManager          m_dockingPaneManager;
			CBrowserCtrl                    m_browserCtrl;
			CEnvBrowserCtrl                 m_envBrowserCtrl;
			CDetailWnd*                     m_pDetailWnd;
			TScriptFile*                    m_pDetailScriptFile;
			CDocGraphWnd*                   m_pGraphWnd;
			CLogWnd*                        m_pLogWndA;
			CLogWnd*                        m_pLogWndB;
			CLogWnd*                        m_pLogWndC;
			CLogWnd*                        m_pLogWndD;
			CCustomRichEditCtrl             m_compilerOutputCtrl;
			CPreviewWnd*                    m_pPreviewWnd;
			TemplateUtils::CConnectionScope m_connectionScope;
			SGotoHelper                     m_gotoHelper;
			SGUID                           m_lastSelectedTreeItem;
	};
}
