// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MainWindow.h"

#include "PropertiesWidget.h"
#include "EnvBrowserWidget.h"
#include "LogWidget.h"
#include "PreviewWidget.h"
#include "ScriptBrowserWidget.h"

#include "ObjectModel.h"
#include "ComponentsWidget.h"
#include "ObjectStructureWidget.h"
#include "GraphViewWidget.h"
#include "GraphViewModel.h"
#include "VariablesWidget.h"
#include "GraphNodeItem.h"

#include "LibraryAssetType.h"
#include "EntityAssetType.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/AssetType.h>
#include <IUndoManager.h>

#include <CrySchematyc/Compiler/ICompiler.h>
#include <CrySchematyc/SerializationUtils/SerializationToString.h>
#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>
#include <CrySchematyc/Script/IScript.h>
#include <CrySchematyc/Script/Elements/IScriptClass.h>
#include <CrySchematyc/Script/Elements/IScriptModule.h>

#include <CrySystem/File/ICryPak.h>
#include <Serialization/Qt.h>
#include <CryIcon.h>
#include <EditorFramework/Inspector.h>
#include <QtUtil.h>
#include <Controls/QuestionDialog.h>
#include <QtUtil.h>

#include <QCollapsibleFrame.h>
#include <QAction>
#include <QMenu>
#include <QToolbar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QSplitter>
#include <QApplication>

namespace CrySchematycEditor {

static const char* g_szSaveIcon = "icons:General/File_Save.ico";
static const char* g_szSettingsFolder = "schematyc";
static const char* g_szSettingsFile = "settings.json";

inline Schematyc::CStackString GetSettingsFolder()
{
	Schematyc::CStackString settingsFolder = GetIEditor()->GetUserFolder();
	settingsFolder.append(g_szSettingsFolder);
	return settingsFolder;
}

inline Schematyc::CStackString GetSettingsFileName()
{
	Schematyc::CStackString settingsFileName = GetSettingsFolder();
	settingsFileName.append("\\");
	settingsFileName.append(g_szSettingsFile);
	return settingsFileName;
}

inline void FormatDetailHeader(Schematyc::CStackString& detailHeader, const Schematyc::IScriptElement* pScriptElement)
{
	detailHeader = "Detail";
	if (pScriptElement)
	{
		detailHeader.append(" - ");
		detailHeader.append(pScriptElement->GetName());
		detailHeader.append(" [");
		{
			Schematyc::CStackString elementType;
			Schematyc::SerializationUtils::ToString(elementType, pScriptElement->GetType());
			detailHeader.append(elementType.c_str());
		}
		detailHeader.append("]");
	}
}

REGISTER_VIEWPANE_FACTORY(CMainWindow, "Schematyc Editor (Experimental)", "Tools", false)

CMainWindow::CMainWindow()
	: CAssetEditor(QStringList { "SchematycEntity", "SchematycLibrary" })
	, m_pAsset(nullptr)
	, m_pCompileAllMenuAction(nullptr)
	, m_pRefreshEnvironmentMenuAction(nullptr)
	, m_pGraphView(nullptr)
	, m_pLog(nullptr)
	, m_pPreview(nullptr)
	, m_pInspector(nullptr)
	, m_pScript(nullptr)
	, m_pScriptBrowser(nullptr)
	, m_pModel(nullptr)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setObjectName(GetEditorName());

	QVBoxLayout* pWindowLayout = new QVBoxLayout();
	pWindowLayout->setContentsMargins(0, 0, 0, 0);
	SetContent(pWindowLayout);

	InitMenu();
	InitToolbar(pWindowLayout);

	RegisterWidgets();

	GetIEditor()->RegisterNotifyListener(this);
}

CMainWindow::~CMainWindow()
{
	delete m_pModel;

	SaveSettings();

	GetIEditor()->UnregisterNotifyListener(this);
}

void CMainWindow::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
		{
			if (m_pPreview)
				m_pPreview->Update();
			break;
		}
	}
}

void CMainWindow::Serialize(Serialization::IArchive& archive)
{
	// TODO: What is going to happen with this?
	int stateVersion = 1;
	QByteArray state;
	if (archive.isOutput())
	{
		//state = QMainWindow::saveState(stateVersion);
	}
	archive(Schematyc::SerializationUtils::ArchiveBlock(state, "state"), "mainWindow");
	if (archive.isInput() && !state.isEmpty())
	{
		//QMainWindow::restoreState(state, stateVersion);
	}

	archive(*m_pScriptBrowser, "scriptBrowser");
	//archive(*m_pDetail, "detail");
	//archive(*m_pGraph, "graph");
	//archive(*m_pLog, "log");
	//archive(*m_pPreview, "preview");
}

void CMainWindow::Show(const CryGUID& elementGUID, const CryGUID& detailGUID)
{
	if (m_pScriptBrowser)
	{
		m_pScriptBrowser->SelectItem(elementGUID);   // #SchematycTODO : What do we do with detail guid?
	}
}

bool CMainWindow::SaveUndo(XmlNodeRef& output) const
{
	return gEnv->pSchematyc->GetScriptRegistry().SaveUndo(output, *m_pModel->GetRootElement());
}

bool CMainWindow::RestoreUndo(const XmlNodeRef& input)
{
	CryGUID seletedItem;
	if (m_pScriptBrowser)
	{
		seletedItem = m_pScriptBrowser->GetSelectedItemGUID();
		m_pScriptBrowser->SetModel(nullptr);
	}

	if (m_pPreview)
	{
		m_pPreview->SetClass(nullptr);
		m_pPreview->SetComponentInstance(nullptr);
	}

	QPoint graphPos;
	CryGraphEditor::GraphItemIds selectedItemIds;
	if (m_pGraphView)
	{
		graphPos = m_pGraphView->GetPosition();
		for (CryGraphEditor::CAbstractNodeGraphViewModelItem* pItem : m_pGraphView->GetSelectedItems())
		{
			if (CNodeItem* pNodeItem = pItem->Cast<CNodeItem>())
				selectedItemIds.emplace_back(pNodeItem->GetId());
		}
		m_pGraphView->SetModel(nullptr);
	}

	gEnv->pSchematyc->GetScriptRegistry().RestoreUndo(input, m_pModel->GetRootElement());
	m_pModel = new Schematyc::CScriptBrowserModel(*this, *m_pAsset, m_pScript->GetRoot()->GetGUID());
	if (m_pScriptBrowser)
	{
		m_pScriptBrowser->SetModel(m_pModel);
		if (seletedItem != CryGUID::Null())
			m_pScriptBrowser->SelectItem(seletedItem);
	}

	if (m_pGraphView)
	{
		m_pGraphView->SetPosition(graphPos);
		m_pGraphView->SelectItems(selectedItemIds);
	}

	if (m_pPreview && m_pScript->GetRoot()->GetType() == Schematyc::EScriptElementType::Class)
	{
		m_pPreview->SetClass(static_cast<const Schematyc::IScriptClass*>(m_pScript->GetRoot()));
	}

	return true;
}

void CMainWindow::CreateDefaultLayout(CDockableContainer* pSender)
{
	CRY_ASSERT(pSender);

	QWidget* pScriptBrowserWidget = pSender->SpawnWidget("Script Browser");
	pSender->SpawnWidget("Graph View", QToolWindowAreaReference::VSplitRight);

	QWidget* pPreviewWidget = pSender->SpawnWidget("Preview", QToolWindowAreaReference::VSplitRight);

	pSender->SpawnWidget("Log", pPreviewWidget, QToolWindowAreaReference::HSplitBottom);
	pSender->SpawnWidget("Properties", pScriptBrowserWidget, QToolWindowAreaReference::HSplitBottom);
}

bool CMainWindow::OnOpenAsset(CAsset* pAsset)
{
	ICrySchematycCore* pSchematycCore = gEnv->pSchematyc;
	Schematyc::IScriptRegistry& scriptRegistry = pSchematycCore->GetScriptRegistry();

	const size_t fileCount = pAsset->GetFilesCount();
	CRY_ASSERT_MESSAGE(fileCount > 0, "Schematyc Editor: Asset '%s' has no files.", pAsset->GetName());

	const stack_string szFile = PathUtil::GetGameFolder() + "/" + string(pAsset->GetFile(0));

	m_pScript = scriptRegistry.GetScriptByFileName(szFile);
	if (m_pScript)
	{
		m_pAsset = pAsset;
		m_pModel = new Schematyc::CScriptBrowserModel(*this, *pAsset, m_pScript->GetRoot()->GetGUID());

		if (m_pScriptBrowser)
		{
			m_pScriptBrowser->SetModel(m_pModel);
		}

		if (m_pPreview && m_pScript->GetRoot()->GetType() == Schematyc::EScriptElementType::Class)
		{
			m_pPreview->SetClass(static_cast<const Schematyc::IScriptClass*>(m_pScript->GetRoot()));
		}

		return true;
	}

	return false;
}

bool CMainWindow::OnSaveAsset(CEditableAsset& editAsset)
{
	if (m_pScript)
	{
		if (m_pAsset == &editAsset.GetAsset())
		{
			ICrySchematycCore* pSchematycCore = gEnv->pSchematyc;
			Schematyc::IScriptRegistry& scriptRegistry = pSchematycCore->GetScriptRegistry();

			scriptRegistry.SaveScript(*m_pScript);
			return true;
		}
	}

	return false;
}

bool CMainWindow::OnAboutToCloseAsset(string& reason) const
{
	if (m_pScriptBrowser && m_pScript && m_pScriptBrowser->HasScriptUnsavedChanges())
	{
		CRY_ASSERT(GetAssetBeingEdited());
		reason = QtUtil::ToString(tr("Asset '%1' has unsaved modifications.").arg(GetAssetBeingEdited()->GetName()));
		return false;
	}
	return true;
}

void CMainWindow::OnCloseAsset()
{
	if (CBroadcastManager* pBroadcastManager = CBroadcastManager::Get(this))
	{
		PopulateInspectorEvent popEvent([](CInspector& inspector) {});
		pBroadcastManager->Broadcast(popEvent);
	}

	if (m_pGraphView)
	{
		m_pGraphView->SetModel(nullptr);
	}

	if (m_pPreview)
	{
		m_pPreview->SetClass(nullptr);
		m_pPreview->SetComponentInstance(nullptr);
	}

	if (m_pScriptBrowser)
	{
		// Revert changes.
		if (m_pScript && m_pScriptBrowser->HasScriptUnsavedChanges())
		{
			ICrySchematycCore* pSchematycCore = gEnv->pSchematyc;
			Schematyc::IScriptRegistry& scriptRegistry = pSchematycCore->GetScriptRegistry();

			const stack_string scriptFile = m_pScript->GetFilePath();
			if (Schematyc::IScript* pReloadedScript = scriptRegistry.LoadScript(scriptFile.c_str()))
			{
				// TODO: Recompilation should not happen in editor code t all!
				gEnv->pSchematyc->GetCompiler().CompileDependencies(pReloadedScript->GetRoot()->GetGUID());
				gEnv->pSchematyc->GetCompiler().CompileAll();
				// ~TODO
			}
		}

		m_pScriptBrowser->SetModel(nullptr);
	}

	delete m_pModel;
	m_pModel = nullptr;

	m_pAsset = nullptr;
}

void CMainWindow::RegisterWidgets()
{
	EnableDockingSystem();

	RegisterDockableWidget("Script Browser", [&]() { return CreateScriptBrowserWidget(); }, true, false);
	RegisterDockableWidget("Graph View", [&]() { return CreateGraphViewWidget(); }, true, false);
	RegisterDockableWidget("Preview", [&]() { return CreatePreviewWidget(); }, true, false);
	RegisterDockableWidget("Log", [&]() { return CreateLogWidget(); }, true, false);
	RegisterDockableWidget("Properties", [&]() { return CreateInspectorWidget(); }, true, false);
}

void CMainWindow::InitMenu()
{
	// TODO: Delete is not working atm. As soon as we have a mechanism to know what was last
	//			 focused we can bring it back.
	const CEditor::MenuItems items[] = {
		CEditor::MenuItems::FileMenu, CEditor::MenuItems::Save,
		CEditor::MenuItems::EditMenu, CEditor::MenuItems::Undo,CEditor::MenuItems::Redo,
		CEditor::MenuItems::Copy,     CEditor::MenuItems::Paste,/*CEditor::MenuItems::Delete*/
	};
	// ~TODO
	AddToMenu(items, sizeof(items) / sizeof(CEditor::MenuItems));
}

void CMainWindow::InitToolbar(QVBoxLayout* pWindowLayout)
{
	QHBoxLayout* pToolBarsLayout = new QHBoxLayout();
	pToolBarsLayout->setDirection(QBoxLayout::LeftToRight);
	pToolBarsLayout->setSizeConstraint(QLayout::SetMaximumSize);

	{
		QToolBar* pToolBar = new QToolBar("Schematyc Tools");

		{
			QAction* pAction = pToolBar->addAction(CryIcon("icons:General/File_Save.ico"), QString());
			pAction->setToolTip("Saves all changes.");
			pAction->setShortcut(QObject::tr("Ctrl+S"));
			QObject::connect(pAction, &QAction::triggered, this, &CMainWindow::OnSave);
		}

		{
			m_pClearLogToolbarAction = pToolBar->addAction(CryIcon("icons:schematyc/toolbar_clear_log.ico"), QString());
			m_pClearLogToolbarAction->setToolTip("Clears log output.");
			m_pClearLogToolbarAction->setEnabled(false);
			QObject::connect(m_pClearLogToolbarAction, &QAction::triggered, this, &CMainWindow::ClearLog);
		}

		{
			m_pShowLogSettingsToolbarAction = pToolBar->addAction(CryIcon("icons:schematyc/toolbar_log_settings.ico"), QString());
			m_pShowLogSettingsToolbarAction->setToolTip("Shows log settings in properties panel.");
			m_pShowLogSettingsToolbarAction->setEnabled(false);
			QObject::connect(m_pShowLogSettingsToolbarAction, &QAction::triggered, this, &CMainWindow::ShowLogSettings);
		}

		{
			m_pShowPreviewSettingsToolbarAction = pToolBar->addAction(CryIcon("icons:schematyc/toolbar_preview_settings.ico"), QString());
			m_pShowPreviewSettingsToolbarAction->setToolTip("Shows preview settings in properties panel.");
			m_pShowPreviewSettingsToolbarAction->setEnabled(false);
			QObject::connect(m_pShowPreviewSettingsToolbarAction, &QAction::triggered, this, &CMainWindow::ShowPreviewSettings);
		}

		pToolBarsLayout->addWidget(pToolBar, 0, Qt::AlignLeft);
	}

	pWindowLayout->addLayout(pToolBarsLayout);
}

bool CMainWindow::OnUndo()
{
	GetIEditor()->GetIUndoManager()->Undo();
	return true;
}

bool CMainWindow::OnRedo()
{
	GetIEditor()->GetIUndoManager()->Redo();
	return true;
}

bool CMainWindow::OnCopy()
{
	if (m_pGraphView && m_pGraphView->OnCopyEvent())
	{
		return true;
	}
	return true;
}

bool CMainWindow::OnCut()
{
	// TODO: Not yet implemented in graph view.
	return true;
}

bool CMainWindow::OnPaste()
{
	if (m_pGraphView && m_pGraphView->OnPasteEvent())
	{
		return true;
	}
	return false;
}

bool CMainWindow::OnDelete()
{
	QWidget* pFocusWidget = focusWidget();
	if (m_pGraphView && pFocusWidget == m_pGraphView)
	{
		m_pGraphView->OnDeleteEvent();
	}
	else if (m_pScriptBrowser && pFocusWidget == m_pScriptBrowser)
	{
		m_pScriptBrowser->OnRemoveItem();
	}

	return true;
}

void CMainWindow::SetLayout(const QVariantMap& state)
{
	CEditor::SetLayout(state);
}

QVariantMap CMainWindow::GetLayout() const
{
	QVariantMap state = CEditor::GetLayout();
	return state;
}

void CMainWindow::OnCompileAll()
{
	gEnv->pSchematyc->GetCompiler().CompileAll();
}

void CMainWindow::OnRefreshEnv()
{
	gEnv->pSchematyc->RefreshEnv();
}

void CMainWindow::OnLogWidgetDestruction(QObject* pObject)
{
	Schematyc::CLogWidget* pWidget = static_cast<Schematyc::CLogWidget*>(pObject);
	if (m_pLog == pWidget)
	{
		m_pLog = nullptr;
	}
}

void CMainWindow::OnPreviewWidgetDestruction(QObject* pObject)
{
	Schematyc::CPreviewWidget* pWidget = static_cast<Schematyc::CPreviewWidget*>(pObject);
	if (m_pPreview == pWidget)
	{
		m_pPreview = nullptr;
	}
}

void CMainWindow::OnScriptBrowserWidgetDestruction(QObject* pObject)
{
	Schematyc::CScriptBrowserWidget* pWidget = static_cast<Schematyc::CScriptBrowserWidget*>(pObject);
	if (m_pScriptBrowser == pWidget)
	{
		m_pScriptBrowser = nullptr;
	}

	pWidget->GetSelectionSignalSlots().Disconnect(m_connectionScope);
}

void CMainWindow::OnInspectorWidgetDestruction(QObject* pObject)
{
	CInspector* pWidget = static_cast<CInspector*>(pObject);
	if (m_pInspector == pWidget)
	{
		m_pInspector = nullptr;
	}
}

void CMainWindow::OnGraphViewWidgetDestruction(QObject* pObject)
{
	CGraphViewWidget* pWidget = static_cast<CGraphViewWidget*>(pObject);
	if (m_pGraphView == pWidget)
	{
		m_pGraphView = nullptr;
	}
}

void CMainWindow::ConfigureLogs()
{
	Schematyc::ILog& log = gEnv->pSchematyc->GetLog();

	// Log
	m_logSettings.streams.push_back(log.GetStreamName(Schematyc::LogStreamId::Default));
	m_logSettings.streams.push_back(log.GetStreamName(Schematyc::LogStreamId::Core));
	m_logSettings.streams.push_back(log.GetStreamName(Schematyc::LogStreamId::Editor));
	m_logSettings.streams.push_back(log.GetStreamName(Schematyc::LogStreamId::Env));
	m_logSettings.streams.push_back(log.GetStreamName(Schematyc::LogStreamId::Compiler));
}

void CMainWindow::LoadSettings()
{
	const Schematyc::CStackString settingsFileName = GetSettingsFileName();
	Serialization::LoadJsonFile(*this, settingsFileName.c_str());
}

void CMainWindow::SaveSettings()
{
	/*{
	   const Schematyc::CStackString szSettingsFolder = GetSettingsFolder();
	   gEnv->pCryPak->MakeDir(szSettingsFolder.c_str());
	   }
	   {
	   const Schematyc::CStackString settingsFileName = GetSettingsFileName();
	   Serialization::SaveJsonFile(settingsFileName.c_str(), *this);
	   }*/
}

void CMainWindow::OnScriptBrowserSelection(const Schematyc::SScriptBrowserSelection& selection)
{
	if (selection.pScriptElement)
	{
		Schematyc::IScriptGraph* pScriptGraph = selection.pScriptElement->GetExtensions().QueryExtension<Schematyc::IScriptGraph>();
		if (pScriptGraph)
		{
			if (m_pGraphView)
			{
				if (CNodeGraphViewModel* pModel = static_cast<CNodeGraphViewModel*>(m_pGraphView->GetModel()))
				{
					const QPoint pos = m_pGraphView->GetPosition();
					pModel->GetScriptGraph().SetPos(Vec2(pos.x(), pos.y()));
				}

				m_pGraphView->SetModel(new CNodeGraphViewModel(*pScriptGraph));

				const Vec2 pos = pScriptGraph->GetPos();
				m_pGraphView->SetPosition(QPoint(pos.x, pos.y));
			}
		}
		else
		{
			if (m_pGraphView)
			{
				CNodeGraphViewModel* pModel = static_cast<CNodeGraphViewModel*>(m_pGraphView->GetModel());
				if (pModel)
				{
					const QPoint pos = m_pGraphView->GetPosition();
					pModel->GetScriptGraph().SetPos(Vec2(pos.x(), pos.y()));
				}
				m_pGraphView->SetModel(nullptr);
				delete pModel;
			}
		}

		// Update properties
		if (CBroadcastManager* pBroadcastManager = CBroadcastManager::Get(this))
		{
			IDetailItem* pDetailItem = new CScriptElementDetailItem(selection.pScriptElement);
			CPropertiesWidget* pPropertiesWidget = new CPropertiesWidget(*pDetailItem, this, m_pPreview);
			Schematyc::IScriptElement* pScriptElement = selection.pScriptElement;

			PopulateInspectorEvent popEvent([pPropertiesWidget, pScriptElement](CInspector& inspector)
				{
					QCollapsibleFrame* pInspectorWidget = new QCollapsibleFrame(pScriptElement->GetName());
					pInspectorWidget->SetWidget(pPropertiesWidget);
					inspector.AddWidget(pInspectorWidget);
			  });

			pBroadcastManager->Broadcast(popEvent);
		}

		// Attach to preview.
		if (m_pPreview)
		{
			const Schematyc::IScriptElement* pScriptClass = selection.pScriptElement;
			for (; pScriptClass && (pScriptClass->GetType() != Schematyc::EScriptElementType::Class); pScriptClass = pScriptClass->GetParent())
			{
			}
			if (pScriptClass)
			{
				m_pPreview->SetClass(static_cast<const Schematyc::IScriptClass*>(pScriptClass));
			}
			if (selection.pScriptElement->GetType() == Schematyc::EScriptElementType::ComponentInstance)
			{
				m_pPreview->SetComponentInstance(static_cast<const Schematyc::IScriptComponentInstance*>(selection.pScriptElement));
			}
			else
			{
				m_pPreview->SetComponentInstance(nullptr);
			}
		}
	}
	else
	{
		if (m_pGraphView)
		{
			CNodeGraphViewModel* pModel = static_cast<CNodeGraphViewModel*>(m_pGraphView->GetModel());
			if (pModel)
			{
				const QPoint pos = m_pGraphView->GetPosition();
				pModel->GetScriptGraph().SetPos(Vec2(pos.x(), pos.y()));
			}
			m_pGraphView->SetModel(nullptr);
			delete pModel;
		}

		if (m_pPreview)
		{
			m_pPreview->SetClass(nullptr);
			m_pPreview->SetComponentInstance(nullptr);
		}
	}
}

void CMainWindow::ClearLog()
{
	if (m_pLog)
	{
		m_pLog->Clear();
	}
}

void CMainWindow::ShowLogSettings()
{
	if (m_pLog)
	{
		if (CBroadcastManager* pBroadcastManager = CBroadcastManager::Get(this))
		{
			PopulateInspectorEvent popEvent([this](CInspector& inspector)
				{
					QCollapsibleFrame* pInspectorWidget = new QCollapsibleFrame("Log Settings");
					pInspectorWidget->SetWidget(new Schematyc::CLogSettingsWidget(m_logSettings));
					inspector.AddWidget(pInspectorWidget);
			  });

			pBroadcastManager->Broadcast(popEvent);
		}
	}
}

void CMainWindow::ShowPreviewSettings()
{
	if (m_pPreview)
	{
		if (CBroadcastManager* pBroadcastManager = CBroadcastManager::Get(this))
		{
			PopulateInspectorEvent popEvent([this](CInspector& inspector)
				{
					QCollapsibleFrame* pInspectorWidget = new QCollapsibleFrame("Preview Settings");
					pInspectorWidget->SetWidget(new Schematyc::CPreviewSettingsWidget(*m_pPreview));
					inspector.AddWidget(pInspectorWidget);
			  });

			pBroadcastManager->Broadcast(popEvent);
		}
	}
}

Schematyc::CLogWidget* CMainWindow::CreateLogWidget()
{
	Schematyc::CLogWidget* pLogWidget = new Schematyc::CLogWidget(m_logSettings);
	pLogWidget->InitLayout();

	m_pClearLogToolbarAction->setEnabled(true);
	m_pShowLogSettingsToolbarAction->setEnabled(true);

	if (m_pLog == nullptr)
	{
		m_pLog = pLogWidget;
	}

	QObject::connect(pLogWidget, &QObject::destroyed, this, &CMainWindow::OnLogWidgetDestruction);
	return pLogWidget;
}

Schematyc::CPreviewWidget* CMainWindow::CreatePreviewWidget()
{
	Schematyc::CPreviewWidget* pPreviewWidget = new Schematyc::CPreviewWidget(this);
	pPreviewWidget->InitLayout();

	m_pShowPreviewSettingsToolbarAction->setEnabled(true);

	if (m_pPreview == nullptr)
	{
		m_pPreview = pPreviewWidget;
	}

	QObject::connect(pPreviewWidget, &QObject::destroyed, this, &CMainWindow::OnPreviewWidgetDestruction);
	return pPreviewWidget;
}

CInspector* CMainWindow::CreateInspectorWidget()
{
	CInspector* pInspectorWidget = new CInspector(this);
	if (m_pInspector == nullptr)
	{
		m_pInspector = pInspectorWidget;
	}

	QObject::connect(pInspectorWidget, &QObject::destroyed, this, &CMainWindow::OnInspectorWidgetDestruction);
	return pInspectorWidget;
}

CGraphViewWidget* CMainWindow::CreateGraphViewWidget()
{
	CGraphViewWidget* pGraphViewWidget = new CGraphViewWidget(*this);
	if (m_pGraphView == nullptr)
	{
		m_pGraphView = pGraphViewWidget;
	}

	QObject::connect(pGraphViewWidget, &QObject::destroyed, this, &CMainWindow::OnGraphViewWidgetDestruction);
	return pGraphViewWidget;
}

Schematyc::CScriptBrowserWidget* CMainWindow::CreateScriptBrowserWidget()
{
	Schematyc::CScriptBrowserWidget* pScriptBrowser = new Schematyc::CScriptBrowserWidget(*this);
	pScriptBrowser->InitLayout();
	pScriptBrowser->GetSelectionSignalSlots().Connect(SCHEMATYC_MEMBER_DELEGATE(&CMainWindow::OnScriptBrowserSelection, *this), m_connectionScope);

	QObject::connect(pScriptBrowser, &Schematyc::CScriptBrowserWidget::OnScriptElementRemoved, [this](Schematyc::IScriptElement& scriptElement)
		{
			if (m_pGraphView)
			{
			  CNodeGraphViewModel* pModel = static_cast<CNodeGraphViewModel*>(m_pGraphView->GetModel());
			  if (pModel)
			  {
			    Schematyc::IScriptGraph* pScriptGraph = scriptElement.GetExtensions().QueryExtension<Schematyc::IScriptGraph>();
			    if (pScriptGraph && &pModel->GetScriptGraph() == pScriptGraph)
			    {
			      m_pGraphView->SetModel(nullptr);
			    }
			  }
			}

	  });

	if (m_pModel)
	{
		pScriptBrowser->SetModel(m_pModel);
	}

	if (m_pScriptBrowser == nullptr)
	{
		m_pScriptBrowser = pScriptBrowser;
	}

	QObject::connect(pScriptBrowser, &QObject::destroyed, this, &CMainWindow::OnScriptBrowserWidgetDestruction);
	return pScriptBrowser;
}

} // Schematyc

