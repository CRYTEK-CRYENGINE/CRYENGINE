// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MainEditorWindow.h"
#include "Blueprints.h"

#include <QDir>
#include <QMenuBar>
#include <QFileDialog>
#include <QDockWidget>
#include <QTabWidget>
#include <QLabel>
#include <QLayout>
#include <QVBoxLayout>
#include <QAbstractButton>
#include <QDialogButtonbox>
#include <QLineEdit>
#include <QMessageBox>
#include <QDesktopServices>

#include <QtUtil.h>

#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <Util/EditorUtils.h>
#include <EditorFramework/Events.h>
#include <Controls/QuestionDialog.h>

#include "Document.h"
#include "QueryListProvider.h"

#include <CrySerialization/IArchive.h>
#include <CrySerialization/IArchiveHost.h>
#include <Serialization/Qt.h>

#include <Explorer/ExplorerPanel.h>
#include <Explorer/Explorer.h>
#include <Explorer/ExplorerFileList.h>
#include <Explorer/ExplorerModel.h>

//////////////////////////////////////////////////////////////////////////
// CNewQueryDialog
//////////////////////////////////////////////////////////////////////////

CNewQueryDialog::CNewQueryDialog(const QString& newName, QWidget* pParent)
	: BaseClass(QStringLiteral("New Query"), pParent, false)
{
	m_resultingString = newName;

	QVBoxLayout* pLayout = new QVBoxLayout(this);

	m_pLineEdit = new QLineEdit(this);
	m_pLineEdit->setText(m_resultingString);

	m_pButtons = new QDialogButtonBox(this);
	m_pButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	pLayout->addWidget(m_pLineEdit);
	pLayout->addWidget(m_pButtons);

	connect(m_pButtons, &QDialogButtonBox::accepted, this, &CNewQueryDialog::accept);
	connect(m_pButtons, &QDialogButtonBox::rejected, this, &CNewQueryDialog::reject);
}

void CNewQueryDialog::accept()
{
	m_resultingString = m_pLineEdit->text();
	BaseClass::accept();
}

const QString& CNewQueryDialog::GetResultingString()
{
	return m_resultingString;
}

//////////////////////////////////////////////////////////////////////////
// CMainEditorWindow
//////////////////////////////////////////////////////////////////////////

CMainEditorWindow::CMainEditorWindow()
	: m_editorContext()
	, m_pExplorerData()
	, m_pLibraryPanel(nullptr)
	, m_pDocumentPropertyTree(nullptr)
	, m_pCurrentDocument(nullptr)
	, m_pSimulatorPanel(nullptr)
	, m_pSimulatorPropertyTree(nullptr)
	, m_pSimulatorButton(nullptr)
	, m_pSimulatorRunModeCheckBox(nullptr)
{
	// TODO pavloi 2017.04.04: derive CMainEditorWindow from CDockableEditor
	// TODO pavloi 2017.04.04: rebuild window layout - we dockable widgets don't work if we derive from CEditor/CDockableEditor.
	// Do we even need list of queries as the dockable widget?

	GetIEditor()->RegisterNotifyListener(this);

	CCentralEventManager::QueryBlueprintRuntimeParamsChanged.Connect(this, &CMainEditorWindow::OnQueryBlueprintRuntimeParamsChanged);
	CCentralEventManager::QuerySimulatorRunningStateChanged.Connect(this, &CMainEditorWindow::OnQuerySimulatorRunningStateChanged);

	BuildLibraryPanel();
	BuildDocumentPanel();
	BuildSimulatorPanel();

	// wait until a query blueprint gets selected from the explorer library before the simulator panel becomes visible
	m_pSimulatorPanel->setVisible(false);

	QSplitter* pSplitter = new QSplitter(this);
	pSplitter->setOrientation(Qt::Horizontal);
	pSplitter->addWidget(m_pLibraryPanel);
	pSplitter->addWidget(m_pDocumentTabsWidget);
	pSplitter->addWidget(m_pSimulatorPanel);
	pSplitter->setStretchFactor(0, 0);
	pSplitter->setStretchFactor(1, 1);	// allow the document with the property tree to adaptively use most of the available space
	pSplitter->setStretchFactor(2, 0);
	setCentralWidget(pSplitter);

	connect(&m_editorContext.GetQueryListProvider(), &CQueryListProvider::DocumentAboutToBeRemoved, this, &CMainEditorWindow::OnDocumentAboutToBeRemoved);

	BuildMenu();

	// disable everything if the UQS engine plugin was not loaded
	if (!UQS::Core::IHubPlugin::GetHubPtr())
	{
		QMessageBox::warning(this, "UQS engine-plugin not loaded", "The UQS Editor will be disabled as the UQS core engine-plugin hasn't been loaded.");
		setEnabled(false);
	}
}

CMainEditorWindow::~CMainEditorWindow()
{
	// TODO pavloi 2016.07.01: ask whether the queries need to be saved

	CCentralEventManager::QueryBlueprintRuntimeParamsChanged.DisconnectObject(this);
	CCentralEventManager::QuerySimulatorRunningStateChanged.DisconnectObject(this);

	GetIEditor()->UnregisterNotifyListener(this);
}

const char* CMainEditorWindow::GetPaneTitle() const
{
	// check for whether the UQS engine plugin has been loaded
	if (UQS::Core::IHubPlugin::GetHubPtr())
	{
		return "UQS Editor";
	}
	else
	{
		return "UQS Editor (disabled due to the UQS engine-plugin not being loaded)";
	}
}

void CMainEditorWindow::OnEditorNotifyEvent(EEditorNotifyEvent ev)
{
}

void CMainEditorWindow::customEvent(QEvent* event)
{
	// TODO: This handler should be removed whenever this editor is refactored to be a CDockableEditor
	if (event->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(event);

		const string& command = commandEvent->GetCommand();
		if (command == "general.help")
		{
			event->setAccepted(EditorUtils::OpenHelpPage(GetPaneTitle()));
		}
	}

	if (!event->isAccepted())
	{
		QWidget::customEvent(event);
	}
}

void CMainEditorWindow::CreateNewDocument()
{
	QString newQueryName;

	// Prepare name which starts with the parent of currently selected entry
	{
		if (Explorer::ExplorerEntry* pEntry = GetLibraryExplorerWidget()->GetCurrentEntry())
		{
			if (Explorer::ExplorerEntry* pParent = pEntry->parent)
			{
				if (!pParent->path.empty())
				{
					newQueryName = pParent->path;
					newQueryName += '/';
				}
			}
		}
	}

	{
		CNewQueryDialog dialog(newQueryName, this);
		if (dialog.exec() == QDialog::Accepted)
		{
			newQueryName = dialog.GetResultingString();
		}
		else
		{
			return;
		}
	}

	stack_string queryName = newQueryName.toStdString().c_str();
	stack_string errorMessage;
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = m_editorContext.GetQueryListProvider().CreateNewQuery(queryName.c_str(), errorMessage))
	{
		Explorer::ExplorerPanel* pPanel = m_pLibraryPanel;

		if (Explorer::ExplorerEntry* pExplorerEntry = m_pExplorerData->FindEntryById(&m_editorContext.GetQueryListProvider(), pEntry->id))
		{
			// TODO pavloi 2016.07.04: without this line, tree view tends to not update after a record inside new group is added
			pPanel->OnRefreshFilter();

			pPanel->SetCurrentEntry(pExplorerEntry);
		}
	}
	else
	{
		CQuestionDialog::SWarning(QStringLiteral("New Query"), errorMessage.c_str());
	}
}

void CMainEditorWindow::BuildLibraryPanel()
{
	m_pExplorerData.reset(new Explorer::ExplorerData(this));
	m_pExplorerData->AddProvider(&m_editorContext.GetQueryListProvider(), "Uqs Queries");
	m_pExplorerData->SetEntryTypeIcon(Explorer::ENTRY_GROUP, "icons:General/Folder.ico");
	m_pExplorerData->Populate();

	m_pLibraryPanel = new Explorer::ExplorerPanel(nullptr, m_pExplorerData.get());

	connect(m_pLibraryPanel, &Explorer::ExplorerPanel::SignalSelectionChanged, this, &CMainEditorWindow::OnLibraryExplorerSelectionChanged);
	//connect(m_pLibraryPanel->widget(), &Explorer::ExplorerPanel::SignalActivated, this, &CMainEditorWindow::OnLibraryExplorerActivated);
}

void CMainEditorWindow::BuildDocumentPanel()
{
	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	treeStyle.levelIndent = 1.0f;
	treeStyle.firstLevelIndent = 1.0f;

	m_pDocumentPropertyTree = new QPropertyTree();
	m_pDocumentPropertyTree->setExpandLevels(5);
	m_pDocumentPropertyTree->setTreeStyle(treeStyle);
	m_pDocumentPropertyTree->setUndoEnabled(true); // TODO pavloi 2017.04.03: use global editor's undo manager

	connect(m_pDocumentPropertyTree, &QPropertyTree::signalChanged, this, &CMainEditorWindow::OnDocumentPropertyTreeChanged);

	m_pDocumentTabsWidget = new QTabWidget;
	m_pDocumentTabsWidget->addTab(m_pDocumentPropertyTree, "Document");

	m_pDocumentTabsWidget->setCurrentIndex(0);
}

void CMainEditorWindow::BuildSimulatorPanel()
{
	QGroupBox* pGroupBox = new QGroupBox;
	pGroupBox->setTitle(QtUtil::ToQString("Simulator"));

	{
		QVBoxLayout* pVBoxLayout = new QVBoxLayout;
		pVBoxLayout->setAlignment(Qt::AlignTop);

		{
			QGroupBox* pGroupBox = new QGroupBox;
			pGroupBox->setTitle(QtUtil::ToQString("Runtime parameters"));

			{
				QVBoxLayout* pVBoxLayout = new QVBoxLayout;
				pVBoxLayout->setAlignment(Qt::AlignTop);

				{
					PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
					treeStyle.propertySplitter = false;	// true would squeeze the labels more, such they quickly become unreadable
					treeStyle.levelIndent = 1.0f;		// 0.0f would swallow nested properties (such as the x/y/z textboxes of a Vec3)
					treeStyle.firstLevelIndent = 1.0f;  // (related)

					m_pSimulatorPropertyTree = new QPropertyTree;
					m_pSimulatorPropertyTree->setTreeStyle(treeStyle);
					m_pSimulatorPropertyTree->setSizeToContent(true); // ensure that the property-tree doesn't use more space than the sum of the individual controls (weird, one would expect this to happen automatically...)

					pVBoxLayout->addWidget(m_pSimulatorPropertyTree);
				}

				pGroupBox->setLayout(pVBoxLayout);
			}

			pVBoxLayout->addWidget(pGroupBox);
		}

		{
			QHBoxLayout* pHBoxLayout = new QHBoxLayout;

			{
				m_pSimulatorButton = new QPushButton;
				m_pSimulatorButton->setText(QtUtil::ToQString("Simulate"));
				connect(m_pSimulatorButton, &QPushButton::clicked, this, &CMainEditorWindow::OnSimulatorButtonClicked);
				pHBoxLayout->addWidget(m_pSimulatorButton, 0, Qt::Alignment(Qt::AlignLeft));
			}

			{
				QPushButton* pSingleStepButton = new QPushButton;
				pSingleStepButton->setText(QtUtil::ToQString("Single step once"));
				connect(pSingleStepButton, &QPushButton::clicked, this, &CMainEditorWindow::OnSingleStepOnceButtonClicked);
				pHBoxLayout->addWidget(pSingleStepButton, 0, Qt::Alignment(Qt::AlignLeft));
			}

			{
				m_pSimulatorRunModeCheckBox = new QCheckBox;
				m_pSimulatorRunModeCheckBox->setText(QtUtil::ToQString("Run infinitely")); // FIXME: the text appears to be cut off
				m_pSimulatorRunModeCheckBox->setToolTip(QtUtil::ToQString("If checked, then the query will be started all over again once finished."));
				pHBoxLayout->addWidget(m_pSimulatorRunModeCheckBox, 0, Qt::Alignment(Qt::AlignLeft));
				pHBoxLayout->addStretch();  // make sure it properly aligns to the left-side "Simulate" button
			}

			QWidget* pLayoutContainer = new QWidget;
			pLayoutContainer->setLayout(pHBoxLayout);

			{
				pVBoxLayout->addWidget(pLayoutContainer, 0, Qt::Alignment(Qt::AlignTop));
			}
		}

		pGroupBox->setLayout(pVBoxLayout);
	}

	m_pSimulatorPanel = pGroupBox;
}

static QAction* AddCheckboxAction(QMenu* pMenu, const QString& label, bool bIsChecked)
{
	QAction* pAction = pMenu->addAction(label);
	pAction->setCheckable(true);
	pAction->setChecked(bIsChecked);
	return pAction;
}

void CMainEditorWindow::BuildMenu()
{
	// TODO pavloi 2017.04.04: convert common actions (New, Save, ...) into the CEditor::MenuItems to support better integration into 
	// the editor framework (key bindings, scripted actions, etc.)

	{
		// TODO pavloi 2016.04.08: implement File menu actions

		QMenu* pFileMenu = menuBar()->addMenu("&File");
		connect(pFileMenu->addAction("&New"), &QAction::triggered, this, &CMainEditorWindow::OnMenuActionFileNew);
		connect(pFileMenu->addAction("&Save"), &QAction::triggered, this, &CMainEditorWindow::OnMenuActionFileSave);
		//connect(pFileMenu->addAction("Save &As"), &QAction::triggered, this, &CMainEditorWindow::OnMenuActionFileSaveAs);
	}

	{
		QMenu* pViewMenu = menuBar()->addMenu("&View");
		connect(AddCheckboxAction(pViewMenu, "Use &selection helpers", m_editorContext.GetSettings().bUseSelectionHelpers), &QAction::triggered,
		        this, &CMainEditorWindow::OnMenuActionViewUseSelectionHelpers);
		connect(AddCheckboxAction(pViewMenu, "Show input param &types", m_editorContext.GetSettings().bShowInputParamTypes), &QAction::triggered,
		        this, &CMainEditorWindow::OnMenuActionViewShowInputParamTypes);
		connect(AddCheckboxAction(pViewMenu, "&Filter inputs by type", m_editorContext.GetSettings().bFilterAvailableInputsByType), &QAction::triggered,
		        this, &CMainEditorWindow::OnMenuActionViewFilterInputsByType);
	}

	{
		QMenu* pHelpMenu = menuBar()->addMenu("&Help");
		connect(pHelpMenu->addAction("&Online documentation"), &QAction::triggered, this, &CMainEditorWindow::OnMenuActionHelpOnlineDocumentation);
	}
}

void CMainEditorWindow::OnMenuActionFileNew()
{
	CreateNewDocument();
}

void CMainEditorWindow::OnMenuActionFileSave()
{
	if (m_pCurrentDocument)
	{
		// TODO pavloi 2016.04.08: instead of checking selected entries, get an entiry ID from a document, which is attached to property tree.
		Explorer::ActionContext context;
		GetLibraryExplorerWidget()->GetSelectedEntries(&context.entries);
		if (!context.entries.empty())
		{
			m_pExplorerData->ActionSave(context);
		}
	}
}

void CMainEditorWindow::OnMenuActionFileSaveAs()
{
	// TODO pavloi 2016.04.08: implement
}

void CMainEditorWindow::OnMenuActionViewUseSelectionHelpers(bool checked)
{
	m_editorContext.GetSettings().bUseSelectionHelpers = checked;

	if (m_pCurrentDocument)
	{
		m_pCurrentDocument->ApplySettings(m_editorContext.GetSettings());
	}
}

void CMainEditorWindow::OnMenuActionViewShowInputParamTypes(bool checked)
{
	m_editorContext.GetSettings().bShowInputParamTypes = checked;

	if (m_pCurrentDocument)
	{
		m_pCurrentDocument->ApplySettings(m_editorContext.GetSettings());
	}
}

void CMainEditorWindow::OnMenuActionViewFilterInputsByType(bool checked)
{
	m_editorContext.GetSettings().bFilterAvailableInputsByType = checked;

	if (m_pCurrentDocument)
	{
		m_pCurrentDocument->ApplySettings(m_editorContext.GetSettings());
	}
}

void CMainEditorWindow::OnMenuActionHelpOnlineDocumentation()
{
	// URL to the UQS Query Editor (as of 2017-04-19)
	const char* szURL = "http://docs.cryengine.com/display/CEMANUAL/Sandbox+Plugin%3A+Query+Editor";
	const QUrl qURL = QtUtil::ToQString(szURL);

	QDesktopServices::openUrl(qURL);
}

void CMainEditorWindow::OnLibraryExplorerSelectionChanged()
{
	// TODO pavloi 2016.04.08: this is an example what to do on selection change.
	// I'm undecided whether we should switch property tree on click (selection) or double click (activation).

	Explorer::ExplorerPanel* pPanel = m_pLibraryPanel;

	Explorer::ExplorerEntries entries;
	pPanel->GetSelectedEntries(&entries);

	if (entries.empty())
		return;

	SetCurrentDocumentFromExplorerEntry(entries[0]);
}

void CMainEditorWindow::OnLibraryExplorerActivated(const Explorer::ExplorerEntry* pExplorerEntry)
{
	if (m_editorContext.GetQueryListProvider().OwnsAssetEntry(pExplorerEntry))
	{
		SetCurrentDocumentFromExplorerEntry(pExplorerEntry);
	}
}

void CMainEditorWindow::SetCurrentDocumentFromExplorerEntry(const Explorer::ExplorerEntry* pExplorerEntry)
{
	if (auto pEntry = m_editorContext.GetQueryListProvider().GetEntryById(pExplorerEntry->id))
	{
		if (m_editorContext.GetQueryListProvider().LoadOrGetChangedEntry(pEntry->id))
		{
			if (m_pCurrentDocument)
			{
				m_pCurrentDocument->DetachFromTree();
			}

			SUqsQueryEntry& asset = pEntry->content;

			m_pCurrentDocument = asset.GetDocument();
			if (m_pCurrentDocument)
			{
				m_pCurrentDocument->OnTreeChanged();	// validate the query blueprint for syntactical errors
				m_pCurrentDocument->AttachToTree(m_pDocumentPropertyTree);
				m_pSimulatorPanel->setVisible(true);
			}

			RebuildRuntimeParamsListForSimulation();
		}
	}
}

void CMainEditorWindow::RebuildRuntimeParamsListForSimulation()
{
	// - build the list of runtime-params for simulating the freshly selected query
	// - we visit all runtime-params of the whole query hierarchy
	// - these runtime-params can (and shall!) be filled by the user with values that he wants to get passed to the query to start simulating it

	m_simulatedRuntimeParams.params.clear();

	RebuildRuntimeParamsListForSimulationRecursively(*m_pCurrentDocument->GetQueryBlueprintPtr());

	// TODO: somehow mark the list of runtime-params as read-only if not all of them can be serialized (which means the user would not be able to enter values for them, e. g. in case of pointers)
	m_pSimulatorPropertyTree->attach(Serialization::SStruct(m_simulatedRuntimeParams));
}

void CMainEditorWindow::RebuildRuntimeParamsListForSimulationRecursively(const UQSEditor::CQueryBlueprint& queryBlueprintToRecurseFrom)
{
	const UQSEditor::CRuntimeParamBlueprint& runtimeParamsBP = queryBlueprintToRecurseFrom.GetRuntimeParamsBlueprint();

	for (size_t i = 0; i < runtimeParamsBP.GetParameterCount(); i++)
	{
		const char* szParamName;
		const char* szType;
		CryGUID typeGUID;
		bool bAddToDebugRenderWorld;
		std::shared_ptr<UQSEditor::CErrorCollector> pErrorCollector;

		runtimeParamsBP.GetParameterInfo(i, szParamName, szType, typeGUID, bAddToDebugRenderWorld, pErrorCollector);

		// skip already added parameters (by name)
		if (std::find_if(
			m_simulatedRuntimeParams.params.cbegin(),
			m_simulatedRuntimeParams.params.cend(),
			[szParamName](const CSimulatedRuntimeParam& param) { return strcmp(param.GetName(), szParamName) == 0; }) != m_simulatedRuntimeParams.params.cend())
		{
			continue;
		}

		UQS::Client::IItemFactory* pItemFactory;

		if (!(pItemFactory = UQS::Core::IHubPlugin::GetHub().GetItemFactoryDatabase().FindFactoryByGUID(typeGUID)))
		{
			pItemFactory = UQS::Core::IHubPlugin::GetHub().GetItemFactoryDatabase().FindFactoryByName(szType);
		}

		// TODO: what if pItemFactory == nullptr ? (it means that the item type is unknown)

		if (pItemFactory)
		{
			m_simulatedRuntimeParams.params.emplace_back(szParamName, *pItemFactory);
		}
	}

	// recurse down all child queries
	for (size_t i = 0; i < queryBlueprintToRecurseFrom.GetChildCount(); i++)
	{
		RebuildRuntimeParamsListForSimulationRecursively(queryBlueprintToRecurseFrom.GetChild(i));
	}
}

void CMainEditorWindow::OnQueryBlueprintRuntimeParamsChanged(const CCentralEventManager::SQueryBlueprintRuntimeParamsChangedEventArgs& args)
{
	RebuildRuntimeParamsListForSimulation();
}

void CMainEditorWindow::OnQuerySimulatorRunningStateChanged(const CCentralEventManager::SQuerySimulatorRunningStateChangedEventArgs& args)
{
	m_pSimulatorButton->setText(QtUtil::ToQString(args.bIsRunningNow ? "Stop Simulation" : "Simulate"));
}

void CMainEditorWindow::OnDocumentPropertyTreeChanged()
{
	// TODO pavloi 2016.04.25: maybe a document itself should attach to the signal?
	if (m_pCurrentDocument)
	{
		m_pCurrentDocument->OnTreeChanged();
	}

	// TODO pavloi 2016.04.08: instead of checking selected entries, get an entiry ID from a document, which is attached to property tree.

	Explorer::ExplorerEntries entries;

	GetLibraryExplorerWidget()->GetSelectedEntries(&entries);
	for (auto& entry : entries)
	{
		m_pExplorerData->CheckIfModified(entry, "Property Change", false);
	}
}

Explorer::ExplorerPanel* CMainEditorWindow::GetLibraryExplorerWidget() const
{
	return m_pLibraryPanel;
}

void CMainEditorWindow::OnDocumentAboutToBeRemoved(CUqsQueryDocument* pDocument)
{
	if (m_pCurrentDocument == pDocument)
	{
		m_pCurrentDocument->DetachFromTree();
		m_pCurrentDocument = nullptr;
		m_pSimulatorPanel->setVisible(true);
	}
}

void CMainEditorWindow::OnSimulatorButtonClicked(bool checked)
{
	// make a copy of the serialized runtime-parameters and only keep those that can be serialized (the other one's will have random values, which can give undefined behavior in the query)

	std::vector<CSimulatedRuntimeParam> serializableParams;

	for (const CSimulatedRuntimeParam& param : m_simulatedRuntimeParams.params)
	{
		if (param.GetItemFactory().CanBePersistantlySerialized())
		{
			serializableParams.push_back(param);
		}
	}

	CCentralEventManager::StartOrStopQuerySimulator(CCentralEventManager::SStartOrStopQuerySimulatorEventArgs(m_pCurrentDocument->GetQueryBlueprintPtr()->GetName().c_str(), serializableParams, m_pSimulatorRunModeCheckBox->checkState() != Qt::Unchecked));
}

void CMainEditorWindow::OnSingleStepOnceButtonClicked(bool checked)
{
	// make a copy of the serialized runtime-parameters and only keep those that can be serialized (the other one's will have random values, which can give undefined behavior in the query)

	std::vector<CSimulatedRuntimeParam> serializableParams;

	for (const CSimulatedRuntimeParam& param : m_simulatedRuntimeParams.params)
	{
		if (param.GetItemFactory().CanBePersistantlySerialized())
		{
			serializableParams.push_back(param);
		}
	}

	CCentralEventManager::SingleStepQuerySimulatorOnce(CCentralEventManager::SSingleStepQuerySimulatorOnceEventArgs(m_pCurrentDocument->GetQueryBlueprintPtr()->GetName().c_str(), serializableParams, m_pSimulatorRunModeCheckBox->checkState() != Qt::Unchecked));
}

void CMainEditorWindow::SSimulatedRuntimeParams::Serialize(Serialization::IArchive& archive)
{
	for (CSimulatedRuntimeParam& param : params)
	{
		param.Serialize(archive);
	}
}
