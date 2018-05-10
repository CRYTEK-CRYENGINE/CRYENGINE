// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MainWindow.h"

#include <functional>

#include <qlayout.h>
#include <qpushbutton.h>

#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>

#include "GenericWidget.h"
#include "LegacyOpenDlg.h"
#include "LegacyOpenDlgModel.h"

//////////////////////////////////////////////////////////////////////////
namespace Cry {
namespace SchematycEd {

REGISTER_VIEWPANE_FACTORY(CMainWindow, "Schematyc Editor (Reworked)", "Tools", false)

CMainWindow::CMainWindow()
	: CAssetEditor(QStringList{ "SchematycEntity", "SchematycLibrary" })
	, m_pLegacyOpenDlg(nullptr)
	, m_pAddComponentButton(nullptr)
{
	EnableDockingSystem();

	InitLegacyOpenDlg();
	InitGenericWidgets();
}

CMainWindow::~CMainWindow()
{
	FinishLegacyOpenDlg();
	FinishGenericWidgets();
}

void CMainWindow::CreateDefaultLayout(CDockableContainer* pSender)
{
	CRY_ASSERT(pSender);

	//auto pComponentsWidget      = pSender->SpawnWidget("Generic0");
	//auto pVariablesWidget       = pSender->SpawnWidget("Generic1", QToolWindowAreaReference::VSplitRight);

	//auto pGraphsWidget          = pSender->SpawnWidget("Generic2", pComponentsWidget, QToolWindowAreaReference::HSplitBottom);
	//auto pSignalsAndTypesWidget = pSender->SpawnWidget("Generic3", pGraphsWidget,     QToolWindowAreaReference::HSplitBottom);	
}

void CMainWindow::OnOpenLegacy()
{	
	m_pLegacyOpenDlg->exec();
}

void CMainWindow::OnOpenLegacyClass(CAbstractDictionaryEntry& classEntry)
{
	CLegacyOpenDlgEntry& legacyEntry = static_cast<CLegacyOpenDlgEntry&>(classEntry);
	
	Schematyc2::IScriptFile* pScriptFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(legacyEntry.GetFullName().toLatin1().data());
	if (pScriptFile)
	{
		Schematyc2::IScriptClass* pScriptClass = pScriptFile->GetClass(legacyEntry.GetTypeGUID());
		for (auto pWidget : m_genericWidgets)
		{
			pWidget->setWindowTitle(pScriptClass->GetName());
			pWidget->LoadClass(pScriptFile->GetGUID(), pScriptClass->GetGUID());
		}
	}

	m_pLegacyOpenDlg->done(0);
}

void CMainWindow::InitLegacyOpenDlg()
{
	QAction* const pAction = GetMenu(MenuItems::FileMenu)->CreateAction(tr("Open Legacy"), 0, 0);
	connect(pAction, &QAction::triggered, this, &CMainWindow::OnOpenLegacy);

	m_pLegacyOpenDlg = new CLegacyOpenDlg();
	connect(m_pLegacyOpenDlg->GetDialogDictWidget(), &CDictionaryWidget::OnEntryDoubleClicked, this, &CMainWindow::OnOpenLegacyClass);
}

void CMainWindow::FinishLegacyOpenDlg()
{
	m_pLegacyOpenDlg->deleteLater();
}

void CMainWindow::InitGenericWidgets()
{
	//RegisterDockableWidget("Generic0", std::bind(&CMainWindow::CreateWidgetTemplateFunc, this, "<Empty>"), true, false);	
	//RegisterDockableWidget("Generic1", std::bind(&CMainWindow::CreateWidgetTemplateFunc, this, "<Empty>"), true, false);
	//RegisterDockableWidget("Generic2", std::bind(&CMainWindow::CreateWidgetTemplateFunc, this, "<Empty>"), true, false);
	//RegisterDockableWidget("Generic3", std::bind(&CMainWindow::CreateWidgetTemplateFunc, this, "<Empty>"), true, false);
}

void CMainWindow::FinishGenericWidgets()
{
	for (auto pWidget : m_genericWidgets)
	{
		pWidget->deleteLater();
	}
	m_genericWidgets.clear();

}

QWidget* CMainWindow::CreateWidgetTemplateFunc(const char* name)
{
	m_genericWidgets.push_back(CGenericWidget::Create());
	m_genericWidgets.back()->setWindowTitle(name);

	return m_genericWidgets.back();
}

} //namespace SchematycEd
} //namespace Cry
