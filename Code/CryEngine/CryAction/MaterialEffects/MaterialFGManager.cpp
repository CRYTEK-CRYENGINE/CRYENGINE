// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MaterialFGManager
//  Version:     v1.00
//  Created:     29/11/2006 by AlexL-Benito GR
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MaterialFGManager.h"
#include "MaterialEffectsCVars.h"

CMaterialFGManager::CMaterialFGManager()
{
}

CMaterialFGManager::~CMaterialFGManager()
{
	m_flowGraphVector.clear();
}

//------------------------------------------------------------------------
void CMaterialFGManager::Reset(bool bCleanUp)
{
	for (int i = 0; i < m_flowGraphVector.size(); i++)
	{
		CMaterialFGManager::SFlowGraphData& current = m_flowGraphVector[i];
		InternalEndFGEffect(&current, !bCleanUp);
	}

	if (bCleanUp)
	{
		stl::free_container(m_flowGraphVector);
	}
}

//------------------------------------------------------------------------
void CMaterialFGManager::Serialize(TSerialize ser)
{
	for (int i = 0; i < m_flowGraphVector.size(); i++)
	{
		CMaterialFGManager::SFlowGraphData& current = m_flowGraphVector[i];
		if (ser.BeginOptionalGroup(current.m_name, true))
		{
			ser.Value("run", current.m_bRunning);
			if (ser.BeginOptionalGroup("fg", true))
			{
				current.m_pFlowGraph->Serialize(ser);
				ser.EndGroup();
			}
			ser.EndGroup();
		}
	}
}

//------------------------------------------------------------------------
// CMaterialFGManager::LoadLibs()
// Iterates through all the files in the folder and load the graphs
// PARAMS
// - path : Folder where the FlowGraph xml files are located
//------------------------------------------------------------------------
bool CMaterialFGManager::LoadLibs(const char* path)
{
	LOADING_TIME_PROFILE_SECTION;
	if (gEnv->pFlowSystem == 0)
		return false;

	Reset(true);

	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	int numLoaded = 0;

	string realPath(path);
	realPath.TrimRight("/\\");
	string search(realPath);
	search += "/*.xml";

	intptr_t handle = pCryPak->FindFirst(search.c_str(), &fd);
	if (handle != -1)
	{
		do
		{
			// fd.name contains the profile name
			string filename = realPath;
			filename += "/";
			filename += fd.name;
			bool ok = LoadFG(filename);
			if (ok)
				++numLoaded;
			SLICE_AND_SLEEP();
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}

	return numLoaded > 0;
}

//------------------------------------------------------------------------
// CMaterialFGManager::LoadFG()
// Here is where the FlowGraph is loaded, storing a pointer to it, its name
// and also the IDs of the special "start" and "end" nodes
// PARAMS
// - filename: ...
//------------------------------------------------------------------------
bool CMaterialFGManager::LoadFG(const string& filename, IFlowGraphPtr* pGraphRet /*= NULL*/)
{
	//Create FG from the XML file
	XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(filename);
	if (rootNode == 0)
		return false;
	IFlowGraphPtr pFlowGraph = gEnv->pFlowSystem->CreateFlowGraph();
	if (pFlowGraph->SerializeXML(rootNode, true) == false)
	{
		// give warning
		GameWarning("MaterialFGManager on LoadFG(%s)==> FlowGraph->SerializeXML failed", filename.c_str());
		return false;
	}

	//Deactivated it by default
	pFlowGraph->SetEnabled(false);
	pFlowGraph->SetType(IFlowGraph::eFGT_MaterialFx);

#ifndef _RELEASE
	stack_string debugName = "[Material FX] ";
	debugName.append(PathUtil::GetFileName(filename).c_str());
	pFlowGraph->SetDebugName(debugName);
#endif

	const TFlowNodeId nodeTypeId_StartFX = gEnv->pFlowSystem->GetTypeId("MaterialFX:HUDStartFX");
	const TFlowNodeId nodeTypeId_EndFX = gEnv->pFlowSystem->GetTypeId("MaterialFX:HUDEndFX");

	//Store needed data...
	SFlowGraphData fgData;
	fgData.m_pFlowGraph = pFlowGraph;

	// search for start and end nodes
	IFlowNodeIteratorPtr pNodeIter = pFlowGraph->CreateNodeIterator();
	TFlowNodeId nodeId;
	while (IFlowNodeData* pNodeData = pNodeIter->Next(nodeId))
	{
		if (pNodeData->GetNodeTypeId() == nodeTypeId_StartFX)
		{
			fgData.m_startNodeId = nodeId;
		}
		else if (pNodeData->GetNodeTypeId() == nodeTypeId_EndFX)
		{
			fgData.m_endNodeId = nodeId;
		}
	}

	if (fgData.m_startNodeId == InvalidFlowNodeId || fgData.m_endNodeId == InvalidFlowNodeId)
	{
		// warning no start/end node found
		GameWarning("MaterialFGManager on LoadFG(%s) ==> No Start/End node found", filename.c_str());
		return false;
	}

	//Finally store the name
	fgData.m_name = PathUtil::GetFileName(filename);
	fgData.m_fileName = filename;
	PathUtil::RemoveExtension(fgData.m_name);
	m_flowGraphVector.push_back(fgData);

	// send initialize event to allow for resource caching in game
	if (gEnv->pCryPak->GetLvlResStatus() && !gEnv->IsEditor())
	{
		SFlowGraphData* pFGData = FindFG(pFlowGraph);
		if (pFGData)
			InternalEndFGEffect(pFGData, true);
	}
	if (pGraphRet)
		*pGraphRet = m_flowGraphVector.rbegin()->m_pFlowGraph;
	return true;
}

//------------------------------------------------------------------------
// CMaterialFGManager::FindFG(const string& fgName)
// Find a FlowGraph by name
// PARAMS
// - fgName: Name of the FlowGraph
//------------------------------------------------------------------------
CMaterialFGManager::SFlowGraphData* CMaterialFGManager::FindFG(const string& fgName)
{
	std::vector<SFlowGraphData>::iterator iter = m_flowGraphVector.begin();
	std::vector<SFlowGraphData>::iterator iterEnd = m_flowGraphVector.end();
	while (iter != iterEnd)
	{
		if (fgName.compareNoCase(iter->m_name) == 0)
			return &*iter;
		++iter;
	}
	return 0;
}

//------------------------------------------------------------------------
// CMaterialFGManager::FindFG(IFlowGraphPtr pFG)
// Find a FlowGraph "by pointer"
// PARAMS
// - pFG: FlowGraph pointer
//------------------------------------------------------------------------
CMaterialFGManager::SFlowGraphData* CMaterialFGManager::FindFG(IFlowGraphPtr pFG)
{
	std::vector<SFlowGraphData>::iterator iter = m_flowGraphVector.begin();
	std::vector<SFlowGraphData>::iterator iterEnd = m_flowGraphVector.end();
	while (iter != iterEnd)
	{
		if (iter->m_pFlowGraph == pFG)
			return &*iter;
		++iter;
	}
	return 0;
}

//------------------------------------------------------------------------
// CMaterialFGManager::StartFGEffect(const string& fgName)
// Activate the MaterialFX:StartHUDEffect node, this way the FG will be executed
// PARAMS
// - fgName: Name of the flowgraph effect
//------------------------------------------------------------------------
bool CMaterialFGManager::StartFGEffect(const SMFXFlowGraphParams& fgParams, float curDistance)
{
	SFlowGraphData* pFGData = FindFG(fgParams.fgName);
	if (pFGData == 0)
	{
		GameWarning("CMaterialFXManager::StartFXEffect: Can't execute FG '%s'", fgParams.fgName.c_str());
	}
	else if (/*pFGData && */ !pFGData->m_bRunning)
	{
		IFlowGraphPtr pFG = pFGData->m_pFlowGraph;
		pFG->SetEnabled(true);
		pFG->InitializeValues();

		TFlowInputData data;

		// set distance output port [1]
		pFG->ActivatePort(SFlowAddress(pFGData->m_startNodeId, 1, true), curDistance);
		// set custom params [2] - [3]
		pFG->ActivatePort(SFlowAddress(pFGData->m_startNodeId, 2, true), fgParams.params[0]);
		pFG->ActivatePort(SFlowAddress(pFGData->m_startNodeId, 3, true), fgParams.params[1]);

		// set intensity (dynamically adjusted from game if needed) [4]
		pFG->ActivatePort(SFlowAddress(pFGData->m_startNodeId, 4, true), 1.0f);

		// set input port [0]
		pFG->ActivatePort(SFlowAddress(pFGData->m_startNodeId, 0, false), data);  // port0: InputPortConfig_Void ("Trigger")
		//data = fgName;
		//pFG->ActivatePort(SFlowAddress(pFGData->m_endNodeId, 0, false), data);		// port0: InputPortConfig<string> ("FGName")
		pFGData->m_bRunning = true;

		if (CMaterialEffectsCVars::Get().mfx_DebugFlowGraphFX != 0)
		{
			GameWarning("Material FlowGraphFX manager: Effect %s was triggered.", fgParams.fgName.c_str());
		}

		return true;
	}
	return false;
}

//------------------------------------------------------------------------
// CMaterialFGManager::EndFGEffect(const string& fgName)
// This method will be automatically called when the effect finish
// PARAMS
// - fgName: Name of the FlowGraph
//------------------------------------------------------------------------
bool CMaterialFGManager::EndFGEffect(const string& fgName)
{
	SFlowGraphData* pFGData = FindFG(fgName);
	if (pFGData)
		return InternalEndFGEffect(pFGData, false);
	return false;
}

//===========================================================
// CMaterialFGManager::SetFGCustomParameter(const SMFXFlowGraphParams& fgParams, const char* customParameter, const SMFXCustomParamValue& customParameterValue)
// This method allow for setting some custom, and dynamically updated outputs from the game, to adjust the FX
// PARAMS
//  - fgParams: Flow graph parameters
//  - customParameter: Name of the custom parameter to adjust
//  - customParameterValue: Contains new value for customParameter
//------------------------------------------------------------------------
void CMaterialFGManager::SetFGCustomParameter(const SMFXFlowGraphParams& fgParams, const char* customParameter, const SMFXCustomParamValue& customParameterValue)
{
	SFlowGraphData* pFGData = FindFG(fgParams.fgName);
	if (pFGData == 0)
	{
		GameWarning("CMaterialFXManager::StartFXEffect: Can't execute FG '%s'", fgParams.fgName.c_str());
	}
	else if (pFGData->m_bRunning)
	{
		IFlowGraphPtr pFG = pFGData->m_pFlowGraph;

		if (stricmp(customParameter, "Intensity") == 0)
		{
			// set intensity (dynamically adjusted from game if needed) [4]
			pFG->ActivatePort(SFlowAddress(pFGData->m_startNodeId, 4, true), customParameterValue.fValue);
		}
		else if (stricmp(customParameter, "BlendOutTime") == 0)
		{
			//Activate blend out timer [5]
			pFG->ActivatePort(SFlowAddress(pFGData->m_startNodeId, 5, true), customParameterValue.fValue);
		}

		if (CMaterialEffectsCVars::Get().mfx_DebugFlowGraphFX != 0)
		{
			GameWarning("Material FlowGraphFX manager: Effect '%s' .Dynamic parameter '%s' set to value %.3f", fgParams.fgName.c_str(), customParameter, customParameterValue.fValue);
		}
	}
}
//------------------------------------------------------------------------
// CMaterialFGManager::EndFGEffect(IFlowGraphPtr pFG)
// This method will be automatically called when the effect finish
// PARAMS
// - pFG: Pointer to the FlowGraph
//------------------------------------------------------------------------
bool CMaterialFGManager::EndFGEffect(IFlowGraphPtr pFG)
{
	SFlowGraphData* pFGData = FindFG(pFG);
	if (pFGData)
		return InternalEndFGEffect(pFGData, false);
	return false;
}

//------------------------------------------------------------------------
// internal method to end effect. can also initialize values
//------------------------------------------------------------------------
bool CMaterialFGManager::InternalEndFGEffect(SFlowGraphData* pFGData, bool bInitialize)
{
	if (pFGData == 0)
		return false;
	if (pFGData->m_bRunning || bInitialize)
	{
		IFlowGraphPtr pFG = pFGData->m_pFlowGraph;
		if (bInitialize)
		{
			if (pFG->IsEnabled() == false)
			{
				pFG->SetEnabled(true);
				pFG->InitializeValues();
			}
		}
		pFG->SetEnabled(false);
		pFGData->m_bRunning = false;
		return true;
	}
	return false;
}

void CMaterialFGManager::PreLoad()
{
#ifndef _RELEASE

	// need to preload the missing FX for CScriptBind_Entity::PreLoadParticleEffect
	//	gEnv->pParticleManager->FindEffect( "MissingFGEffect", "MissingFGEffect" );

#endif

	TFlowGraphData::iterator iBegin = m_flowGraphVector.begin();
	TFlowGraphData::const_iterator iEnd = m_flowGraphVector.end();
	for (TFlowGraphData::iterator i = iBegin; i != iEnd; ++i)
	{
		i->m_pFlowGraph->PrecacheResources();
	}
}

//------------------------------------------------------------------------
// CMaterialFGManager::EndFGEffect(IFlowGraphPtr pFG)
// This method will be automatically called when the effect finish
// PARAMS
// - pFG: Pointer to the FlowGraph
//------------------------------------------------------------------------
bool CMaterialFGManager::IsFGEffectRunning(const string& fgName)
{
	SFlowGraphData* pFGData = FindFG(fgName);
	if (pFGData)
		return pFGData->m_bRunning;
	return false;
}

//------------------------------------------------------------------------
// CMaterialFGManager::ReloadFlowGraphs
// Reload the FlowGraphs (invoked through console command, see MaterialEffectsCVars.cpp)
//------------------------------------------------------------------------
void CMaterialFGManager::ReloadFlowGraphs()
{
	LoadLibs();
}

void CMaterialFGManager::GetMemoryUsage(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "flowgraphs");
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_flowGraphVector);
}

size_t CMaterialFGManager::GetFlowGraphCount() const
{
	return m_flowGraphVector.size();
}

IFlowGraphPtr CMaterialFGManager::GetFlowGraph(int index, string* pFileName /*= NULL*/) const
{
	if (index >= 0 && index < GetFlowGraphCount())
	{
		const SFlowGraphData& fgData = m_flowGraphVector[index];
		if (pFileName)
			*pFileName = fgData.m_fileName;
		return fgData.m_pFlowGraph;
	}
	return NULL;
}
