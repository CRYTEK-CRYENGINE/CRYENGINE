// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DrsResponseEditorWindow.h"
#include "DrsEditorMainWindow.h"

#include <Controls/QuestionDialog.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryIcon.h>
#include <CrySerialization/IArchiveHost.h>
#include <CryString/CryString.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <Explorer/Explorer.h>
#include <Explorer/ExplorerFileList.h>
#include <Explorer/ExplorerPanel.h>
#include <GameEngine.h>
#include <IEditor.h>
#include <Objects/BaseObject.h>
#include <QAction>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget.h>
#include <QWidget>
#include <QtViewPane.h>
#include <Serialization.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>

REGISTER_VIEWPANE_FACTORY(CDrsEditorMainWindow, "Dynamic Response System", "Tools", true)

using namespace Explorer;

namespace
{
enum eCurrentlyDisplayedSubelements
{
	eCDS_UNKNOWN          = -1,
	eCDS_eSignalsFromFile = 0,
	eCDS_eRecentResponses = 1,
};

static QPropertyTree* s_pPropertyTree = nullptr;
static QPropertyTree* s_pSubElementsTree = nullptr;
static QDockWidget* s_ResponseDockWidget = nullptr;

static CDrsResponseEditorWindow* s_ResponseEditorWindow = nullptr;
static eCurrentlyDisplayedSubelements s_CurrentlyDisplayedSubelements = eCDS_UNKNOWN;

static SResponseSerializerHelper s_ResponseSerializer;
static SRecentResponseSerializerHelper s_RecentResponseSerializer;

static CResponsesFileLoader s_FileSaveHelper;
static RecentResponsesWidget* s_RecentSignalsWidget;
typedef CEntryLoader<CResponsesFileLoader> ResponsesFileLoader;

static int s_currentExpandLevel = 4;

class PathHelper
{
public:
	static stack_string GetSignalNameFromFileName(stack_string szFileName)
	{
		if (szFileName.empty())
			return "ALL";
		PathUtil::RemoveExtension(szFileName);
		return PathUtil::GetFile(szFileName.c_str());
	}

	static string GetPathForSignal(ExplorerEntry* pEntry, const stack_string& signalName)
	{
		string foundPath;
		if (pEntry->type == ENTRY_ASSET && strncmp(pEntry->name.c_str(), signalName.c_str(), signalName.size()) == 0)
		{
			return pEntry->path;
		}
		for (int i = 0; i < pEntry->children.size(); i++)
		{
			foundPath = GetPathForSignal(pEntry->children[i], signalName);
			if (!foundPath.empty())
			{
				return foundPath;
			}
		}
		return foundPath;
	}

	static void CreateDirectoryIfNotExisting(const stack_string& szFileName)
	{
		stack_string currentPathToCreate;
		size_t currentPos = szFileName.rfind('/');
		if (currentPos != stack_string::npos)
		{
			currentPathToCreate = szFileName.substr(0, currentPos + 1);
			if (!QDir(currentPathToCreate.c_str()).exists())
			{
				QDir().mkpath(currentPathToCreate.c_str());
			}
		}
	}
};
};

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CDrsResponseEditorWindow::CDrsResponseEditorWindow()
{
	if (gEnv->pDynamicResponseSystem->GetResponseManager())
	{
		CRY_ASSERT(s_pPropertyTree == nullptr);
		CRY_ASSERT(s_pSubElementsTree == nullptr);
		s_ResponseEditorWindow = this;

		m_pExplorerFiles = new ExplorerFileList();
		m_pExplorerFiles->AddPakColumn();
		m_pExplorerFiles->AddEntryType<CResponsesFileLoader>().AddFormat("response", new ResponsesFileLoader(), FORMAT_LIST | FORMAT_LOAD | FORMAT_SAVE);
		m_pExplorerFiles->Populate();

		m_pExplorerData = new ExplorerData();
		m_pExplorerData->AddProvider(m_pExplorerFiles, "Response Collection");
		m_pExplorerData->SetEntryTypeIcon(ENTRY_GROUP, "icons:General/Folder.ico");
		m_pExplorerData->SetEntryTypeIcon(ENTRY_ASSET, "icons:General/Arrow_Right.ico");
		m_pExplorerData->Populate();

		m_pFileExplorerWidget = new FileExplorerWithButtons(0, m_pExplorerData);
		m_pRecentSignalsWidget = new RecentResponsesWidget;
		s_RecentSignalsWidget = m_pRecentSignalsWidget;

		QTabWidget* pSignalViewTabs = new QTabWidget;
		m_pSignalViews = new DockedWidget<QTabWidget>(this, pSignalViewTabs, "Signals", Qt::BottomDockWidgetArea);  //new QTabWidget;
		pSignalViewTabs->setMinimumSize(220, 0);
		pSignalViewTabs->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

		pSignalViewTabs->addTab(m_pFileExplorerWidget, "Files");
		pSignalViewTabs->addTab(m_pRecentSignalsWidget, "Recent");

		QObject::connect(pSignalViewTabs, &QTabWidget::currentChanged, this, [=](int index)
		{
			if (index == eCDS_eSignalsFromFile)
			{
			  s_CurrentlyDisplayedSubelements = eCDS_eSignalsFromFile;
			  OnResponseFileWidgetVisibilityChanged(true);
			  m_pRecentSignalsWidget->SetVisible(false);
			}
			else if (index == eCDS_eRecentResponses)
			{
			  s_CurrentlyDisplayedSubelements = eCDS_eRecentResponses;
			  m_pRecentSignalsWidget->UpdateElements();
			  m_pRecentSignalsWidget->UpdateTrees();
			  m_pRecentSignalsWidget->SetVisible(true);
			  OnResponseFileWidgetVisibilityChanged(false);
			}
		});

		s_pPropertyTree = new QPropertyTree(this);
		PropertyTreeStyle customStyle(QPropertyTree::defaultTreeStyle());
		customStyle.compact = false;
		customStyle.doNotIndentSecondLevel = true;
		customStyle.propertySplitter = false;
		customStyle.levelIndent = 0.9f;
		s_pPropertyTree->setTreeStyle(customStyle);
		s_pPropertyTree->setUndoEnabled(true);

		m_pResponseDockWidget = new DockedWidget<CPropertyTreeWithOptions>(this, new CPropertyTreeWithOptions(this), "Response", Qt::BottomDockWidgetArea);
		m_pResponseDockWidget->widget()->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
		s_ResponseDockWidget = m_pResponseDockWidget->dock();

		s_pSubElementsTree = new QPropertyTree(this);
		PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
		treeStyle.propertySplitter = false;
		s_pSubElementsTree->setTreeStyle(treeStyle);
		s_pSubElementsTree->setUndoEnabled(true);
		m_pPropertyTree = new CPropertyTreeWithAutoupdateOption(this);
		DockedWidget<CPropertyTreeWithAutoupdateOption>* pSubElementsWidget = new DockedWidget<CPropertyTreeWithAutoupdateOption>(this, m_pPropertyTree, "Execution Info", Qt::BottomDockWidgetArea);
		m_pPropertyTree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

		EXPECTED(connect(s_pPropertyTree, SIGNAL(signalChanged()), this, SLOT(OnPropertyTreeChanged())));
		EXPECTED(connect(GetExplorerPanel(), SIGNAL(SignalSelectionChanged()), SLOT(OnFileExplorerSelectionChanged())));

		QTimer::singleShot(10, this, [ = ]
		{
			pSignalViewTabs->setCurrentIndex((int)eCDS_eSignalsFromFile);
			s_CurrentlyDisplayedSubelements = eCDS_eSignalsFromFile;
			s_ResponseSerializer.m_signalNames.clear(); // = "ALL";
			s_ResponseSerializer.m_fileNames.clear();
			s_RecentResponseSerializer.m_signalNames.clear(); // = "ALL";
			s_pPropertyTree->attach(Serialization::SStruct(s_ResponseSerializer));
			s_pSubElementsTree->attach(Serialization::SStruct(s_RecentResponseSerializer));
		});
	}
}

//--------------------------------------------------------------------------------------------------
CDrsResponseEditorWindow::~CDrsResponseEditorWindow()
{	
	delete m_pExplorerFiles;

	m_pExplorerData->deleteLater();

	s_pPropertyTree = nullptr;
	s_pSubElementsTree = nullptr;
	s_ResponseDockWidget = nullptr;
}

//--------------------------------------------------------------------------------------------------
void CDrsResponseEditorWindow::OnVisibilityChanged(bool bVisible)
{
	if (bVisible)
	{
		UpdateAll();
	}
	m_pPropertyTree->SetAutoUpdateActive(bVisible);
}

//--------------------------------------------------------------------------------------------------
void CDrsResponseEditorWindow::OnFileExplorerSelectionChanged()
{
	ExplorerPanel* panel = GetExplorerPanel();

	ExplorerEntries entries;
	panel->GetSelectedEntries(&entries);
	m_pExplorerData->LoadEntries(entries);

	s_pSubElementsTree->detach();
	s_ResponseSerializer.m_signalNames.clear();
	s_ResponseSerializer.m_fileNames.clear();
	s_RecentResponseSerializer.m_signalNames.clear();

	for (auto& entry : entries)
	{
		if (entry->type == ENTRY_ASSET)
		{
			m_pExplorerData->CheckIfModified(entry, 0, false);
			s_ResponseSerializer.m_signalNames.push_back(PathHelper::GetSignalNameFromFileName(entry->name.c_str()));
			s_ResponseSerializer.m_fileNames.push_back(entry->path.c_str());
			s_RecentResponseSerializer.m_signalNames = s_ResponseSerializer.m_signalNames;
		}
	}

	s_pPropertyTree->detach();
	const int selectedSignals = s_RecentResponseSerializer.m_signalNames.size();
	if (selectedSignals == 0)  //all signals will be displayed
	{
		s_currentExpandLevel = 0;
		s_pPropertyTree->setExpandLevels(0);
		s_pPropertyTree->attach(Serialization::SStruct(s_ResponseSerializer));
		s_pPropertyTree->collapseAll();
	}
	else
	{
		if (selectedSignals == 1)
		{
			s_currentExpandLevel = 8;
			s_pPropertyTree->setExpandLevels(32);  //expand all
		}
		else
		{
			s_currentExpandLevel = 4;
			s_pPropertyTree->setExpandLevels(4);  //expand base segment
		}
		s_pPropertyTree->attach(Serialization::SStruct(s_ResponseSerializer));
	}

	s_ResponseEditorWindow->GetResponsePropertyTree()->SetCurrentSignal(s_ResponseSerializer.GetFirstSignal());
	s_pSubElementsTree->attach(Serialization::SStruct(s_RecentResponseSerializer));
	s_pSubElementsTree->setExpandLevels(16);
}

//--------------------------------------------------------------------------------------------------
void CDrsResponseEditorWindow::OnResponseFileWidgetVisibilityChanged(bool bVisible)
{
	m_pFileExplorerWidget->m_bVisible = bVisible;
	if (bVisible)
	{
		s_CurrentlyDisplayedSubelements = eCDS_eSignalsFromFile;
		OnFileExplorerSelectionChanged();
	}
}

//--------------------------------------------------------------------------------------------------
void CDrsResponseEditorWindow::UpdateAll()
{
	if (m_pFileExplorerWidget)
	{
		if (m_pFileExplorerWidget->m_bVisible)
		{
			OnFileExplorerSelectionChanged();
		}
		if (m_pRecentSignalsWidget->IsVisible())
		{
			m_pRecentSignalsWidget->UpdateElements();
			m_pRecentSignalsWidget->UpdateTrees();
		}
	}
}

//--------------------------------------------------------------------------------------------------
void CDrsResponseEditorWindow::OnPropertyTreeChanged()
{
	ExplorerEntries entries;

	GetExplorerPanel()->GetSelectedEntries(&entries);
	for (auto& entry : entries)
	{
		m_pExplorerData->CheckIfModified(entry, "Property Change", false);
	}
}

//--------------------------------------------------------------------------------------------------
Explorer::ExplorerPanel* CDrsResponseEditorWindow::GetExplorerPanel()
{
	return m_pFileExplorerWidget->m_pExplorerPanel;
}

//--------------------------------------------------------------------------------------------------
void SResponseSerializerHelper::Serialize(Serialization::IArchive& ar)
{
	DRS::IResponseActor* pCurrentActor = gEnv->pDynamicResponseSystem->GetResponseActor(currentActor);
	ar.setFilter(m_filter);
	gEnv->pDynamicResponseSystem->GetResponseManager()->SerializeResponse(ar, m_signalNames, pCurrentActor);
}

//--------------------------------------------------------------------------------------------------
void SRecentResponseSerializerHelper::Serialize(Serialization::IArchive& ar)
{
	ar.setFilter(filter);
	gEnv->pDynamicResponseSystem->GetResponseManager()->SerializeRecentResponse(ar, m_signalNames, maxNumber);
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CResponsesFileLoader::~CResponsesFileLoader()
{
	if (!m_currentFile.empty())
	{
		DRS::IResponseManager* pResponseMgr = gEnv->pDynamicResponseSystem->GetResponseManager();
		pResponseMgr->RemoveResponse(PathHelper::GetSignalNameFromFileName(m_currentFile));
	}
}

//--------------------------------------------------------------------------------------------------
bool CResponsesFileLoader::Load(const char* szFilename)
{
	m_currentFile = szFilename;
	if (!m_currentFile.empty())
	{
		DRS::IResponseManager* pResponseMgr = gEnv->pDynamicResponseSystem->GetResponseManager();
		pResponseMgr->AddResponse(PathHelper::GetSignalNameFromFileName(m_currentFile));
	}
	bool result = Serialization::LoadJsonFile(*this, szFilename);
	s_pPropertyTree->revert();
	return result;
}

//--------------------------------------------------------------------------------------------------
bool CResponsesFileLoader::Save(const char* szFilename)
{
	m_currentFile = szFilename;
	s_pPropertyTree->apply(false);
	PathHelper::CreateDirectoryIfNotExisting(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + szFilename);
	return Serialization::SaveJsonFile(szFilename, *this);
}

//--------------------------------------------------------------------------------------------------
void CResponsesFileLoader::Serialize(Serialization::IArchive& ar)
{
	DRS::IResponseManager* pResponseMgr = gEnv->pDynamicResponseSystem->GetResponseManager();
	DynArray<stack_string> temp;
	temp.push_back(PathHelper::GetSignalNameFromFileName(m_currentFile));
	pResponseMgr->SerializeResponse(ar, temp);
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
BaseResponseTreeWidget::BaseResponseTreeWidget()
{
	m_pTree = new QTreeWidget;
	QStringList headerLabels;
	headerLabels.push_back(tr("Signal"));
	headerLabels.push_back(tr("Count"));
	m_pTree->setColumnCount(headerLabels.count());
	m_pTree->setColumnWidth(0, 180);
	m_pTree->setColumnWidth(1, 35);
	m_pTree->setHeaderLabels(headerLabels);
	m_pTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pUpdateButton = new QPushButton("Update", this);
	m_pVLayout = new QVBoxLayout(this);
	m_pVLayout->addWidget(m_pTree);
	m_pVLayout->addWidget(m_pUpdateButton);
	m_bVisible = false;
}

//--------------------------------------------------------------------------------------------------
void BaseResponseTreeWidget::FillWithSignals(DynArray<const char*>& elements)
{
	DynArray<signalNameWithCounter> filteredElements;
	for (DynArray<const char*>::iterator it = elements.begin(); it != elements.end(); ++it)
	{
		bool alreadyExists = false;
		for (DynArray<signalNameWithCounter>::iterator itFiltered = filteredElements.begin(); itFiltered != filteredElements.end(); ++itFiltered)
		{
			if (itFiltered->signalName == *it)
			{
				itFiltered->counter++;
				alreadyExists = true;
				break;
			}
		}
		if (!alreadyExists)
		{
			filteredElements.push_back(*it);
		}
	}

	//preserve the currently selected items
	vector<string> selectedItems;
	QList<QTreeWidgetItem*> items = m_pTree->selectedItems();
	for (int i = 0; i < items.count(); i++)
	{
		selectedItems.push_back(items[i]->text(0).toStdString().c_str());
	}

	m_pTree->clear();

	if (filteredElements.empty())
	{
		QTreeWidgetItem* newItem = new QTreeWidgetItem(m_pTree);
		newItem->setText(0, "No Signals yet");
		newItem->setText(1, CryStringUtils::toString(0).c_str());
		newItem->setDisabled(true);
	}
	else
	{
		for (DynArray<signalNameWithCounter>::iterator it = filteredElements.begin(); it != filteredElements.end(); ++it)
		{
			QTreeWidgetItem* newItem = new QTreeWidgetItem(m_pTree);
			newItem->setText(0, it->signalName.c_str());
			newItem->setText(1, CryStringUtils::toString(it->counter).c_str());

			for (vector<string>::iterator itPreviousSelected = selectedItems.begin(); itPreviousSelected != selectedItems.end(); ++itPreviousSelected)
			{
				if (it->signalName == *itPreviousSelected)
				{
					newItem->setSelected(true);
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
RecentResponsesWidget::RecentResponsesWidget()
{
	QObject::connect(m_pUpdateButton, &QPushButton::clicked, this, [ = ] { UpdateElements();
	                 });
	QObject::connect(m_pTree, &QTreeWidget::itemClicked, this, [=](QTreeWidgetItem* pItem, int column) { UpdateTrees(); });

	QPushButton* pShowAllButton = new QPushButton("Show all", this);
	QObject::connect(pShowAllButton, &QPushButton::clicked, this, [ = ]
	{
		s_pPropertyTree->detach();
		s_RecentResponseSerializer.m_signalNames.clear();
		s_pSubElementsTree->attach(Serialization::SStruct(s_RecentResponseSerializer));
		s_pSubElementsTree->revert();
	});

	m_pCreateNewResponseButton = new QPushButton("Create response for signal", this);
	m_pCreateNewResponseButton->setEnabled(false);
	QObject::connect(m_pCreateNewResponseButton, &QPushButton::clicked, this, [ = ]
	{
		if (!s_ResponseSerializer.m_signalNames.empty())
		{
		  string signalToCreate = s_ResponseSerializer.GetFirstSignal();
		  if (gEnv->pDynamicResponseSystem->GetResponseManager()->AddResponse(signalToCreate))
		  {
		    s_ResponseSerializer.m_signalNames.clear();
		    s_ResponseSerializer.m_fileNames.clear();
		    string path = PathUtil::GetGameFolder() + "/libs/DynamicResponseSystem/Responses/";
		    stack_string outName = path + signalToCreate + ".response";
		    PathHelper::CreateDirectoryIfNotExisting(outName);
		    s_ResponseSerializer.m_signalNames.push_back(signalToCreate);
		    s_ResponseSerializer.m_fileNames.push_back(outName);
		    s_pPropertyTree->attach(Serialization::SStruct(s_ResponseSerializer));
		    s_pPropertyTree->revert();

		    if (!s_FileSaveHelper.Save(outName))
		    {
		      CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "(DRS) Failed to save file %s", outName.c_str());
		    }
		  }
		}
	});

	QHBoxLayout* pHLayout = new QHBoxLayout(this);
	pHLayout->addWidget(m_pUpdateButton);
	pHLayout->addWidget(pShowAllButton);

	QHBoxLayout* pHLayout2 = new QHBoxLayout(this);
	m_pShowResponselessSignals = new QCheckBox("Signals without Response", this);
	m_pShowResponselessSignals->setChecked(true);
	m_pShowSignalsWithResponses = new QCheckBox("Signals with Response", this);
	m_pShowSignalsWithResponses->setChecked(true);

	QObject::connect(m_pShowResponselessSignals, &QPushButton::toggled, this, [ = ] { UpdateElements();
	                 });
	QObject::connect(m_pShowSignalsWithResponses, &QPushButton::toggled, this, [ = ] { UpdateElements();
	                 });

	pHLayout2->addWidget(m_pShowResponselessSignals);
	pHLayout2->addWidget(m_pShowSignalsWithResponses);

	m_pVLayout->addItem(pHLayout2);
	m_pVLayout->addWidget(m_pCreateNewResponseButton);
	m_pVLayout->addItem(pHLayout);
}

//--------------------------------------------------------------------------------------------------
void RecentResponsesWidget::UpdateElements()
{
	DynArray<const char*> elements;

	if (m_pShowResponselessSignals->isChecked() || m_pShowSignalsWithResponses->isChecked())
	{
		DRS::IResponseManager::eSignalFilter filter = DRS::IResponseManager::eSF_All;
		if (!m_pShowSignalsWithResponses->isChecked())
		{
			filter = DRS::IResponseManager::eSF_OnlyWithoutResponses;
		}
		else if (!m_pShowResponselessSignals->isChecked())
		{
			filter = DRS::IResponseManager::eSF_OnlyWithResponses;
		}
		elements = gEnv->pDynamicResponseSystem->GetResponseManager()->GetRecentSignals(filter);
	}

	FillWithSignals(elements);
	s_pSubElementsTree->revert();
}

//--------------------------------------------------------------------------------------------------
void RecentResponsesWidget::UpdateTrees()
{
	s_pSubElementsTree->detach();
	QList<QTreeWidgetItem*> items = m_pTree->selectedItems();
	s_RecentResponseSerializer.m_signalNames.clear();
	s_ResponseSerializer.m_signalNames.clear();
	s_ResponseSerializer.m_fileNames.clear();

	if (!items.empty())
	{
		for (auto& item : items)
		{
			string text = item->text(0).toStdString().c_str();
			s_ResponseSerializer.m_signalNames.push_back(text);
			s_RecentResponseSerializer.m_signalNames.push_back(text);
		}
		s_pPropertyTree->attach(Serialization::SStruct(s_ResponseSerializer));
		s_pSubElementsTree->attach(Serialization::SStruct(s_RecentResponseSerializer));

		//we mis-use the fact, that AddResponse fails, if there is already a response for this signal
		bool bResponseForSignalExists = !gEnv->pDynamicResponseSystem->GetResponseManager()->AddResponse(s_ResponseSerializer.GetFirstSignal());
		if (!bResponseForSignalExists)
		{
			gEnv->pDynamicResponseSystem->GetResponseManager()->RemoveResponse(s_ResponseSerializer.GetFirstSignal());
		}
		m_pCreateNewResponseButton->setDisabled(bResponseForSignalExists);

	}
	else
	{
		s_pPropertyTree->detach();
		s_pSubElementsTree->attach(Serialization::SStruct(s_RecentResponseSerializer));
		s_pSubElementsTree->revert();
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

CPropertyTreeWithOptions::CPropertyTreeWithOptions(QWidget* pParent) : QWidget(pParent)
{
	QToolBar* pToolBar = new QToolBar("Actions", this);

	QAction* pAction = pToolBar->addAction(CryIcon("icons:General/File_Save.ico"), QString("Save current response (Ctrl+s)"));
	pAction->setShortcut(tr("Ctrl+S"));
	QObject::connect(pAction, &QAction::triggered, this, [ = ]
	{
		if (s_CurrentlyDisplayedSubelements == eCDS_eSignalsFromFile)
		{
		  ActionOutput saveResults;
		  ExplorerEntries entries;
		  s_ResponseEditorWindow->GetExplorerPanel()->GetSelectedEntries(&entries);

		  int numSavedEntries = 0;
		  int numSelectedEntries = entries.size();
		  for (int i = 0; i < numSelectedEntries; i++)
		  {
		    if (entries[i]->type == ENTRY_ASSET)
		    {
		      if (s_ResponseEditorWindow->GetExplorerData()->SaveEntry(&saveResults, entries[i]))
		      {
		        numSavedEntries++;
		      }
			  else
			  {
				  CQuestionDialog::SWarning("Save Error", stack_string().Format("Could not save selected Response '%' to path '%s'.", entries[i]->name.c_str(), entries[i]->path.c_str()).c_str());
			  }
		    }
		  }
		  if (numSavedEntries == 0)
		  {
		    CQuestionDialog::SWarning("Save Error", "Could not save all currently selected Responses. Please make sure to select the response you want to save first.");
		  }

		  if (saveResults.errorCount > 0)
		  {
		    saveResults.Show(this);
		    return;
		  }
		}
		else if (s_CurrentlyDisplayedSubelements == eCDS_eRecentResponses)
		{
		  for (const stack_string& signalName : s_ResponseSerializer.m_signalNames)
		  {
		    const string pathForSignal = PathHelper::GetPathForSignal(s_ResponseEditorWindow->GetExplorerData()->GetRoot(), signalName);
		    if (!pathForSignal.empty())
		    {
		      PathHelper::CreateDirectoryIfNotExisting(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + pathForSignal);
			  if (!s_FileSaveHelper.Save(pathForSignal))
			  {
				  CQuestionDialog::SWarning("Save Error", stack_string().Format("Could not save selected Response '%s' to path '%s'.", signalName.c_str(), pathForSignal.c_str()).c_str());
			  }
		    }
		  }
		}
	});

	pAction = pToolBar->addAction(CryIcon("icons:General/Load_From_Disc.ico"), QString("Reload from File (Ctrl+l)"));
	pAction->setShortcut(tr("Ctrl+L"));
	QObject::connect(pAction, &QAction::triggered, this, [ = ]
	{
		if (s_CurrentlyDisplayedSubelements == eCDS_eSignalsFromFile)
		{
		  ExplorerEntries entries;
		  s_ResponseEditorWindow->GetExplorerPanel()->GetSelectedEntries(&entries);
		  for (auto& entry : entries)
		  {
		    s_ResponseEditorWindow->GetExplorerData()->Revert(entry);
		  }
		}
		else
		{
		  for (DynArray<stack_string>::iterator it = s_ResponseSerializer.m_fileNames.begin(); it != s_ResponseSerializer.m_fileNames.end(); ++it)
		  {
		    s_FileSaveHelper.Load(*it);
		  }
		}
	});

	pToolBar->addSeparator();

	pAction = pToolBar->addAction(CryIcon("icons:General/Visibility_False.ico"), QString("Show/hide Actions and Conditions (Ctrl+h)"));
	pAction->setCheckable(true);
	pAction->setChecked(false);
	pAction->setShortcut(tr("Ctrl+H"));
	QObject::connect(pAction, &QAction::toggled, this, [=](bool bChecked)
	{
		if (bChecked)
			s_ResponseSerializer.m_filter |= BIT(1);  //eSH_CollapsedResponseSegments
		else
			s_ResponseSerializer.m_filter &= ~BIT(1);
		s_pPropertyTree->revert();
	});

	pAction = pToolBar->addAction(CryIcon("icons:General/Tick.ico"), QString("Evaluate Conditions (for current signal-sender) (Ctrl+e)"));
	pAction->setCheckable(true);
	pAction->setChecked(false);
	pAction->setShortcut(tr("Ctrl+E"));
	QObject::connect(pAction, &QAction::toggled, this, [=](bool bChecked)
	{
		if (bChecked)
			s_ResponseSerializer.m_filter |= BIT(2);  //eSH_EvaluateResponses
		else
			s_ResponseSerializer.m_filter &= ~BIT(2);
		s_pPropertyTree->revert();
		//Force redraw
		const PropertyTreeStyle& currentStyle = s_pPropertyTree->treeStyle();
		s_pPropertyTree->setTreeStyle(currentStyle);
	});

	pAction = pToolBar->addAction(CryIcon("icons:General/Exit.ico"), QString("Reset DRS"));
	QObject::connect(pAction, &QAction::triggered, this, [ = ]
	{
		gEnv->pDynamicResponseSystem->Reset();
	});

	pToolBar->addSeparator();

	pAction = pToolBar->addAction(CryIcon("icons:General/Pointer_Up.ico"), QString("Collapse all (Ctrl+1)"));
	pAction->setShortcut(tr("Ctrl+1"));
	QObject::connect(pAction, &QAction::triggered, this, [ = ]
	{
		if (s_pPropertyTree->attached())
		{
		  s_pPropertyTree->collapseAll();
		  s_currentExpandLevel = 0;
		}
	});

	pAction = pToolBar->addAction(CryIcon("icons:General/Pointer_Up_Tinted.ico"), QString("collapse one level (Ctrl+2)"));
	pAction->setShortcut(tr("Ctrl+2"));
	QObject::connect(pAction, &QAction::triggered, this, [ = ]
	{
		if (s_pPropertyTree->attached())
		{
		  if (s_currentExpandLevel > 0)
				s_currentExpandLevel--;

		  if (s_currentExpandLevel > 0)
		  {
		    s_pPropertyTree->detach();
		    s_pPropertyTree->setExpandLevels(s_currentExpandLevel);
		    s_pPropertyTree->attach(Serialization::SStruct(s_ResponseSerializer));
		  }
		  else
		  {
		    s_pPropertyTree->collapseAll();
		  }
		}
	});

	pAction = pToolBar->addAction(CryIcon("icons:General/Tick_Box_Tinted.ico"), QString("collapse to default depth (Ctrl+3)"));
	pAction->setShortcut(tr("Ctrl+3"));
	QObject::connect(pAction, &QAction::triggered, this, [ = ]
	{
		if (s_pPropertyTree->attached())
		{
		  s_pPropertyTree->detach();
		  s_pPropertyTree->setExpandLevels(4);
		  s_currentExpandLevel = 4;
		  s_pPropertyTree->attach(Serialization::SStruct(s_ResponseSerializer));
		}
	});

	pAction = pToolBar->addAction(CryIcon("icons:General/Pointer_Down_Tinted.ico"), QString("expand one level(Ctrl+4)"));
	pAction->setShortcut(tr("Ctrl+4"));
	QObject::connect(pAction, &QAction::triggered, this, [ = ]
	{
		if (s_pPropertyTree->attached())
		{
		  if (s_currentExpandLevel < 20)
		  {
		    s_currentExpandLevel++;
		    s_pPropertyTree->detach();
		    s_pPropertyTree->setExpandLevels(s_currentExpandLevel);
		    s_pPropertyTree->attach(Serialization::SStruct(s_ResponseSerializer));
		  }
		}
	});

	pAction = pToolBar->addAction(CryIcon("icons:General/Pointer_Down.ico"), QString("Expand All (Ctrl+5)"));
	pAction->setShortcut(tr("Ctrl+5"));
	QObject::connect(pAction, &QAction::triggered, this, [ = ]
	{
		if (s_pPropertyTree->attached())
		{
		  s_pPropertyTree->detach();
		  s_currentExpandLevel = 8;
		  s_pPropertyTree->setExpandLevels(32);
		  s_pPropertyTree->attach(Serialization::SStruct(s_ResponseSerializer));
		}
	});

	pToolBar->addSeparator();

	QLabel* pSignalLabel = new QLabel("Send ", this);
	pSignalLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	pSignalLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
	pToolBar->addWidget(pSignalLabel);

	m_pSignalNameEdit = new QLineEdit("none", this);
	pToolBar->addWidget(m_pSignalNameEdit);

	pSignalLabel = new QLabel(" on ", this);
	pToolBar->addWidget(pSignalLabel);

	m_pSenderNameEdit = new QLineEdit("selected", this);
	s_ResponseSerializer.currentActor = m_pSenderNameEdit->text().toStdString().c_str();
	connect(m_pSenderNameEdit, &QLineEdit::textChanged, [&](const QString& str)
	{
		s_ResponseSerializer.currentActor = str.toStdString().c_str();
	});

	pToolBar->addWidget(m_pSenderNameEdit);
	m_pSendSignalButton = pToolBar->addAction(CryIcon("icons:General/Arrow_Right.ico"), QString("Send Signal on Actor (Ctrl+B)"));
	m_pSendSignalButton->setShortcut(tr("Ctrl+B"));
	QObject::connect(m_pSendSignalButton, &QAction::triggered, this, [=]
	{
		string senderName = m_pSenderNameEdit->text().toStdString().c_str();
		if (senderName.empty() || senderName == "selected")
		{
			CBaseObject* pCurrentlySelectedObject = GetIEditor()->GetSelectedObject();
			if (pCurrentlySelectedObject)
				senderName = pCurrentlySelectedObject->GetName();
			else
			{
				CGameEngine* pGameEngine = GetIEditor()->GetGameEngine();
				if (pGameEngine && pGameEngine->GetPlayerEntity())
					senderName = pGameEngine->GetPlayerEntity()->GetName(); //fallback, if nothing is selected, we try to run the response on the player
				else
					senderName.clear();
			}
		}
		const string signalName = m_pSignalNameEdit->text().toStdString().c_str();

		if (!senderName.empty() && !signalName.empty())
		{
		  DRS::IResponseActor* pSender = gEnv->pDynamicResponseSystem->GetResponseActor(senderName);
		  if (pSender)
		  {
		    SET_DRS_USER_SCOPED("DRS UI");
		    pSender->QueueSignal(signalName.c_str());

		    //update the recent response tree
		    if (!s_RecentResponseSerializer.m_signalNames.empty())
		    {
		      bool bFound = false;
		      for (DynArray<stack_string>::iterator it = s_RecentResponseSerializer.m_signalNames.begin(); it != s_RecentResponseSerializer.m_signalNames.end(); ++it)
		      {
		        if (signalName == it->c_str())
		        {
		          bFound = true;
		          break;
		        }
		      }
		      if (!bFound)
		      {
		        s_RecentResponseSerializer.m_signalNames.push_back(signalName);
		      }

		    }

		    QTimer::singleShot(100, this, [ = ]
				{
					s_pSubElementsTree->revert();
					s_RecentSignalsWidget->UpdateElements();
		    });
		  }
		  else
		  {
			IEntity* pTargetEntity = gEnv->pEntitySystem->FindEntityByName(senderName.c_str());
			if (pTargetEntity)
			{
				string errormessage = "Could not find DRS actor with name '" + senderName + "'. But there is an entity with this name. Should a DRS actor be created for this entity?";
				if (CQuestionDialog::SQuestion("Create DRS actor?", errormessage.c_str()) == QDialogButtonBox::Yes)
				{
					gEnv->pDynamicResponseSystem->CreateResponseActor(senderName.c_str(), pTargetEntity->GetId());
				}
			}
			else
			{
				string errormessage = "Could not find DRS actor (or even an entity) with name '" + senderName + "' to send signal '" + signalName + "'";
				CQuestionDialog::SWarning("DRS actor not found", errormessage.c_str());
			}
		  }
		}
		else
		{
				CQuestionDialog::SWarning("Signal cannot be send", "No actor specified/selected or no signal specified");
		}
	});
	QVBoxLayout* pVLayout = new QVBoxLayout(this);
	pVLayout->addWidget(pToolBar);
	pVLayout->addWidget(s_pPropertyTree);
}

//--------------------------------------------------------------------------------------------------
void CPropertyTreeWithOptions::SetCurrentSignal(const stack_string& signalName)
{
	if (signalName != "ALL")
	{
		m_pSignalNameEdit->setText(signalName.c_str());
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CPropertyTreeWithAutoupdateOption::CPropertyTreeWithAutoupdateOption(QWidget* pParent) : QWidget(pParent)
{
	QTimer* pAutoUpdateTimer = new QTimer(this);
	pAutoUpdateTimer->setInterval(666);
	QObject::connect(pAutoUpdateTimer, &QTimer::timeout, this, [ = ]
	{
		s_pSubElementsTree->revert();
		pAutoUpdateTimer->start();
	});

	QToolBar* pToolBar = new QToolBar("Actions", this);

	m_pAutoupdateButton = pToolBar->addAction(CryIcon("icons:General/Reload.ico"), QString("Auto Update"));
	m_pAutoupdateButton->setCheckable(true);
	QObject::connect(m_pAutoupdateButton, &QAction::toggled, this, [=](bool bChecked)
	{
		if (bChecked)
			pAutoUpdateTimer->start();
		else
			pAutoUpdateTimer->stop();
	});

	QAction* pAction = pToolBar->addAction(CryIcon("icons:General/File_Save.ico"), QString("Save current log to JSON"));
	QObject::connect(pAction, &QAction::triggered, this, [ = ]
	{
		Serialization::SaveJsonFile(PathUtil::Make(PathUtil::GetGameFolder(), "libs/DynamicResponseSystem/DebugData/RecentResponses.json"), s_RecentResponseSerializer);
	});

	pAction = pToolBar->addAction(CryIcon("icons:General/TrashCan.ico"), QString("Clear History"));
	QObject::connect(pAction, &QAction::triggered, this, [ = ]
	{
		s_RecentResponseSerializer.filter = 1;
		Serialization::SaveJsonFile("recent", s_RecentResponseSerializer);
		s_RecentResponseSerializer.filter = 0;
	});

	QSpinBox* pMaxElements = new QSpinBox(this);
	pMaxElements->setRange(1, 100);
	pMaxElements->setValue(128);
	pMaxElements->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

	QObject::connect(pMaxElements, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [=](int newValue)
	{
		s_RecentResponseSerializer.maxNumber = newValue;
		s_pSubElementsTree->revert();
	});

	pToolBar->addSeparator();
	pToolBar->addWidget(new QLabel(" Max elements:", this));
	pToolBar->addWidget(pMaxElements);

	QVBoxLayout* pVLayout = new QVBoxLayout(this);

	pVLayout->addWidget(pToolBar);
	pVLayout->addWidget(s_pSubElementsTree);
}

//--------------------------------------------------------------------------------------------------
void CPropertyTreeWithAutoupdateOption::SetAutoUpdateActive(bool bValue)
{
	m_pAutoupdateButton->setChecked(bValue);
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
FileExplorerWithButtons::FileExplorerWithButtons(QWidget* pParent, Explorer::ExplorerData* pExplorerData) : QWidget(pParent), m_bVisible(false)
{
	m_pExplorerPanel = new ExplorerPanel(this, pExplorerData);

	QLineEdit* pNewResponseNameEdit = new QLineEdit(this);
	pNewResponseNameEdit->setPlaceholderText(tr("Enter new response name"));
	QPushButton* pCreateNewButton = new QPushButton(tr("Create new"), this);

	QVBoxLayout* pVLayout = new QVBoxLayout(this);
	QHBoxLayout* pHLayout = new QHBoxLayout(this);

	pCreateNewButton->setEnabled(false);
	QObject::connect(pNewResponseNameEdit, &QLineEdit::textChanged, this, [=](const QString& str)
	{
		//we mis-use the fact, that AddResponse fails, if there is already a response for this signal
		if (!str.isEmpty() && gEnv->pDynamicResponseSystem->GetResponseManager()->AddResponse(str.toStdString().c_str()))
		{
		  gEnv->pDynamicResponseSystem->GetResponseManager()->RemoveResponse(str.toStdString().c_str());
		  pCreateNewButton->setEnabled(true);
		}
		else
		{
		  pCreateNewButton->setEnabled(false);
		}
	});

	pHLayout->addWidget(pNewResponseNameEdit);
	pHLayout->addWidget(pCreateNewButton);
	pVLayout->addWidget(m_pExplorerPanel);
	pVLayout->addItem(pHLayout);

	QObject::connect(pCreateNewButton, &QPushButton::clicked, this, [ = ]
	{
		string newSignalName = pNewResponseNameEdit->text().toStdString().c_str();
		if (!newSignalName.empty())
		{
		  s_RecentResponseSerializer.m_signalNames.clear();
		  s_ResponseSerializer.m_signalNames.clear();
		  s_ResponseSerializer.m_fileNames.clear();

		  gEnv->pDynamicResponseSystem->GetResponseManager()->AddResponse(newSignalName);
		  s_ResponseSerializer.m_signalNames.push_back(newSignalName);
		  string path = PathUtil::GetGameFolder() + "/libs/DynamicResponseSystem/Responses/";
		  string outName = path + newSignalName + ".response";
		  PathHelper::CreateDirectoryIfNotExisting(outName);
		  s_ResponseSerializer.m_fileNames.push_back(outName);
		  s_pPropertyTree->attach(Serialization::SStruct(s_ResponseSerializer));
		  s_pPropertyTree->revert();
		  if (!s_FileSaveHelper.Save(outName))
		  {
		    CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "(DRS) Failed to save file %s", outName.c_str());
		  }

		  s_ResponseEditorWindow->GetResponsePropertyTree()->SetCurrentSignal(s_ResponseSerializer.GetFirstSignal());
		  s_RecentResponseSerializer.m_signalNames.push_back(newSignalName);
		}
	});
}

