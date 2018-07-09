// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MaterialHUDFXNodes.cpp
//  Version:     v1.00
//  Created:     29-11-2006 by AlexL - Benito GR
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MaterialEffects.h"
#include <CryFlowGraph/IFlowBaseNode.h>
#include <CrySystem/Scaleform/IFlashPlayer.h>

//CHUDStartFXNode
//Special node that let us trigger a FlowGraph HUD effect related to
//some kind of material, impact, explosion...
class CHUDStartFXNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CHUDStartFXNode(SActivationInfo* pActInfo)
	{
	}

	~CHUDStartFXNode()
	{
	}

	enum
	{
		EIP_Trigger = 0,
	};

	enum
	{
		EOP_Started = 0,
		EOP_Distance,
		EOP_Param1,
		EOP_Param2,
		EOP_Intensity,
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Start", _HELP("Triggered automatically by MaterialEffects")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Started",        _HELP("Triggered when Effect is started.")),
			OutputPortConfig<float>("Distance",     _HELP("Distance to player")),
			OutputPortConfig<float>("Param1",       _HELP("MFX Custom Param 1")),
			OutputPortConfig<float>("Param2",       _HELP("MFX Custom Param 2")),
			OutputPortConfig<float>("Intensity",    _HELP("Dynamic value set from game code (0.0 - 1.0)")),
			OutputPortConfig<float>("BlendOutTime", _HELP("Outputs time value in seconds; request to blend out FX in that time")),
			{ 0 }
		};
		config.sDescription = _HELP("MaterialFX StartNode");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	//Just activate the output when the input is activated
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Trigger))
				ActivateOutput(pActInfo, EOP_Started, true);
			break;
		}
	}

};

//CHUDEndFXNode
//Special node that let us know when the FlowGraph HUD Effect has finish
class CHUDEndFXNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CHUDEndFXNode(SActivationInfo* pActInfo)
	{
	}

	~CHUDEndFXNode()
	{
	}

	enum
	{
		EIP_Trigger = 0,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Trigger", _HELP("Trigger to mark end of effect.")),
			{ 0 }
		};
		config.sDescription = _HELP("MaterialFX EndNode");
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}

	//When activated, just notify the MaterialFGManager that the effect ended
	//See MaterialFGManager.cpp
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Trigger))
			{
				CMaterialEffects* pMatFX = static_cast<CMaterialEffects*>(gEnv->pGameFramework->GetIMaterialEffects());
				if (pMatFX)
				{
					//const string& name = GetPortString(pActInfo, EIP_Name);
					pMatFX->NotifyFGHudEffectEnd(pActInfo->pGraph);
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

class CMaterialFlashInvokeNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CMaterialFlashInvokeNode(SActivationInfo* pActInfo)
	{
	}

	~CMaterialFlashInvokeNode()
	{
	}

	enum
	{
		EIP_Trigger = 0,
		EIP_Function,
		EIP_Int,
		EIP_Float,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Trigger",     _HELP("Trigger the Invoke.")),
			InputPortConfig<string>("Function", _HELP("Function Name")),
			InputPortConfig<int>("Integer",     0,                            _HELP("INTEGER parameter")),
			InputPortConfig<float>("Float",     0.0f,                         _HELP("FLOAT parameter")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Material Flash Invoke");
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}

	static IFlashPlayer* GetFlashPlayerFromMaterial(IMaterial* pMaterial)
	{
		IFlashPlayer* pRetFlashPlayer(NULL);
		const SShaderItem& shaderItem(pMaterial->GetShaderItem());
		if (shaderItem.m_pShaderResources)
		{
			SEfResTexture* pTex = shaderItem.m_pShaderResources->GetTexture(0);
			if (pTex)
			{
				IDynTextureSource* pDynTexSrc = pTex->m_Sampler.m_pDynTexSource;
				if (pDynTexSrc)
					pRetFlashPlayer = (IFlashPlayer*) pDynTexSrc->GetSourceTemp(IDynTextureSource::DTS_I_FLASHPLAYER);
			}
		}

		return pRetFlashPlayer;
	}

	static IFlashPlayer* GetFlashPlayerFromMaterialIncludingSubMaterials(IMaterial* pMaterial)
	{
		IFlashPlayer* pRetFlashPlayer = CMaterialFlashInvokeNode::GetFlashPlayerFromMaterial(pMaterial);

		const int subMtlCount = pMaterial->GetSubMtlCount();
		for (int i = 0; i != subMtlCount; ++i)
		{
			IMaterial* pSubMat = pMaterial->GetSubMtl(i);
			if (!pSubMat)
			{
				continue;
			}

			IFlashPlayer* pFlashPlayer = CMaterialFlashInvokeNode::GetFlashPlayerFromMaterial(pSubMat);
			if (pFlashPlayer)
			{
				if (pRetFlashPlayer)
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "GetFlashPlayerFromMaterialIncludingSubMaterials: Multiple flash players detected, returning last sub material one");
				}

				pRetFlashPlayer = pFlashPlayer;
			}
		}

		return pRetFlashPlayer;
	}

	//When activated, just notify the MaterialFGManager that the effect ended
	//See MaterialFGManager.cpp
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Trigger))
			{
				if (pActInfo->pEntity)
				{
					IEntityRender* pIEntityRender(pActInfo->pEntity->GetRenderInterface());

					
					{
						IMaterial* pMtl(pIEntityRender->GetRenderMaterial(-1)); // material will be taken from the first renderable slot.
						if (pMtl)
						{
							IFlashPlayer* pFlashPlayer = CMaterialFlashInvokeNode::GetFlashPlayerFromMaterialIncludingSubMaterials(pMtl);
							if (pFlashPlayer)
							{
								const string& funcName = GetPortString(pActInfo, EIP_Function);
								SFlashVarValue args[2] = { GetPortInt(pActInfo, EIP_Int), GetPortFloat(pActInfo, EIP_Float) };
								pFlashPlayer->Invoke(funcName.c_str(), args, 2);
							}
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

class CMaterialFlashGotoAndPlayNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CMaterialFlashGotoAndPlayNode(SActivationInfo* pActInfo)
	{
	}

	~CMaterialFlashGotoAndPlayNode()
	{
	}

	enum INPUTS
	{
		EIP_Trigger = 0,
		EIP_ObjectPath,
		EIP_FrameName,
		EIP_FrameNumber,
	};

	enum OUTPUTS
	{
		EOP_Done = 0,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] = {
			InputPortConfig_Void("Trigger",       _HELP("Trigger the Invoke.")),
			InputPortConfig<string>("ObjectPath", _HELP("Full path to flash object")),
			InputPortConfig<string>("FrameName",  _HELP("Name of flash frame to goto")),
			InputPortConfig<int>("FrameNumber",   -1,                                   _HELP("Number of flash frame to goto (If not using FrameName)")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("Done", _HELP("When triggered")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Material Flash GotoAndPlay");
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Trigger))
			{
				if (pActInfo->pEntity)
				{
					IEntityRender* pIEntityRender(pActInfo->pEntity->GetRenderInterface());

					
					{
						IMaterial* pMtl(pIEntityRender->GetRenderMaterial(-1)); // material will be taken from the first renderable slot.
						if (pMtl)
						{
							IFlashPlayer* pFlashPlayer = CMaterialFlashInvokeNode::GetFlashPlayerFromMaterialIncludingSubMaterials(pMtl);
							if (pFlashPlayer)
							{
								const string& objectPath = GetPortString(pActInfo, EIP_ObjectPath);
								if (!objectPath.empty())
								{
									IFlashVariableObject* pFlashObject = NULL;
									pFlashPlayer->GetVariable(objectPath.c_str(), pFlashObject);
									if (pFlashObject)
									{
										const string& frameName = GetPortString(pActInfo, EIP_FrameName);
										if (!frameName.empty())
										{
											bool bResult = pFlashObject->GotoAndPlay(frameName);
											if (!bResult)
											{
												CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CMaterialFlashGotoAndPlayNode::ProcessEvent: Failed on object: %s with framename: %s", objectPath.c_str(), frameName.c_str());
											}
										}
										else // Use number
										{
											const int iFrameNumber = GetPortInt(pActInfo, EIP_FrameNumber);
											bool bResult = pFlashObject->GotoAndPlay(iFrameNumber);
											if (!bResult)
											{
												CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CMaterialFlashGotoAndPlayNode::ProcessEvent: Failed on object: %s with framenumber: %d", objectPath.c_str(), iFrameNumber);
											}
										}

										SAFE_RELEASE(pFlashObject);
									}
									else
									{
										CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CMaterialFlashGotoAndPlayNode::ProcessEvent: Failed finding object: %s", objectPath.c_str());
									}
								}
								else
								{
									CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CMaterialFlashGotoAndPlayNode::ProcessEvent: ObjectPath can't be empty");
								}
							}
						}
					}
				}

				ActivateOutput(pActInfo, EOP_Done, true);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CMaterialFlashHandleFSCommandNode : public CFlowBaseNode<eNCT_Instanced>, public IFSCommandHandler
{
	enum INPUTS
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_DisableHandlingOnEvent,
	};

	enum OUTPUTS
	{
		EOP_OnEvent = 0,
		EOP_CommandName,
		EOP_CommandParam,
	};

public:
	CMaterialFlashHandleFSCommandNode(SActivationInfo* pActInfo)
		: m_bActive(false)
	{
	}

	~CMaterialFlashHandleFSCommandNode()
	{
		Deinit();
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Enable",                  _HELP("Enables handling of event")),
			InputPortConfig_Void("Disable",                 _HELP("Disables handling of event")),
			InputPortConfig<bool>("DisableHandlingOnEvent", true,                                _HELP("Disables handling when event is received")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("OnEvent",      _HELP("Triggers when event happens")),
			OutputPortConfig_Void("CommandName",  _HELP("Command name associated with event")),
			OutputPortConfig_Void("CommandParam", _HELP("Command param associated with event")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Handles flash FS command");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, EIP_Enable))
			{
				bool bResult = SetFSCommandHandler(pActInfo->pEntity, true);
				m_actInfo = *pActInfo;
				m_bActive = true;
			}
			else if (IsPortActive(pActInfo, EIP_Disable))
			{
				Deinit();
			}
		}
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CMaterialFlashHandleFSCommandNode(pActInfo);
	}

private:
	void Deinit()
	{
		if (m_bActive)
		{
			m_bActive = false;
			SetFSCommandHandler(m_actInfo.pEntity, false);
		}
	}

	bool SetFSCommandHandler(IEntity* pEntity, const bool bEnable)
	{
		IEntityRender* pIEntityRender = pEntity->GetRenderInterface();

		
		{
			IMaterial* pMtl(pIEntityRender->GetRenderMaterial(-1)); // material will be taken from the first renderable slot.
			if (pMtl)
			{
				IFlashPlayer* pFlashPlayer = CMaterialFlashInvokeNode::GetFlashPlayerFromMaterialIncludingSubMaterials(pMtl);
				if (pFlashPlayer)
				{
					if (bEnable)
					{
						pFlashPlayer->SetFSCommandHandler(this);
					}
					else
					{
						pFlashPlayer->SetFSCommandHandler(NULL);
					}

					return true;
				}
			}
		}

		return false;
	}

	// IFSCommandHandler
	void HandleFSCommand(const char* pCommand, const char* pArgs, void* pUserData)
	{
		if (IsActive())
		{
			ActivateOutput(&m_actInfo, EOP_OnEvent, true);
			ActivateOutput(&m_actInfo, EOP_CommandName, string(pCommand));
			ActivateOutput(&m_actInfo, EOP_CommandParam, string(pArgs));

			const bool bDisableHandlingOnEvent = GetPortBool(&m_actInfo, EIP_DisableHandlingOnEvent);
			if (bDisableHandlingOnEvent)
			{
				Deinit();
			}
		}
	}
	// ~IFSCommandHandler

	ILINE bool IsActive() const { return m_bActive; }

	SActivationInfo m_actInfo;

	bool            m_bActive;
};

REGISTER_FLOW_NODE("MaterialFX:HUDStartFX", CHUDStartFXNode);
REGISTER_FLOW_NODE("MaterialFX:HUDEndFX", CHUDEndFXNode);
REGISTER_FLOW_NODE("MaterialFX:FlashInvokeOnObject", CMaterialFlashInvokeNode);
REGISTER_FLOW_NODE("MaterialFX:FlashGotoAndPlayOnObject", CMaterialFlashGotoAndPlayNode);
REGISTER_FLOW_NODE("MaterialFX:FlashHandleFSCommand", CMaterialFlashHandleFSCommandNode);
