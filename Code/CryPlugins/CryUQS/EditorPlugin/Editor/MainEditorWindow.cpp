// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MainEditorWindow.h"

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
	, m_pPropertyTreeWidget(nullptr)
	, m_pLibraryPanel(nullptr)
	, m_pDocumentPropertyTree(nullptr)
	, m_pCurrentDocument(nullptr)
{
	// TODO pavloi 2017.04.04: derive CMainEditorWindow from CDockableEditor
	// TODO pavloi 2017.04.04: rebuild window layout - we dockable widgets don't work if we derive from CEditor/CDockableEditor.
	// Do we even need list of queries as the dockable widget?

	GetIEditor()->RegisterNotifyListener(this);

	BuildLibraryPanel();

	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	treeStyle.levelIndent = 1.0f;
	treeStyle.firstLevelIndent = 1.0f;

	m_pDocumentPropertyTree = new QPropertyTree();
	m_pDocumentPropertyTree->setExpandLevels(5);
	m_pDocumentPropertyTree->setTreeStyle(treeStyle);
	m_pDocumentPropertyTree->setUndoEnabled(true); // TODO pavloi 2017.04.03: use global editor's undo manager

	connect(m_pDocumentPropertyTree, &QPropertyTree::signalChanged, this, &CMainEditorWindow::OnPropertyTreeChanged);

	m_pDocumentTabsWidget = new QTabWidget;
	m_pDocumentTabsWidget->addTab(m_pDocumentPropertyTree, "Document");

	m_pDocumentTabsWidget->setCurrentIndex(0);

	setCentralWidget(m_pDocumentTabsWidget);

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

	if (m_pLibraryPanel)
	{
		Explorer::ExplorerPanel* pPanel = m_pLibraryPanel->widget();
		m_pLibraryPanel->dock()->setWidget(nullptr);
		delete pPanel;
	}

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
		Explorer::ExplorerPanel* pPanel = m_pLibraryPanel->widget();

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

	m_pLibraryPanel = new DockedWidget<Explorer::ExplorerPanel>(this, new Explorer::ExplorerPanel(nullptr, m_pExplorerData.get()), "Library", Qt::LeftDockWidgetArea);

	connect(m_pLibraryPanel->widget(), &Explorer::ExplorerPanel::SignalSelectionChanged, this, &CMainEditorWindow::OnLibraryExplorerSelectionChanged);
	//connect(m_pLibraryPanel->widget(), &Explorer::ExplorerPanel::SignalActivated, this, &CMainEditorWindow::OnLibraryExplorerActivated);
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

	Explorer::ExplorerPanel* pPanel = m_pLibraryPanel->widget();

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
				m_pCurrentDocument->AttachToTree(GetDocumentPropertyTreeWidget());
			}
		}
	}
}

void CMainEditorWindow::OnPropertyTreeChanged()
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
	return m_pLibraryPanel->widget();
}

QPropertyTree* CMainEditorWindow::GetDocumentPropertyTreeWidget() const
{
	return m_pDocumentPropertyTree;
}

void CMainEditorWindow::OnDocumentAboutToBeRemoved(CUqsQueryDocument* pDocument)
{
	if (m_pCurrentDocument == pDocument)
	{
		m_pCurrentDocument->DetachFromTree();
		m_pCurrentDocument = nullptr;
	}
}
