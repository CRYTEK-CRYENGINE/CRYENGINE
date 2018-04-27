// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialFXGraphMan.h"

#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/Controls/HyperGraphEditorWnd.h"

#include <CryAction/IMaterialEffects.h>

#define MATERIAL_FX_PATH  ("Libs/MaterialEffects/FlowGraphs")
#define GRAPH_FILE_FILTER "Graph XML Files (*.xml)|*.xml"

#include "Controls/QuestionDialog.h"

CMaterialFXGraphMan::CMaterialFXGraphMan()
{

}

CMaterialFXGraphMan::~CMaterialFXGraphMan()
{
	GetIEditorImpl()->GetLevelIndependentFileMan()->UnregisterModule(this);
}

void CMaterialFXGraphMan::Init()
{
	GetIEditorImpl()->GetLevelIndependentFileMan()->RegisterModule(this);
	ReloadFXGraphs();
}

void CMaterialFXGraphMan::ReloadFXGraphs()
{
	if (!gEnv->pMaterialEffects) return;

	ClearEditorGraphs();

	gEnv->pMaterialEffects->ReloadMatFXFlowGraphs();

	CFlowGraphManager* const pFGMgr = GetIEditorImpl()->GetFlowGraphManager();
	size_t numMatFGraphs = gEnv->pMaterialEffects->GetMatFXFlowGraphCount();
	for (size_t i = 0; i < numMatFGraphs; ++i)
	{
		string filename;
		IFlowGraphPtr pGraph = gEnv->pMaterialEffects->GetMatFXFlowGraph(i, &filename);
		CHyperFlowGraph* pFG = pFGMgr->CreateGraphForMatFX(pGraph, CString(filename.c_str()));
		assert(pFG);
		if (pFG)
		{
			pFG->AddRef();
			m_matFxGraphs.push_back(pGraph);
		}
	}
}

void CMaterialFXGraphMan::SaveChangedGraphs()
{
	if (!gEnv->pMaterialEffects) return;

	CFlowGraphManager* const pFGMgr = GetIEditorImpl()->GetFlowGraphManager();
	size_t numMatFGraphs = gEnv->pMaterialEffects->GetMatFXFlowGraphCount();
	for (size_t i = 0; i < numMatFGraphs; ++i)
	{
		IFlowGraphPtr pGraph = gEnv->pMaterialEffects->GetMatFXFlowGraph(i);
		CHyperFlowGraph* pFG = pFGMgr->FindGraph(pGraph);
		assert(pFG);
		if (pFG && pFG->IsModified())
		{
			CString filename(MATERIAL_FX_PATH);
			filename += '/';
			filename += pFG->GetName();
			filename += ".xml";
			pFG->Save(filename);
		}
	}
}

bool CMaterialFXGraphMan::HasModifications()
{
	if (!gEnv->pMaterialEffects) return false;

	CFlowGraphManager* const pFGMgr = GetIEditorImpl()->GetFlowGraphManager();
	size_t numMatFGraphs = gEnv->pMaterialEffects->GetMatFXFlowGraphCount();
	for (size_t i = 0; i < numMatFGraphs; ++i)
	{
		IFlowGraphPtr pGraph = gEnv->pMaterialEffects->GetMatFXFlowGraph(i);
		CHyperFlowGraph* pFG = pFGMgr->FindGraph(pGraph);
		assert(pFG);
		if (pFG && pFG->IsModified())
			return true;
	}
	return false;
}

bool CMaterialFXGraphMan::NewMaterialFx(CString& filename, CHyperGraph** pHyperGraph)
{
	if (!gEnv->pMaterialEffects) return false;

	CFileUtil::CreateDirectory(PathUtil::Make(PathUtil::GetGameFolder(), MATERIAL_FX_PATH));

	if (!CFileUtil::SelectSaveFile(GRAPH_FILE_FILTER, "xml", PathUtil::Make(PathUtil::GetGameFolder(), MATERIAL_FX_PATH).c_str(), filename))
		return false;

	// check if file exists.
	FILE* file = fopen(filename, "rb");
	if (file)
	{
		fclose(file);
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Can't create Material FX Graph because another Material FX Graph with this name already exists!\n\nCreation canceled..."));
		return false;
	}

	// create default MatFX Graph
	CHyperGraph* pGraph = GetIEditorImpl()->GetFlowGraphManager()->CreateGraph();

	CHyperNode* pStartNode = (CHyperNode*) pGraph->CreateNode("MaterialFX:HUDStartFX");
	pStartNode->SetPos(Gdiplus::PointF(80, 10));
	CHyperNode* pEndNode = (CHyperNode*) pGraph->CreateNode("MaterialFX:HUDEndFX");
	pEndNode->SetPos(Gdiplus::PointF(400, 10));

	pGraph->UnselectAll();
	pGraph->ConnectPorts(pStartNode, &pStartNode->GetOutputs()->at(0), pEndNode, &pEndNode->GetInputs()->at(0));

	bool saved = pGraph->Save(filename.GetString());
	delete pGraph;

	if (saved)
	{
		IFlowGraphPtr pNewGraph = gEnv->pMaterialEffects->LoadNewMatFXFlowGraph(filename.GetString());
		assert(pNewGraph);
		if (pNewGraph)
		{
			CHyperFlowGraph* pFG = GetIEditorImpl()->GetFlowGraphManager()->CreateGraphForMatFX(pNewGraph, filename);
			assert(pFG);
			if (pFG) pFG->AddRef();
			if (pHyperGraph) *pHyperGraph = pFG;
			return true;
		}
	}
	else
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Can't create Material FX Graph!\nCould not save new file!\n\nCreation canceled..."));
	}
	return false;
}

bool CMaterialFXGraphMan::PromptChanges()
{
	if (HasModifications())
	{
		CString msg(_T("Some Material FX flowgraphs are modified!\nDo you want to save your changes?"));
		QDialogButtonBox::StandardButtons result = CQuestionDialog::SQuestion(QObject::tr("Material FX Graph(s) not saved!"), msg.GetString(), QDialogButtonBox::Save | QDialogButtonBox::Discard | QDialogButtonBox::Cancel);
		if (result == QDialogButtonBox::Save)
		{
			SaveChangedGraphs();
		}
		else if (result == QDialogButtonBox::Cancel)
		{
			return false;
		}
	}
	return true;
}

void CMaterialFXGraphMan::ClearEditorGraphs()
{
	TGraphList::iterator iter = m_matFxGraphs.begin();
	for (; iter != m_matFxGraphs.end(); ++iter)
	{
		IFlowGraphPtr pGraph = *iter;
		CHyperFlowGraph* pFG = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pGraph);
		SAFE_RELEASE(pFG);
	}

	m_matFxGraphs.clear();
}

