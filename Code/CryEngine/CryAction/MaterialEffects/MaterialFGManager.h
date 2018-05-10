// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MaterialFGManager
//  Version:     v1.00
//  Created:     29/11/2006 by AlexL-Benito GR
//  Compilers:   Visual Studio.NET
//  Description: This class manage all the FlowGraph HUD effects related to a
//								material FX.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MATERIALFGMANAGER_H__
#define __MATERIALFGMANAGER_H__

#pragma once

#include <CryFlowGraph/IFlowSystem.h>
#include "MaterialEffects/MFXFlowGraphEffect.h"

class CMaterialFGManager
{
public:
	CMaterialFGManager();
	virtual ~CMaterialFGManager();

	// load FlowGraphs from specified path
	bool LoadLibs(const char* path = "Libs/MaterialEffects/FlowGraphs");

	// reset (deactivate all FlowGraphs)
	void Reset(bool bCleanUp);

	// serialize
	void          Serialize(TSerialize ser);

	bool          StartFGEffect(const SMFXFlowGraphParams& fgParams, float curDistance);
	bool          EndFGEffect(const string& fgName);
	bool          EndFGEffect(IFlowGraphPtr pFG);
	bool          IsFGEffectRunning(const string& fgName);
	void          SetFGCustomParameter(const SMFXFlowGraphParams& fgParams, const char* customParameter, const SMFXCustomParamValue& customParameterValue);

	void          ReloadFlowGraphs();

	size_t        GetFlowGraphCount() const;
	IFlowGraphPtr GetFlowGraph(int index, string* pFileName = NULL) const;
	bool          LoadFG(const string& filename, IFlowGraphPtr* pGraphRet = NULL);

	void          GetMemoryUsage(ICrySizer* s) const;

	void          PreLoad();

protected:
	struct SFlowGraphData
	{
		SFlowGraphData()
		{
			m_startNodeId = InvalidFlowNodeId;
			m_endNodeId = InvalidFlowNodeId;
			m_bRunning = false;
		}

		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(m_name);
		}
		string        m_name;
		string        m_fileName;
		IFlowGraphPtr m_pFlowGraph;
		TFlowNodeId   m_startNodeId;
		TFlowNodeId   m_endNodeId;
		bool          m_bRunning;
	};

	SFlowGraphData* FindFG(const string& fgName);
	SFlowGraphData* FindFG(IFlowGraphPtr pFG);
	bool            InternalEndFGEffect(SFlowGraphData* pFGData, bool bInitialize);

protected:

	typedef std::vector<SFlowGraphData> TFlowGraphData;
	TFlowGraphData m_flowGraphVector;     //List of FlowGraph Effects
};

#endif
