// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGraphModuleManager.h"

#include "FlowGraphManager.h"
#include "Controls/HyperGraphEditorWnd.h"

#include <CryFlowGraph/IFlowGraphDebugger.h>
#include "Controls/QuestionDialog.h"


//////////////////////////////////////////////////////////////////////////
CEditorFlowGraphModuleManager::CEditorFlowGraphModuleManager()
{
}

//////////////////////////////////////////////////////////////////////////
CEditorFlowGraphModuleManager::~CEditorFlowGraphModuleManager()
{
	GetIEditorImpl()->GetLevelIndependentFileMan()->UnregisterModule(this);

	IFlowGraphModuleManager* pModuleManager = gEnv->pFlowSystem->GetIModuleManager();
	if (pModuleManager)
	{
		pModuleManager->UnregisterListener(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorFlowGraphModuleManager::Init()
{
	GetIEditorImpl()->GetLevelIndependentFileMan()->RegisterModule(this);

	if (gEnv->pFlowSystem == NULL)
		return;

	// get modules from engine module manager, create editor-side graphs to match
	IFlowGraphModuleManager* pModuleManager = gEnv->pFlowSystem->GetIModuleManager();
	if (pModuleManager == NULL)
		return;

	pModuleManager->RegisterListener(this, "CEditorFlowGraphModuleManager");

	CreateEditorFlowgraphs();
}

//////////////////////////////////////////////////////////////////////////
void CEditorFlowGraphModuleManager::SaveChangedGraphs()
{
	for (int i = 0; i < m_FlowGraphs.size(); ++i)
	{
		IFlowGraphPtr pGraph = GetModuleFlowGraph(i);
		CHyperFlowGraph* pFG = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pGraph);
		assert(pFG);
		if (pFG && pFG->IsModified())
		{
			SaveModuleGraph(pFG);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorFlowGraphModuleManager::DiscardChangedGraphs()
{
	for (int i = 0; i < m_FlowGraphs.size(); ++i)
	{
		IFlowGraphPtr pGraph = GetModuleFlowGraph(i);
		CHyperFlowGraph* pFG = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pGraph);
		assert(pFG);
		if (pFG && pFG->IsModified())
		{
			pFG->Reload(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEditorFlowGraphModuleManager::SaveModuleGraph(CHyperFlowGraph* pFG)
{
	// Saves from the editor have to save the editor flowgraph (not the game side one as
	//	it is not updated with new nodes). So allow the game-side module manager to
	//	add the ports to the xml from the editor flowgraph, then write to file.

	const char* filename = gEnv->pFlowSystem->GetIModuleManager()->GetModulePath(pFG->GetName());

	XmlNodeRef rootNode = gEnv->pSystem->CreateXmlNode("Graph");

	IFlowGraphModuleManager* pModuleManager = gEnv->pFlowSystem->GetIModuleManager();
	if (pModuleManager)
	{
		pModuleManager->SaveModule(pFG->GetName(), rootNode);

		pFG->Serialize(rootNode, false);

		if (XmlHelpers::SaveXmlNode(rootNode, filename))
		{
			return true;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to save modified flowgraph module '%s' to '%s'", pFG->GetName(), filename);
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorFlowGraphModuleManager::HasModifications()
{
	if (!gEnv->pFlowSystem)
		return false;

	IFlowGraphModuleManager* pModuleManager = gEnv->pFlowSystem->GetIModuleManager();

	if (pModuleManager == NULL)
		return false;

	for (int i = 0; i < m_FlowGraphs.size(); ++i)
	{
		IFlowGraphPtr pGraph = GetModuleFlowGraph(i);
		CHyperFlowGraph* pFG = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pGraph);
		assert(pFG);
		if (pFG && pFG->IsModified())
			return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorFlowGraphModuleManager::NewModule(string& filename, bool bGlobal /*=true*/, CHyperGraph** pHyperGraph)
{
	IFlowGraphModuleManager* pModuleManager = gEnv->pFlowSystem->GetIModuleManager();
	if (!pModuleManager)
		return false;


	CryPathString path(PathUtil::Make(
		bGlobal ? PathUtil::GetGameFolder().c_str() : GetIEditorImpl()->GetLevelFolder(),
		bGlobal ? pModuleManager->GetGlobalModulesPath() : pModuleManager->GetLevelModulesPath()));

	CFileUtil::CreateDirectory(PathUtil::AddSlash(path));
	if (!CFileUtil::SelectSaveFile("Graph XML Files (*.xml)|*.xml", "xml", path, filename))
		return false;


	// create (temporary) default graph for module
	CHyperGraph* pGraph = GetIEditorImpl()->GetFlowGraphManager()->CreateGraph();
	pGraph->AddRef();

	stack_string moduleName = PathUtil::GetFileName(filename);

	if (IFlowGraphModule* pModule = pModuleManager->GetModule(moduleName))
	{
		CString msg;
		msg.Format(_T("Flowgraph Module: %s already exist. %s Module"), moduleName, pModule->GetType() == IFlowGraphModule::eT_Global ? "Global" : "Level");
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, msg);
		return false;
	}

	// serialize graph to file, with module flag set
	XmlNodeRef root = gEnv->pSystem->CreateXmlNode("Graph");
	root->setAttr("isModule", true);
	root->setAttr("moduleName", moduleName);
	pGraph->Serialize(root, false);
	bool saved = root->saveToFile(filename.GetString());

	pGraph->Release();
	pGraph = nullptr;

	if (saved)
	{
		// load module into module manager
		IFlowGraphModule* const pModule = pModuleManager->LoadModuleFile(moduleName, filename.GetString(), bGlobal);
		CFlowGraphManager* const pFGmgr = GetIEditorImpl()->GetFlowGraphManager();

		CHyperFlowGraph* const pFG = pFGmgr->FindGraph(pModule->GetRootGraph());
		if (pHyperGraph)
			*pHyperGraph = pFG;

		// now create a start node
		string startName = pModuleManager->GetStartNodeName(moduleName);
		pFGmgr->UpdateNodePrototypeWithoutReload(startName);
		CHyperNode* pStartNode = (CHyperNode*)pFG->CreateNode(startName.c_str());
		pStartNode->SetPos(Gdiplus::PointF(80, 10));

		// and end node
		string returnName = pModuleManager->GetReturnNodeName(moduleName);
		pFGmgr->UpdateNodePrototypeWithoutReload(returnName);
		CHyperNode* pReturnNode = (CHyperNode*)pFG->CreateNode(returnName.c_str());
		pReturnNode->SetPos(Gdiplus::PointF(400, 10));

		// and connect them
		pFG->UnselectAll();
		pFG->ConnectPorts(pStartNode, &pStartNode->GetOutputs()->at(1), pReturnNode, &pReturnNode->GetInputs()->at(0));
		pFG->ConnectPorts(pStartNode, &pStartNode->GetOutputs()->at(3), pReturnNode, &pReturnNode->GetInputs()->at(1));

		// register the also the call node in the flowsystem
		string callName = pModuleManager->GetCallerNodeName(moduleName);
		pFGmgr->UpdateNodePrototypeWithoutReload(callName);

		return SaveModuleGraph(pFG);
	}
	else
	{
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Can't create Flowgraph Module!\nCould not save new file!\n\nCreation canceled..."));
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorFlowGraphModuleManager::PromptChanges()
{
	if (HasModifications())
	{
		string msg(_T("Some Flowgraph Modules were modified!\nDo you want to save your changes?"));
		QDialogButtonBox::StandardButtons result = CQuestionDialog::SQuestion(QObject::tr("Flowgraph Module(s) not saved!"), msg.GetString(), QDialogButtonBox::Yes | QDialogButtonBox::Discard | QDialogButtonBox::Cancel);
		if (result == QDialogButtonBox::Yes)
		{
			SaveChangedGraphs();
		}
		else if (result == QDialogButtonBox::Discard)
		{
			DiscardChangedGraphs();
		}
		else if (result == QDialogButtonBox::Cancel)
		{
			return false;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
IFlowGraphPtr CEditorFlowGraphModuleManager::GetModuleFlowGraph(int index) const
{
	if (index >= 0 && index < m_FlowGraphs.size())
	{
		CHyperFlowGraph* pGraph = m_FlowGraphs[index];

		if (pGraph == NULL)
			return NULL;

		return pGraph->GetIFlowGraph();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IFlowGraphPtr CEditorFlowGraphModuleManager::GetModuleFlowGraph(const char* name) const
{
	assert(name != NULL);
	if (name == NULL)
		return NULL;

	for (TFlowGraphs::const_iterator iter = m_FlowGraphs.begin(); iter != m_FlowGraphs.end(); ++iter)
	{
		CHyperFlowGraph* pGraph = (*iter);
		if (pGraph == NULL)
			continue;

		if (string(pGraph->GetName()).MakeUpper() == string(name).MakeUpper())
			return pGraph->GetIFlowGraph();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEditorFlowGraphModuleManager::DeleteModuleFlowGraph(CHyperFlowGraph* pGraph)
{
	assert(pGraph != NULL);
	if (!pGraph)
		return;

	// 1. delete all caller nodes used in any of the flowgraphs
	// 2. delete the CHyperFlowGraph from the internal list
	// 3. release our CHyperFlowGraph reference
	if (IFlowGraphModuleManager* pModuleManager = gEnv->pFlowSystem->GetIModuleManager())
	{
		const char* moduleGraphName = pGraph->GetName();

		if (CFlowGraphManager* pFlowGraphManager = GetIEditorImpl()->GetFlowGraphManager())
		{
			const size_t numFlowgraphs = pFlowGraphManager->GetFlowGraphCount();
			const char* callerNodeName = pModuleManager->GetCallerNodeName(moduleGraphName);

			for (size_t i = 0; i < numFlowgraphs; ++i)
			{
				CHyperFlowGraph* pFlowgraph = pFlowGraphManager->GetFlowGraph(i);
				IHyperGraphEnumerator* pEnum = pFlowgraph->GetNodesEnumerator();

				for (IHyperNode* pINode = pEnum->GetFirst(); pINode; )
				{
					CHyperNode* pHyperNode = reinterpret_cast<CHyperNode*>(pINode);
					if (!stricmp(pHyperNode->GetClassName(), callerNodeName))
					{
						IHyperNode* pTempNode = pEnum->GetNext();
						pFlowgraph->RemoveNode(pINode);
						pINode = pTempNode;
					}
					else
						pINode = pEnum->GetNext();
				}
				pEnum->Release();
			}
		}
	}

	if (stl::find_and_erase(m_FlowGraphs, pGraph))
		SAFE_RELEASE(pGraph);
}

//////////////////////////////////////////////////////////////////////////
void CEditorFlowGraphModuleManager::CreateModuleNodes(const char* moduleName)
{
	IFlowGraphModuleManager* pModuleManager = gEnv->pFlowSystem->GetIModuleManager();
	IFlowGraphModule* pModule = pModuleManager->GetModule(moduleName);
	if (pModule)
	{
		pModuleManager->CreateModuleNodes(moduleName, pModule->GetId());
		CFlowGraphManager* const pFGmgr = GetIEditorImpl()->GetFlowGraphManager();
		pFGmgr->UpdateNodePrototypeWithoutReload(pModuleManager->GetStartNodeName(moduleName));
		pFGmgr->UpdateNodePrototypeWithoutReload(pModuleManager->GetReturnNodeName(moduleName));
		pFGmgr->UpdateNodePrototypeWithoutReload(pModuleManager->GetCallerNodeName(moduleName));
		pFGmgr->ReloadGraphs();
	}
}

void CEditorFlowGraphModuleManager::OnModuleDestroyed(IFlowGraphModule* module)
{
	IFlowGraphPtr pRootGraph = module->GetRootGraph();
	CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(pRootGraph);

	if (pFlowgraph)
	{
		DeleteModuleFlowGraph(pFlowgraph);
	}
}

void CEditorFlowGraphModuleManager::OnRootGraphChanged(IFlowGraphModule* module, ERootGraphChangeReason reason)
{
	LOADING_TIME_PROFILE_SECTION_ARGS(module->GetName());

	if (reason == ERootGraphChangeReason::ScanningForModules)
	{
		// Editor Graphs are created in bulk in CreateEditorFlowgraphs in a separate step while scanning.
		// It is only needed to create a new editor FG when a single module is created explicitly from the interface
		return;
	}

	if (GetIEditorImpl()->GetFlowGraphManager()->FindGraph(module->GetRootGraph())) // already exists
		return;


	CHyperFlowGraph* pFG = GetIEditorImpl()->GetFlowGraphManager()->CreateEditorGraphForModule(module);
	assert(pFG);
	if (pFG)
	{
		pFG->AddRef();
		m_FlowGraphs.push_back(pFG);
	}
}

void CEditorFlowGraphModuleManager::OnModulesScannedAndReloaded()
{
	// Reload the classes in case the new modules registered node types.
	GetIEditorImpl()->GetFlowGraphManager()->ReloadClasses();

	LOADING_TIME_PROFILE_SECTION;
	CFlowGraphManager* const pFlowMan = GetIEditorImpl()->GetFlowGraphManager();
	pFlowMan->SetGUIControlsProcessEvents(false, false);

	CreateEditorFlowgraphs();

	pFlowMan->SetGUIControlsProcessEvents(true, true);
}

void CEditorFlowGraphModuleManager::CreateEditorFlowgraphs()
{
	LOADING_TIME_PROFILE_SECTION;

	// TODO: possible optimization - separate global and level module loading to reload only what is necessary

	// clear all existing graphs
	if (!m_FlowGraphs.empty())
	{
		CWnd* pWnd = GetIEditorImpl()->FindView("Flow Graph");
		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
		{
			static_cast<CHyperGraphDialog*>(pWnd)->SetGraph(nullptr);
		}

		for (TFlowGraphs::iterator iter = m_FlowGraphs.begin(); iter != m_FlowGraphs.end(); ++iter)
		{
			SAFE_RELEASE((*iter))
		}
		m_FlowGraphs.clear();
	}

	// create new graphs for all modules
	IModuleIteratorPtr pIter = gEnv->pFlowSystem->GetIModuleManager()->CreateModuleIterator();

	const size_t numModules = pIter->Count();
	if (numModules == 0)
		return;
	m_FlowGraphs.reserve(numModules);

	for (IFlowGraphModule* pModule; pModule = pIter->Next();)
	{
		CHyperFlowGraph* pFG = GetIEditorImpl()->GetFlowGraphManager()->CreateEditorGraphForModule(pModule);
		assert(pFG);
		if (pFG)
		{
			pFG->AddRef();
			m_FlowGraphs.push_back(pFG);
		}
	}

}

