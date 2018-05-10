// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MainWindow.h"

#include <QAction.h>
#include <QDockWidget.h>
#include <QToolbar.h>

#include "BrowserWidget.h"
#include "DetailWidget.h"
#include "DocGraphView.h"
#include "LogWidget.h"
#include "PreviewWidget.h"

namespace Schematyc2
{
	// TODO pavloi 2017.01.24: window gets automatically registered and can't be disabled with sc_ExperimentalFeatures CVar. Disable its registration for now.
	//REGISTER_VIEWPANE_FACTORY(CMainWindow, "Schematyc Qt", "Tools", false);

	CMainWindow::CMainWindow()
	{
		QMainWindow::setWindowFlags(Qt::Widget);

		m_pBrowser = new CBrowserWidget(this);
		m_pDetail  = new CDetailWidget(this);
		m_pGraph   = new CDocGraphWidget(this, AfxGetMainWnd());
		m_pLog     = new CLogWidget(this);
		m_pPreview = new CPreviewWidget(this);

		QToolBar* pToolBar = new QToolBar("Toolbar");
		pToolBar->setObjectName("Schematyc Toolbar");
		{
			QAction* pSaveAllAction = pToolBar->addAction(QIcon("editor/icons/save.png"), QString());
			pSaveAllAction->setToolTip(QString("Save All"));
			connect(pSaveAllAction, SIGNAL(triggered()), this, SLOT(OnToolbarSaveAll()));

			QMainWindow::addToolBar(pToolBar);
		}

		m_pBrowser->InitLayout();
		m_pDetail->InitLayout();
		m_pGraph->InitLayout();
		m_pLog->InitLayout();
		m_pPreview->InitLayout();

		QMainWindow::setCentralWidget(m_pGraph);
		DockWidget(m_pBrowser, "Browser", Qt::LeftDockWidgetArea);
		DockWidget(m_pDetail, "Detail", Qt::RightDockWidgetArea);
		DockWidget(m_pPreview, "Preview", Qt::BottomDockWidgetArea);
		DockWidget(m_pLog, "Log", Qt::BottomDockWidgetArea);

		m_pBrowser->Signals().selection.Connect(BrowserSelectionSignal::Delegate::FromMemberFunction<CMainWindow, &CMainWindow::OnBrowserSelection>(*this), m_connectionScope);
	}

	const char* CMainWindow::GetPaneTitle() const
	{
		return "Schematyc Qt";
	}

	void CMainWindow::OnEditorNotifyEvent(EEditorNotifyEvent event)
	{
		switch(event)
		{
		case eNotify_OnIdleUpdate:
			{
				m_pPreview->Update();
				break;
			}
		}
	}

	void CMainWindow::OnUriReceived(const char* szUri) {}

	void CMainWindow::Serialize(Serialization::IArchive& archive) {}

	void CMainWindow::OnToolbarSaveAll()
	{
		GetSchematyc()->GetScriptRegistry().Save();
	}

	void CMainWindow::DockWidget(QWidget* pWidget, const char* szName, Qt::DockWidgetArea area)
	{
		QDockWidget* pDockWidget = new QDockWidget(szName, this);
		pDockWidget->setObjectName(szName);
		pDockWidget->setWidget(pWidget);
		QMainWindow::addDockWidget(area, pDockWidget);
	}

	void CMainWindow::OnBrowserSelection(IScriptElement* pScriptElement)
	{
		m_pDetail->Detach();
		if(pScriptElement)
		{
			IDetailItemPtr pDetailItem = std::make_shared<CScriptElementDetailItem>(nullptr, pScriptElement);
			m_pDetail->AttachItem(pDetailItem);

			IScriptGraphExtension* pScriptGraphExtension = static_cast<IScriptGraphExtension*>(pScriptElement->GetExtensions().QueryExtension(EScriptExtensionId::Graph));
			if(pScriptGraphExtension)
			{
				#ifdef CRY_USE_SCHEMATYC2_BRIDGE
				m_pGraph->LoadGraph(GetSchematyc()->GetScriptRegistry().Wrapfile(&pScriptElement->GetFile()), pScriptGraphExtension); //HARDCODED:
				#else
				m_pGraph->LoadGraph(&pScriptElement->GetFile(), pScriptGraphExtension);
				#endif //CRY_USE_SCHEMATYC2_BRIDGE
			}
		}
	}
}
