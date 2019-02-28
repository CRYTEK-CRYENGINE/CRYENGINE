// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UIManager.h"

#include "../HyperGraph/FlowGraph.h"
#include "../HyperGraph/FlowGraphManager.h"

#include "Controls/QuestionDialog.h"
#include "FilePathUtil.h"

#define UI_ACTIONS_FOLDER "UIActions"
#define GRAPH_FILE_FILTER "Graph XML Files (*.xml)|*.xml"

float CUIManager::CV_gfx_FlashReloadTime = 0;
int CUIManager::CV_gfx_FlashReloadEnabled = 0;

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
CUIManager::CUIManager()
	: m_pEditor(NULL)
{
}

////////////////////////////////////////////////////////////////////
CUIManager::~CUIManager()
{
	GetIEditorImpl()->GetLevelIndependentFileMan()->UnregisterModule(this);
	if (gEnv->pConsole)
		gEnv->pConsole->UnregisterVariable("gfx_FlashReloadTime");
	if (gEnv->pFlashUI)
		gEnv->pFlashUI->UnregisterModule(this);
}

////////////////////////////////////////////////////////////////////
void CUIManager::Init()
{
	if (gEnv->pFlashUI)
		gEnv->pFlashUI->RegisterModule(this, "CUIManagerSandbox");
	GetIEditorImpl()->GetLevelIndependentFileMan()->RegisterModule(this);
	// TODO: Rethink managing/loading of graphs.
	ReloadActionGraphs();
	// ~
	REGISTER_CVAR2("gfx_FlashReloadTime", &CV_gfx_FlashReloadTime, 1.f, VF_NULL, "Time in seconds to wait for flash to publish a file (used for auto-reload of UIEmulator)");
	REGISTER_CVAR2("gfx_FlashReloadEnabled", &CV_gfx_FlashReloadEnabled, 0, VF_NULL, "Enable live reloading of changed flash assets. 0=Disabled, 1=Enabled");
}

////////////////////////////////////////////////////////////////////
bool CUIManager::IsFlashEnabled() const
{
	return true;
}

////////////////////////////////////////////////////////////////////
void CUIManager::ReloadActionGraphs(bool bReloadGraphs)
{
	if (!gEnv->pFlashUI)
		return;

	int i = 0;
	IUIAction* pAction;
	while (pAction = gEnv->pFlashUI->GetUIAction(i++))
	{
		if (pAction->GetType() != IUIAction::eUIAT_FlowGraph)
			continue;

		CHyperFlowGraph* pFlowGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraphForUIAction(pAction);
		bool bReloadAction = bReloadGraphs || !pFlowGraph;
		if (!pFlowGraph)
		{
			pFlowGraph = GetIEditorImpl()->GetFlowGraphManager()->CreateGraphForUIAction(pAction);
		}
		if (bReloadAction)
		{
			const char* actionName = pAction->GetName();

			CString filename(GetUIActionFolder());
			filename += '/';
			filename += actionName;
			filename += ".xml";
			pFlowGraph->SetName(actionName);
			pFlowGraph->Load(filename);
		}
	}
}

////////////////////////////////////////////////////////////////////
void CUIManager::SaveChangedGraphs()
{
	if (!gEnv->pFlashUI)
		return;

	int i = 0;
	IUIAction* pAction;
	while (pAction = gEnv->pFlashUI->GetUIAction(i++))
	{
		if (pAction->GetType() != IUIAction::eUIAT_FlowGraph)
			continue;

		CHyperFlowGraph* pFlowGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraphForUIAction(pAction);
		assert(pFlowGraph);
		if (pFlowGraph && pFlowGraph->IsModified())
		{
			CString filename(GetUIActionFolder());
			filename += '/';
			filename += pAction->GetName();
			filename += ".xml";
			pFlowGraph->SetName("");
			pFlowGraph->Save(filename);
		}
	}
}

////////////////////////////////////////////////////////////////////
bool CUIManager::HasModifications()
{
	if (!gEnv->pFlashUI)
		return false;

	int i = 0;
	IUIAction* pAction;
	while (pAction = gEnv->pFlashUI->GetUIAction(i++))
	{
		if (pAction->GetType() != IUIAction::eUIAT_FlowGraph)
			continue;

		CHyperFlowGraph* pFlowGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraphForUIAction(pAction);

		if (pFlowGraph && pFlowGraph->IsModified())
			return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////
bool CUIManager::NewUIAction(CString& filename)
{
	if (!gEnv->pFlashUI)
		return false;

	string defaultFolder = PathUtil::Make(PathUtil::GetGameFolder(), GetUIActionFolder());
	defaultFolder = PathUtil::GamePathToCryPakPath(defaultFolder);
	defaultFolder.append("\\");

	CFileUtil::CreateDirectory(defaultFolder);

	if (!CFileUtil::SelectSaveFile(GRAPH_FILE_FILTER, "xml", defaultFolder.c_str(), filename))
		return false;

	// check if file exists.
	FILE* file = fopen(filename, "rb");
	if (file)
	{
		fclose(file);
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Can't create UI Action because another UI Action with this name already exists!\n\nCreation canceled..."));
		return false;
	}
	CString luafilename = filename;
	luafilename.Replace(".xml", ".lua");
	file = fopen(luafilename, "rb");
	if (file)
	{
		fclose(file);
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Can't create UI Action because another Lua UI Action with this name already exists!\n\nCreation canceled..."));
		return false;
	}

	// Make a new graph.
	CFlowGraphManager* pManager = GetIEditorImpl()->GetFlowGraphManager();
	CHyperGraph* pGraph = pManager->CreateGraph();

	if (pGraph->Save(filename))
	{
		gEnv->pFlashUI->LoadActionFromFile(filename, IUIAction::eUIAT_FlowGraph);
		// TODO: This looks like a lot of overhead we could avoid.
		ReloadActionGraphs();
		//  ~
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////
void CUIManager::ReloadScripts()
{
	gEnv->pFlashUI->Reload();
}

////////////////////////////////////////////////////////////////////
bool CUIManager::PromptChanges()
{
	if (HasModifications())
	{
		QString msg(QObject::tr("Some UIAction flowgraphs are modified!\nDo you want to save your changes?"));
		QDialogButtonBox::StandardButton result = CQuestionDialog::SQuestion(QObject::tr("UIAction(s) not saved!"), msg, QDialogButtonBox::Discard | QDialogButtonBox::Save | QDialogButtonBox::Cancel);
		if (QDialogButtonBox::Save == result)
		{
			SaveChangedGraphs();
		}
		else if (QDialogButtonBox::Cancel == result)
		{
			return false;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////
CString CUIManager::GetUIActionFolder()
{
	ICVar* pFolderVar = gEnv->pConsole->GetCVar("gfx_uiaction_folder");

	CString folder;
	folder.Format("%s/%s", pFolderVar ? pFolderVar->GetString() : "Libs/UI", UI_ACTIONS_FOLDER);
	return folder;
}

////////////////////////////////////////////////////////////////////
bool CUIManager::EditorAllowReload()
{
	return PromptChanges();
}

////////////////////////////////////////////////////////////////////
void CUIManager::EditorReload()
{
	ReloadActionGraphs(true);
}

