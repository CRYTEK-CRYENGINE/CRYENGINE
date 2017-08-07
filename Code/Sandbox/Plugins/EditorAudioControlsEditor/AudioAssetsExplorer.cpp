// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioAssetsExplorer.h"
#include "AudioAssets.h"
#include "AudioAssetsManager.h"
#include "QAudioControlEditorIcons.h"
#include <IEditor.h>
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>
#include <CryMath/Cry_Camera.h>
#include <CryAudio/IAudioSystem.h>
#include <IAudioSystemEditor.h>
#include <IAudioSystemItem.h>
#include "QtUtil.h"
#include <EditorStyleHelper.h>
#include "AudioControlsEditorPlugin.h"
#include "AudioSystemPanel.h"
#include "AudioSystemModel.h"
#include "QAudioControlTreeWidget.h"
#include "IUndoObject.h"
#include "Controls/QuestionDialog.h"
#include "FilePathUtil.h"
#include "AudioAdvancedTreeView.h"

#include <QPushButton>
#include <QKeyEvent>
#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QVBoxLayout>
#include <ProxyModels/MountingProxyModel.h>

class QCheckableMenu final : public QMenu
{
public:
	QCheckableMenu(QWidget* pParent) : QMenu(pParent)
	{}

	virtual bool event(QEvent* event) override
	{
		switch (event->type())
		{
		case QEvent::KeyPress:
			{
				QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
				switch (keyEvent->key())
				{
				case Qt::Key_Space:
				case Qt::Key_Return:
				case Qt::Key_Enter:
					break;
				default:
					return QMenu::event(event);
				}
			}
		//Intentional fall through
		case QEvent::MouseButtonRelease:
			{
				auto action = activeAction();
				if (action)
				{
					action->trigger();
					return true;
				}
			}
		}
		return QMenu::event(event);
	}
};

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CAudioAssetsExplorer::CAudioAssetsExplorer(CAudioAssetsManager* pAssetsManager)
	: m_pAssetsManager(pAssetsManager)
	, m_pProxyModel(new QControlsProxyFilter(this))
{
	resize(299, 674);
	setWindowTitle(tr("ATL Controls Panel"));
	setProperty("text", QVariant(tr("Reload")));

	QIcon icon;
	icon.addFile(QStringLiteral(":/Icons/Load_Icon.png"), QSize(), QIcon::Normal, QIcon::Off);
	setProperty("icon", QVariant(icon));
	QVBoxLayout* pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	QHBoxLayout* pHorizontalLayout = new QHBoxLayout();
	m_pTextFilter = new QLineEdit(this);
	m_pTextFilter->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);
	m_pTextFilter->setClearButtonEnabled(true);
	pHorizontalLayout->addWidget(m_pTextFilter);

	QPushButton* pFiltersButton = new QPushButton(this);
	pFiltersButton->setEnabled(true);
	pFiltersButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	pFiltersButton->setMaximumSize(QSize(60, 16777215));
	pFiltersButton->setCheckable(false);
	pFiltersButton->setFlat(false);
	pFiltersButton->setToolTip(tr("Filter controls by type"));
	pFiltersButton->setText(tr("Filters"));
	pHorizontalLayout->addWidget(pFiltersButton);

	m_pFilterMenu = new QCheckableMenu(this);

	QAction* pAction = new QAction(tr("Triggers"), this);
	connect(pAction, &QAction::triggered, [&](bool const bShow)
		{
			ShowControlType(eItemType_Trigger, bShow);
	  });
	pAction->setCheckable(true);
	pAction->setChecked(true);
	m_pFilterMenu->addAction(pAction);

	pAction = new QAction(tr("Parameters"), this);
	connect(pAction, &QAction::triggered, [&](bool const bShow)
		{
			ShowControlType(eItemType_Parameter, bShow);
	  });
	pAction->setCheckable(true);
	pAction->setChecked(true);
	m_pFilterMenu->addAction(pAction);

	pAction = new QAction(tr("Switches"), this);
	connect(pAction, &QAction::triggered, [&](bool const bShow)
		{
			ShowControlType(eItemType_Switch, bShow);
	  });
	pAction->setCheckable(true);
	pAction->setChecked(true);
	m_pFilterMenu->addAction(pAction);

	pAction = new QAction(tr("Environments"), this);
	connect(pAction, &QAction::triggered, [&](bool const bShow)
		{
			ShowControlType(eItemType_Environment, bShow);
	  });
	pAction->setCheckable(true);
	pAction->setChecked(true);
	m_pFilterMenu->addAction(pAction);

	pAction = new QAction(tr("Preloads"), this);
	connect(pAction, &QAction::triggered, this, [&](bool const bShow)
		{
			ShowControlType(eItemType_Preload, bShow);
	  });
	pAction->setCheckable(true);
	pAction->setChecked(true);
	m_pFilterMenu->addAction(pAction);

	m_pFilterMenu->addSeparator();

	pAction = new QAction(tr("Select All"), this);
	connect(pAction, &QAction::triggered, this, [&]()
		{
			for (auto const pFilterAction : m_pFilterMenu->actions())
			{
				if (pFilterAction->isCheckable() && !pFilterAction->isChecked())
				{
					pFilterAction->trigger();
				}
			}
	  });
	pAction->setCheckable(false);
	m_pFilterMenu->addAction(pAction);

	pAction = new QAction(tr("Select None"), this);
	connect(pAction, &QAction::triggered, this, [&]()
		{
			for (auto const pFilterAction : m_pFilterMenu->actions())
			{
				if (pFilterAction->isCheckable() && pFilterAction->isChecked())
				{
					pFilterAction->trigger();
				}
			}
	  });
	pAction->setCheckable(false);
	m_pFilterMenu->addAction(pAction);

	pFiltersButton->setMenu(m_pFilterMenu);

	QPushButton* pAddButton = new QPushButton(this);
	pAddButton->setMaximumSize(QSize(60, 16777215));
	pHorizontalLayout->addWidget(pAddButton);

	pHorizontalLayout->setStretch(0, 1);
	pMainLayout->addLayout(pHorizontalLayout);

	m_pControlsTree = new CAudioAdvancedTreeView();
	m_pControlsTree->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
	m_pControlsTree->setDragEnabled(true);
	m_pControlsTree->setDragDropMode(QAbstractItemView::DragDrop);
	m_pControlsTree->setDefaultDropAction(Qt::MoveAction);
	m_pControlsTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pControlsTree->setSortingEnabled(true);
	m_pControlsTree->sortByColumn(0, Qt::AscendingOrder);
	m_pControlsTree->header()->setVisible(false);
	m_pControlsTree->installEventFilter(this);
	m_pControlsTree->viewport()->installEventFilter(this);
	pMainLayout->addWidget(m_pControlsTree);

	m_pTextFilter->setToolTip(tr("Show only controls with this name"));
	m_pTextFilter->setPlaceholderText(tr("Search"));
	connect(m_pTextFilter, &QLineEdit::textChanged, [&](QString const& filter)
		{
			if (m_filter != filter)
			{
			  if (m_filter.isEmpty() && !filter.isEmpty())
			  {
			    m_pControlsTree->BackupExpanded();
			    m_pControlsTree->expandAll();
			  }
			  else if (!m_filter.isEmpty() && filter.isEmpty())
			  {
			    m_pControlsTree->collapseAll();
			    m_pControlsTree->RestoreExpanded();
			  }
			  m_filter = filter;
			}

			m_pProxyModel->setFilterFixedString(filter);
	  });

	pAddButton->setToolTip(tr("Add new library, folder or control"));
	pAddButton->setText(tr("Add"));

	// ************ Context Menu ************
	m_addItemMenu.addAction(GetItemTypeIcon(eItemType_Library), tr("Library"), [this]()
		{
			m_bCreatedFromMenu = true;
			m_pAssetsManager->CreateLibrary(Utils::GenerateUniqueLibraryName("new_library", *m_pAssetsManager));
		});
	m_addItemMenu.addAction(GetItemTypeIcon(eItemType_Folder), tr("Folder"), [&]() { CreateFolder(GetSelectedAsset()); });
	m_addItemMenu.addAction(GetItemTypeIcon(eItemType_Trigger), tr("Trigger"), [&]() { CreateControl("new_trigger", eItemType_Trigger, GetSelectedAsset()); });
	m_addItemMenu.addAction(GetItemTypeIcon(eItemType_Parameter), tr("Parameter"), [&]() { CreateControl("new_parameter", eItemType_Parameter, GetSelectedAsset()); });
	m_addItemMenu.addAction(GetItemTypeIcon(eItemType_Switch), tr("Switch"), [&]() { CreateControl("new_switch", eItemType_Switch, GetSelectedAsset()); });
	m_addItemMenu.addAction(GetItemTypeIcon(eItemType_Environment), tr("Environment"), [&]() { CreateControl("new_environment", eItemType_Environment, GetSelectedAsset()); });
	m_addItemMenu.addAction(GetItemTypeIcon(eItemType_Preload), tr("Preload"), [&]() { CreateControl("new_preload", eItemType_Preload, GetSelectedAsset()); });
	m_addItemMenu.addSeparator();
	pAddButton->setMenu(&m_addItemMenu);

	m_pControlsTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pControlsTree, &QAdvancedTreeView::customContextMenuRequested, this, &CAudioAssetsExplorer::ShowControlsContextMenu);
	// *********************************

	m_pAssetsManager->signalItemAdded.Connect([&](IAudioAsset* pItem)
		{
			if (!m_bReloading)
			{
			  ResetFilters();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	m_pAssetsModel = new CAudioAssetsExplorerModel(m_pAssetsManager);

	auto const count = m_pAssetsManager->GetLibraryCount();
	m_libraryModels.resize(count);
	for (int i = 0; i < count; ++i)
	{
		m_libraryModels[i] = new CAudioLibraryModel(m_pAssetsManager, m_pAssetsManager->GetLibrary(i));
	}

	m_pMountedModel = new CMountingProxyModel(WrapMemberFunction(this, &CAudioAssetsExplorer::CreateLibraryModelFromIndex));
	m_pMountedModel->SetHeaderDataCallbacks(1, &GetHeaderData);
	m_pMountedModel->SetSourceModel(m_pAssetsModel);

	m_pProxyModel->setSourceModel(m_pMountedModel);
	m_pProxyModel->setDynamicSortFilter(true);
	m_pControlsTree->setModel(m_pProxyModel);

	connect(m_pControlsTree->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CAudioAssetsExplorer::SelectedControlChanged);
	connect(m_pControlsTree->selectionModel(), &QItemSelectionModel::currentChanged, this, &CAudioAssetsExplorer::StopControlExecution);

	connect(m_pMountedModel, &CMountingProxyModel::rowsInserted, this, &CAudioAssetsExplorer::SelectNewAsset);

	m_pAssetsManager->signalLibraryAboutToBeRemoved.Connect([&](CAudioLibrary* pLibrary)
		{
			if (!m_bReloading)
			{
			  int const libCount = m_pAssetsManager->GetLibraryCount();
			  for (int i = 0; i < libCount; ++i)
			  {
			    if (m_pAssetsManager->GetLibrary(i) == pLibrary)
			    {
			      m_libraryModels[i]->deleteLater();
			      m_libraryModels.erase(m_libraryModels.begin() + i);

			      break;
			    }
			  }
			}
	  }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CAudioAssetsExplorer::~CAudioAssetsExplorer()
{
	m_pAssetsManager->signalLibraryAboutToBeRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalItemAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));

	StopControlExecution();
	delete m_pAssetsModel;
	int const libCount = m_libraryModels.size();
	for (int i = 0; i < libCount; ++i)
	{
		m_libraryModels[i]->DisconnectFromSystem();
		delete m_libraryModels[i];
	}
	m_libraryModels.clear();
}

//////////////////////////////////////////////////////////////////////////
bool CAudioAssetsExplorer::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if ((pEvent->type() == QEvent::KeyRelease) && !m_pControlsTree->IsEditing())
	{
		QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);
		if (pKeyEvent)
		{
			if (pKeyEvent->key() == Qt::Key_Delete)
			{
				DeleteSelectedControl();
			}
			else if (pKeyEvent->key() == Qt::Key_Space)
			{
				ExecuteControl();
			}
		}
	}
	return QWidget::eventFilter(pObject, pEvent);
}

//////////////////////////////////////////////////////////////////////////
std::vector<CAudioControl*> CAudioAssetsExplorer::GetSelectedControls()
{
	QModelIndexList indexes = m_pControlsTree->selectionModel()->selectedIndexes();
	std::vector<CAudioLibrary*> libraries;
	std::vector<CAudioFolder*> folders;
	std::vector<CAudioControl*> controls;
	AudioModelUtils::GetAssetsFromIndices(indexes, libraries, folders, controls);
	return controls;
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsExplorer::Reload()
{
	ResetFilters();
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsExplorer::ShowControlType(EItemType type, bool bShow)
{
	m_pProxyModel->EnableControl(bShow, type);

	if (type == eItemType_Switch)
	{
		m_pProxyModel->EnableControl(bShow, eItemType_State);
	}
	ControlTypeFiltered(type, bShow);
}

//////////////////////////////////////////////////////////////////////////
CAudioControl* CAudioAssetsExplorer::CreateControl(string const& name, EItemType type, IAudioAsset* pParent)
{
	m_bCreatedFromMenu = true;

	if (type != eItemType_State)
	{
		return m_pAssetsManager->CreateControl(Utils::GenerateUniqueControlName(name, type, *m_pAssetsManager), type, pParent);
	}
	else
	{
		return m_pAssetsManager->CreateControl(Utils::GenerateUniqueName(name, type, pParent), type, pParent);
	}
}

//////////////////////////////////////////////////////////////////////////
IAudioAsset* CAudioAssetsExplorer::CreateFolder(IAudioAsset* pParent)
{
	m_bCreatedFromMenu = true;
	return m_pAssetsManager->CreateFolder(Utils::GenerateUniqueName("new_folder", EItemType::eItemType_Folder, pParent), pParent);
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsExplorer::ShowControlsContextMenu(QPoint const& pos)
{
	QMenu contextMenu(tr("Context menu"), this);
	QMenu addMenu(tr("Add"));

	std::vector<CAudioLibrary*> libraries;
	std::vector<CAudioFolder*> folders;
	std::vector<CAudioControl*> controls;
	auto selection = m_pControlsTree->selectionModel()->selectedRows();
	AudioModelUtils::GetAssetsFromIndices(selection, libraries, folders, controls);

	if (libraries.size() == 1 || folders.size() == 1)
	{
		IAudioAsset* pParent = nullptr;
		if (libraries.empty())
		{
			pParent = static_cast<IAudioAsset*>(folders[0]);

		}
		else
		{
			pParent = static_cast<IAudioAsset*>(libraries[0]);
			if (pParent->IsModified())
			{
				contextMenu.addAction(tr("Save"), [&]()
					{
						CAudioControlsEditorPlugin::SaveModels();
				  });
				contextMenu.addSeparator();
			}
		}

		addMenu.addAction(GetItemTypeIcon(eItemType_Folder), tr("Folder"), [&]() { CreateFolder(pParent); });
		addMenu.addSeparator();
		addMenu.addAction(GetItemTypeIcon(eItemType_Trigger), tr("Trigger"), [&]() { CreateControl("new_trigger", eItemType_Trigger, pParent); });
		addMenu.addAction(GetItemTypeIcon(eItemType_Parameter), tr("Parameter"), [&]() { CreateControl("new_parameter", eItemType_Parameter, pParent); });
		addMenu.addAction(GetItemTypeIcon(eItemType_Switch), tr("Switch"), [&]() { CreateControl("new_switch", eItemType_Switch, pParent); });
		addMenu.addAction(GetItemTypeIcon(eItemType_Environment), tr("Environment"), [&]() { CreateControl("new_environment", eItemType_Environment, pParent); });
		addMenu.addAction(GetItemTypeIcon(eItemType_Preload), tr("Preload"), [&]() { CreateControl("new_preload", eItemType_Preload, pParent); });
		contextMenu.addMenu(&addMenu);
		contextMenu.addSeparator();

		if (pParent->GetType() == EItemType::eItemType_Library)
		{
			//TODO: Take into account files in the "levels" folder
			contextMenu.addAction(tr("Open Containing Folder"), [&]()
				{
					QtUtil::OpenInExplorer(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), Utils::GetAssetFolder() + pParent->GetName() + ".xml").c_str());
			  });
		}
	}

	if (controls.size() == 1)
	{
		CAudioControl* pControl = controls[0];
		switch (pControl->GetType())
		{
		case EItemType::eItemType_Trigger:
			contextMenu.addAction(tr("Execute Trigger"), [&]() { ExecuteControl(); });
			contextMenu.addSeparator();
			break;
		case EItemType::eItemType_Switch:
			contextMenu.addAction(GetItemTypeIcon(eItemType_State), tr("Add State"), [&]() { CreateControl("new_state", eItemType_State, pControl); });
			contextMenu.addSeparator();
			break;
		case EItemType::eItemType_Preload:
			if (pControl->GetScope() == CCrc32::Compute("global") && !pControl->IsAutoLoad())
			{
				contextMenu.addAction(tr("Load Global Preload Request"), [&]() { gEnv->pAudioSystem->PreloadSingleRequest(CCrc32::ComputeLowercase(pControl->GetName()), false); });
				contextMenu.addAction(tr("Unload Global Preload Request"), [&]() { gEnv->pAudioSystem->UnloadSingleRequest(CCrc32::ComputeLowercase(pControl->GetName())); });
				contextMenu.addSeparator();
			}
			break;
		}
	}
	else
	{
		bool bOnlyGlobalPreloads = false;

		for (auto const pControl : controls)
		{
			if (pControl->GetType() == EItemType::eItemType_Preload && pControl->GetScope() == CCrc32::Compute("global") && !pControl->IsAutoLoad())
			{
				bOnlyGlobalPreloads = true;
			}
			else
			{
				bOnlyGlobalPreloads = false;
				break;
			}
		}

		if (bOnlyGlobalPreloads)
		{
			contextMenu.addAction(tr("Load Global Preload Requests"), [&]()
				{
					for (auto const pControl : controls)
					{
						gEnv->pAudioSystem->PreloadSingleRequest(CCrc32::ComputeLowercase(pControl->GetName()), false);
					}
			  });
			contextMenu.addAction(tr("Unload Global Preload Requests"), [&]()
				{
					for (auto const pControl : controls)
					{
						gEnv->pAudioSystem->UnloadSingleRequest(CCrc32::ComputeLowercase(pControl->GetName()));
					}
			  });
			contextMenu.addSeparator();
		}
	}

	contextMenu.addAction(tr("Rename"), [&]() { m_pControlsTree->edit(m_pControlsTree->currentIndex()); });
	contextMenu.addAction(tr("Delete"), [&]() { DeleteSelectedControl(); });
	contextMenu.addSeparator();

	if (!selection.isEmpty())
	{
		contextMenu.addAction(tr("Expand Selection"), [&]() { m_pControlsTree->ExpandSelection(m_pControlsTree->GetSelectedIndexes()); });
		contextMenu.addAction(tr("Collapse Selection"), [&]() { m_pControlsTree->CollapseSelection(m_pControlsTree->GetSelectedIndexes()); });
		contextMenu.addSeparator();
	}
	
	contextMenu.addAction(tr("Expand All"), [&]() { m_pControlsTree->expandAll(); });
	contextMenu.addAction(tr("Collapse All"), [&]() { m_pControlsTree->collapseAll(); });

	contextMenu.exec(m_pControlsTree->mapToGlobal(pos));
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsExplorer::DeleteSelectedControl()
{
	auto selection = m_pControlsTree->selectionModel()->selectedRows();
	int const size = selection.length();
	if (size > 0)
	{
		QString text;
		if (size == 1)
		{
			text = R"(Are you sure you want to delete ")" + selection[0].data(Qt::DisplayRole).toString() + R"("?.)";
		}
		else
		{
			text = "Are you sure you want to delete the selected controls and folders?.";
		}

		CQuestionDialog messageBox;
		messageBox.SetupQuestion("Audio Controls Editor", text);
		if (messageBox.Execute() == QDialogButtonBox::Yes)
		{
			CUndo undo("Audio Control Removed");

			std::vector<IAudioAsset*> selectedItems;
			for (auto index : selection)
			{
				if (index.isValid())
				{
					selectedItems.push_back(AudioModelUtils::GetAssetFromIndex(index));
				}
			}

			std::vector<IAudioAsset*> itemsToDelete;
			Utils::SelectTopLevelAncestors(selectedItems, itemsToDelete);

			for (auto pItem : itemsToDelete)
			{
				m_pAssetsManager->DeleteItem(pItem);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsExplorer::ExecuteControl()
{
	IAudioAsset* pAsset = AudioModelUtils::GetAssetFromIndex(m_pControlsTree->currentIndex());
	if (pAsset && pAsset->GetType() == eItemType_Trigger)
	{
		CAudioControlsEditorPlugin::ExecuteTrigger(pAsset->GetName());
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsExplorer::StopControlExecution()
{
	CAudioControlsEditorPlugin::StopTriggerExecution();
}

//////////////////////////////////////////////////////////////////////////
QAbstractItemModel* CAudioAssetsExplorer::CreateLibraryModelFromIndex(QModelIndex const& sourceIndex)
{
	if (sourceIndex.model() != m_pAssetsModel)
	{
		return nullptr;
	}

	size_t const numLibraries = m_libraryModels.size();
	size_t const row = static_cast<size_t>(sourceIndex.row());
	if (row >= numLibraries)
	{
		m_libraryModels.resize(row + 1);
		for (size_t i = numLibraries; i < row + 1; ++i)
		{
			m_libraryModels[i] = new CAudioLibraryModel(m_pAssetsManager, m_pAssetsManager->GetLibrary(i));
		}
	}

	return m_libraryModels[row];
}

//////////////////////////////////////////////////////////////////////////
IAudioAsset* CAudioAssetsExplorer::GetSelectedAsset() const
{
	QModelIndex const& index = m_pControlsTree->currentIndex();
	if (index.isValid())
	{
		return AudioModelUtils::GetAssetFromIndex(index);
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsExplorer::SelectNewAsset(QModelIndex const& parent, int const row)
{
	if (parent.isValid())
	{
		if (m_bCreatedFromMenu)
		{
			QModelIndex const& assetIndex = m_pProxyModel->mapFromSource(m_pMountedModel->index(row, 0, parent));
			m_pControlsTree->setCurrentIndex(assetIndex);
			m_pControlsTree->edit(assetIndex);
			m_bCreatedFromMenu = false;
		}
		else if (!CAudioControlsEditorPlugin::GetAssetsManager()->IsLoading())
		{
			QModelIndex const& parentIndex = m_pProxyModel->mapFromSource(parent);
			m_pControlsTree->expand(parentIndex);
			m_pControlsTree->setCurrentIndex(parentIndex);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioAssetsExplorer::ResetFilters()
{
	for (QAction* pAction : m_pFilterMenu->actions())
	{
		pAction->setChecked(true);
	}
	ShowControlType(eItemType_Trigger, true);
	ShowControlType(eItemType_Parameter, true);
	ShowControlType(eItemType_Environment, true);
	ShowControlType(eItemType_Preload, true);
	ShowControlType(eItemType_Switch, true);
	ShowControlType(eItemType_State, true);
	m_pTextFilter->setText("");
}
} // namespace ACE
