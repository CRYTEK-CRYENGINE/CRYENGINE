// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CustomActionsEditorManager.h"

#include <CryEntitySystem/IEntitySystem.h>

#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/Controls/HyperGraphEditorWnd.h"

#include "SplashScreen.h"
#include "Objects/EntityObject.h"
#include <CryAction/ICustomActions.h>
#include <CryGame/IGameFramework.h>
#include "Controls/QuestionDialog.h"

#define GRAPH_FILE_FILTER "Graph XML Files (*.xml)|*.xml"

//////////////////////////////////////////////////////////////////////////
// Custom Actions Editor Manager
//////////////////////////////////////////////////////////////////////////
CCustomActionsEditorManager::CCustomActionsEditorManager()
{
}

CCustomActionsEditorManager::~CCustomActionsEditorManager()
{
	FreeCustomActionGraphs();
}

void CCustomActionsEditorManager::Init(ISystem* system)
{
	SplashScreen::SetText("Loading Custom Action Flowgraphs...");

	ReloadCustomActionGraphs();
}

//////////////////////////////////////////////////////////////////////////
void CCustomActionsEditorManager::ReloadCustomActionGraphs()
{
	//	FreeActionGraphs();
	ICustomActionManager* pCustomActionManager = gEnv->pGameFramework->GetICustomActionManager();
	if (!pCustomActionManager)
		return;

	pCustomActionManager->LoadLibraryActions(CUSTOM_ACTIONS_PATH);
	LoadCustomActionGraphs();
}

void CCustomActionsEditorManager::LoadCustomActionGraphs()
{
	ICustomActionManager* pCustomActionManager = gEnv->pGameFramework->GetICustomActionManager();
	if (!pCustomActionManager)
		return;

	CFlowGraphManager* pFlowGraphManager = GetIEditorImpl()->GetFlowGraphManager();
	CRY_ASSERT(pFlowGraphManager != NULL);

	const size_t numLibraryActions = pCustomActionManager->GetNumberOfCustomActionsFromLibrary();
	for (size_t i = 0; i < numLibraryActions; i++)
	{
		ICustomAction* pCustomAction = pCustomActionManager->GetCustomActionFromLibrary(i);
		CRY_ASSERT(pCustomAction != NULL);
		if (pCustomAction)
		{
			CHyperFlowGraph* pFlowGraph = pFlowGraphManager->FindGraphForCustomAction(pCustomAction);
			if (!pFlowGraph)
			{
				pFlowGraph = pFlowGraphManager->CreateGraphForCustomAction(pCustomAction);
				pFlowGraph->AddRef();
				CString filename(CUSTOM_ACTIONS_PATH);
				filename += '/';
				filename += pCustomAction->GetCustomActionGraphName();
				filename += ".xml";
				pFlowGraph->SetName("");
				pFlowGraph->Load(filename);
			}
		}
	}
}

void CCustomActionsEditorManager::SaveAndReloadCustomActionGraphs()
{
	if (!gEnv->pGameFramework)
	{
		return;
	}

	ICustomActionManager* pCustomActionManager = gEnv->pGameFramework->GetICustomActionManager();
	CRY_ASSERT(pCustomActionManager != NULL);
	if (!pCustomActionManager)
		return;

	CString actionName;
	CWnd* pWnd = GetIEditorImpl()->FindView("Flow Graph");
	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		CHyperGraphDialog* pHGDlg = (CHyperGraphDialog*) pWnd;
		CHyperGraph* pGraph = pHGDlg->GetGraphView()->GetGraph();
		if (pGraph)
		{
			ICustomAction* pCustomAction = pGraph->GetCustomAction();
			if (pCustomAction)
			{
				actionName = pCustomAction->GetCustomActionGraphName();
				pHGDlg->GetGraphView()->SetGraph(NULL);
			}
		}
	}

	SaveCustomActionGraphs();
	ReloadCustomActionGraphs();

	if (!actionName.IsEmpty())
	{
		ICustomAction* pCustomAction = pCustomActionManager->GetCustomActionFromLibrary(actionName);
		if (pCustomAction)
		{
			CFlowGraphManager* pManager = GetIEditorImpl()->GetFlowGraphManager();
			CHyperFlowGraph* pFlowGraph = pManager->FindGraphForCustomAction(pCustomAction);
			assert(pFlowGraph);
			if (pFlowGraph)
				pManager->OpenView(pFlowGraph);
		}
	}
}

void CCustomActionsEditorManager::SaveCustomActionGraphs()
{
	ICustomActionManager* pCustomActionManager = gEnv->pGameFramework->GetICustomActionManager();
	CRY_ASSERT(pCustomActionManager != NULL);
	if (!pCustomActionManager)
		return;

	CWaitCursor waitCursor;

	const size_t numActions = pCustomActionManager->GetNumberOfCustomActionsFromLibrary();
	ICustomAction* pCustomAction;
	for (size_t i = 0; i < numActions; i++)
	{
		pCustomAction = pCustomActionManager->GetCustomActionFromLibrary(i);
		CRY_ASSERT(pCustomAction != NULL);

		CFlowGraphManager* pFlowGraphManager = GetIEditorImpl()->GetFlowGraphManager();
		CRY_ASSERT(pFlowGraphManager != NULL);

		CHyperFlowGraph* m_pFlowGraph = pFlowGraphManager->FindGraphForCustomAction(pCustomAction);
		if (m_pFlowGraph->IsModified())
		{
			m_pFlowGraph->Save(m_pFlowGraph->GetName() + CString(".xml"));
			pCustomAction->Invalidate();
		}
	}
}

void CCustomActionsEditorManager::FreeCustomActionGraphs()
{
	CFlowGraphManager* pFGMgr = GetIEditorImpl()->GetFlowGraphManager();
	if (pFGMgr)
	{
		pFGMgr->FreeGraphsForCustomActions();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CCustomActionsEditorManager::NewCustomAction(CString& filename)
{
	CFileUtil::CreateDirectory(PathUtil::Make(PathUtil::GetGameFolder(), CUSTOM_ACTIONS_PATH));

	if (!CFileUtil::SelectSaveFile(GRAPH_FILE_FILTER, "xml", PathUtil::Make(PathUtil::GetGameFolder(), CUSTOM_ACTIONS_PATH).c_str(), filename))
		return false;

	filename.MakeLower();

	// check if file exists.
	if (CFileUtil::FileExists(filename))
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Can't create Custom Action because another Custom Action with this name already exists!\n\nCreation canceled..."));
		return false;
	}

	// Make a new graph.
	CFlowGraphManager* pManager = GetIEditorImpl()->GetFlowGraphManager();
	CRY_ASSERT(pManager != NULL);

	CHyperGraph* pGraph = pManager->CreateGraph();

	CHyperNode* pStartNode = (CHyperNode*) pGraph->CreateNode("CustomAction:Start");
	pStartNode->SetPos(Gdiplus::PointF(80, 10));
	CHyperNode* pSucceedNode = (CHyperNode*) pGraph->CreateNode("CustomAction:Succeed");
	pSucceedNode->SetPos(Gdiplus::PointF(80, 70));
	CHyperNode* pAbortNode = (CHyperNode*) pGraph->CreateNode("CustomAction:Abort");
	pAbortNode->SetPos(Gdiplus::PointF(80, 140));
	CHyperNode* pEndNode = (CHyperNode*) pGraph->CreateNode("CustomAction:End");
	pEndNode->SetPos(Gdiplus::PointF(400, 70));

	pGraph->UnselectAll();

	// Set up the default connections
	pGraph->ConnectPorts(pStartNode, &pStartNode->GetOutputs()->at(0), pEndNode, &pEndNode->GetInputs()->at(1));
	pGraph->ConnectPorts(pSucceedNode, &pSucceedNode->GetOutputs()->at(0), pEndNode, &pEndNode->GetInputs()->at(5));
	pGraph->ConnectPorts(pAbortNode, &pAbortNode->GetOutputs()->at(0), pEndNode, &pEndNode->GetInputs()->at(6));

	bool r = pGraph->Save(filename);

	ReloadCustomActionGraphs();

	return r;
}

//////////////////////////////////////////////////////////////////////////
void CCustomActionsEditorManager::GetCustomActions(std::vector<CString>& values) const
{
	ICustomActionManager* pCustomActionManager = gEnv->pGameFramework->GetICustomActionManager();
	CRY_ASSERT(pCustomActionManager != NULL);
	if (!pCustomActionManager)
		return;

	values.clear();

	const size_t numCustomActions = pCustomActionManager->GetNumberOfCustomActionsFromLibrary();
	for (size_t i = 0; i < numCustomActions; i++)
	{
		ICustomAction* pCustomAction = pCustomActionManager->GetCustomActionFromLibrary(i);
		CRY_ASSERT(pCustomAction != NULL);

		const char* szActionName = pCustomAction->GetCustomActionGraphName();
		if (szActionName)
		{
			values.push_back(szActionName);
		}
	}
}

