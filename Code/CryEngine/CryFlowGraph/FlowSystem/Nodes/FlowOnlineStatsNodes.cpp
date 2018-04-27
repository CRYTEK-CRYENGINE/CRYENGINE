// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IPlayerProfiles.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_ProfileAttribute : public CFlowBaseNode<eNCT_Singleton>
{
private:
	enum EInputPorts
	{
		eIP_Name = 0,
		eIP_Get,
		eIP_Set,
	};

	enum EOutputPorts
	{
		eOP_Value = 0,
		eOP_Error,
	};

public:
	CFlowNode_ProfileAttribute(SActivationInfo* pActInfo)
	{

	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("Name", _HELP("Name of the attribute as provided in the XMLs")),
			InputPortConfig_Void("Get",     _HELP("Retrieve the current value, outputted in 'Val'")),
			InputPortConfig_AnyType("Set",  _HELP("Set a new value for the attribute, input the value in this port")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Val"),
			OutputPortConfig_Void("Error"),
			{ 0 }
		};

		config.sDescription = _HELP("Set or Get profile attributes. \n the available attributes can be found in \\Libs\\Config\\Profiles\\default\\attributes.xml and \\Scripts\\Network\\OnlineAttributes.xml. \n The online attributes require OnlineAttributes to be loaded first - see PlayerProfile:OnlineAttributes");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_Get))
				{
					IPlayerProfile* pProfile = NULL;
					if (IPlayerProfileManager* pProfileMan = gEnv->pGameFramework->GetIPlayerProfileManager())
					{
						const char* user = pProfileMan->GetCurrentUser();
						pProfile = pProfileMan->GetCurrentProfile(user);
						TFlowInputData data;
						if (!pProfile || pProfile->GetAttribute(GetPortString(pActInfo, eIP_Name), data))
						{
							ActivateOutput(pActInfo, eOP_Value, data);
						}
						else
						{
							ActivateOutput(pActInfo, eOP_Error, 1);
						}
					}
				}

				if (IsPortActive(pActInfo, eIP_Set))
				{
					IPlayerProfile* pProfile = NULL;
					if (IPlayerProfileManager* pProfileMan = gEnv->pGameFramework->GetIPlayerProfileManager())
					{
						const char* user = pProfileMan->GetCurrentUser();
						pProfile = pProfileMan->GetCurrentProfile(user);
						if (!pProfile || !pProfile->SetAttribute(GetPortString(pActInfo, eIP_Name), GetPortAny(pActInfo, eIP_Set)))
						{
							ActivateOutput(pActInfo, eOP_Error, 1);
						}
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_OnlineAttributes : public CFlowBaseNode<eNCT_Singleton>
{
private:
	enum EInputPorts
	{
		eIP_Load = 0,
		eIP_Save,
	};

	enum EOutputPorts
	{
		eOP_Done = 0,
	};

public:
	CFlowNode_OnlineAttributes(SActivationInfo* pActInfo)
	{

	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Load"),
			InputPortConfig_Void("Save"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done", _HELP("Outputs when request was done, doesnt inform whether attributes are loaded or saved successfully")),
			{ 0 }
		};

		config.sDescription = _HELP("This Loads or Saves the online attributes, use the ProfileAttributes to set values for them. Load needs to be called before you are able to edit them");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_Load) || IsPortActive(pActInfo, eIP_Save))
				{
					IPlayerProfile* pProfile = NULL;
					if (IPlayerProfileManager* pProfileMan = gEnv->pGameFramework->GetIPlayerProfileManager())
					{
						const char* user = pProfileMan->GetCurrentUser();
						pProfile = pProfileMan->GetCurrentProfile(user);

						if (!pProfile)
							break;

						if (IsPortActive(pActInfo, eIP_Save))
						{
							pProfileMan->SaveOnlineAttributes(pProfile);
						}
						else
						{
							pProfileMan->LoadOnlineAttributes(pProfile);
						}

						ActivateOutput(pActInfo, eOP_Done, 1);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("PlayerProfile:ProfileAttribute", CFlowNode_ProfileAttribute);
REGISTER_FLOW_NODE("PlayerProfile:OnlineAttributes", CFlowNode_OnlineAttributes);
