// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "VegetationEditor.h"

#include "QT/QToolTabManager.h"
#include "QT/Widgets/QEditToolButton.h"
#include "QT/Widgets/QPreviewWidget.h"
#include "Vegetation/VegetationEraseTool.h"
#include "Vegetation/VegetationMap.h"
#include "Vegetation/VegetationModel.h"
#include "Vegetation/VegetationObject.h"
#include "Vegetation/VegetationPaintTool.h"
#include "Vegetation/VegetationPlaceTool.h"
#include "Vegetation/VegetationSelectTool.h"
#include "Vegetation/VegetationTreeView.h"
#include "IEditorImpl.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/Browser/AssetBrowserDialog.h>
#include <EditorFramework/Events.h>
#include <FileDialogs/SystemFileDialog.h>
#include <IUndoObject.h>
#include <Controls/QuestionDialog.h>
#include <Menu/AbstractMenu.h>
#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>
#include <Util/FileUtil.h>

#include <CrySandbox/CrySignal.h>

#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QLayout>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>

DECLARE_PYTHON_MODULE(vegetation);

namespace Private_VegetationEditor
{
void EnsureVegetationEditorActive()
{
	CTabPaneManager* pPaneManager = CTabPaneManager::GetInstance();
	if (pPaneManager)
	{
		pPaneManager->OpenOrCreatePane("Vegetation Editor");
	}
}

void Paint()
{
	EnsureVegetationEditorActive();
	CommandEvent("vegetation.paint").SendToKeyboardFocus();
}

void Erase()
{
	EnsureVegetationEditorActive();
	CommandEvent("vegetation.erase").SendToKeyboardFocus();
}

void Select()
{
	EnsureVegetationEditorActive();
	CommandEvent("vegetation.select").SendToKeyboardFocus();
}

void TryClearSelection()
{
	auto pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationSelectTool)))
	{
		auto pVegetationTool = static_cast<CVegetationSelectTool*>(pTool);
		pVegetationTool->ClearThingSelection();
	}
}

void ClearVegetationObjects()
{
	CUndo undo("Vegetation Clear");
	auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	const auto selectedObjects = pVegetationMap->GetSelectedObjects();
	if (selectedObjects.empty())
	{
		return;
	}

	for (auto pVegetationObject : selectedObjects)
	{
		pVegetationMap->ClearVegetationObject(pVegetationObject);
	}

	// UpdateViews via not neccessary: GetIEditorImpl()->UpdateViews(eUpdateStatObj);
	TryClearSelection();
}

void ScaleVegetationObjects()
{
	CUndo undo("Vegetation Scale");
	auto ok = false;
	auto scale = QInputDialog::getDouble(nullptr, CVegetationEditor::tr("Scale selected object(s)"), CVegetationEditor::tr("Scale factor"), 1, 0, 300, 2, &ok);
	if (!ok)
	{
		return;
	}

	auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	const auto selectedObjects = pVegetationMap->GetSelectedObjects();
	for (auto pVegetationObject : selectedObjects)
	{
		pVegetationMap->ScaleObjectInstances(pVegetationObject, scale);
	}

	// ViewUpdate not necessary
}

void RotateVegetationObjectsRandomly()
{
	CUndo undo("Vegetation Random Rotate");
	auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	const auto selectedObjects = pVegetationMap->GetSelectedObjects();
	for (auto pVegetationObject : selectedObjects)
	{
		pVegetationMap->RandomRotateInstances(pVegetationObject);
	}
}

void ClearVegetationObjectsRotations()
{
	CUndo undo("Vegetation Clear Rotate");
	auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	const auto selectedObjects = pVegetationMap->GetSelectedObjects();
	for (auto pVegetationObject : selectedObjects)
	{
		pVegetationMap->ClearRotateInstances(pVegetationObject);
	}
}

void MergeVegetationObjects()
{
	CUndo undo("Vegetation Merge");
	auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	const auto selectedObjects = pVegetationMap->GetSelectedObjects();
	if (selectedObjects.size() < 2)
	{
		CQuestionDialog::SWarning(CVegetationEditor::tr("Merge Vegetation Objects"), CVegetationEditor::tr("Select 2 vegetation objects or more."));
		return;
	}

	pVegetationMap->MergeVegetationObjects(selectedObjects);
}

void RemoveDuplicatedVegetation()
{
	auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	pVegetationMap->RemoveDuplVegetation();
}

void ImportObjectsFromXml(const char* xmlFile)
{
	CUndo undo("Import Vegetation");
	auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	pVegetationMap->ImportObjectsFromXml(xmlFile);
}

void ExportObjectsToXml(const char* xmlFile)
{
	auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	pVegetationMap->ExportObjectsToXml(xmlFile);
}

// Old tool offered this function, although it was not available to the user
// in any form (e.g. as a menu entry)
void ClearMask(const char* maskFile)
{
	auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	pVegetationMap->ClearMask(maskFile);
}
}

struct CVegetationEditor::SImplementation : public IEditorNotifyListener
{
	struct SVegetationBrush
	{
		SVegetationBrush()
			: m_pVegetationPaintTool(nullptr)
		{
			m_varObject.AddVariable(m_radius, "Radius", functor(*this, &SVegetationBrush::OnRadiusChange));
			m_radius->SetLimits(CVegetationPaintTool::GetMinBrushRadius(), CVegetationPaintTool::GetMaxBrushRadius(), true, true);
			m_radius = 1.0f;
		}

		void SetVegetationPaintTool(CVegetationPaintTool* pPaintTool)
		{
			m_pVegetationPaintTool = pPaintTool;
			if (HasValidVegetationPaintTool())
			{
				m_pVegetationPaintTool->SetBrushRadius(m_radius);
			}
		}

		void OnRadiusChange(IVariable*)
		{
			if (HasValidVegetationPaintTool())
			{
				m_pVegetationPaintTool->SetBrushRadius(m_radius);
			}
		}

		bool HasValidVegetationPaintTool() const
		{
			return m_pVegetationPaintTool;
		}

		void UpdateRadius()
		{
			if (HasValidVegetationPaintTool())
			{
				m_radius = m_pVegetationPaintTool->GetBrushRadius();
			}
		}

		CVarObject& GetVarObject() { return m_varObject; }

	private:
		CVegetationPaintTool* m_pVegetationPaintTool;
		CSmartVariable<float> m_radius;

		CVarObject            m_varObject;
	};

	struct SVegetationSerializer
	{
		SVegetationSerializer(SVegetationBrush& brush, CVegetationObject* pObject = nullptr)
			: m_brush(brush)
			, m_pVegetationObject(pObject)
		{}

		const CVegetationObject* GetVegetationObject() const
		{
			return m_pVegetationObject;
		}

		void YASLI_SERIALIZE_METHOD(Serialization::IArchive& ar)
		{
			if (m_brush.HasValidVegetationPaintTool())
			{
				ar(m_brush.GetVarObject(), "properties", "Brush");
			}

			if (m_pVegetationObject)
			{
				if (m_pVegetationObject->GetVarObject())
				{
					ar(*m_pVegetationObject->GetVarObject(), "properties", "Vegetation");
				}

				if (ar.isInput())
				{
					signalVegetationObjectChanged(m_pVegetationObject);
				}
			}
		}

		CCrySignal<void(CVegetationObject*)> signalVegetationObjectChanged;

	private:
		SVegetationBrush&  m_brush;
		CVegetationObject* m_pVegetationObject;
	};

	struct SActions
	{
		SActions(CVegetationEditor* pParentEditor)
		{
			// File menu
			pParentEditor->AddToMenu(CEditor::MenuItems::FileMenu);
			auto pFileMenu = pParentEditor->GetMenu(CEditor::MenuItems::FileMenu);
			SetupFileMenu(pFileMenu);

			// edit menu
			pParentEditor->AddToMenu(CEditor::MenuItems::EditMenu);
			auto pEditMenu = pParentEditor->GetMenu(CEditor::MenuItems::EditMenu);
			SetupEditMenu(pEditMenu);

			CAbstractMenu* pToolsMenu = pParentEditor->GetRootMenu()->CreateMenu(tr("Tools"), 0);
			SetupToolsMenu(pToolsMenu, pParentEditor);

			pParentEditor->AddToMenu(CEditor::MenuItems::ViewMenu);
			auto pViewMenu = pParentEditor->GetMenu(CEditor::MenuItems::ViewMenu);
			SetupViewMenu(pViewMenu, pParentEditor);
		}

		void SetupFileMenu(CAbstractMenu* pFileMenu)
		{
			pAddNewGroupAction = pFileMenu->CreateAction(tr("Add Group"));
			pAddNewGroupAction->setIcon(CryIcon("icons:Vegetation/Tools_Add_Vegetation_Category.ico"));
			toolBarActions.push_back(pAddNewGroupAction);

			pAddNewVegetationObjectAction = pFileMenu->CreateAction(tr("Add Object"));
			pAddNewVegetationObjectAction->setIcon(CryIcon("icons:Vegetation/Tools_Add_Vegetation_Object.ico"));
			toolBarActions.push_back(pAddNewVegetationObjectAction);

			const int sec = pFileMenu->GetNextEmptySection();
			pImportAction = pFileMenu->CreateAction(tr("Import Vegetation Objects"), sec);
			pExportAction = pFileMenu->CreateAction(tr("Export Vegetation Objects"), sec);
		}

		void SetupEditMenu(CAbstractMenu* pEditMenu)
		{
			pRemoveAction = pEditMenu->CreateAction(tr("Remove"));
			pRemoveAction->setIcon(CryIcon("icons:Vegetation/Tools_Clear_Vegetation.ico"));
			toolBarActions.push_back(pRemoveAction);

			auto pGroupMenu = pEditMenu->CreateMenu(tr("Group"));
			pRenameGroupAction = pGroupMenu->CreateAction(tr("Rename Group"));
			pMoveSelectionToGroupAction = pGroupMenu->CreateAction(tr("Move Selection to Group"));

			auto pVegetationObjectMenu = pEditMenu->CreateMenu(tr("Objects"));
			pReplaceVegetationObjectAction = pVegetationObjectMenu->CreateAction(tr("Replace Vegetation Object"));
			pCloneVegetationObjectAction = pVegetationObjectMenu->CreateAction(tr("Duplicate"));
			pCloneVegetationObjectAction->setIcon(CryIcon("icons:Vegetation/Tools_Duplicate_Vegetation_Object.ico"));
			toolBarActions.push_back(pCloneVegetationObjectAction);
		}

		void SetupToolsMenu(CAbstractMenu* pToolsMenu, CVegetationEditor* pParentEditor)
		{
			pToolsMenu->AddCommandAction(pParentEditor->GetAction_Deprecated("vegetation.paint"));
			pToolsMenu->AddCommandAction(pParentEditor->GetAction_Deprecated("vegetation.erase"));
			pToolsMenu->AddCommandAction(pParentEditor->GetAction_Deprecated("vegetation.select"));

			const int section = pToolsMenu->GetNextEmptySection();

			pClearAction = pToolsMenu->CreateAction(tr("Clear selected Vegetation Objects"), section);
			pScaleAction = pToolsMenu->CreateAction(tr("Scale selected Vegetation Objects"), section);
			pRotateRndAction = pToolsMenu->CreateAction(tr("Randomly rotate all instances"), section);
			pClearRotationAction = pToolsMenu->CreateAction(tr("Clear rotation"), section);
			pMergeObjectsAction = pToolsMenu->CreateAction(tr("Merge Vegetation Objects"), section);
			pRemoveDuplicatedVegetation = pToolsMenu->CreateAction(tr("Remove duplicated Vegetation"), section);
		}

		void SetupViewMenu(CAbstractMenu* pViewMenu, CVegetationEditor* pParentEditor)
		{
			pShowPreviewAction = pViewMenu->CreateAction(tr("Show Preview"));
			pShowPreviewAction->setCheckable(true);

			bool bShowPreview = true;
			pParentEditor->GetProperty(s_showPreviewPropertyName, bShowPreview);
			pShowPreviewAction->setChecked(bShowPreview);
		}

		QAction*              pImportAction;
		QAction*              pExportAction;
		QAction*              pAddNewGroupAction;
		QAction*              pRenameGroupAction;
		QAction*              pMoveSelectionToGroupAction;
		QAction*              pAddNewVegetationObjectAction;
		QAction*              pReplaceVegetationObjectAction;
		QAction*              pCloneVegetationObjectAction;
		QAction*              pClearAction;
		QAction*              pScaleAction;
		QAction*              pRotateRndAction;
		QAction*              pClearRotationAction;
		QAction*              pMergeObjectsAction;
		QAction*              pRemoveDuplicatedVegetation;
		QAction*              pRemoveAction;
		QAction*              pShowPreviewAction;

		std::vector<QAction*> toolBarActions;
	};

	struct SUi
	{
		SUi(QAbstractItemModel* pVegetationModel, const std::vector<QAction*>& toolBarActions)
		{
			pContentLayout = new QVBoxLayout();

			auto pToolBar = CreateToolBar(toolBarActions);
			pContentLayout->addWidget(pToolBar);

			pVegetationTreeView = CreateVegetationTreeView(pVegetationModel);

			auto pToolButtonPanel = CreateToolButtonPanel();

			pPropertyTree = CreatePropertyTree();

			auto pPropertyLayout = new QVBoxLayout;
			pPropertyLayout->setContentsMargins(0, 0, 0, 0);
			pPropertyLayout->setSpacing(0);
			pPropertyLayout->addWidget(pToolButtonPanel);
			pPropertyLayout->addWidget(pPropertyTree);

			// extra widget because splitter only supports adding widgets, not layouts
			auto pPropertyWidget = new QWidget;
			pPropertyWidget->setLayout(pPropertyLayout);

			pPreviewWidget = CreatePreviewWidget();

			pSplitter = new QSplitter(Qt::Vertical);
			pSplitter->addWidget(pVegetationTreeView);
			pSplitter->addWidget(pPropertyWidget);
			pSplitter->addWidget(pPreviewWidget);
			pContentLayout->addWidget(pSplitter);

			pSplitter->setStretchFactor(0, 0);
			pSplitter->setStretchFactor(1, 1);
			pSplitter->setStretchFactor(2, 1);

			pStatusLabel = new QLabel();
			auto pStatusBar = new QStatusBar();
			pStatusBar->addWidget(pStatusLabel);
			pContentLayout->addWidget(pStatusBar);
		}

		static QToolBar* CreateToolBar(const std::vector<QAction*>& toolBarActions)
		{
			auto pToolBar = new QToolBar();
			for (auto pAction : toolBarActions)
			{
				pToolBar->addAction(pAction);
			}

			return pToolBar;
		}

		static CVegetationTreeView* CreateVegetationTreeView(QAbstractItemModel* pModel)
		{
			auto pVegetationTreeView = new CVegetationTreeView();
			auto pFilterModel = new QSortFilterProxyModel();
			pFilterModel->setSourceModel(pModel);
			pVegetationTreeView->setModel(pFilterModel);

			// show only name column by default
			for (int i = 1; i < pModel->columnCount(); ++i)
			{
				pVegetationTreeView->SetColumnVisible(i, false);
			}

			return pVegetationTreeView;
		}

		static QEditToolButtonPanel* CreateToolButtonPanel()
		{
			auto pToolButtonPanel = new QEditToolButtonPanel(QEditToolButtonPanel::LayoutType::Horizontal);
			pToolButtonPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
			const auto paintToolButtonInfo = CVegetationPaintTool::CreatePaintToolButtonInfo();
			const auto eraseToolButtonInfo = CVegetationEraseTool::CreateEraseToolButtonInfo();
			const auto selectToolButtonInfo = CVegetationSelectTool::CreateSelectToolButtonInfo();
			pToolButtonPanel->AddButton(paintToolButtonInfo);
			pToolButtonPanel->AddButton(eraseToolButtonInfo);
			pToolButtonPanel->AddButton(selectToolButtonInfo);

			return pToolButtonPanel;
		}

		static QPropertyTreeLegacy* CreatePropertyTree()
		{
			auto pPropertyTree = new QPropertyTreeLegacy();
			pPropertyTree->setValueColumnWidth(0.6f);
			pPropertyTree->setSizeToContent(false);
			pPropertyTree->setAutoRevert(false);
			pPropertyTree->setAggregateMouseEvents(false);
			pPropertyTree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

			return pPropertyTree;
		}

		static QPreviewWidget* CreatePreviewWidget()
		{
			auto pPreviewWidget = new QPreviewWidget;
			pPreviewWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
			// initially there is no selection
			pPreviewWidget->hide();

			return pPreviewWidget;
		}

		// Using QSortFilterProxyModel, we need to unmap indices received from selection before usage.
		QModelIndex GetSelectedSourceIndex() const
		{
			QAbstractProxyModel* pProxy = static_cast<QAbstractProxyModel*>(pVegetationTreeView->model());
			return pProxy->mapToSource(pVegetationTreeView->selectionModel()->currentIndex());
		}

		QModelIndexList GetAllSelectedSourceRows() const
		{
			// Remap from proxy model indices to indices for operation
			QAbstractProxyModel* pProxy = static_cast<QAbstractProxyModel*>(pVegetationTreeView->model());

			auto selectedRows = pVegetationTreeView->selectionModel()->selectedRows();
			for (auto& row : selectedRows)
			{
				row = pProxy->mapToSource(row);
			}

			return selectedRows;
		}

		void MapFromSourceIndices(QModelIndexList& indices)
		{
			QAbstractProxyModel* pProxy = static_cast<QAbstractProxyModel*>(pVegetationTreeView->model());
			for (auto& index : indices)
			{
				index = pProxy->mapFromSource(index);
			}
		}

		QVBoxLayout*         pContentLayout;
		CVegetationTreeView* pVegetationTreeView;
		QPropertyTreeLegacy*       pPropertyTree;
		QPreviewWidget*      pPreviewWidget;
		QSplitter*           pSplitter;
		QLabel*              pStatusLabel;
	};

	SImplementation(CVegetationEditor* pEditor)
		: m_pEditor(pEditor)
		, m_pVegetationModel(new CVegetationModel())
		, m_actions(m_pEditor)
		, m_ui(m_pVegetationModel, m_actions.toolBarActions)
		, m_splitterState(QtUtil::ToQByteArray(m_pEditor->GetProperty(s_previewSplitterStatePropertyName)))
	{
		SetupControls();

		GetIEditorImpl()->RegisterNotifyListener(this);
		GetIEditorImpl()->GetLevelEditorSharedState()->signalEditToolChanged.Connect(this, &SImplementation::OnEditToolChanged);
	}

	~SImplementation()
	{
		GetIEditorImpl()->UnregisterNotifyListener(this);
		GetIEditorImpl()->GetLevelEditorSharedState()->signalEditToolChanged.DisconnectObject(this);
	}

	void SetupControls()
	{
		SetupVegetationTreeView();
		SetupSplitter();
		SetupStatusLabel();
		SetupActions();

		connect(m_ui.pVegetationTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, [this]()
		{
			int selectionCount = m_ui.pVegetationTreeView->selectionModel()->selectedRows().size();
			// These actions can only be performed when there's a single item selected
			m_actions.pReplaceVegetationObjectAction->setEnabled(selectionCount == 1);
			m_actions.pCloneVegetationObjectAction->setEnabled(selectionCount == 1);
		});

		connect(m_ui.pVegetationTreeView->model(), &QAbstractItemModel::modelAboutToBeReset, [this]()
		{
			m_ui.pVegetationTreeView->selectionModel()->clearSelection();
		});
	}

	void SetupVegetationTreeView()
	{
		QObject::connect(m_ui.pVegetationTreeView, &QTreeView::customContextMenuRequested, [=]
		{
			auto pMenu = CreateContextMenu();
			pMenu->exec(QCursor::pos());
		});

		auto pSelectionModel = m_ui.pVegetationTreeView->selectionModel();
		QObject::connect(pSelectionModel, &QItemSelectionModel::selectionChanged, [this, pSelectionModel]
		{
			static auto bBlockSignal = false;
			if (bBlockSignal)
			{
			  return;
			}

			const auto currentSelection = m_ui.GetAllSelectedSourceRows();
			const auto selectFlags = QItemSelectionModel::Select | QItemSelectionModel::Rows;
			bBlockSignal = true;
			for (const auto& currentIndex : currentSelection)
			{
			  if (m_pVegetationModel->IsGroup(currentIndex))
			  {
			    const auto pGroupItem = m_pVegetationModel->GetGroup(currentIndex);
			    for (int i = 0; i < pGroupItem->rowCount(); ++i)
			    {
			      pSelectionModel->select(m_pVegetationModel->index(i, 0, currentIndex), selectFlags);
					}
				}
			  else if (pSelectionModel->isSelected(currentIndex.parent()))
			  {
			    // do not deselect vegetation objects when their groups are still selected
			    pSelectionModel->select(currentIndex, selectFlags);
				}
			}
			bBlockSignal = false;

			m_pVegetationModel->Select(currentSelection);

			const auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
			const auto selectedObjects = pVegetationMap->GetSelectedObjects();
			for (auto pVegetationObject : selectedObjects)
			{
			  pVegetationObject->UpdateTerrainLayerVariables();
			}

			UpdateVegetationSerializers(selectedObjects);
		});

		QObject::connect(m_ui.pVegetationTreeView, &QTreeView::doubleClicked, [this]()
		{
			QModelIndex currSelection = m_ui.GetSelectedSourceIndex();
			auto pVegetationObject = m_pVegetationModel->GetVegetationObject(currSelection);
			if (!pVegetationObject)
			{
			  return;
			}

			ActivatePlaceTool(pVegetationObject);
		});

		QObject::connect(m_ui.pVegetationTreeView, &CVegetationTreeView::dragStarted, [this]()
		{
			auto dragRows = m_ui.GetAllSelectedSourceRows();
			if (std::any_of(dragRows.cbegin(), dragRows.cend(), [this](const QModelIndex& rowIndex) { return m_pVegetationModel->IsGroup(rowIndex); }))
			{
			  return;
			}
			ActivatePlaceTool(nullptr);
		});
	}

	void SetupSplitter()
	{
		if (!m_splitterState.isEmpty())
		{
			m_ui.pSplitter->restoreState(m_splitterState);
		}

		connect(m_ui.pSplitter, &QSplitter::splitterMoved, [this]
		{
			m_splitterState = m_ui.pSplitter->saveState();
			const auto bytes = QtUtil::ToQVariant(m_splitterState);
			m_pEditor->SetProperty(s_previewSplitterStatePropertyName, bytes);
		});
	}

	void SetupStatusLabel()
	{
		QObject::connect(m_pVegetationModel, &CVegetationModel::InfoDataChanged, [this](int objectCount, int instanceCount, int textureUsage)
		{
			auto infoText = QString("Total Objects: %1  Total Instances: %2  Texture usage: %3").arg(objectCount).arg(instanceCount).arg(textureUsage);
			m_ui.pStatusLabel->setText(infoText);
		});
	}

	void SetupActions()
	{
		QObject::connect(m_actions.pRemoveAction, &QAction::triggered, [this]
		{
			RemoveTreeSelection();
		});

		SetupFileActions();
		SetupGroupActions();
		SetupVegetationObjectActions();
		SetupToolActions();
		SetupViewActions();
	}

	void SetupFileActions()
	{
		using namespace Private_VegetationEditor;

		QObject::connect(m_actions.pImportAction, &QAction::triggered, [this]
		{
			const auto levelFolder = GetIEditorImpl()->GetLevelFolder().GetString();

			CSystemFileDialog::RunParams runParams;
			runParams.title = tr("Import Vegetation Objects");
			runParams.initialDir = levelFolder;
			runParams.extensionFilters << CExtensionFilter(tr("Vegetation Objects (veg)"), "veg");
			runParams.extensionFilters << CExtensionFilter(QObject::tr("All Files"), "*");

			const QString importFileName = CSystemFileDialog::RunImportFile(runParams, m_pEditor->window());
			if (!importFileName.isEmpty())
			{
			  ImportObjectsFromXml(importFileName.toStdString().c_str());
			}
		});

		QObject::connect(m_actions.pExportAction, &QAction::triggered, [this]
		{
			const auto levelFolder = GetIEditorImpl()->GetLevelFolder().GetString();

			CSystemFileDialog::RunParams runParams;
			runParams.title = tr("Export Vegetation Objects");
			runParams.initialDir = levelFolder;
			runParams.extensionFilters << CExtensionFilter(tr("Vegetation Objects (veg)"), "veg");

			CVegetationMap* vegMap = GetIEditorImpl()->GetVegetationMap();
			const auto selectedObjects = vegMap->GetSelectedObjects();

			if (selectedObjects.size() > 0)
			{
			  const QString exportFileName = CSystemFileDialog::RunExportFile(runParams, m_pEditor->window());
			  if (!exportFileName.isEmpty())
			  {
			    ExportObjectsToXml(exportFileName.toStdString().c_str());
				}
			}
			else
			{
			  CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "You need to select object(s) to export");
			}
		});
	}

	void SetupGroupActions()
	{
		QObject::connect(m_actions.pAddNewGroupAction, &QAction::triggered, [this]
		{
			m_pVegetationModel->AddGroup();
		});

		QObject::connect(m_actions.pRenameGroupAction, &QAction::triggered, [this]
		{
			auto currentGroupIndex = m_ui.GetSelectedSourceIndex();
			if (!m_pVegetationModel->IsGroup(currentGroupIndex))
			{
			  return;
			}
			auto newGroupName = QInputDialog::getText(nullptr, tr("Rename Group"), tr("Input group name:"));
			if (newGroupName.isNull())
			{
			  return;
			}
			m_pVegetationModel->RenameGroup(currentGroupIndex, newGroupName);
		});

		QObject::connect(m_actions.pMoveSelectionToGroupAction, &QAction::triggered, [this]
		{
			auto pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
			if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationSelectTool)))
			{
			  auto pVegetationTool = static_cast<CVegetationSelectTool*>(pTool);
			  if (pVegetationTool->GetCountSelectedInstances() <= 0)
			  {
			    return;
				}
			  auto newGroupName = QInputDialog::getText(nullptr, tr("Rename Group"), tr("Input group name:"));
			  if (newGroupName.isNull())
			  {
			    return;
				}
			  m_pVegetationModel->MoveInstancesToGroup(newGroupName, pVegetationTool->GetSelectedInstances());
			}
		});
	}

	void SetupVegetationObjectActions()
	{
		QObject::connect(m_actions.pAddNewVegetationObjectAction, &QAction::triggered, [this]
		{
			AddVegetationObject();
		});

		QObject::connect(m_actions.pReplaceVegetationObjectAction, &QAction::triggered, [this]
		{
			auto currentObjectIndex = m_ui.GetSelectedSourceIndex();
			if (!m_pVegetationModel->IsVegetationObject(currentObjectIndex))
			{
			  return;
			}

			const CAsset* pAsset = CAssetBrowserDialog::OpenSingleAssetForTypes({ "Mesh" });
			if (!pAsset)
			{
			  return;
			}

			CUndo undo("Replace Vegetation Object");
			m_pVegetationModel->ReplaceVegetationObject(currentObjectIndex, QtUtil::ToQString(pAsset->GetFile(0)));

			const auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
			const auto selectedObjects = pVegetationMap->GetSelectedObjects();
			UpdateVegetationSerializers(selectedObjects);
		});

		QObject::connect(m_actions.pCloneVegetationObjectAction, &QAction::triggered, [this]
		{
			CloneVegetationObject();
		});
	}

	void SetupToolActions()
	{
		using namespace Private_VegetationEditor;

		QObject::connect(m_actions.pClearAction, &QAction::triggered, [this]
		{
			ClearVegetationObjects();
		});

		QObject::connect(m_actions.pScaleAction, &QAction::triggered, [this]
		{
			ScaleVegetationObjects();
		});

		QObject::connect(m_actions.pRotateRndAction, &QAction::triggered, [this]
		{
			RotateVegetationObjectsRandomly();
		});

		QObject::connect(m_actions.pClearRotationAction, &QAction::triggered, [this]
		{
			ClearVegetationObjectsRotations();
		});

		QObject::connect(m_actions.pMergeObjectsAction, &QAction::triggered, [this]
		{
			MergeVegetationObjects();
		});

		QObject::connect(m_actions.pRemoveDuplicatedVegetation, &QAction::triggered, [this]
		{
			RemoveDuplicatedVegetation();
		});
	}

	void SetupViewActions()
	{
		connect(m_actions.pShowPreviewAction, &QAction::toggled, [this](bool bIsChecked)
		{
			m_pEditor->SetProperty(s_showPreviewPropertyName, bIsChecked);
			UpdatePreviewVisibility();
		});
	}

	QMenu* CreateContextMenu() const
	{
		const auto selectedRows = m_ui.GetAllSelectedSourceRows();

		bool isGroupSelected = false, isObjectSelected = false;
		for (const auto& selectedRow : selectedRows)
		{
			if (m_pVegetationModel->IsVegetationObject(selectedRow))
			{
				isObjectSelected = true;
			}
			else if (m_pVegetationModel->IsGroup(selectedRow))
			{
				isGroupSelected = true;
			}
		}

		auto pContextMenu = new QMenu(m_pEditor);

		pContextMenu->addAction(m_actions.pAddNewVegetationObjectAction);
		if (isGroupSelected && !isObjectSelected)
		{
			pContextMenu->addAction(m_actions.pRenameGroupAction);
		}
		else if (isObjectSelected && !isGroupSelected)
		{
			pContextMenu->addAction(m_actions.pReplaceVegetationObjectAction);
			pContextMenu->addAction(m_actions.pCloneVegetationObjectAction);
		}
		else if (!isObjectSelected && !isGroupSelected)
		{
			pContextMenu->addAction(m_actions.pAddNewGroupAction);
		}

		if (isGroupSelected || isObjectSelected)
		{
			if (!pContextMenu->isEmpty())
			{
				pContextMenu->addSeparator();
			}
			pContextMenu->addAction(m_actions.pClearAction);
			pContextMenu->addAction(m_actions.pScaleAction);
			pContextMenu->addAction(m_actions.pRotateRndAction);
			pContextMenu->addAction(m_actions.pClearRotationAction);
			pContextMenu->addAction(m_actions.pMergeObjectsAction);
			pContextMenu->addAction(m_actions.pRemoveDuplicatedVegetation);
			pContextMenu->addSeparator();
			pContextMenu->addAction(m_actions.pRemoveAction);
		}

		return pContextMenu;
	}

	void OnEditToolChanged()
	{
		m_actions.pMoveSelectionToGroupAction->setVisible(false);

		auto pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
		if (pTool)
		{
			if (pTool->IsKindOf(RUNTIME_CLASS(CVegetationPaintTool)))
			{
				auto pPaintTool = static_cast<CVegetationPaintTool*>(pTool);
				pPaintTool->signalBrushRadiusChanged.Connect(this, &SImplementation::UpdateBrushRadius);
			}
			else if (pTool->IsKindOf(RUNTIME_CLASS(CVegetationSelectTool)))
			{
				auto pSelectTool = static_cast<CVegetationSelectTool*>(pTool);
				pSelectTool->signalSelectionChanged.Connect(this, &SImplementation::InstanceSelected);
				m_actions.pMoveSelectionToGroupAction->setVisible(true);
			}
		}
		UpdatePaintBrushSerializer();
	}

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override
	{
		switch (event)
		{
		case eNotify_OnBeginSceneOpen: // Level Open
		case eNotify_OnBeginNewScene:  // New Level
			m_pVegetationModel->BeginResetOnLevelChange();
			// Reset property tree/preview widget
			ResetVegetationSerializers();
			break;

		case eNotify_OnEndSceneOpen: // Level Open
		case eNotify_OnEndNewScene:  // New Level
			m_pVegetationModel->EndResetOnLevelChange();
			break;
		case eNotify_OnClearLevelContents:
			m_pVegetationModel->Clear();
			break;
		}
	}

	void UpdateVegetationSerializers(const std::vector<CVegetationObject*>& vegetationObjects)
	{
		if (vegetationObjects.empty())
		{
			// no objects -> deselection
			ResetVegetationSerializers();
			return;
		}

		m_vegetationSerializers.clear();
		m_serializers.clear();

		// rely on memory reservation to avoid reallocations and invalidations of pointers
		m_vegetationSerializers.reserve(vegetationObjects.size());
		m_serializers.reserve(vegetationObjects.size());
		for (auto pObject : vegetationObjects)
		{
			AddVegetationSerializer(pObject);
			auto& vegetationSerializer = m_vegetationSerializers.back();
			vegetationSerializer.signalVegetationObjectChanged.Connect(m_pVegetationModel, &CVegetationModel::UpdateVegetationObject);
		}

		UpdatePropertyTreePreviewContainer();
	}

	void ResetVegetationSerializers()
	{
		m_vegetationSerializers.clear();
		m_serializers.clear();

		// if paint tool is active the brush has to be serializable
		auto pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationPaintTool)))
		{
			AddVegetationSerializer();
		}

		UpdatePropertyTreePreviewContainer();
	}

	void UpdatePaintBrushSerializer()
	{
		CVegetationPaintTool* pPaintTool = nullptr;
		auto pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationPaintTool)))
		{
			pPaintTool = static_cast<CVegetationPaintTool*>(pTool);
		}

		// If paint tool is active, the brush has to be serializable even if no objects are selected
		// If no paint tool is present the brush property tree is removed
		m_vegetationBrush.SetVegetationPaintTool(pPaintTool);
		if (m_vegetationSerializers.empty() && pPaintTool)
		{
			CRY_ASSERT(m_serializers.empty());
			AddVegetationSerializer();
		}

		UpdatePropertyTreePreviewContainer();
	}

	void AddVegetationSerializer(CVegetationObject* pObject = nullptr)
	{
		m_vegetationSerializers.emplace_back(m_vegetationBrush, pObject);
		m_serializers.emplace_back(m_vegetationSerializers.back());
	}

	void ActivatePlaceTool(CVegetationObject* pVegetationObject)
	{
		auto pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
		auto pPlaceTool = [pTool]() -> CVegetationPlaceTool*
											{
												if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationPlaceTool)))
												{
													return static_cast<CVegetationPlaceTool*>(pTool);
												}

												auto pPlaceTool = new CVegetationPlaceTool();
												GetIEditorImpl()->GetLevelEditorSharedState()->SetEditTool(pPlaceTool);
												return pPlaceTool;
											} ();

		pPlaceTool->SelectObjectToCreate(pVegetationObject);
	}

	void AbortPlaceTool()
	{
		auto pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationPlaceTool)))
		{
			pTool->Abort();
		}
	}

	void RemoveTreeSelection()
	{
		// Abort placing of just removed vegetation objects
		AbortPlaceTool();

		CUndo undo("Vegetation Item Remove");
		auto selectedRows = m_ui.GetAllSelectedSourceRows();
		m_pVegetationModel->RemoveItems(selectedRows);
		// Reset property tree/preview widget
		ResetVegetationSerializers();
	}

	void AddVegetationObject()
	{
		const std::vector<CAsset*> assets = CAssetBrowserDialog::OpenMultipleAssetsForTypes({ "Mesh" });
		if (assets.empty())
		{
			return;
		}

		auto pSelectionModel = m_ui.pVegetationTreeView->selectionModel();
		auto currentIndex = m_ui.GetSelectedSourceIndex();

		auto pGroupItem = m_pVegetationModel->GetGroup(currentIndex);
		const auto oldObjectCount = pGroupItem ? pGroupItem->rowCount() : -1;
		CVegetationObject* pVegObject = nullptr;

		CUndo undo("Add Vegetation Object(s)");
		for (const CAsset* pAsset : assets)
		{
			pVegObject = m_pVegetationModel->AddVegetationObject(currentIndex, QtUtil::ToQString(pAsset->GetFile(0)));
		}

		if (!pGroupItem && pVegObject)
		{
			currentIndex = m_pVegetationModel->GetGroup(pVegObject);
			pGroupItem = m_pVegetationModel->GetGroup(currentIndex);
		}

		// when a vegetation object is added to a selected group it has to be selected
		// so that e.g. painting of the newly added object works properly
		if (pGroupItem)
		{
			for (int i = oldObjectCount; i < pGroupItem->rowCount(); ++i)
			{
				auto selectionIndex = m_pVegetationModel->index(i, 0, currentIndex);
				pSelectionModel->select(selectionIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);

				if (selectionIndex.isValid() && selectionIndex.parent().isValid())
				{
					m_ui.pVegetationTreeView->expand(selectionIndex.parent());
				}
			}
		}
	}

	void CloneVegetationObject()
	{
		auto currentObjectIndex = m_ui.GetSelectedSourceIndex();
		if (m_pVegetationModel->IsGroup(currentObjectIndex))
		{
			return;
		}

		CUndo undo("Clone Vegetation Object");
		m_pVegetationModel->CloneObject(currentObjectIndex);
	}

	void SelectAll()
	{
		const auto rowCount = m_pVegetationModel->rowCount();
		auto pSelectionModel = m_ui.pVegetationTreeView->selectionModel();
		for (int i = 0; i < rowCount; ++i)
		{
			pSelectionModel->select(m_pVegetationModel->index(i, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
		}

		const auto selectedRows = m_ui.GetAllSelectedSourceRows();
		m_pVegetationModel->Select(selectedRows);
	}

	void UpdatePropertyTreePreviewContainer()
	{
		UpdatePropertyTree();

		const auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
		const auto selectedObjects = pVegetationMap->GetSelectedObjects();
		bool isPreviewVisible = m_actions.pShowPreviewAction->isChecked();
		QString objectFileName;
		// only show a preview if only a single object is selected
		if (selectedObjects.size() == 1)
		{
			const auto pVegetationObject = selectedObjects.front();
			CRY_ASSERT(pVegetationObject);
			if (pVegetationObject)
			{
				objectFileName = pVegetationObject->GetFileName();
			}
		}
		else
		{
			isPreviewVisible = false;
		}

		m_ui.pPreviewWidget->LoadFile(objectFileName);
		m_ui.pPreviewWidget->setVisible(isPreviewVisible);
	}

	void UpdatePropertyTree()
	{
		// attach also calls revertNoninterrupting
		m_ui.pPropertyTree->attach(m_serializers);
		m_ui.pPropertyTree->expandAll();
	}

	void UpdatePreviewVisibility()
	{
		const auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
		const auto selectedObjects = pVegetationMap->GetSelectedObjects();
		m_ui.pPreviewWidget->setVisible(m_actions.pShowPreviewAction->isChecked() && (selectedObjects.size() == 1));
	}

	void UpdateBrushRadius()
	{
		m_vegetationBrush.UpdateRadius();
		UpdatePropertyTree();
	}

	void InstanceSelected()
	{
		auto pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
		if (!pTool || !pTool->IsKindOf(RUNTIME_CLASS(CVegetationSelectTool)))
		{
			return;
		}

		CVegetationTreeView* pTreeView = m_pEditor->findChild<CVegetationTreeView*>();
		if (!pTreeView)
		{
			return;
		}

		auto pVegetationTool = static_cast<CVegetationSelectTool*>(pTool);
		QModelIndexList indices = m_pVegetationModel->FindVegetationObjects(pVegetationTool->GetSelectedInstances());
		m_ui.MapFromSourceIndices(indices);

		QItemSelection selection;
		pTreeView->selectionModel()->clearSelection();
		QModelIndex firstParent;

		for (QModelIndex idx : indices)
		{
			if (!firstParent.isValid())
			{
				firstParent = idx.parent();
			}

			pTreeView->expand(idx.parent());
			selection.select(idx, idx);
		}

		pTreeView->selectionModel()->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);

		// Scroll to the last selected index
		if (!indices.empty())
		{
			auto lastIdx = indices.last();
			if (lastIdx.isValid())
			{
				pTreeView->scrollTo(lastIdx);
			}
		}
	}

	QLayout* GetContentLayout() const
	{
		return m_ui.pContentLayout;
	}

private:
	CVegetationEditor*                 m_pEditor;
	CVegetationModel*                  m_pVegetationModel;
	SActions                           m_actions;
	SUi                                m_ui;
	SVegetationBrush                   m_vegetationBrush;
	std::vector<SVegetationSerializer> m_vegetationSerializers;
	Serialization::SStructs            m_serializers; // use member to avoid copies for inspector event lambda
	QByteArray                         m_splitterState;

	static const QString               s_showPreviewPropertyName;
	static const QString               s_previewSplitterStatePropertyName;
};

const QString CVegetationEditor::SImplementation::s_showPreviewPropertyName = QStringLiteral("showPreview");
const QString CVegetationEditor::SImplementation::s_previewSplitterStatePropertyName = QStringLiteral("previewSplitterState");

REGISTER_VIEWPANE_FACTORY(CVegetationEditor, "Vegetation Editor", "Tools", true)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_VegetationEditor::Paint, vegetation, paint,
                                   CCommandDescription("Vegetation paint tool"))
REGISTER_EDITOR_UI_COMMAND_DESC(vegetation, paint, "Paint", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_VegetationEditor::Erase, vegetation, erase,
                                   CCommandDescription("Vegetation erase tool"))
REGISTER_EDITOR_UI_COMMAND_DESC(vegetation, erase, "Erase", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_VegetationEditor::Select, vegetation, select,
                                   CCommandDescription("Vegetation select tool"))
REGISTER_EDITOR_UI_COMMAND_DESC(vegetation, select, "Select", "", "", false)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_VegetationEditor::ClearVegetationObjects, vegetation, clear,
                                     "Clear selected vegetation objects",
                                     "vegetation.clear()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_VegetationEditor::ScaleVegetationObjects, vegetation, scale,
                                     "Scale selected vegetation objects",
                                     "vegetation.scale()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_VegetationEditor::RotateVegetationObjectsRandomly, vegetation, rotateRandomly,
                                     "Randomly rotate selected vegetation objects",
                                     "vegetation.rotateRandomly()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_VegetationEditor::ClearVegetationObjectsRotations, vegetation, clearRotations,
                                     "Clear selected vegetation objects rotations",
                                     "vegetation.clearRotations()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_VegetationEditor::MergeVegetationObjects, vegetation, merge,
                                     "Merge selected vegetation objects",
                                     "vegetation.merge()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_VegetationEditor::RemoveDuplicatedVegetation, vegetation, removeDuplicatedVegetation,
                                     "Removes duplicated vegetation object instances",
                                     "vegetation.removeDuplicatedVegetation()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_VegetationEditor::ImportObjectsFromXml, vegetation, importObjectsFromXml,
                                     "Imports vegetation objects from xml",
                                     "vegetation.importObjectsFromXml(str filename)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_VegetationEditor::ExportObjectsToXml, vegetation, exportObjectsToXml,
                                     "Exports vegetation objects from xml",
                                     "vegetation.exportObjectsFromXml(str filename)");

CVegetationEditor::CVegetationEditor(QWidget* parent)
	: CDockableEditor(parent)
	, p(new SImplementation(this))
{
	SetContent(p->GetContentLayout());
	RegisterActions();
	setAttribute(Qt::WA_DeleteOnClose);
}

CVegetationEditor::~CVegetationEditor()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetEditTool(nullptr);
}

void CVegetationEditor::RegisterActions()
{
	// Register general commands
	RegisterAction("general.new", &CVegetationEditor::OnNew);
	RegisterAction("general.delete", &CVegetationEditor::OnDelete);
	RegisterAction("general.duplicate", &CVegetationEditor::OnDuplicate);
	RegisterAction("general.select_all", &CVegetationEditor::OnSelectAll);
}

bool CVegetationEditor::OnNew()
{
	p->AddVegetationObject();
	return true;
}

bool CVegetationEditor::OnDelete()
{
	if (CQuestionDialog::SQuestion("Delete Objects", "Are you sure you want to delete selected vegetation objects?") != QDialogButtonBox::Yes)
	{
		return false;
	}

	p->RemoveTreeSelection();
	return true;
}

bool CVegetationEditor::OnDuplicate()
{
	p->CloneVegetationObject();
	return true;
}

bool CVegetationEditor::OnSelectAll()
{
	p->SelectAll();
	return true;
}

void CVegetationEditor::customEvent(QEvent* pEvent)
{
	CDockableEditor::customEvent(pEvent);

	if (!pEvent->isAccepted() && pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* pCommandEvent = static_cast<CommandEvent*>(pEvent);
		const string& command = pCommandEvent->GetCommand();

		if (command == "vegetation.paint")
		{
			GetIEditor()->GetLevelEditorSharedState()->SetEditTool(new CVegetationPaintTool());
		}
		else if (command == "vegetation.erase")
		{
			GetIEditor()->GetLevelEditorSharedState()->SetEditTool(new CVegetationEraseTool());
		}
		else if (command == "vegetation.select")
		{
			GetIEditor()->GetLevelEditorSharedState()->SetEditTool(new CVegetationSelectTool());
		}
	}
}
