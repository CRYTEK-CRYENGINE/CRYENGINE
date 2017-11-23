// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SystemControlsWidget.h"

#include "SystemControlsModel.h"
#include "AudioControlsEditorPlugin.h"
#include "MiddlewareDataWidget.h"
#include "MiddlewareDataModel.h"
#include "SystemAssets.h"
#include "SystemAssetsManager.h"
#include "SystemControlsEditorIcons.h"
#include "AudioTreeView.h"

#include <CryAudio/IAudioSystem.h>
#include <QtUtil.h>
#include <FilePathUtil.h>
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>
#include <CryMath/Cry_Camera.h>
#include <CryIcon.h>
#include <Controls/QuestionDialog.h>
#include <ProxyModels/MountingProxyModel.h>
#include <QSearchBox.h>
#include <IUndoObject.h>

#include <QAction>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CSystemControlsWidget::CSystemControlsWidget(CSystemAssetsManager* pAssetsManager)
	: m_pAssetsManager(pAssetsManager)
	, m_pLibraryModel(new CAudioLibraryModel(m_pAssetsManager))
	, m_pFilterProxyModel(new CSystemControlsFilterProxyModel(this))
	, m_pSearchBox(new QSearchBox())
	, m_pFilterButton(new QToolButton())
	, m_pFilterWidget(new QWidget())
	, m_pTreeView(new CAudioTreeView())
{
	auto const libCount = m_pAssetsManager->GetLibraryCount();
	m_controlsModels.resize(libCount);

	for (int i = 0; i < libCount; ++i)
	{
		m_controlsModels[i] = new CSystemControlsModel(m_pAssetsManager, m_pAssetsManager->GetLibrary(i));
	}

	m_pMountingProxyModel = new CMountingProxyModel(WrapMemberFunction(this, &CSystemControlsWidget::CreateControlsModelFromIndex));
	m_pMountingProxyModel->SetHeaderDataCallbacks(1, &GetHeaderData);
	m_pMountingProxyModel->SetSourceModel(m_pLibraryModel);

	m_pFilterProxyModel->setSourceModel(m_pMountingProxyModel);
	m_pFilterProxyModel->setDynamicSortFilter(true);
	
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	QVBoxLayout* const pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);

	QSplitter* const pSplitter = new QSplitter();
	pSplitter->setOrientation(Qt::Vertical);
	pSplitter->setChildrenCollapsible(false);

	InitFilterWidgets(pMainLayout);
	pSplitter->addWidget(m_pFilterWidget);

	m_pTreeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
	m_pTreeView->setDragEnabled(true);
	m_pTreeView->setDragDropMode(QAbstractItemView::DragDrop);
	m_pTreeView->setDefaultDropAction(Qt::MoveAction);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pFilterProxyModel);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pTreeView->viewport()->installEventFilter(this);
	m_pTreeView->installEventFilter(this);
	pSplitter->addWidget(m_pTreeView);
	
	pSplitter->setStretchFactor(0, 0);
	pSplitter->setStretchFactor(1, 1);
	pMainLayout->addWidget(pSplitter);

	QObject::connect(m_pTreeView, &CAudioTreeView::customContextMenuRequested, this, &CSystemControlsWidget::OnContextMenu);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CSystemControlsWidget::SelectedControlChanged);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, m_pTreeView, &CAudioTreeView::OnSelectionChanged);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &CSystemControlsWidget::StopControlExecution);
	
	QObject::connect(m_pMountingProxyModel, &CMountingProxyModel::rowsInserted, this, &CSystemControlsWidget::SelectNewAsset);
	
	m_pAssetsManager->signalItemAboutToBeAdded.Connect([&](CSystemAsset* const pItem)
	{
		ResetFilters();
	}, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalLibraryAboutToBeAdded.Connect([&]()
	{
		ResetFilters();
	}, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalLibraryAboutToBeRemoved.Connect([&](CSystemLibrary* const pLibrary)
	{
		int const libCount = m_pAssetsManager->GetLibraryCount();

		for (int i = 0; i < libCount; ++i)
		{
			if (m_pAssetsManager->GetLibrary(i) == pLibrary)
			{
				m_controlsModels[i]->deleteLater();
				m_controlsModels.erase(m_controlsModels.begin() + i);
				break;
			}
		}
	}, reinterpret_cast<uintptr_t>(this));

	m_pAssetsManager->signalAssetRenamed.Connect([&]()
	{
		m_pFilterProxyModel->invalidate();
		m_pTreeView->scrollTo(m_pTreeView->currentIndex());
	}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CSystemControlsWidget::~CSystemControlsWidget()
{
	m_pAssetsManager->signalLibraryAboutToBeRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalItemAboutToBeAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalLibraryAboutToBeAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	m_pAssetsManager->signalAssetRenamed.DisconnectById(reinterpret_cast<uintptr_t>(this));

	StopControlExecution();
	delete m_pLibraryModel;

	for (auto const pControlsModel : m_controlsModels)
	{
		pControlsModel->DisconnectFromSystem();
		delete pControlsModel;
	}

	m_controlsModels.clear();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::InitAddControlWidget(QHBoxLayout* const pLayout)
{
	QPushButton* const pAddButton = new QPushButton(tr("Add"));
	pAddButton->setToolTip(tr("Add new library, folder or control"));
	
	QMenu* const pAddButtonMenu = new QMenu();
	QObject::connect(pAddButtonMenu, &QMenu::aboutToShow, this, &CSystemControlsWidget::UpdateCreateButtons);

	pAddButtonMenu->addAction(GetItemTypeIcon(ESystemItemType::Library), tr("Library"), [&]()
	{
		m_isCreatedFromMenu = true;
		m_pAssetsManager->CreateLibrary(Utils::GenerateUniqueLibraryName("new_library", *m_pAssetsManager));
	});

	pAddButtonMenu->addSeparator();

	m_pCreateParentFolderAction = new QAction(GetItemTypeIcon(ESystemItemType::Folder), tr("Parent Folder"), pAddButtonMenu);
	QObject::connect(m_pCreateParentFolderAction, &QAction::triggered, [&]() { CreateParentFolder(); });
	m_pCreateParentFolderAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateParentFolderAction);

	m_pCreateFolderAction = new QAction(GetItemTypeIcon(ESystemItemType::Folder), tr("Folder"), pAddButtonMenu);
	QObject::connect(m_pCreateFolderAction, &QAction::triggered, [&]() { CreateFolder(GetSelectedAsset()); });
	m_pCreateFolderAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateFolderAction);

	m_pCreateTriggerAction = new QAction(GetItemTypeIcon(ESystemItemType::Trigger), tr("Trigger"), pAddButtonMenu);
	QObject::connect(m_pCreateTriggerAction, &QAction::triggered, [&]() { CreateControl("new_trigger", ESystemItemType::Trigger, GetSelectedAsset()); });
	m_pCreateTriggerAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateTriggerAction);

	m_pCreateParameterAction = new QAction(GetItemTypeIcon(ESystemItemType::Parameter), tr("Parameter"), pAddButtonMenu);
	QObject::connect(m_pCreateParameterAction, &QAction::triggered, [&]() { CreateControl("new_parameter", ESystemItemType::Parameter, GetSelectedAsset()); });
	m_pCreateParameterAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateParameterAction);

	m_pCreateSwitchAction = new QAction(GetItemTypeIcon(ESystemItemType::Switch), tr("Switch"), pAddButtonMenu);
	QObject::connect(m_pCreateSwitchAction, &QAction::triggered, [&]() { CreateControl("new_switch", ESystemItemType::Switch, GetSelectedAsset()); });
	m_pCreateSwitchAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateSwitchAction);

	m_pCreateStateAction = new QAction(GetItemTypeIcon(ESystemItemType::State), tr("State"), pAddButtonMenu);
	QObject::connect(m_pCreateStateAction, &QAction::triggered, [&]() { CreateControl("new_state", ESystemItemType::State, GetSelectedAsset()); });
	m_pCreateStateAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateStateAction);

	m_pCreateEnvironmentAction = new QAction(GetItemTypeIcon(ESystemItemType::Environment), tr("Environment"), pAddButtonMenu);
	QObject::connect(m_pCreateEnvironmentAction, &QAction::triggered, [&]() { CreateControl("new_environment", ESystemItemType::Environment, GetSelectedAsset()); });
	m_pCreateEnvironmentAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateEnvironmentAction);

	m_pCreatePreloadAction = new QAction(GetItemTypeIcon(ESystemItemType::Preload), tr("Preload"), pAddButtonMenu);
	QObject::connect(m_pCreatePreloadAction, &QAction::triggered, [&]() { CreateControl("new_preload", ESystemItemType::Preload, GetSelectedAsset()); });
	m_pCreatePreloadAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreatePreloadAction);

	pAddButton->setMenu(pAddButtonMenu);
	pLayout->addWidget(pAddButton);
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::InitFilterWidgets(QVBoxLayout* const pMainLayout)
{
	QHBoxLayout* const pFilterLayout = new QHBoxLayout();
	pFilterLayout->addWidget(m_pSearchBox);

	m_pFilterButton->setIcon(CryIcon("icons:General/Filter.ico"));
	m_pFilterButton->setToolTip(tr("Show Control Type Filters"));
	m_pFilterButton->setCheckable(true);
	m_pFilterButton->setMaximumSize(QSize(20, 20));
	pFilterLayout->addWidget(m_pFilterButton);

	InitAddControlWidget(pFilterLayout);
	InitTypeFilters();

	QObject::connect(m_pFilterButton, &QToolButton::toggled, [&](bool const isChecked)
	{
		m_pFilterWidget->setVisible(isChecked);

		if (isChecked)
		{
			m_pFilterButton->setToolTip(tr("Hide Control Type Filters"));
		}
		else
		{
			m_pFilterButton->setToolTip(tr("Show Control Type Filters"));
		}
	});


	QObject::connect(m_pSearchBox, &QSearchBox::textChanged, [&](QString const& filter)
	{
		if (m_filter != filter)
		{
			if (m_filter.isEmpty() && !filter.isEmpty())
			{
				BackupTreeViewStates();
				StartTextFiltering();
				m_pTreeView->expandAll();
			}
			else if (!m_filter.isEmpty() && filter.isEmpty())
			{
				m_pFilterProxyModel->setFilterFixedString(filter);
				m_pTreeView->collapseAll();
				RestoreTreeViewStates();
				StopTextFiltering();
			}
			else if (!m_filter.isEmpty() && !filter.isEmpty())
			{
				m_pFilterProxyModel->setFilterFixedString(filter);
				m_pTreeView->expandAll();
			}

			m_filter = filter;
		}

		m_pFilterProxyModel->setFilterFixedString(filter);
	});

	pMainLayout->addLayout(pFilterLayout);
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::InitTypeFilters()
{
	QVBoxLayout* const pTypeFiltersLayout = new QVBoxLayout();
	pTypeFiltersLayout->setContentsMargins(0, 5, 0, 5);

	QCheckBox* const pFilterTriggersCheckbox = new QCheckBox(tr("Triggers"));
	QObject::connect(pFilterTriggersCheckbox, &QCheckBox::toggled, [&](bool const isVisible)
	{
		ShowControlType(ESystemItemType::Trigger, isVisible);
	});

	pFilterTriggersCheckbox->setChecked(true);
	pFilterTriggersCheckbox->setToolTip(tr("Show/hide Triggers"));
	pFilterTriggersCheckbox->setIcon(GetItemTypeIcon(ESystemItemType::Trigger));
	pTypeFiltersLayout->addWidget(pFilterTriggersCheckbox);

	QCheckBox* const pFilterParametersCheckbox = new QCheckBox(tr("Parameters"));
	QObject::connect(pFilterParametersCheckbox, &QCheckBox::toggled, [&](bool const isVisible)
	{
		ShowControlType(ESystemItemType::Parameter, isVisible);
	});

	pFilterParametersCheckbox->setChecked(true);
	pFilterParametersCheckbox->setToolTip(tr("Show/hide Parameters"));
	pFilterParametersCheckbox->setIcon(GetItemTypeIcon(ESystemItemType::Parameter));
	pTypeFiltersLayout->addWidget(pFilterParametersCheckbox);

	QCheckBox* const pFilterSwitchesCheckbox = new QCheckBox(tr("Switches"));
	QObject::connect(pFilterSwitchesCheckbox, &QCheckBox::toggled, [&](bool const isVisible)
	{
		ShowControlType(ESystemItemType::Switch, isVisible);
	});

	pFilterSwitchesCheckbox->setChecked(true);
	pFilterSwitchesCheckbox->setToolTip(tr("Show/hide Switches"));
	pFilterSwitchesCheckbox->setIcon(GetItemTypeIcon(ESystemItemType::Switch));
	pTypeFiltersLayout->addWidget(pFilterSwitchesCheckbox);

	QCheckBox* const pFilterEnvironmentsCheckbox = new QCheckBox(tr("Environments"));
	QObject::connect(pFilterEnvironmentsCheckbox, &QCheckBox::toggled, [&](bool const isVisible)
	{
		ShowControlType(ESystemItemType::Environment, isVisible);
	});

	pFilterEnvironmentsCheckbox->setChecked(true);
	pFilterEnvironmentsCheckbox->setToolTip(tr("Show/hide Environments"));
	pFilterEnvironmentsCheckbox->setIcon(GetItemTypeIcon(ESystemItemType::Environment));
	pTypeFiltersLayout->addWidget(pFilterEnvironmentsCheckbox);

	QCheckBox* const pFilterPreloadsCheckbox = new QCheckBox(tr("Preloads"));
	QObject::connect(pFilterPreloadsCheckbox, &QCheckBox::toggled, [&](bool const isVisible)
	{
		ShowControlType(ESystemItemType::Preload, isVisible);
	});

	pFilterPreloadsCheckbox->setChecked(true);
	pFilterPreloadsCheckbox->setToolTip(tr("Show/hide Preloads"));
	pFilterPreloadsCheckbox->setIcon(GetItemTypeIcon(ESystemItemType::Preload));
	pTypeFiltersLayout->addWidget(pFilterPreloadsCheckbox);

	m_pFilterWidget = QtUtil::MakeScrollable(pTypeFiltersLayout);
	m_pFilterWidget->setHidden(true);
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsWidget::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if ((pEvent->type() == QEvent::KeyRelease) && !m_pTreeView->IsEditing())
	{
		QKeyEvent const* const pKeyEvent = static_cast<QKeyEvent*>(pEvent);

		if (pKeyEvent != nullptr)
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
	else if ((pEvent->type() == QEvent::MouseButtonDblClick) && !m_pTreeView->IsEditing())
	{
		ExecuteControl();
	}
	else if (pEvent->type() == QEvent::Drop)
	{
		m_pTreeView->selectionModel()->clearSelection();
		m_pTreeView->selectionModel()->clearCurrentIndex();
	}

	return QWidget::eventFilter(pObject, pEvent);
}

//////////////////////////////////////////////////////////////////////////
std::vector<CSystemAsset*> CSystemControlsWidget::GetSelectedAssets() const
{
	QModelIndexList const& indexes = m_pTreeView->selectionModel()->selectedIndexes();
	std::vector<CSystemAsset*> assets;
	AudioModelUtils::GetAssetsFromIndexesCombined(indexes, assets);
	return assets;
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::Reload()
{
	ResetFilters();
	m_pFilterProxyModel->invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::ShowControlType(ESystemItemType const type, bool const isVisible)
{
	m_pFilterProxyModel->EnableControl(isVisible, type);

	if (type == ESystemItemType::Switch)
	{
		m_pFilterProxyModel->EnableControl(isVisible, ESystemItemType::State);
	}
}

//////////////////////////////////////////////////////////////////////////
CSystemControl* CSystemControlsWidget::CreateControl(string const& name, ESystemItemType type, CSystemAsset* const pParent)
{
	m_isCreatedFromMenu = true;

	if (type != ESystemItemType::State)
	{
		return m_pAssetsManager->CreateControl(Utils::GenerateUniqueControlName(name, type, *m_pAssetsManager), type, pParent);
	}
	else
	{
		return m_pAssetsManager->CreateControl(Utils::GenerateUniqueName(name, type, pParent), type, pParent);
	}
}

//////////////////////////////////////////////////////////////////////////
CSystemAsset* CSystemControlsWidget::CreateFolder(CSystemAsset* const pParent)
{
	m_isCreatedFromMenu = true;
	return m_pAssetsManager->CreateFolder(Utils::GenerateUniqueName("new_folder", ESystemItemType::Folder, pParent), pParent);
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::CreateParentFolder()
{
	std::vector<CSystemLibrary*> libraries;
	std::vector<CSystemFolder*> folders;
	std::vector<CSystemControl*> controls;
	std::vector<CSystemAsset*> assetsToMove;

	auto const& selection = m_pTreeView->selectionModel()->selectedRows();
	AudioModelUtils::GetAssetsFromIndexesSeparated(selection, libraries, folders, controls);

	for (auto const pFolder : folders)
	{
		assetsToMove.emplace_back(static_cast<CSystemAsset*>(pFolder));
	}

	for (auto const pControl : controls)
	{
		assetsToMove.emplace_back(static_cast<CSystemAsset*>(pControl));
	}

	auto const pParent = assetsToMove[0]->GetParent();
	auto const pNewFolder = CreateFolder(pParent);
	m_pAssetsManager->MoveItems(pNewFolder, assetsToMove);
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::OnContextMenu(QPoint const& pos)
{
	QMenu* const pContextMenu = new QMenu();
	QMenu* const pAddMenu = new QMenu(tr("Add"));
	auto const& selection = m_pTreeView->selectionModel()->selectedRows();

	pContextMenu->addMenu(pAddMenu);
	pContextMenu->addSeparator();

	pAddMenu->addAction(GetItemTypeIcon(ESystemItemType::Library), tr("Library"), [&]()
	{
		m_isCreatedFromMenu = true;
		m_pAssetsManager->CreateLibrary(Utils::GenerateUniqueLibraryName("new_library", *m_pAssetsManager));
	});

	pAddMenu->addSeparator();

	if (!selection.isEmpty())
	{
		std::vector<CSystemLibrary*> libraries;
		std::vector<CSystemFolder*> folders;
		std::vector<CSystemControl*> controls;

		AudioModelUtils::GetAssetsFromIndexesSeparated(selection, libraries, folders, controls);

		if (IsParentFolderAllowed())
		{
			pAddMenu->addAction(GetItemTypeIcon(ESystemItemType::Folder), tr("Parent Folder"), [&]() { CreateParentFolder(); });
		}

		if (selection.size() == 1)
		{
			QModelIndex const& index = m_pTreeView->currentIndex();

			if (index.isValid())
			{
				CSystemAsset const* const pAsset = AudioModelUtils::GetAssetFromIndex(index);

				if (pAsset != nullptr)
				{
					ESystemItemType const assetType = pAsset->GetType();

					if ((assetType == ESystemItemType::Library) || (assetType == ESystemItemType::Folder))
					{
						CSystemAsset* pParent = nullptr;

						if (assetType == ESystemItemType::Folder)
						{
							pParent = static_cast<CSystemAsset*>(folders[0]);
						}
						else
						{
							pParent = static_cast<CSystemAsset*>(libraries[0]);
						}

						pAddMenu->addAction(GetItemTypeIcon(ESystemItemType::Folder), tr("Folder"), [&]() { CreateFolder(pParent); });
						pAddMenu->addAction(GetItemTypeIcon(ESystemItemType::Trigger), tr("Trigger"), [&]() { CreateControl("new_trigger", ESystemItemType::Trigger, pParent); });
						pAddMenu->addAction(GetItemTypeIcon(ESystemItemType::Parameter), tr("Parameter"), [&]() { CreateControl("new_parameter", ESystemItemType::Parameter, pParent); });
						pAddMenu->addAction(GetItemTypeIcon(ESystemItemType::Switch), tr("Switch"), [&]() { CreateControl("new_switch", ESystemItemType::Switch, pParent); });
						pAddMenu->addAction(GetItemTypeIcon(ESystemItemType::Environment), tr("Environment"), [&]() { CreateControl("new_environment", ESystemItemType::Environment, pParent); });
						pAddMenu->addAction(GetItemTypeIcon(ESystemItemType::Preload), tr("Preload"), [&]() { CreateControl("new_preload", ESystemItemType::Preload, pParent); });

						if (pParent->GetType() == ESystemItemType::Library)
						{
							//TODO: Take into account files in the "levels" folder
							pContextMenu->addAction(tr("Open Containing Folder"), [&]()
							{
								QtUtil::OpenInExplorer(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), Utils::GetAssetFolder() + pParent->GetName() + ".xml").c_str());
							});

							pContextMenu->addSeparator();
						}
					}
				}
			}

			if (controls.size() == 1)
			{
				CSystemControl* const pControl = controls[0];
				ESystemItemType const controlType = pControl->GetType();

				if (controlType == ESystemItemType::Trigger)
				{
					QAction* const pTriggerAction = new QAction(tr("Execute Trigger"), pContextMenu);
					QObject::connect(pTriggerAction, &QAction::triggered, [&]() { ExecuteControl(); });
					pContextMenu->insertSeparator(pContextMenu->actions().at(0));
					pContextMenu->insertAction(pContextMenu->actions().at(0), pTriggerAction);
				}
				else if (controlType == ESystemItemType::Switch)
				{
					pAddMenu->addAction(GetItemTypeIcon(ESystemItemType::State), tr("State"), [&]() { CreateControl("new_state", ESystemItemType::State, pControl); });
				}
				else if (controlType == ESystemItemType::Preload)
				{
					if (pControl->GetScope() == CCrc32::Compute("global") && !pControl->IsAutoLoad())
					{
						QAction* const pLoadAction = new QAction(tr("Load Global Preload Request"), pContextMenu);
						QAction* const pUnloadAction = new QAction(tr("Unload Global Preload Request"), pContextMenu);
						QObject::connect(pLoadAction, &QAction::triggered, [&]() { gEnv->pAudioSystem->PreloadSingleRequest(CryAudio::StringToId(pControl->GetName()), false); });
						QObject::connect(pUnloadAction, &QAction::triggered, [&]() { gEnv->pAudioSystem->UnloadSingleRequest(CryAudio::StringToId(pControl->GetName())); });
						pContextMenu->insertSeparator(pContextMenu->actions().at(0));
						pContextMenu->insertAction(pContextMenu->actions().at(0), pUnloadAction);
						pContextMenu->insertAction(pContextMenu->actions().at(0), pLoadAction);
					}
				}
			}
		}
		else if (controls.size() > 1)
		{
			bool hasOnlyGlobalPreloads = false;

			for (auto const pControl : controls)
			{
				if ((pControl->GetType() == ESystemItemType::Preload) && (pControl->GetScope() == CCrc32::Compute("global")) && !pControl->IsAutoLoad())
				{
					hasOnlyGlobalPreloads = true;
				}
				else
				{
					hasOnlyGlobalPreloads = false;
					break;
				}
			}

			if (hasOnlyGlobalPreloads)
			{

				QAction* const pLoadAction = new QAction(tr("Load Global Preload Requests"), pContextMenu);
				QAction* const pUnloadAction = new QAction(tr("Unload Global Preload Requests"), pContextMenu);
				QObject::connect(pLoadAction, &QAction::triggered, [&]()
				{ 
					for (auto const pControl : controls)
					{
						gEnv->pAudioSystem->PreloadSingleRequest(CryAudio::StringToId(pControl->GetName()), false);
					}
				});

				QObject::connect(pUnloadAction, &QAction::triggered, [&]()
				{ 
					for (auto const pControl : controls)
					{
						gEnv->pAudioSystem->UnloadSingleRequest(CryAudio::StringToId(pControl->GetName()));
					}
				});

				pContextMenu->insertSeparator(pContextMenu->actions().at(0));
				pContextMenu->insertAction(pContextMenu->actions().at(0), pUnloadAction);
				pContextMenu->insertAction(pContextMenu->actions().at(0), pLoadAction);
			}
		}

		pContextMenu->addAction(tr("Rename"), [&]() { m_pTreeView->edit(m_pTreeView->currentIndex()); });
		pContextMenu->addAction(tr("Delete"), [&]() { DeleteSelectedControl(); });
		pContextMenu->addSeparator();
		pContextMenu->addAction(tr("Expand Selection"), [&]() { m_pTreeView->ExpandSelection(m_pTreeView->GetSelectedIndexes()); });
		pContextMenu->addAction(tr("Collapse Selection"), [&]() { m_pTreeView->CollapseSelection(m_pTreeView->GetSelectedIndexes()); });
		pContextMenu->addSeparator();
	}

	pContextMenu->addAction(tr("Expand All"), [&]() { m_pTreeView->expandAll(); });
	pContextMenu->addAction(tr("Collapse All"), [&]() { m_pTreeView->collapseAll(); });

	pContextMenu->exec(QCursor::pos());
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::DeleteSelectedControl()
{
	auto const& selection = m_pTreeView->selectionModel()->selectedRows();
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
			text = "Are you sure you want to delete the selected controls and folders?";
		}

		CQuestionDialog* const messageBox = new CQuestionDialog();
		messageBox->SetupQuestion("Audio Controls Editor", text);

		if (messageBox->Execute() == QDialogButtonBox::Yes)
		{
			CUndo undo("Audio Control Removed");

			std::vector<CSystemAsset*> selectedItems;

			for (auto const& index : selection)
			{
				if (index.isValid())
				{
					selectedItems.emplace_back(AudioModelUtils::GetAssetFromIndex(index));
				}
			}

			std::vector<CSystemAsset*> itemsToDelete;
			Utils::SelectTopLevelAncestors(selectedItems, itemsToDelete);

			for (auto const pItem : itemsToDelete)
			{
				m_pAssetsManager->DeleteItem(pItem);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::ExecuteControl()
{
	CSystemAsset const* const pAsset = AudioModelUtils::GetAssetFromIndex(m_pTreeView->currentIndex());

	if ((pAsset != nullptr) && (pAsset->GetType() == ESystemItemType::Trigger))
	{
		CAudioControlsEditorPlugin::ExecuteTrigger(pAsset->GetName());
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::StopControlExecution()
{
	CAudioControlsEditorPlugin::StopTriggerExecution();
}

//////////////////////////////////////////////////////////////////////////
QAbstractItemModel* CSystemControlsWidget::CreateControlsModelFromIndex(QModelIndex const& sourceIndex)
{
	if (sourceIndex.model() != m_pLibraryModel)
	{
		return nullptr;
	}

	size_t const numLibraries = m_controlsModels.size();
	size_t const row = static_cast<size_t>(sourceIndex.row());

	if (row >= numLibraries)
	{
		m_controlsModels.resize(row + 1);

		for (size_t i = numLibraries; i < row + 1; ++i)
		{
			m_controlsModels[i] = new CSystemControlsModel(m_pAssetsManager, m_pAssetsManager->GetLibrary(i));
		}
	}

	return m_controlsModels[row];
}

//////////////////////////////////////////////////////////////////////////
CSystemAsset* CSystemControlsWidget::GetSelectedAsset() const
{
	QModelIndex const& index = m_pTreeView->currentIndex();

	if (index.isValid())
	{
		return AudioModelUtils::GetAssetFromIndex(index);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::SelectNewAsset(QModelIndex const& parent, int const row)
{
	if (!CAudioControlsEditorPlugin::GetAssetsManager()->IsLoading())
	{
		QModelIndex const& assetIndex = m_pFilterProxyModel->mapFromSource(m_pMountingProxyModel->index(row, 0, parent));

		if (m_isCreatedFromMenu)
		{
			m_pTreeView->setCurrentIndex(assetIndex);
			m_pTreeView->edit(assetIndex);
			m_isCreatedFromMenu = false;
		}
		else
		{
			QModelIndex const& parentIndex = m_pFilterProxyModel->mapFromSource(parent);
			m_pTreeView->expand(parentIndex);
			m_pTreeView->selectionModel()->select(assetIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);

			if (m_pTreeView->selectionModel()->selectedRows().size() == 1)
			{
				m_pTreeView->setCurrentIndex(assetIndex);
			}
			else
			{
				m_pTreeView->scrollTo(assetIndex);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::UpdateCreateButtons()
{
	m_pCreateParentFolderAction->setVisible(IsParentFolderAllowed());

	auto const& selection = m_pTreeView->selectionModel()->selectedRows();

	if (selection.size() == 1)
	{
		QModelIndex const& index = m_pTreeView->currentIndex();

		if (index.isValid())
		{
			CSystemAsset const* const pAsset = AudioModelUtils::GetAssetFromIndex(index);

			if (pAsset != nullptr)
			{
				ESystemItemType const itemType = pAsset->GetType();
				bool const isLibraryOrFolder = (itemType == ESystemItemType::Library) || (itemType == ESystemItemType::Folder);

				m_pCreateFolderAction->setVisible(isLibraryOrFolder);
				m_pCreateTriggerAction->setVisible(isLibraryOrFolder);
				m_pCreateParameterAction->setVisible(isLibraryOrFolder);
				m_pCreateSwitchAction->setVisible(isLibraryOrFolder);
				m_pCreateStateAction->setVisible(itemType == ESystemItemType::Switch);
				m_pCreateEnvironmentAction->setVisible(isLibraryOrFolder);
				m_pCreatePreloadAction->setVisible(isLibraryOrFolder);
				return;
			}
		}
	}

	m_pCreateFolderAction->setVisible(false);
	m_pCreateTriggerAction->setVisible(false);
	m_pCreateParameterAction->setVisible(false);
	m_pCreateSwitchAction->setVisible(false);
	m_pCreateStateAction->setVisible(false);
	m_pCreateEnvironmentAction->setVisible(false);
	m_pCreatePreloadAction->setVisible(false);
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsWidget::IsParentFolderAllowed()
{
	auto const& selection = m_pTreeView->selectionModel()->selectedRows();

	if (!selection.isEmpty())
	{
		std::vector<CSystemLibrary*> libraries;
		std::vector<CSystemFolder*> folders;
		std::vector<CSystemControl*> controls;

		AudioModelUtils::GetAssetsFromIndexesSeparated(selection, libraries, folders, controls);

		if (libraries.empty() && (!folders.empty() || !controls.empty()))
		{
			bool isAllowed = true;
			CSystemAsset const* pParent;

			if (!folders.empty())
			{
				pParent = folders[0]->GetParent();

				for (auto const pFolder : folders)
				{
					if (pFolder->GetParent() != pParent)
					{
						isAllowed = false;
						break;
					}
				}
			}
			else
			{
				pParent = controls[0]->GetParent();
			}

			if (isAllowed)
			{
				for (auto const pControl : controls)
				{
					if ((pControl->GetParent() != pParent) || (pControl->GetType() == ESystemItemType::State))
					{
						isAllowed = false;
						break;
					}
				}
			}

			return isAllowed;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::ResetFilters()
{
	for (auto const pCheckBox : m_pFilterWidget->findChildren<QCheckBox*>())
	{
		pCheckBox->setChecked(true);
	}

	m_pSearchBox->clear();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::BackupTreeViewStates()
{
	m_pTreeView->BackupExpanded();
	m_pTreeView->BackupSelection();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::RestoreTreeViewStates()
{
	m_pTreeView->RestoreExpanded();
	m_pTreeView->RestoreSelection();
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsWidget::IsEditing() const
{
	return m_pTreeView->IsEditing();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::SelectConnectedSystemControl(CSystemControl const* const pControl)
{
	if (pControl != nullptr)
	{
		ResetFilters();
		auto const matches = m_pFilterProxyModel->match(m_pFilterProxyModel->index(0, 0, QModelIndex()), static_cast<int>(EDataRole::Id), pControl->GetId(), 1, Qt::MatchRecursive);

		if (!matches.isEmpty())
		{
			m_pTreeView->setFocus();
			m_pTreeView->selectionModel()->setCurrentIndex(matches.first(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		}
	}
}
} // namespace ACE
