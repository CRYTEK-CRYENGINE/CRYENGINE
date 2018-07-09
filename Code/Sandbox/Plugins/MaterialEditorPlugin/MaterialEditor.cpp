// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MaterialEditor.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/EditableAsset.h>
#include <FilePathUtil.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>

#include "EditorFramework/Inspector.h"

#include "SubMaterialView.h"
#include "MaterialPreview.h"
#include "MaterialProperties.h"
#include "PickMaterialTool.h"

#include "Dialogs/QNumericBoxDialog.h"

#include "IUndoObject.h"

#include <CryCore/CryTypeInfo.h>

REGISTER_VIEWPANE_FACTORY(CMaterialEditor, "Material Editor", "Tools", false);

CMaterialEditor::CMaterialEditor()
	: CAssetEditor("Material")
	, m_pMaterial(nullptr)
{
	EnableDockingSystem();

	RegisterDockableWidget("Properties", [&]() 
	{
		auto inspector = new CInspector(this);
		inspector->SetLockable(false);
		return inspector;
	}, true);

	RegisterDockableWidget("Material", [&]() { return new CSubMaterialView(this); }, true);
	RegisterDockableWidget("Preview", [&]() { return new CMaterialPreviewWidget(this); });

	InitMenuBar();

	GetIEditor()->GetMaterialManager()->AddListener(this);
}

CMaterialEditor::~CMaterialEditor()
{
	GetIEditor()->GetMaterialManager()->RemoveListener(this);
}

void CMaterialEditor::InitMenuBar()
{
	AddToMenu(CEditor::MenuItems::SaveAs);
	AddToMenu(CEditor::MenuItems::EditMenu);
	AddToMenu(CEditor::MenuItems::Undo);
	AddToMenu(CEditor::MenuItems::Redo);

	CAbstractMenu* fileMenu = GetMenu(CEditor::MenuItems::FileMenu);
	QAction* action = fileMenu->CreateAction(CryIcon("icons:General/Picker.ico") ,tr("Pick Material From Scene"), 0, 1);
	connect(action, &QAction::triggered, this, &CMaterialEditor::OnPickMaterial);

	//Add a material actions menu
	//TODO: consider adding a toolbar for material actions
	CAbstractMenu* materialMenu = GetRootMenu()->CreateMenu(tr("Material"), 0, 3);
	materialMenu->SetEnabled(!IsReadOnly());
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
		if(GetAssetBeingEdited())
			OnOpenAsset(GetAssetBeingEdited());
	}
}

void CMaterialEditor::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	if (!pItem || !m_pMaterial || event == EDB_ITEM_EVENT_SELECTED)
		return;

	CMaterial* item = (CMaterial*)pItem;

	if(pItem == m_pMaterial.get())
	{
		switch (event)
		{
		case EDB_ITEM_EVENT_CHANGED:
		case EDB_ITEM_EVENT_UPDATE_PROPERTIES:
			signalMaterialPropertiesChanged(item);
			if (GetAssetBeingEdited())
				GetAssetBeingEdited()->SetModified(true);
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
			if(GetAssetBeingEdited())
				GetAssetBeingEdited()->SetModified(true);
			break;
		case EDB_ITEM_EVENT_DELETE: //deleted from DB, what to do ?
			if (item == m_pEditedMaterial)
				SelectMaterialForEdit(nullptr);
			break;
		default:
			break;
		}
	}	
}

void CMaterialEditor::OnSubMaterialsChanged(CMaterial::SubMaterialChange change)
{
	if (GetAssetBeingEdited())
		GetAssetBeingEdited()->SetModified(true);

	switch (change)
	{
	case CMaterial::MaterialConverted:
		if (m_pMaterial->IsMultiSubMaterial())
		{
			if (m_pMaterial->GetSubMaterialCount() > 0)
				SelectMaterialForEdit(m_pMaterial->GetSubMaterial(0));
			else
				SelectMaterialForEdit(nullptr);
		}
		else
			SelectMaterialForEdit(m_pMaterial);
		break;
	case CMaterial::SubMaterialSet:
		//If the material we are currently editing is no longer a child of loaded material, clear it
		if (m_pEditedMaterial && m_pMaterial->IsMultiSubMaterial() && m_pEditedMaterial->GetParent() != m_pMaterial)
			SelectMaterialForEdit(nullptr);
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

void CMaterialEditor::OnReadOnlyChanged()
{
	//Refresh the tree, will make it read-only or not depending on the state
	if(GetAssetBeingEdited())
		BroadcastPopulateInspector();

	CAbstractMenu* materialMenu = GetMenu(tr("Material"));
	CRY_ASSERT(materialMenu);
	materialMenu->SetEnabled(!IsReadOnly());
}

void CMaterialEditor::CreateDefaultLayout(CDockableContainer* sender)
{
	auto centerWidget = sender->SpawnWidget("Properties");
	sender->SpawnWidget("Preview", centerWidget, QToolWindowAreaReference::Right);
	auto materialWidget = sender->SpawnWidget("Material", centerWidget, QToolWindowAreaReference::Top);
	sender->SetSplitterSizes(materialWidget, { 1, 4 });
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
	const auto& filename = pAsset->GetFile(0);
	CRY_ASSERT(filename && *filename);

	const auto materialName = GetIEditor()->GetMaterialManager()->FilenameToMaterial(filename);

	CMaterial* material = (CMaterial*)GetIEditor()->GetMaterialManager()->FindItemByName(materialName);
	if (!material)
		material = GetIEditor()->GetMaterialManager()->LoadMaterial(materialName, false);
	else
		material->Reload(); //Enforce loading from file every time as we cannot assume the file has not changed compared to in-memory material

	CRY_ASSERT(material);

	SetMaterial(material);

	return true;
}

bool CMaterialEditor::OnSaveAsset(CEditableAsset& editAsset)
{
	bool ret = m_pMaterial->Save(false);

	//Fill metadata and dependencies to update cryasset file
	if (ret)
	{
		editAsset.SetFiles("", { m_pMaterial->GetFilename() });

		std::vector<SAssetDependencyInfo> filenames;

		//TODO : not all dependencies are found, some paths are relative to same folder which is not good ...
		int textureCount = m_pMaterial->GetTextureDependencies(filenames);

		std::vector<std::pair<string, string>> details;
		details.push_back(std::pair<string, string>("subMaterialCount" , ToString(m_pMaterial->GetSubMaterialCount())));
		details.push_back(std::pair<string, string>( "textureCount", ToString(textureCount) ));

		editAsset.SetDetails(details);
		editAsset.SetDependencies(filenames);
	}

	return ret;
}

void CMaterialEditor::OnCloseAsset()
{
	SetMaterial(nullptr);
}

void CMaterialEditor::OnDiscardAssetChanges()
{
	m_pMaterial->Reload();
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
		m_pMaterialSerializer.reset(new CMaterialSerializer(m_pEditedMaterial, IsReadOnly()));
		string title = m_pMaterial->GetName();
		if (m_pMaterial->IsMultiSubMaterial())
		{
			title += " [";
			title += m_pEditedMaterial->GetName();
			title += "]";
		}

		PopulateInspectorEvent event([this](CInspector& inspector) {
			inspector.AddWidget(m_pMaterialSerializer->CreatePropertyTree());
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

	GetIEditor()->SetEditTool(pickMaterialTool);
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



