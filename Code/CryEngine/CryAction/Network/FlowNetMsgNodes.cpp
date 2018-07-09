// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowNetMsgNodes.cpp
//  Description: FG nodes to send bidirectional client <--> server simple messages
// -------------------------------------------------------------------------
//  History: Dario Sancho. 15.10.2014. Created
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Network/NetMsgDispatcher.h"

#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
class CFlowNode_MsgSender : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		IN_MSG,
		IN_VALUE,
		IN_TARGET,
		IN_SEND,
	};
	enum EOutputs
	{
		OUT_DONE,
		OUT_ERROR,
	};

	CFlowNode_MsgSender(SActivationInfo* pActInfo) {}
	virtual IFlowNodePtr Clone(SActivationInfo *pActInfo)
	{
		return new CFlowNode_MsgSender(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("Message", _HELP("The message to be sent")),
			InputPortConfig<string>("Value",   _HELP("An optional value to be sent along with the message")),
			InputPortConfig<int>("Target",     0,                                                            _HELP("Default (client sends msg to server and vice versa) - All (default + client messages are loopback to itself)"),_HELP("Target"), _UICONFIG("enum_int:Default=0,All=1")), // the values needs to coincide with SNetMsgData::ETarget
			InputPortConfig_AnyType("Send",    _HELP("Sends the message to the target")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Done",  _HELP("Outputs event if the message was successfully sent")),
			OutputPortConfig_Void("Error", _HELP("Outputs event if there was a problem with the message")),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, IN_SEND))
			{
				const string& key = GetPortString(pActInfo, IN_MSG);
				const string& value = GetPortString(pActInfo, IN_VALUE);
				const int iTarget = GetPortInt(pActInfo, IN_TARGET);
				SNetMsgData::ETarget target = SNetMsgData::ConvertTargetIntToEnum(iTarget);
				CRY_ASSERT_MESSAGE(target != SNetMsgData::eT_Invalid, "CFlowNode_MsgSender: IN_TARGET input got converted to an invalid Enum");

				if (target != SNetMsgData::eT_Invalid)
				{
					if (CNetMessageDistpatcher* pNetMsgDispatcher = CCryAction::GetCryAction()->GetNetMessageDispatcher())
					{
						NET_MSG_DISPATCHER_LOG("CFlowNode_MsgSender: %s Send message [%s/%s]", CNetMessageDistpatcher::DbgTranslateLocationToString(), key.c_str(), value.c_str());
						pNetMsgDispatcher->SendMessage(SNetMsgData(CNetMessageDistpatcher::GetLocation(), target, key, value));
						ActivateOutput(pActInfo, OUT_DONE, true);
						return;
					}
				}

				ActivateOutput(pActInfo, OUT_ERROR, true);
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
class CFlowNode_MsgReceiver : public CFlowBaseNode<eNCT_Instanced>, public INetMsgListener
{
public:
	enum EInputs
	{
		IN_ENABLED,
		IN_MSG,
	};
	enum EOutputs
	{
		OUT_RECEIVED,
		OUT_VALUE
	};

	// INetMsgListener
	virtual void OnMessage(const SNetMsgData& data) override
	{
		// Notify if the expected message has arrived
		const string& key = GetPortString(&m_actInfo, IN_MSG);
		if (data.key == key)
		{
			NET_MSG_DISPATCHER_LOG("CFlowNode_MsgReceiver: %s Listener received message [%s->%s]-[%s/%s]", CNetMessageDistpatcher::DbgTranslateLocationToString(), data.DbgTranslateSourceToString(), data.DbgTranslateTargetToString(), key.c_str(), data.value.c_str());
			ActivateOutput(&m_actInfo, OUT_RECEIVED, true);
			ActivateOutput(&m_actInfo, OUT_VALUE, data.value);
		}
	}
	// ~INetMsgListener

	CFlowNode_MsgReceiver(SActivationInfo* pActInfo)
	{
	}

	~CFlowNode_MsgReceiver()
	{
		Enable(false); // This unregisters the listener
	}
	virtual IFlowNodePtr Clone(SActivationInfo *pActInfo) override
	{
		return new CFlowNode_MsgReceiver(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Enable",    _HELP("Enable/disable")),
			InputPortConfig<string>("Message", _HELP("Listen to this message")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Received", _HELP("Outputs event if the message was received")),
			OutputPortConfig<string>("Value", _HELP("Optional value sent along with the message")),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;  // Keep a copy of the activation info to be able to use the output ports on a call back
				const bool enabled = GetPortBool(pActInfo, IN_ENABLED);
				Enable(enabled);
			}
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, IN_ENABLED))
			{
				const bool enabled = GetPortBool(pActInfo, IN_ENABLED);
				Enable(enabled);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

private:

	void Enable(bool enable)
	{
		if (CNetMessageDistpatcher* pNetMsgDispatcher = CCryAction::GetCryAction()->GetNetMessageDispatcher())
		{
			if (enable)
				pNetMsgDispatcher->RegisterListener(this, "CFlowNode_MsgSender");
			else
				pNetMsgDispatcher->UnRegisterListener(this);
		}
	}

private:

	SActivationInfo m_actInfo;
};

REGISTER_FLOW_NODE("Multiplayer:MsgSender", CFlowNode_MsgSender)
REGISTER_FLOW_NODE("Multiplayer:MsgListener", CFlowNode_MsgReceiver)
