// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "MainEditorWindow.h"

#include "Models/EffectAsset.h"
#include "Models/EffectAssetModel.h"

#include "Widgets/EffectViewWidget.h"

#include "Objects/SelectionGroup.h"
#include "Objects/ParticleEffectObject.h"
#include "Particles/ParticleManager.h"
#include "Particles/ParticleItem.h"

#include "ParticleAssetType.h"

#include "ViewManager.h"
#include "IUndoObject.h"
#include <EditorFramework/Inspector.h>

#include <AssetSystem/Asset.h>
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/EditableAsset.h>
#include <FileDialogs/EngineFileDialog.h>
#include <Controls/QuestionDialog.h>
#include <Controls/CurveEditorPanel.h>
#include <DragDrop.h>
#include <FilePathUtil.h>

#include <CrySerialization/IArchive.h>
#include <CrySerialization/IArchiveHost.h>
#include <CryString/CryPath.h>

#include <../../CryPlugins/CryDefaultEntities/Module/DefaultComponents/Effects/ParticleComponent.h>

#include <CurveEditor.h>
#include <CryIcon.h>

#include <IObjectManager.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QSplitter>

#include <QHeaderView>
#include <QToolBar>
#include <QAction>
#include <QDir>
#include <QInputDialog>
#include <QLabel>
#include <QTabWidget>
#include <QDir>

#include <QAdvancedPropertyTree.h>
#include <QCollapsibleFrame.h>

#define APPLICATION_USER_DIRECTORY      "/CryEngine"
#define APPLICATION_USER_STATE_FILENAME "EditorState.json"

namespace CryParticleEditor {

static const char* const s_szEffectAssetName = "Effect";
static const char* const s_szCurveEditorPanelName = "Curve Editor";
static const char* const s_szInspectorName = "Inspector";

static CCurveEditorPanel* CreateCurveEditorPanel(QWidget* pParent = nullptr)
{
	CCurveEditorPanel* const pCurveEditorPanel = new CCurveEditorPanel(pParent);
	CCurveEditor& curveEditor = pCurveEditorPanel->GetEditor();
	curveEditor.SetHandlesVisible(true);
	curveEditor.SetGridVisible(true);
	curveEditor.SetRulerVisible(false);
	curveEditor.SetTimeSliderVisible(false);
	curveEditor.SetAllowDiscontinuous(false);
	curveEditor.SetFitMargin(20);
	return pCurveEditorPanel;
}

///////////////////////////////////////////////////////////////////////
CParticleEditor::CParticleEditor()
	: CAssetEditor("Particles")
	, m_pEffectAssetModel(new CEffectAssetModel())
{
	setAttribute(Qt::WA_DeleteOnClose);
	setObjectName(GetEditorName());

	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setContentsMargins(1, 1, 1, 1);
	SetContent(pMainLayout);

	InitMenu();
	InitToolbar(pMainLayout);

	RegisterDockingWidgets();
}

void CParticleEditor::InitMenu()
{
	const CEditor::MenuItems items[] = {
		CEditor::MenuItems::FileMenu, CEditor::MenuItems::Save, CEditor::MenuItems::SaveAs,
		CEditor::MenuItems::EditMenu, CEditor::MenuItems::Undo,  CEditor::MenuItems::Redo,
		CEditor::MenuItems::Copy,     CEditor::MenuItems::Paste, CEditor::MenuItems::Delete
	};
	AddToMenu(items, sizeof(items) / sizeof(CEditor::MenuItems));

	{
		CAbstractMenu* const pFileMenu = GetMenu(CEditor::MenuItems::FileMenu);

		int sec;

		sec = pFileMenu->GetNextEmptySection();

		m_pReloadEffectMenuAction = pFileMenu->CreateAction(CryIcon("icons:General/Reload.ico"), tr("Reload Effect"), sec);
		connect(m_pReloadEffectMenuAction, &QAction::triggered, this, &CParticleEditor::OnReloadEffect);
		m_pReloadEffectMenuAction->setEnabled(false);

		m_pShowEffectOptionsMenuAction = pFileMenu->CreateAction(CryIcon("icons:General/Options.ico"), tr("Show Effect Options"), sec);
		connect(m_pShowEffectOptionsMenuAction, &QAction::triggered, this, &CParticleEditor::OnShowEffectOptions);
		m_pShowEffectOptionsMenuAction->setEnabled(false);

		sec = pFileMenu->GetNextEmptySection();

		auto action = pFileMenu->CreateAction(CryIcon("icons:General/Get_From_Selection.ico"), tr("Load from Selected Entity"), sec);
		connect(action, &QAction::triggered, this, &CParticleEditor::OnLoadFromSelectedEntity);

		action = pFileMenu->CreateAction(CryIcon("icons:General/Assign_To_Selection.ico"), tr("Apply to Selected Entity"), sec);
		connect(action, &QAction::triggered, this, &CParticleEditor::OnApplyToSelectedEntity);

		sec = pFileMenu->GetNextEmptySection();

		action = pFileMenu->CreateAction(CryIcon("icons:General/File_Import.ico"), tr("Import Selected Pfx1 Effect"), sec);
		connect(action, &QAction::triggered, this, &CParticleEditor::OnImportPfx1);
	}
}

void CParticleEditor::InitToolbar(QVBoxLayout* pWindowLayout)
{
#define ADD_BUTTON(slot, name, shortcut, icon)                           \
  { QAction* pAction = pToolBar->addAction(CryIcon(icon), QString());    \
    pAction->setToolTip(name);                                           \
    if (shortcut) pAction->setShortcut(tr(shortcut));                    \
    connect(pAction, &QAction::triggered, this, &CParticleEditor::slot); \
  }

	// TODO: make this better, the toolbars should behave like the main frame
	//			 toolbars, where you can move them around in dedicated areas!
	QHBoxLayout* pToolBarsLayout = new QHBoxLayout();
	pToolBarsLayout->setDirection(QBoxLayout::LeftToRight);
	pToolBarsLayout->setSizeConstraint(QLayout::SetMaximumSize);

	// Add Library Toolbar
	{
		QToolBar* pToolBar = new QToolBar("Library");

		pToolBar->addAction(GetAction("general.save"));

		pToolBar->addSeparator();
		ADD_BUTTON(OnLoadFromSelectedEntity, "Load from Selected Entity", 0, "icons:General/Get_From_Selection.ico");
		ADD_BUTTON(OnApplyToSelectedEntity, "Apply to Selected Entity", 0, "icons:General/Assign_To_Selection.ico");
		ADD_BUTTON(OnImportPfx1, "Import Selected Pfx1 Effect", 0, "icons:General/File_Import.ico");

		pToolBarsLayout->addWidget(pToolBar, 0, Qt::AlignLeft);
	}

	// Add Effect Toolbar
	{
		QToolBar* pToolBar = new QToolBar("Effect");

		QMenu* pTemplatesMenu = new QMenu();

		pToolBar->addSeparator();
		ADD_BUTTON(OnReloadEffect, "Reload Effect", 0, "icons:General/Reload.ico")
		ADD_BUTTON(OnShowEffectOptions, "Show Effect Options", 0, "icons:General/Options.ico")

		pToolBarsLayout->addWidget(pToolBar, 0, Qt::AlignLeft);
		QSpacerItem* pSpacer = new QSpacerItem(20, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
		pToolBarsLayout->addSpacerItem(pSpacer);

		m_pEffectToolBar = pToolBar;
		m_pEffectToolBar->hide();
	}

	pWindowLayout->addLayout(pToolBarsLayout);
}

void CParticleEditor::RegisterDockingWidgets()
{
	EnableDockingSystem();

	RegisterDockableWidget(s_szEffectAssetName, [&]() { return CreateEffectAssetWidget(); }, false, false);
	RegisterDockableWidget(s_szCurveEditorPanelName, [&]() { return CreateCurveEditorPanel(this); }, false, false);

	// #TODO: The inspector is a unique panel for practical reasons. Right now, there is not code
	// to keep two instances of the inspector in sync when either one of them changes.
	// Similar to the object properties, the property trees should be reverted when a feature item
	// signals SignalInvalidated.
	RegisterDockableWidget(s_szInspectorName, [&]() { return new CInspector(this); }, true, false);
}

CEffectAssetWidget* CParticleEditor::CreateEffectAssetWidget()
{
	CEffectAssetWidget* const pEffectAssetWidget = new CEffectAssetWidget(m_pEffectAssetModel, this);
	return pEffectAssetWidget;
}

bool CParticleEditor::OnUndo()
{
	GetIEditor()->GetIUndoManager()->Undo();
	return true;
}

bool CParticleEditor::OnRedo()
{
	GetIEditor()->GetIUndoManager()->Redo();
	return true;
}

void CParticleEditor::SetLayout(const QVariantMap& state)
{
	CEditor::SetLayout(state);
}

QVariantMap CParticleEditor::GetLayout() const
{
	QVariantMap state = CEditor::GetLayout();
	return state;
}

void CParticleEditor::Serialize(Serialization::IArchive& archive)
{
	// TODO: Remove this?
}

bool CParticleEditor::OnOpenAsset(CAsset* pAsset)
{
	CRY_ASSERT(pAsset);

	if (m_pEffectAssetModel->OpenAsset(pAsset))
	{
		m_pEffectToolBar->show();
		m_pReloadEffectMenuAction->setEnabled(true);
		m_pShowEffectOptionsMenuAction->setEnabled(true);

		return true;
	}
	return false;
}

bool CParticleEditor::OnSaveAsset(CEditableAsset& editAsset)
{
	SaveEffect(editAsset);

	return true;
}

void CParticleEditor::OnDiscardAssetChanges()
{
	// Reload from file
	if (auto* pAffectAsset = m_pEffectAssetModel->GetEffectAsset())
		if (auto* pAsset = pAffectAsset->GetAsset())
			m_pEffectAssetModel->OpenAsset(pAsset);
}

void CParticleEditor::OnCloseAsset()
{
	m_pEffectAssetModel->ClearAsset();
}

bool LoadFile(std::vector<char>& content, const char* filename)
{
	FILE* f = gEnv->pCryPak->FOpen(filename, "r");
	if (!f)
		return false;

	const size_t size = gEnv->pCryPak->FGetSize(f);

	content.resize(size);
	bool result = true;
	if (size != 0)
		result = gEnv->pCryPak->FRead(&content[0], size, f) == size;
	gEnv->pCryPak->FClose(f);
	return result;
}

bool CParticleEditor::OnAboutToCloseAsset(string& reason) const
{
	CEffectAsset* const pEffectAsset = m_pEffectAssetModel->GetEffectAsset();
	if (!pEffectAsset)
	{
		return true;
	}

	pfx2::IParticleEffectPfx2* const pEffect = pEffectAsset->GetEffect();
	DynArray<char> newPfx;
	Serialization::SaveJsonBuffer(newPfx, *pEffect);

	std::vector<char> oldPfx;
	if (!LoadFile(oldPfx, pEffectAsset->GetAsset()->GetFile(0)))
	{
		return true;
	}

	if (newPfx.size() != oldPfx.size())
	{
		return false;
	}

	return !strncmp(&newPfx[0], &oldPfx[0], newPfx.size());
}

void CParticleEditor::CreateDefaultLayout(CDockableContainer* pSender)
{
	CRY_ASSERT(pSender);
	pSender->SpawnWidget(s_szCurveEditorPanelName);
	pSender->SpawnWidget(s_szEffectAssetName, QToolWindowAreaReference::HSplitTop);
	pSender->SpawnWidget(s_szInspectorName, QToolWindowAreaReference::VSplitRight);
}

///////////////////////////////////////////////////////////////////////
// Data maintenance functions

void CParticleEditor::AssignToEntity(CBaseObject* pObject, const string& newAssetName)
{
	CEntityObject* pEntityObject = DYNAMIC_DOWNCAST(CEntityObject, pObject);
	if (pEntityObject == nullptr)
		return;

	if (IEntity* pEntity = pEntityObject->GetIEntity())
	{
		Cry::DefaultComponents::CParticleComponent* pParticleComponent = pEntity->GetOrCreateComponent<Cry::DefaultComponents::CParticleComponent>();
		pParticleComponent->GetComponentFlags().Add(EEntityComponentFlags::UserAdded);

		pParticleComponent->SetEffectName(newAssetName);
		pParticleComponent->LoadEffect(true);
	}

	CParticleEffectObject* pLegacyParticleObject = DYNAMIC_DOWNCAST(CParticleEffectObject, pObject);
	if (pLegacyParticleObject != nullptr)
	{
		pLegacyParticleObject->AssignEffect(newAssetName);
	}

	GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
}

bool CParticleEditor::AssetSaveDialog(string* pOutputName)
{
	CRY_ASSERT(pOutputName != nullptr);

	CEngineFileDialog::RunParams runParams;
	runParams.title = QStringLiteral("Save Particle Effects");
	runParams.extensionFilters << CExtensionFilter(QStringLiteral("Particle Effects (pfx)"), "pfx");

	QString filename = CEngineFileDialog::RunGameSave(runParams, nullptr);
	if (!filename.isEmpty())
	{
		*pOutputName = filename.toLocal8Bit().data();
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////
// Actions

void CParticleEditor::SaveEffect(CEditableAsset& editAsset)
{
	CEffectAsset* const pEffectAsset = m_pEffectAssetModel->GetEffectAsset();
	if (!pEffectAsset)
	{
		return;
	}

	CRY_ASSERT(pEffectAsset->GetAsset() && pEffectAsset->GetAsset()->GetFilesCount());

	const string basePath = PathUtil::RemoveExtension(PathUtil::RemoveExtension(editAsset.GetAsset().GetMetadataFile()));
	const string pfxFilePath = PathUtil::ReplaceExtension(basePath, "pfx"); // Relative to asset directory.

	pfx2::IParticleEffectPfx2* pEffect = pEffectAsset->GetEffect();
	Serialization::SaveJsonFile(pfxFilePath.c_str(), *pEffect);

	// Get effect dependency
	{
		std::vector<SAssetDependencyInfo> dependencies;
		for (size_t component = 0, componentsCount = pEffect->GetNumComponents(); component < componentsCount; ++component)
		{
			const pfx2::IParticleComponent* pComponent = pEffect->GetComponent(component);
			for (size_t feature = 0, featuresCount = pComponent->GetNumFeatures(); feature < featuresCount; ++feature)
			{
				const pfx2::IParticleFeature* pFeature = pComponent->GetFeature(feature);
				for (size_t resource = 0, resourcesCount = pFeature->GetNumResources(); resource < resourcesCount; ++resource)
				{
					const char* szResourceFilename = pFeature->GetResourceName(resource);
					auto it = std::find_if(dependencies.begin(), dependencies.end(), [szResourceFilename](const auto& x)
					{
						return x.path.CompareNoCase(szResourceFilename);
					});

					if (it == dependencies.end())
					{
						dependencies.emplace_back(szResourceFilename, 1);
					}
					else // increment instance count
					{
						++(it->usageCount);
					}
				}
			}
		}
		editAsset.SetDependencies(dependencies);
	}

	editAsset.SetFiles("", { pfxFilePath });
}

void CParticleEditor::OnReloadEffect()
{
	if (!m_pEffectAssetModel->GetEffectAsset())
	{
		return;
	}

	CEffectAsset* const pEffectAsset = m_pEffectAssetModel->GetEffectAsset();

	CAsset* const pAsset = pEffectAsset->GetAsset();
	CRY_ASSERT(pAsset && pAsset->GetFilesCount());

	const string pfxFilePath = pAsset->GetFile(0);

	const QString questionText = QString("Reload effect ") + pfxFilePath + " and lose all changes?";
	if (CQuestionDialog::SQuestion("Revert Effect", questionText) == QDialogButtonBox::Yes)
	{
		pfx2::IParticleEffectPfx2* pEffect = pEffectAsset->GetEffect();
		if (pAsset)
		{
			OpenAsset(pAsset);
		}
	}
}

void CParticleEditor::OnImportPfx1()
{
	// Find selected Pfx1 effect
	IDataBaseManager* particleMgr = static_cast<CParticleManager*>(GetIEditor()->GetDBItemManager(EDB_TYPE_PARTICLE));
	if (particleMgr)
	{
		if (CParticleItem* pSelectedItem = static_cast<CParticleItem*>(particleMgr->GetSelectedItem()))
		{
			if (pfx2::PParticleEffect pNewEffect = GetParticleSystem()->ConvertEffect(pSelectedItem->GetEffect(), true))
			{
				const cstr pfxFilePath = pNewEffect->GetName(); // Relative to project root.

				const string assetFilePath = PathUtil::MakeGamePath(pfxFilePath) + ".cryasset";

				if (CAssetManager::GetInstance()->FindAssetForMetadata(assetFilePath.c_str()))
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Particle asset '%s' already exists.", assetFilePath.c_str());
				}

				const CParticlesType* const pParticlesType = (const CParticlesType*)CAssetManager::GetInstance()->FindAssetType("Particles");
				CRY_ASSERT(!strcmp(CParticlesType().GetTypeName(), "Particles"));

				if (pParticlesType->CreateForExistingEffect(assetFilePath.c_str()))
				{
					if (CAsset* const pAsset = CAssetManager::GetInstance()->FindAssetForMetadata(assetFilePath.c_str()))
					{
						OpenAsset(pAsset);
					}
				}
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "No particle selected for import in database view");
		}
	}
}

void CParticleEditor::OnLoadFromSelectedEntity()
{
	const CSelectionGroup* pGroup = GetIEditor()->GetObjectManager()->GetSelection();
	if (!pGroup || pGroup->GetCount() == 0)
		return;

	for (int objectIndex = 0, numObjects = pGroup->GetCount(); objectIndex < numObjects; ++objectIndex)
	{
		CEntityObject* pEntityObject = DYNAMIC_DOWNCAST(CEntityObject, pGroup->GetObject(objectIndex));
		if (pEntityObject == nullptr)
			continue;

		if (IEntity* pEntity = pEntityObject->GetIEntity())
		{
			for (int slotIndex = 0, n = pEntity->GetSlotCount(); slotIndex < n; ++slotIndex)
			{
				IParticleEmitter* pEmitter = pEntity->GetParticleEmitter(slotIndex);
				if (pEmitter == nullptr)
					continue;

				CAsset* const pAsset = CAssetManager::GetInstance()->FindAssetForFile(pEmitter->GetEffect()->GetFullName());
				if (pAsset == nullptr)
					continue;

				OpenAsset(pAsset);
				return;
			}
		}
	}
}

void CParticleEditor::OnApplyToSelectedEntity()
{
	const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
	if (0 == pSelection->GetCount())
		return;

	CEffectAsset* const pEffectAsset = m_pEffectAssetModel->GetEffectAsset();
	if (!pEffectAsset)
	{
		return;
	}

	const CAsset* const pAsset = pEffectAsset->GetAsset();
	CRY_ASSERT(pAsset && pAsset->GetFilesCount());

	const string& effectName = pAsset->GetFile(0);

	CUndo undo("Assign ParticleEffect");

	for (int i = 0; i < pSelection->GetCount(); i++)
		AssignToEntity(pSelection->GetObject(i), effectName);
}

void CParticleEditor::OnNewComponent()
{
	QAction* pAction = qobject_cast<QAction*>(sender());
	if (!pAction)
		return;

	QString templateFile = pAction->data().toString();
	if (templateFile.isEmpty())
		return;

	CEffectAsset* const pEffectAsset = m_pEffectAssetModel->GetEffectAsset();
	if (!pEffectAsset)
	{
		return;
	}

	auto templateFileStd = templateFile.toStdString();
	pEffectAsset->MakeNewComponent(templateFileStd.c_str());
}

void CParticleEditor::OnShowEffectOptions()
{
	CEffectAsset* const pEffectAsset = m_pEffectAssetModel->GetEffectAsset();
	if (!pEffectAsset)
	{
		return;
	}

	pfx2::IParticleEffectPfx2* const pEffect = pEffectAsset->GetEffect();

		PopulateInspectorEvent popEvent([this, pEffect](CInspector& inspector)
		{
			QAdvancedPropertyTree* pPropertyTree = new QAdvancedPropertyTree(pEffect->GetName());
			// WORKAROUND: Serialization of features doesn't work with the default style.
			//						 We either need to fix serialization or property tree. As soon as it's
			//						 done use the commented out code below.
			PropertyTreeStyle treeStyle(pPropertyTree->treeStyle());
			treeStyle.propertySplitter = false;
			pPropertyTree->setTreeStyle(treeStyle);
			pPropertyTree->setSizeToContent(true);
			// ~WORKAROUND
			//pPropertyTree->setExpandLevels(2);
			//pPropertyTree->setValueColumnWidth(0.6f);
			//pPropertyTree->setAutoRevert(false);
			//pPropertyTree->setAggregateMouseEvents(false);
			//pPropertyTree->setFullRowContainers(true);
			pPropertyTree->attach(pEffect->GetEffectOptionsSerializer());
			QObject::connect(pPropertyTree, &QPropertyTree::signalChanged, this, &CParticleEditor::OnEffectOptionsChanged);

			QCollapsibleFrame* pInspectorWidget = new QCollapsibleFrame("Particle Effect");
			pInspectorWidget->SetWidget(pPropertyTree);
			inspector.AddWidget(pInspectorWidget);
		});

		GetBroadcastManager().Broadcast(popEvent);
	}

void CParticleEditor::OnEffectOptionsChanged()
{
	CEffectAsset* const pEffectAsset = m_pEffectAssetModel->GetEffectAsset();
	if (!pEffectAsset)
		return;
	pfx2::IParticleEffectPfx2* pEffect = pEffectAsset->GetEffect();
	pEffectAsset->SetModified(true);
	pEffect->SetChanged();
}

}

