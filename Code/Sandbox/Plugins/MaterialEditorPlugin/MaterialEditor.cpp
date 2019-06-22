// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MaterialEditor.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/EditableAsset.h>
#include <PathUtils.h>

#include <EditorFramework/Events.h>
#include <EditorFramework/Inspector.h>
#include <LevelEditor/LevelEditorSharedState.h>

#include "SubMaterialView.h"
#include "MaterialPreview.h"
#include "MaterialProperties.h"
#include "PickMaterialTool.h"

#include "Dialogs/QNumericBoxDialog.h"

#include "IUndoObject.h"

#include <CryCore/CryTypeInfo.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

namespace Private_MaterialEditor
{

REGISTER_VIEWPANE_FACTORY(CMaterialEditor, "Material Editor", "Tools", false);

class CSession : public IAssetEditingSession
{
public:
	CSession(CMaterial* pMaterial) : m_pMaterial(pMaterial) {}

	virtual const char* GetEditorName() const override
	{
		return "Material Editor";
	}

	virtual bool OnSaveAsset(IEditableAsset& asset) override
	{
		if (!m_pMaterial || !m_pMaterial->Save(false))
		{
			return false;
		}

		//Fill metadata and dependencies to update cryasset file
		asset.SetFiles({ m_pMaterial->GetFilename() });

		std::vector<SAssetDependencyInfo> filenames;

		//TODO : not all dependencies are found, some paths are relative to same folder which is not good ...
		int textureCount = m_pMaterial->GetTextureDependencies(filenames);

		std::vector<std::pair<string, string>> details;
		details.push_back(std::pair<string, string>("subMaterialCount", ToString(m_pMaterial->GetSubMaterialCount())));
		details.push_back(std::pair<string, string>("textureCount", ToString(textureCount)));

		asset.SetDetails(details);
		asset.SetDependencies(filenames);
		return true;
	}

	virtual void DiscardChanges(IEditableAsset& asset) override
	{
		if (!m_pMaterial)
		{
			return;
		}

		m_pMaterial->Reload();
	}

	virtual bool OnCopyAsset(INewAsset& asset) override
	{
		CMaterial* pMaterial = GetIEditor()->GetMaterialManager()->DuplicateMaterial(PathUtil::Make(asset.GetFolder(), asset.GetName()), m_pMaterial);
		CSession session(pMaterial);
		return session.OnSaveAsset(asset);
	}

	static CMaterial* GetSessionMaterial(CAsset* pAsset)
	{
		IAssetEditingSession* pSession = pAsset->GetEditingSession();
		if (!pSession || strcmp(pSession->GetEditorName(), "Material Editor") != 0)
		{
			return nullptr;
		}
		return static_cast<CSession*>(pSession)->GetMaterial();
	}

private:
	CMaterial* GetMaterial() { return m_pMaterial; }

	_smart_ptr<CMaterial> m_pMaterial;
};

}

CMaterialEditor::CMaterialEditor()
	: CAssetEditor("Material")
	, m_pMaterial(nullptr)
{
	RegisterActions();
	GetIEditor()->GetMaterialManager()->AddListener(this);
}

CMaterialEditor::~CMaterialEditor()
{
	GetIEditor()->GetMaterialManager()->RemoveListener(this);
}

void CMaterialEditor::RegisterActions()
{
	RegisterAction("general.undo", &CMaterialEditor::OnUndo);
	RegisterAction("general.redo", &CMaterialEditor::OnRedo);
}

void CMaterialEditor::InitMenuBar()
{
	AddToMenu({ CEditor::MenuItems::FileMenu, CEditor::MenuItems::SaveAs, CEditor::MenuItems::EditMenu, CEditor::MenuItems::Undo, CEditor::MenuItems::Redo });

	CAbstractMenu* pFileMenu = GetMenu(CEditor::MenuItems::FileMenu);
	QAction* pAction = pFileMenu->CreateAction(CryIcon("icons:General/Picker.ico"), tr("Pick Material From Scene"), 0, 1);
	connect(pAction, &QAction::triggered, this, &CMaterialEditor::OnPickMaterial);

	//Add a material actions menu
	//TODO: consider adding a toolbar for material actions
	CAbstractMenu* materialMenu = GetRootMenu()->CreateMenu(tr("Material"), 0, 3);
	materialMenu->signalAboutToShow.Connect(this, &CMaterialEditor::FillMaterialMenu);
}

void CMaterialEditor::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnEndSceneOpen:
	case eNotify_OnEndNewScene:
		//HACK : Due to the questionable behavior of material manager, which clears itself when the level is loaded
		//The materials used in the scene (and the material editor) will be recreated after the scene is loaded,
		//making the material editor appear to be not in sync. A quick hack is to reload from filename, to get the updated material.
		if (GetAssetBeingEdited())
		{
			OnOpenAsset(GetAssetBeingEdited());
		}
	}
}

void CMaterialEditor::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	if (!pItem || !m_pMaterial || event == EDB_ITEM_EVENT_SELECTED)
		return;

	CMaterial* item = (CMaterial*)pItem;

	if (pItem == m_pMaterial.get())
	{
		switch (event)
		{
		case EDB_ITEM_EVENT_CHANGED:
		case EDB_ITEM_EVENT_UPDATE_PROPERTIES:
			signalMaterialPropertiesChanged(item);
			if (GetAssetBeingEdited())
			{
				GetAssetBeingEdited()->SetModified(true);
			}
			break;
		case EDB_ITEM_EVENT_DELETE:
			//Note: this happens on loading of the level. We will not handle it but things may be unexpected if the item is actually being deleted from the old material editor
			/*CRY_ASSERT_MESSAGE(0, "Material was deleted by other means while it was being edited, this case is not well handled");
			   TryCloseAsset();*/
			break;
		default:
			break;
		}
	}
	else if (item->GetParent() == m_pMaterial.get())
	{
		switch (event)
		{
		case EDB_ITEM_EVENT_CHANGED:
		case EDB_ITEM_EVENT_UPDATE_PROPERTIES:
			signalMaterialPropertiesChanged(item);
			if (GetAssetBeingEdited())
			{
				GetAssetBeingEdited()->SetModified(true);
			}
			break;
		case EDB_ITEM_EVENT_DELETE: //deleted from DB, what to do ?
			if (item == m_pEditedMaterial)
			{
				SelectMaterialForEdit(nullptr);
			}
			break;
		default:
			break;
		}
	}
}

void CMaterialEditor::OnSubMaterialsChanged(CMaterial::SubMaterialChange change)
{
	if (GetAssetBeingEdited())
	{
		GetAssetBeingEdited()->SetModified(true);
	}

	switch (change)
	{
	case CMaterial::MaterialConverted:
		if (m_pMaterial->IsMultiSubMaterial())
		{
			if (m_pMaterial->GetSubMaterialCount() > 0)
			{
				SelectMaterialForEdit(m_pMaterial->GetSubMaterial(0));
			}
			else
			{
				SelectMaterialForEdit(nullptr);
			}
		}
		else
		{
			SelectMaterialForEdit(m_pMaterial);
		}
		break;
	case CMaterial::SubMaterialSet:
		//If the material we are currently editing is no longer a child of loaded material, clear it
		if (m_pEditedMaterial && m_pMaterial->IsMultiSubMaterial() && m_pEditedMaterial->GetParent() != m_pMaterial)
		{
			SelectMaterialForEdit(nullptr);
		}
		break;
	case CMaterial::SlotCountChanged:
		if (m_pMaterial->IsMultiSubMaterial() && m_pMaterial->GetSubMaterialCount() == 0)
		{
			SelectMaterialForEdit(nullptr);
		}
		break;
	default:
		break;
	}
}

void CMaterialEditor::OnInitialize()
{
	RegisterDockableWidget("Properties", [&]()
	{
		CInspector* pInspector = new CInspector(this);
		pInspector->SetLockable(false);
		return pInspector;
	}, true);

	RegisterDockableWidget("Material", [&]() { return new CSubMaterialView(this); }, true);
	RegisterDockableWidget("Preview", [&]() { return new CMaterialPreviewWidget(this); });

	InitMenuBar();
}

void CMaterialEditor::OnCreateDefaultLayout(CDockableContainer* pSender, QWidget* pAssetBrowser)
{
	QWidget* pCenterWidget = pSender->SpawnWidget("Properties", pAssetBrowser, QToolWindowAreaReference::VSplitRight);
	pSender->SpawnWidget("Preview", pCenterWidget, QToolWindowAreaReference::Right);
	QWidget* pMaterialWidget = pSender->SpawnWidget("Material", pCenterWidget, QToolWindowAreaReference::Top);
	pSender->SetSplitterSizes(pMaterialWidget, { 1, 4 });
}

void CMaterialEditor::OnLayoutChange(const QVariantMap& state)
{
	//Rebroadcast on layout change
	//Do not update inspector as the properties pane is unique anyway
	if (m_pMaterial)
	{
		signalMaterialLoaded(m_pMaterial);
		if (m_pEditedMaterial)
		{
			signalMaterialForEditChanged(m_pEditedMaterial);
			BroadcastPopulateInspector();
		}
	}
	CAssetEditor::OnLayoutChange(state);
}

bool CMaterialEditor::OnOpenAsset(CAsset* pAsset)
{
	using namespace Private_MaterialEditor;

	CAbstractMenu* materialMenu = GetMenu(tr("Material"));
	CRY_ASSERT(materialMenu);
	materialMenu->SetEnabled(!pAsset->IsImmutable());

	const auto& filename = pAsset->GetFile(0);
	CRY_ASSERT(filename && *filename);

	const string materialName = GetIEditor()->GetMaterialManager()->FilenameToMaterial(filename);

	CMaterial* pMaterial = CSession::GetSessionMaterial(pAsset);
	if (!pMaterial)
	{
		pMaterial = static_cast<CMaterial*>(GetIEditor()->GetMaterialManager()->FindItemByName(materialName));
	}

	if (!pMaterial)
	{
		pMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial(materialName, false);
	}
	else if (!pAsset->IsModified())
	{
		pMaterial->Reload(); //Enforce loading from file every time as we cannot assume the file has not changed compared to in-memory material
	}

	if (!pMaterial)
	{
		return false;
	}

	SetMaterial(pMaterial);

	return true;
}

void CMaterialEditor::OnCloseAsset()
{
	SetMaterial(nullptr);
}

void CMaterialEditor::OnDiscardAssetChanges(CEditableAsset& editAsset)
{
	CRY_ASSERT(GetAssetBeingEdited());
	CRY_ASSERT(GetAssetBeingEdited()->GetEditingSession());

	GetAssetBeingEdited()->GetEditingSession()->DiscardChanges(editAsset);
}

std::unique_ptr<IAssetEditingSession> CMaterialEditor::CreateEditingSession()
{
	using namespace Private_MaterialEditor;

	if (!m_pMaterial)
	{
		return nullptr;
	}

	CSession* pSession = new CSession(m_pMaterial);
	return std::unique_ptr<IAssetEditingSession>(pSession);
}

void CMaterialEditor::SelectMaterialForEdit(CMaterial* pMaterial)
{
	if (pMaterial != m_pEditedMaterial)
	{
		m_pEditedMaterial = pMaterial;

		signalMaterialForEditChanged(pMaterial);

		BroadcastPopulateInspector();
	}
}

void CMaterialEditor::BroadcastPopulateInspector()
{
	if (m_pEditedMaterial)
	{
		m_pMaterialSerializer.reset(new CMaterialSerializer(m_pEditedMaterial, !m_pEditedMaterial->CanModify()));
		string title = m_pMaterial->GetName();
		if (m_pMaterial->IsMultiSubMaterial())
		{
			title += " [";
			title += m_pEditedMaterial->GetName();
			title += "]";
		}

		PopulateInspectorEvent event([this](CInspector& inspector)
		  {
		                             inspector.AddPropertyTree(m_pMaterialSerializer->CreatePropertyTree());
			}, title);
		event.Broadcast(this);
	}
	else
	{
		m_pMaterialSerializer.reset();
		ClearInspectorEvent().Broadcast(this);
	}
}

void CMaterialEditor::SetMaterial(CMaterial* pMaterial)
{
	if (pMaterial != m_pMaterial)
	{
		if (m_pMaterial)
		{
			m_pMaterial->signalSubMaterialsChanged.DisconnectObject(this);
		}

		SelectMaterialForEdit(nullptr);

		m_pMaterial = pMaterial;

		signalMaterialLoaded(m_pMaterial);

		if (m_pMaterial)
		{
			m_pMaterial->signalSubMaterialsChanged.Connect(this, &CMaterialEditor::OnSubMaterialsChanged);

			if (m_pMaterial->IsMultiSubMaterial())
			{
				if (m_pMaterial->GetSubMaterialCount() > 0)
					SelectMaterialForEdit(m_pMaterial->GetSubMaterial(0));
				else
					SelectMaterialForEdit(nullptr);
			}
			else
				SelectMaterialForEdit(m_pMaterial);
		}
	}
}

void CMaterialEditor::FillMaterialMenu(CAbstractMenu* menu)
{
	menu->Clear();

	int sec = menu->GetNextEmptySection();

	auto action = menu->CreateAction(tr("Convert To Multi-Material"), sec);
	action->setEnabled(m_pMaterial && !m_pMaterial->IsMultiSubMaterial());
	connect(action, &QAction::triggered, this, &CMaterialEditor::OnConvertToMultiMaterial);

	sec = menu->GetNextEmptySection();

	action = menu->CreateAction(tr("Convert To Single Material"), sec);
	action->setEnabled(m_pMaterial && m_pMaterial->IsMultiSubMaterial());
	connect(action, &QAction::triggered, this, &CMaterialEditor::OnConvertToSingleMaterial);

	action = menu->CreateAction(tr("Add Sub-Material"), sec);
	action->setEnabled(m_pMaterial && m_pMaterial->IsMultiSubMaterial());
	connect(action, &QAction::triggered, this, &CMaterialEditor::OnAddSubMaterial);

	action = menu->CreateAction(tr("Set Sub-Material Slot Count..."), sec);
	action->setEnabled(m_pMaterial && m_pMaterial->IsMultiSubMaterial());
	connect(action, &QAction::triggered, this, &CMaterialEditor::OnSetSubMaterialSlotCount);
}

void CMaterialEditor::OnConvertToMultiMaterial()
{
	CUndo undo("Convert To Multi-Material");
	CRY_ASSERT(m_pMaterial && !m_pMaterial->IsMultiSubMaterial());
	m_pMaterial->ConvertToMultiMaterial();
}

void CMaterialEditor::OnConvertToSingleMaterial()
{
	CUndo undo("Convert To Single Material");
	CRY_ASSERT(m_pMaterial && m_pMaterial->IsMultiSubMaterial());
	m_pMaterial->ConvertToSingleMaterial();
}

void CMaterialEditor::OnAddSubMaterial()
{
	CUndo undo("Add Sub-Material");
	CRY_ASSERT(m_pMaterial && m_pMaterial->IsMultiSubMaterial());

	const auto count = m_pMaterial->GetSubMaterialCount();
	m_pMaterial->SetSubMaterialCount(count + 1);
	m_pMaterial->ResetSubMaterial(count, false);
}

void CMaterialEditor::OnSetSubMaterialSlotCount()
{
	CRY_ASSERT(m_pMaterial && m_pMaterial->IsMultiSubMaterial());
	QNumericBoxDialog dialog("Set Sub-Material Slot Count", m_pMaterial->GetSubMaterialCount(), this);

	dialog.RestrictToInt();
	dialog.SetMin(0);

	if (dialog.Execute())
	{
		CUndo undo("Set Sub-Material Slot Count");
		const int newCount = dialog.GetValue();
		m_pMaterial->SetSubMaterialCount(newCount);

		for (int i = 0; i < newCount; i++)
		{
			m_pMaterial->ResetSubMaterial(i, true);
		}
	}
}

void CMaterialEditor::OnPickMaterial()
{
	auto pickMaterialTool = new CPickMaterialTool();
	pickMaterialTool->SetActiveEditor(this);

	GetIEditor()->GetLevelEditorSharedState()->SetEditTool(pickMaterialTool);
}

void CMaterialEditor::OnResetSubMaterial(int slot)
{
	CUndo undo("Reset Sub-Material");
	CRY_ASSERT(m_pMaterial && m_pMaterial->IsMultiSubMaterial());
	m_pMaterial->ResetSubMaterial(slot, false);
}

void CMaterialEditor::OnRemoveSubMaterial(int slot)
{
	CUndo undo("Remove Sub-Material");
	CRY_ASSERT(m_pMaterial && m_pMaterial->IsMultiSubMaterial());
	m_pMaterial->SetSubMaterial(slot, nullptr);
}
