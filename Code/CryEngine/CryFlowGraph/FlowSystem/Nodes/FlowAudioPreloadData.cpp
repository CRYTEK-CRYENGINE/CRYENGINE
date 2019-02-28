// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_AudioPreloadData final : public CFlowBaseNode<eNCT_Instanced>
{
public:

	explicit CFlowNode_AudioPreloadData(SActivationInfo* pActInfo)
		: m_bEnable(false)
	{}

	virtual ~CFlowNode_AudioPreloadData() override = default;

	CFlowNode_AudioPreloadData(CFlowNode_AudioPreloadData const&) = delete;
	CFlowNode_AudioPreloadData(CFlowNode_AudioPreloadData&&) = delete;
	CFlowNode_AudioPreloadData& operator=(CFlowNode_AudioPreloadData const&) = delete;
	CFlowNode_AudioPreloadData& operator=(CFlowNode_AudioPreloadData&&) = delete;

	virtual IFlowNodePtr        Clone(SActivationInfo* pActInfo) override
	{
		return new CFlowNode_AudioPreloadData(pActInfo);
	}

	enum INPUTS
	{
		eIn_PreloadRequestFirst = 0,
		eIn_PreloadRequest1,
		eIn_PreloadRequest2,
		eIn_PreloadRequest3,
		eIn_PreloadRequest4,
		eIn_PreloadRequest5,
		eIn_PreloadRequest6,
		eIn_PreloadRequestLast,
		eIn_Enable,
		eIn_Disable,
	};

	enum OUTPUTS
	{
	};

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<string>("audioPreloadRequest_PreloadRequest0", _HELP("name of preload request"),                "Preload Request"),
			InputPortConfig<string>("audioPreloadRequest_PreloadRequest1", _HELP("name of preload request"),                "Preload Request"),
			InputPortConfig<string>("audioPreloadRequest_PreloadRequest2", _HELP("name of preload request"),                "Preload Request"),
			InputPortConfig<string>("audioPreloadRequest_PreloadRequest3", _HELP("name of preload request"),                "Preload Request"),
			InputPortConfig<string>("audioPreloadRequest_PreloadRequest4", _HELP("name of preload request"),                "Preload Request"),
			InputPortConfig<string>("audioPreloadRequest_PreloadRequest5", _HELP("name of preload request"),                "Preload Request"),
			InputPortConfig<string>("audioPreloadRequest_PreloadRequest6", _HELP("name of preload request"),                "Preload Request"),
			InputPortConfig<string>("audioPreloadRequest_PreloadRequest7", _HELP("name of preload request"),                "Preload Request"),
			InputPortConfig_Void("Load",                                   _HELP("loads all supplied preload requests")),
			InputPortConfig_Void("Unload",                                 _HELP("unloads all supplied preload requests")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Node that allows for handling audio preload requests.");
		config.SetCategory(EFLN_APPROVED);
	}

	void Enable(SActivationInfo* pActInfo, bool bEnable)
	{
		if (m_bEnable != bEnable && gEnv->pAudioSystem != nullptr)
		{
			for (uint32 i = eIn_PreloadRequestFirst; i <= eIn_PreloadRequestLast; ++i)
			{
				string const& preloadName = GetPortString(pActInfo, static_cast<int>(i));

				if (!preloadName.empty())
				{
					CryAudio::PreloadRequestId const preloadRequestId = CryAudio::StringToId(preloadName.c_str());
					
					if (bEnable)
					{
						gEnv->pAudioSystem->PreloadSingleRequest(preloadRequestId, false);
					}
					else
					{
						gEnv->pAudioSystem->UnloadSingleRequest(preloadRequestId);
					}
				}
			}

			m_bEnable = bEnable;
		}
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) override
	{
		bool bEnable = m_bEnable;
		ser.BeginGroup("FlowAudioPreloadData");
		ser.Value("enable", bEnable);
		ser.EndGroup();

		if (ser.IsReading())
		{
			Enable(pActInfo, bEnable);
		}
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				Enable(pActInfo, false);

				break;
			}
		case eFE_Activate:
			{
				// Enable
				if (IsPortActive(pActInfo, eIn_Enable))
				{
					Enable(pActInfo, true);
				}

				// Disable
				if (IsPortActive(pActInfo, eIn_Disable))
				{
					Enable(pActInfo, false);
				}

				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

private:

	bool m_bEnable;
};

REGISTER_FLOW_NODE("Audio:PreloadData", CFlowNode_AudioPreloadData);
