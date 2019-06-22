// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EnvironmentEditor.h"

#include "UI/ConstantsWidget.h"
#include "UI/CurveEditorWidget.h"
#include "UI/VariablesWidget.h"

#include <AssetSystem/EditableAsset.h>
#include <EditorFramework/PersonalizationManager.h>
#include <EditorFramework/Events.h>
#include <FileUtils.h>

#include <Cry3DEngine/I3DEngine.h>

namespace Private_EnvironmentEditor
{

class CSession final : public IAssetEditingSession
{
public:
	CSession(const char* szPresetPath, ITimeOfDay::IPreset* pPreset)
		: m_presetPath(szPresetPath)
		, m_pPreset(pPreset)
	{
		CRY_ASSERT(m_pPreset);
	}

	virtual const char* GetEditorName() const { return "Environment Editor"; }

	virtual bool        OnSaveAsset(IEditableAsset& asset)
	{
		asset.SetFiles({ m_presetPath });
		ITimeOfDay* const pTimeOfDay = GetIEditor()->Get3DEngine()->GetTimeOfDay();
		return pTimeOfDay->SavePreset(m_presetPath);
	}

	virtual void DiscardChanges(IEditableAsset& asset)
	{
		ITimeOfDay* const pTimeOfDay = GetIEditor()->Get3DEngine()->GetTimeOfDay();
		pTimeOfDay->DiscardPresetChanges(m_presetPath);
	}

	virtual bool OnCopyAsset(INewAsset& asset)
	{
		const string dataFilePath = PathUtil::RemoveExtension(asset.GetMetadataFile());

		ITimeOfDay* const pTimeOfDay = GetIEditor()->Get3DEngine()->GetTimeOfDay();
		if (!pTimeOfDay->ExportPreset(m_presetPath, dataFilePath) || !pTimeOfDay->LoadPreset(dataFilePath))
		{
			return false;
		}

		ITimeOfDay::IPreset& preset = pTimeOfDay->GetCurrentPreset();
		CSession session(dataFilePath, &preset);
		return session.OnSaveAsset(asset);
	}

	ITimeOfDay::IPreset* GetPreset()
	{
		return m_pPreset;
	}

private:
	const string               m_presetPath;
	ITimeOfDay::IPreset* const m_pPreset;
};

}

REGISTER_VIEWPANE_FACTORY(CEnvironmentEditor, "Environment Editor", "Tools", false);

CEnvironmentEditor::CEnvironmentEditor()
	: CAssetEditor("Environment")
	, m_pPreset(nullptr)
	, m_controller(*this)
{
	RegisterActions();
	AddToMenu({ CEditor::MenuItems::SaveAs, CEditor::MenuItems::EditMenu, CEditor::MenuItems::Undo, CEditor::MenuItems::Redo });
}

void CEnvironmentEditor::RegisterActions()
{
	RegisterAction("general.undo", &CEnvironmentEditor::OnUndo);
	RegisterAction("general.redo", &CEnvironmentEditor::OnRedo);
}

std::unique_ptr<IAssetEditingSession> CEnvironmentEditor::CreateEditingSession()
{
	using Private_EnvironmentEditor::CSession;

	const string& filename = GetAssetBeingEdited()->GetFile(0);
	return std::make_unique<CSession>(filename, m_pPreset);
}

ITimeOfDay::IPreset* CEnvironmentEditor::GetPreset() const
{
	return m_pPreset;
}

void CEnvironmentEditor::OnInitialize()
{
	RegisterDockableWidget("Constants", [=]() { return new CConstantsWidget(m_controller); }, true, false);
	RegisterDockableWidget("Variables", [=]() { return new CVariablesWidget(m_controller); }, true, false);
	RegisterDockableWidget("Curve Editor", [=]() { return new CCurveEditorWidget(m_controller); }, true, false);
}

void CEnvironmentEditor::OnCreateDefaultLayout(CDockableContainer* pSender, QWidget* pAssetBrowser)
{
	QWidget* pConstantsTab = pSender->SpawnWidget("Constants", pAssetBrowser, QToolWindowAreaReference::Right);
	QWidget* pVariablesTab = pSender->SpawnWidget("Variables", pConstantsTab, QToolWindowAreaReference::Right);
	pSender->SpawnWidget("Curve Editor", pVariablesTab, QToolWindowAreaReference::Right);
	pSender->SetSplitterSizes(pConstantsTab, { 1, 1, 1, 4 });

	if (m_pPreset)
	{
		m_controller.signalAssetOpened();
	}
}

bool CEnvironmentEditor::OnOpenAsset(CAsset* pAsset)
{
	using Private_EnvironmentEditor::CSession;

	const string& filename = pAsset->GetFile(0);

	if (!GetTimeOfDay()->PreviewPreset(filename))
	{
		return false;
	}

	m_pPreset = &GetTimeOfDay()->GetCurrentPreset();
	m_presetFileName = filename;
	m_controller.OnOpenAsset();

	return true;
}

void CEnvironmentEditor::OnCloseAsset()
{
	m_pPreset = nullptr;
	m_presetFileName.clear();
	m_controller.OnCloseAsset();
}

string CEnvironmentEditor::GetPresetFileName() const
{
	return m_presetFileName;
}

ITimeOfDay* CEnvironmentEditor::GetTimeOfDay()
{
	ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	assert(pTimeOfDay); //pTimeOfDay always exists
	return pTimeOfDay;
}

void CEnvironmentEditor::OnDiscardAssetChanges(CEditableAsset& editAsset)
{
	CRY_ASSERT(GetAssetBeingEdited());
	CRY_ASSERT(GetAssetBeingEdited()->GetEditingSession());

	GetAssetBeingEdited()->GetEditingSession()->DiscardChanges(editAsset);

	CAsset* const pAsset = GetAssetBeingEdited();
	OnOpenAsset(pAsset);
}

bool CEnvironmentEditor::event(QEvent* event)
{
	if (event->type() == QEvent::WindowActivate)
	{
		if (!m_presetFileName.empty())
		{
			GetTimeOfDay()->PreviewPreset(m_presetFileName);
		}
	}
	return CAssetEditor::event(event);
}
