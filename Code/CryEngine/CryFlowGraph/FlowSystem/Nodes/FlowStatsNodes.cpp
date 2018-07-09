// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 30:6:2005   18:43 : Created by Mrcio Martins
   - 21:10:2008			Mario Silva added extended version of the FrameStats node

*************************************************************************/
#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CrySystem/IStreamEngine.h>

class CFlowNode_MemoryStats : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum Outputs
	{
		OUT_SYSMEM,
		OUT_VIDEOMEM_THISFRAME,
		OUT_VIDEOMEM_RECENTLY,
		OUT_MESHMEM,
	};
	CFlowNode_MemoryStats(SActivationInfo* pActInfo)
	{
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("sysmem"),
			OutputPortConfig<int>("videomem_thisframe"),
			OutputPortConfig<int>("videomem_recently"),
			OutputPortConfig<int>("meshmem"),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_DEBUG);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			break;
		case eFE_Update:
			{
				ISystem* pSystem = GetISystem();
				IRenderer* pRenderer = gEnv->pRenderer;

				int sysMem = pSystem->GetUsedMemory();
				size_t vidMemThisFrame(0);
				size_t vidMemRecently(0);
				pRenderer->GetVideoMemoryUsageStats(vidMemThisFrame, vidMemRecently);
				int meshMem = 0;
				pRenderer->EF_Query(EFQ_Alloc_APIMesh, meshMem);

				ActivateOutput(pActInfo, OUT_SYSMEM, sysMem);
				// potentially unsafe if we start using >2gb of video memory...?
				ActivateOutput(pActInfo, OUT_VIDEOMEM_THISFRAME, int(vidMemThisFrame));
				ActivateOutput(pActInfo, OUT_VIDEOMEM_RECENTLY, int(vidMemRecently));
				ActivateOutput(pActInfo, OUT_MESHMEM, meshMem);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_FrameStats : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum Outputs
	{
		OUT_FRAMETIME,
		OUT_FRAMERATE,
		OUT_FRAMEID,
	};
	CFlowNode_FrameStats(SActivationInfo* pActInfo)
	{
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("frametime"),
			OutputPortConfig<float>("framerate"),
			OutputPortConfig<int>("frameid"),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_DEBUG);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			break;
		case eFE_Update:
			{
				ISystem* pSystem = GetISystem();
				IRenderer* pRenderer = gEnv->pRenderer;

				float frameTime = gEnv->pTimer->GetFrameTime();
				float frameRate = gEnv->pTimer->GetFrameRate();
				int frameId = pRenderer->GetFrameID(false);

				ActivateOutput(pActInfo, OUT_FRAMETIME, frameTime);
				ActivateOutput(pActInfo, OUT_FRAMERATE, frameRate);
				ActivateOutput(pActInfo, OUT_FRAMEID, frameId);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Extended version of the FrameStats node
class CFlowNode_FrameStatsEx : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputs
	{
		eIN_Start,
		eIN_Stop,
		eIN_Reset,
	};
	enum EOutputs
	{
		eOUT_FrameTime,
		eOUT_FrameRate,
		eOUT_FrameID,
		eOUT_Min,
		eOUT_Max,
		eOUT_Average,
	};

public:
	CFlowNode_FrameStatsEx(SActivationInfo* pActInfo)
	{
		Reset();
	};

	~CFlowNode_FrameStatsEx()
	{
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("Start", _HELP("Trigger to start gathering the frame rate data, it also resets the previous values")),
			InputPortConfig_AnyType("Stop",  _HELP("Trigger to stop gathering the frame rate data")),
			InputPortConfig_AnyType("Reset", _HELP("Trigger to reset the frame rate data")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("FrameTime",        _HELP("Current frame time")),
			OutputPortConfig<float>("FrameRate",        _HELP("Current frame rate")),
			OutputPortConfig<int>("FrameId",            _HELP("Current frame id")),
			OutputPortConfig<float>("MinFrameRate",     _HELP("Minimum frame rate")),
			OutputPortConfig<float>("MaxFrameRate",     _HELP("Maximum frame rate")),
			OutputPortConfig<float>("AverageFrameRate", _HELP("Average frame rate")),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("When activated, this node outputs the current frame rate data and the lowest, highest and the average frame rate.");
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CRY_ASSERT(pActInfo != NULL);

		switch (event)
		{
		case eFE_Initialize:
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				Reset();
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIN_Start))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				}
				if (IsPortActive(pActInfo, eIN_Stop))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				}
				if (IsPortActive(pActInfo, eIN_Reset))
				{
					Reset();
				}
			}
			break;

		case eFE_Update:
			{
				IRenderer* pRenderer = gEnv->pRenderer;

				float fFrameTime = gEnv->pTimer->GetFrameTime();
				float fFrameRate = gEnv->pTimer->GetFrameRate();
				int fFrameId = pRenderer->GetFrameID(false);

				m_fMinFrameRate = min(fFrameRate, m_fMinFrameRate);
				m_fMaxFrameRate = max(fFrameRate, m_fMaxFrameRate);
				m_fSumFrameRate += fFrameRate;
				++m_lFrameCounter;

				ActivateOutput(pActInfo, eOUT_FrameTime, fFrameTime);
				ActivateOutput(pActInfo, eOUT_FrameRate, fFrameRate);
				ActivateOutput(pActInfo, eOUT_FrameID, fFrameId);
				ActivateOutput(pActInfo, eOUT_Min, m_fMinFrameRate);
				ActivateOutput(pActInfo, eOUT_Max, m_fMaxFrameRate);
				ActivateOutput(pActInfo, eOUT_Average, (m_fSumFrameRate / m_lFrameCounter));
			}
			break;

		default:
			break;
		}
	}

	void Reset()
	{
		m_fMinFrameRate = FLT_MAX;
		m_fMaxFrameRate = 0.0f;
		m_fSumFrameRate = 0.0f;
		m_lFrameCounter = 0;
	}

	virtual void GetMemoryUsage(ICrySizer* crySizer) const
	{
		crySizer->Add(*this);
	}

private:
	float         m_fMinFrameRate;
	float         m_fMaxFrameRate;
	float         m_fSumFrameRate;
	unsigned long m_lFrameCounter;
};

class CFlowNode_TextureStreamingStats : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum Outputs
	{
		OUT_REQUESTCOUNT,
	};
	CFlowNode_TextureStreamingStats(SActivationInfo * pActInfo)
	{
	}
	virtual void GetConfiguration(SFlowNodeConfig &config)
	{
		static const SInputPortConfig in_config[] = {
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("CurrentRequestCount"),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo *pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			break;
		case eFE_Update:
			{
				IRenderer* pRenderer = gEnv->pRenderer;
				STextureStreamingStats statsTex(false);
				pRenderer->EF_Query(EFQ_GetTexStreamingInfo, statsTex);

				ActivateOutput(pActInfo, OUT_REQUESTCOUNT, int(statsTex.nNumStreamingRequests));
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Debug:Memory", CFlowNode_MemoryStats);
REGISTER_FLOW_NODE("Debug:Frame", CFlowNode_FrameStats);
REGISTER_FLOW_NODE("Debug:FrameExtended", CFlowNode_FrameStatsEx);
REGISTER_FLOW_NODE("Debug:TextureStreaming", CFlowNode_TextureStreamingStats);
