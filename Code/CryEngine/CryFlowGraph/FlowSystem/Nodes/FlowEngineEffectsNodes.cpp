// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowFrameworkBaseNode.h"

// Note: why is there a Enable + Disable ? Quite redundant

////////////////////////////////////////////////////////////////////////////////////////////////////
// Get custom value helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////

template<ENodeCloneType CLONE_TYPE>
class CFlowFrameworkBaseNodeEngineEffects : public CFlowBaseNode<CLONE_TYPE>
{
protected:
	bool InputEntityIsLocalPlayer(const IFlowNode::SActivationInfo* const pActInfo) const
	{
		bool bRet = true;

		if (pActInfo->pEntity)
		{
			IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId());
			if (pActor != gEnv->pGameFramework->GetClientActor())
			{
				bRet = false;
			}
		}
		else if (gEnv->bMultiplayer)
		{
			bRet = false;
		}

		return bRet;
	}

	//-------------------------------------------------------------
	// returns the actor associated with the input entity. In single player, it returns the local player if that actor does not exists.
	IActor* GetInputActor(const IFlowNode::SActivationInfo* const pActInfo) const
	{
		IActor* pActor = pActInfo->pEntity ? gEnv->pGameFramework->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId()) : nullptr;
		if (!pActor && !gEnv->bMultiplayer)
		{
			pActor = gEnv->pGameFramework->GetClientActor();
		}

		return pActor;
	}
};
typedef CFlowFrameworkBaseNodeEngineEffects<eNCT_Singleton> TBaseNodeClass;


static float GetCustomValue(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo, int portIndex, bool& isOk)
{
	float customValue = 0.0f;
	if (pFlowBaseNode && pActInfo && isOk)
	{
		if (bIsEnabled)
		{
			isOk = GetPortAny(pActInfo, portIndex).GetValueWithConversion(customValue);
		}
		else
		{
			isOk = config.pInputPorts[portIndex].defaultData.GetValueWithConversion(customValue);
		}
	}
	return customValue;
}

static void GetCustomValueVec3(Vec3& customValue, const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo, int portIndex, bool& isOk)
{
	if (pFlowBaseNode && pActInfo && isOk)
	{
		if (bIsEnabled)
		{
			isOk = GetPortAny(pActInfo, portIndex).GetValueWithConversion(customValue);
		}
		else
		{
			isOk = config.pInputPorts[portIndex].defaultData.GetValueWithConversion(customValue);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Color correction
////////////////////////////////////////////////////////////////////////////////////////////////////

struct SColorCorrection
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                 false, "Enables node",  "Enabled"),
			InputPortConfig<bool>("Disabled",                true,  "Disables node", "Disabled"),
			InputPortConfig<float>("Global_User_ColorC",     0.0f,  0,               "Cyan"),
			InputPortConfig<float>("Global_User_ColorM",     0.0f,  0,               "Magenta"),
			InputPortConfig<float>("Global_User_ColorY",     0.0f,  0,               "Yellow"),
			InputPortConfig<float>("Global_User_ColorK",     0.0f,  0,               "Luminance"),
			InputPortConfig<float>("Global_User_Brightness", 1.0f,  0,               "Brightness"),
			InputPortConfig<float>("Global_User_Contrast",   1.0f,  0,               "Contrast"),
			InputPortConfig<float>("Global_User_Saturation", 1.0f,  0,               "Saturation"),
			InputPortConfig<float>("Global_User_ColorHue",   0.0f,  "Change Hue",    "Hue"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Sets color correction parameters");
	}

	static void Disable()
	{
		SFlowNodeConfig config;
		GetConfiguration(config);
		I3DEngine* pEngine = gEnv->p3DEngine;

		for (int i = 2; config.pInputPorts[i].name; ++i)
		{
			if (config.pInputPorts[i].defaultData.GetType() == eFDT_Float)
			{
				float fVal;
				bool ok = config.pInputPorts[i].defaultData.GetValueWithConversion(fVal);
				if (ok)
					pEngine->SetPostEffectParam(config.pInputPorts[i].name, fVal);
			}
		}
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }

	static void Serialize(IFlowNode::SActivationInfo* actInfo, TSerialize ser)                                                                     {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Filters
////////////////////////////////////////////////////////////////////////////////////////////////////

struct SFilterBlur
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                false, "Enables node",                 "Enabled"),
			InputPortConfig<bool>("Disabled",               true,  "Disables node",                "Disabled"),
			InputPortConfig<int>("FilterBlurring_Type",     0,     "Set blur type, 0 is Gaussian", "Type"),
			InputPortConfig<float>("FilterBlurring_Amount", 0.0f,  "Amount of blurring",           "Amount"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Sets blur filter");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("FilterBlurring_Amount", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SFilterRadialBlur
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                          false, "Enables node",               "Enabled"),
			InputPortConfig<bool>("Disabled",                         true,  "Disables node",              "Disabled"),
			InputPortConfig<float>("FilterRadialBlurring_Amount",     0.0f,  "Amount of blurring",         "Amount"),
			InputPortConfig<float>("FilterRadialBlurring_ScreenPosX", 0.5f,  "Horizontal screen position", "ScreenPosX"),
			InputPortConfig<float>("FilterRadialBlurring_ScreenPosY", 0.5f,  "Vertical screen position",   "ScreenPosY"),
			InputPortConfig<float>("FilterRadialBlurring_Radius",     1.0f,  "Blurring radius",            "BlurringRadius"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Sets radial blur filter");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_Amount", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SFilterSharpen
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                  false, "Enables node",                              "Enabled"),
			InputPortConfig<bool>("Disabled",                 true,  "Disables node",                             "Disabled"),
			InputPortConfig<int>("FilterSharpening_Type",     0,     "Sets sharpening filter, 0 is Unsharp Mask", "Type"),
			InputPortConfig<float>("FilterSharpening_Amount", 1.0f,  "Amount of sharpening",                      "Amount"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Sets sharpen filter");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("FilterSharpening_Amount", 1.0f);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SFilterChromaShift
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                        false, "Enables node",           "Enabled"),
			InputPortConfig<bool>("Disabled",                       true,  "Disables node",          "Disabled"),
			InputPortConfig<float>("FilterChromaShift_User_Amount", 0.0f,  "Amount of chroma shift", "Amount"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Sets chroma shift filter");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("FilterChromaShift_User_Amount", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SFilterGrain
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",             false, "Enables node",    "Enabled"),
			InputPortConfig<bool>("Disabled",            true,  "Disables node",   "Disabled"),
			InputPortConfig<float>("FilterGrain_Amount", 0.0f,  "Amount of grain", "Amount"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Sets grain filter");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("FilterGrain_Amount", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SFilterVisualArtifacts
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                            false,                  "Enables node",                  "Enabled"),
			InputPortConfig<bool>("Disabled",                           true,                   "Disables node",                 "Disabled"),

			InputPortConfig<float>("VisualArtifacts_Vsync",             0.0f,                   "Amount of vsync visible",       "VSync"),
			InputPortConfig<float>("VisualArtifacts_VsyncFreq",         1.0f,                   "Vsync frequency",               "VSync frequency"),

			InputPortConfig<float>("VisualArtifacts_Interlacing",       0.0f,                   "Amount of interlacing visible", "Interlacing"),
			InputPortConfig<float>("VisualArtifacts_InterlacingTile",   1.0f,                   "Interlacing tilling",           "Interlacing tiling"),
			InputPortConfig<float>("VisualArtifacts_InterlacingRot",    0.0f,                   "Interlacing rotation",          "Interlacing rotation"),

			InputPortConfig<float>("VisualArtifacts_SyncWavePhase",     0.0f,                   "Sync wave phase",               "Sync wave phase"),
			InputPortConfig<float>("VisualArtifacts_SyncWaveFreq",      0.0f,                   "Sync wave frequency",           "Sync wave frequency"),
			InputPortConfig<float>("VisualArtifacts_SyncWaveAmplitude", 0.0f,                   "Sync wave amplitude",           "Sync wave amplitude"),

			InputPortConfig<float>("FilterArtifacts_ChromaShift",       0.0f,                   "Chroma shift",                  "Chroma shift"),

			InputPortConfig<float>("FilterArtifacts_Grain",             0.0f,                   "Image grain amount",            "Grain"),

			InputPortConfig<Vec3>("clr_VisualArtifacts_ColorTint",      Vec3(1.0f,              1.0f,                            1.0f),                  0,"Color tinting"),

			InputPortConfig<string>("tex_VisualArtifacts_Mask",         _HELP("Texture Name")),

			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Sets image visual artefacts parameters");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("tex_VisualArtifacts_Mask", 0);
		gEnv->p3DEngine->SetPostEffectParamVec4("clr_VisualArtifacts_ColorTint", Vec4(1.0f, 1.0f, 1.0f, 1.0f));
		gEnv->p3DEngine->SetPostEffectParam("VisualArtifacts_Vsync", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("VisualArtifacts_VsyncFreq", 1.0f);
		gEnv->p3DEngine->SetPostEffectParam("VisualArtifacts_Interlacing", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("VisualArtifacts_InterlacingTile", 1.0f);
		gEnv->p3DEngine->SetPostEffectParam("VisualArtifacts_InterlacingRot", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("VisualArtifacts_Pixelation", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("VisualArtifacts_Noise", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("VisualArtifacts_SyncWavePhase", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("VisualArtifacts_SyncWaveFreq", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("VisualArtifacts_SyncWaveAmplitude", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("FilterArtifacts_ChromaShift", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("FilterArtifacts_Grain", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("FilterArtifacts_GrainTile", 1.0f);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Effects
////////////////////////////////////////////////////////////////////////////////////////////////////

struct SEffectFrost
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                   false, "Enables node",                        "Enabled"),
			InputPortConfig<bool>("Disabled",                  true,  "Disables node",                       "Disabled"),
			InputPortConfig<float>("ScreenFrost_Amount",       0.0f,  "Amount of frost",                     "Amount"),
			InputPortConfig<float>("ScreenFrost_CenterAmount", 1.0f,  "Amount of frost at center of screen", "CenterAmount"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Freezing effect");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("ScreenFrost_Amount", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SEffectWaterDroplets
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",               false, "Enables node",    "Enabled"),
			InputPortConfig<bool>("Disabled",              true,  "Disables node",   "Disabled"),
			InputPortConfig<float>("WaterDroplets_Amount", 0.0f,  "Amount of water", "Amount"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Water droplets effect - Use when camera goes out of water or similar extreme condition");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("WaterDroplets_Amount", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SEffectWaterFlow
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",           false, "Enables node",    "Enabled"),
			InputPortConfig<bool>("Disabled",          true,  "Disables node",   "Disabled"),
			InputPortConfig<float>("WaterFlow_Amount", 0.0f,  "Amount of water", "Amount"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Water flow effect - Use when camera receiving splashes of water or similar");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("WaterFlow_Amount", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SEffectBloodSplats
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",             false, "Enables node",               "Enabled"),
			InputPortConfig<bool>("Disabled",            true,  "Disables node",              "Disabled"),
			InputPortConfig<int>("BloodSplats_Type",     0,     "Blood type: human or alien", "Type"),
			InputPortConfig<float>("BloodSplats_Amount", 0.0f,  "Amount of visible blood",    "Amount"),
			InputPortConfig<bool>("BloodSplats_Spawn",   false, "Spawn more blood particles", "Spawn"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Blood splats effect");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("BloodSplats_Amount", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SEffectDof
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                 false, "Enables node",                  "Enabled"),
			InputPortConfig<bool>("Disabled",                true,  "Disables node",                 "Disabled"),
			InputPortConfig<bool>("Dof_User_Active",         false, "Enables dof effect",            "EnableDof"),
			InputPortConfig<float>("Dof_User_FocusDistance", 3.5f,  "Set focus distance",            "FocusDistance"),
			InputPortConfig<float>("Dof_User_FocusRange",    5.0f,  "Set focus range",               "FocusRange"),
			InputPortConfig<float>("Dof_User_BlurAmount",    1.0f,  "Set blurring amount",           "BlurAmount"),
			InputPortConfig<float>("Dof_MaxCoC",             12.0f, "Set circle of confusion scale", "ScaleCoC"),
			InputPortConfig<float>("Dof_CenterWeight",       1.0f,  "Set central samples weight",    "CenterWeight"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Depth of field effect");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("Dof_User_Active", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SEffectDirectionalBlur
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                    false,     "Enables node",  "Enabled"),
			InputPortConfig<bool>("Disabled",                   true,      "Disables node", "Disabled"),
			InputPortConfig<Vec3>("Global_DirectionalBlur_Vec", Vec3(0.0f, 0.0f,            0.0f),      "Sets directional blur direction","Direction"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Directional Blur effect");
	}

	static void Disable()
	{
		Vec4 vec(0, 0, 0, 1);
		gEnv->p3DEngine->SetPostEffectParamVec4("Global_DirectionalBlur_Vec", vec);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SEffectAlienInterference
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                     false,      "Enables node",                   "Enabled"),
			InputPortConfig<bool>("Disabled",                    false,      "Disables node",                  "Disabled"),
			InputPortConfig<float>("AlienInterference_Amount",   0.0f,       "Sets alien interference amount", "Amount"),
			InputPortConfig<Vec3>("clr_AlienInterference_Color", Vec3(0.85f, 0.95f,                            1.25f) * 0.5f,"Sets volumetric scattering amount","Color"),

			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Alien interference effect");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("AlienInterference_Amount", 0);
		gEnv->p3DEngine->SetPostEffectParamVec4("clr_AlienInterference_TintColor", Vec4(Vec3(0.85f, 0.95f, 1.25f) * 0.5f, 1.0f));
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SEffectGhostVision
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("GhostVision_Bool1",     false,      "User defined bool",  "UserBool1"),
			InputPortConfig<bool>("GhostVision_Bool2",     false,      "User defined bool",  "UserBool2"),
			InputPortConfig<bool>("GhostVision_Bool3",     false,      "User defined bool",  "UserBool3"),
			InputPortConfig<float>("GhostVision_Amount1",  0.0f,       "User defined value", "FloatValue1"),
			InputPortConfig<float>("GhostVision_Amount2",  0.0f,       "User defined value", "FloatValue2"),
			InputPortConfig<float>("GhostVision_Amount3",  0.0f,       "User defined value", "FloatValue3"),
			InputPortConfig<Vec3>("clr_GhostVision_Color", Vec3(0.85f, 0.95f,                1.25f) * 0.5f, "Sets volumetric scattering amount","Color"),

			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Ghost Vision Effect");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("GhostVision_Bool1", 0);
		gEnv->p3DEngine->SetPostEffectParam("GhostVision_Bool2", 0);
		gEnv->p3DEngine->SetPostEffectParam("GhostVision_Bool3", 0);
		gEnv->p3DEngine->SetPostEffectParam("User_FloatValue_1", 0);
		gEnv->p3DEngine->SetPostEffectParam("User_FloatValue_2", 0);
		gEnv->p3DEngine->SetPostEffectParam("User_FloatValue_3", 0);
		gEnv->p3DEngine->SetPostEffectParamVec4("clr_GhostVision_TintColor", Vec4(Vec3(0.85f, 0.95f, 1.25f) * 0.5f, 1.0f));
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SEffectRainDrops
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                      false, "Enables node",                         "Enabled"),
			InputPortConfig<bool>("Disabled",                     true,  "Disables node",                        "Disabled"),
			InputPortConfig<float>("RainDrops_Amount",            0.0f,  "Sets rain drops visibility",           "Amount"),
			InputPortConfig<float>("RainDrops_SpawnTimeDistance", 0.35f, "Sets rain drops spawn time distance",  "Spawn Time Distance"),
			InputPortConfig<float>("RainDrops_Size",              5.0f,  "Sets rain drops size",                 "Size"),
			InputPortConfig<float>("RainDrops_SizeVariation",     2.5f,  "Sets rain drops size variation",       "Size Variation"),

			InputPortConfig<float>("Moisture_Amount",             0.0f,  "Sets moisture visibility/area size",   "Moisture Amount"),
			InputPortConfig<float>("Moisture_Hardness",           10.0f, "Sets noise texture blending factor",   "Moisture Hardness"),
			InputPortConfig<float>("Moisture_DropletAmount",      7.0f,  "Sets droplet texture blending factor", "Moisture Droplet Amount"),
			InputPortConfig<float>("Moisture_Variation",          0.2f,  "Sets moisture variation",              "Moisture Variation"),
			InputPortConfig<float>("Moisture_Speed",              1.0f,  "Sets moisture animation speed",        "Moisture Speed"),
			InputPortConfig<float>("Moisture_FogAmount",          1.0f,  "Sets amount of fog in moisture",       "Moisture Fog Amount"),

			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Rain drops effect");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("RainDrops_Amount", 0);
		gEnv->p3DEngine->SetPostEffectParam("Moisture_Amount", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SEffectVolumetricScattering
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                        false,     "Enables node",                               "Enabled"),
			InputPortConfig<bool>("Disabled",                       true,      "Disables node",                              "Disabled"),
			InputPortConfig<float>("VolumetricScattering_Amount",   0.0f,      "Sets volumetric scattering amount",          "Amount"),
			InputPortConfig<float>("VolumetricScattering_Tilling",  1.0f,      "Sets volumetric scattering tilling",         "Tilling"),
			InputPortConfig<float>("VolumetricScattering_Speed",    1.0f,      "Sets volumetric scattering animation speed", "Speed"),
			InputPortConfig<Vec3>("clr_VolumetricScattering_Color", Vec3(0.6f, 0.75f,                                        1.0f),      "Sets volumetric scattering amount","Color"),
			InputPortConfig<int>("VolumetricScattering_Type",       0,         "Set volumetric scattering type",             "Type"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Volumetric scattering effect");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("VolumetricScattering_Amount", 0);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};

struct SEffect3DHudInterference
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",                        false,      "Enables node",                                                                                                                 "Enabled"),
			InputPortConfig<bool>("Disabled",                       true,       "Disables node",                                                                                                                "Disabled"),
			InputPortConfig<float>("Amount",                        0.0f,       "Sets 3D Hud inteference amount [0-1]",                                                                                         "Amount"),
			InputPortConfig<float>("DistruptScale",                 0.3f,       "Sets 3D Hud interference distruption scale [0-1]",                                                                             "Distrupt scale"),
			InputPortConfig<float>("DistruptMovementScale",         1.0f,       "Sets 3D Hud interference distruption movement scale [0-1]",                                                                    "Distrupt movement scale"),
			InputPortConfig<float>("RandomGrainStrengthScale",      0.3f,       "Sets 3D Hud interference random grain strength scale [0-1]",                                                                   "Random grain strength scale"),
			InputPortConfig<float>("RandomFadeStrengthScale",       0.4f,       "Sets 3D Hud interference random fade strength scale [0-1]",                                                                    "Random fade strength scale"),
			InputPortConfig<float>("NoiseStrength",                 0.2f,       "Sets 3D Hud interference noise strength [0-1]",                                                                                "Noise strength"),
			InputPortConfig<float>("ChromaShiftDistance",           1.0f,       "Sets 3D Hud interference chroma shift distance [0-1]",                                                                         "Chroma shift dist"),
			InputPortConfig<float>("ChromaShiftStrength",           0.25f,      "Sets 3D Hud interference chroma shift strength [0-1]",                                                                         "Chroma shift strength"),
			InputPortConfig<float>("RandFrequency",                 0.055f,     "Sets 3D Hud interference random number generation frequency [0-1]",                                                            "Rand frequency"),
			InputPortConfig<float>("ItemFilterStrength",            0.0f,       "Sets 3D Hud interference item filter strength (uses red channel in vertex color to control item interference strength) [0-1]", "Item filter strength"),
			InputPortConfig<float>("DofInterferenceStrength",       0.8f,       "Sets 3D Hud interference Depth of Field strength [0-1]",                                                                       "Depth of Field strength"),
			InputPortConfig<float>("BarScale",                      0.8f,       "Sets 3D Hud interference bar scale [0-1]",                                                                                     "Bar scale"),
			InputPortConfig<float>("BarColorMultiplier",            5.0f,       "Sets 3D Hud interference bar color multiplier [0-20]",                                                                         "Bar Color Multiplier"),
			InputPortConfig<Vec3>("clr_3DHudInterference_BarColor", Vec3(0.39f, 0.6f,                                                                                                                           1.0f),                         "Sets 3D Hud interference bar color","Bar Color"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("3D Hud interference effect");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("HUD3D_Interference", 0.0f);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo)
	{
		if (pFlowBaseNode && pActInfo)
		{
			bool isOk = true;

			float interferenceScale = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 2, isOk);
			float disruptScale = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 3, isOk);
			float disruptMovementScale = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 4, isOk);
			float randomGrainStrengthScale = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 5, isOk);
			float randomFadeStrengthScale = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 6, isOk);
			float noiseStrength = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 7, isOk);
			float chromaShiftDist = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 8, isOk);
			float chromaShiftStrength = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 9, isOk);
			float randFrequency = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 10, isOk);
			float itemOverrideStrength = 1.0f - GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 11, isOk);
			float depthOfFieldStrength = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 12, isOk);
			float barScale = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 13, isOk);
			float barColorMultiplier = GetCustomValue(config, pFlowBaseNode, bIsEnabled, pActInfo, 14, isOk);

			Vec3 barColor;
			GetCustomValueVec3(barColor, config, pFlowBaseNode, bIsEnabled, pActInfo, 15, isOk);

			if (isOk)
			{
				I3DEngine* p3DEngine = gEnv->p3DEngine;
				p3DEngine->SetPostEffectParam("HUD3D_Interference", interferenceScale);
				p3DEngine->SetPostEffectParamVec4("HUD3D_InterferenceParams0", Vec4(itemOverrideStrength, randFrequency, depthOfFieldStrength, 0.0f), true);
				p3DEngine->SetPostEffectParamVec4("HUD3D_InterferenceParams1", Vec4(randomGrainStrengthScale, randomFadeStrengthScale, chromaShiftStrength, chromaShiftDist), true);
				p3DEngine->SetPostEffectParamVec4("HUD3D_InterferenceParams2", Vec4(disruptScale, disruptMovementScale, noiseStrength, barScale), true);
				p3DEngine->SetPostEffectParamVec4("HUD3D_InterferenceParams3", Vec4(barColor.x, barColor.y, barColor.z, barColorMultiplier), true);
			}
		}

		return true;
	}

	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)
	{
		//CRY_TODO(11, 1, 2010, "make a propper serialization here");
		if (ser.IsReading())
		{
			float interferenceScale = 0.0f;
			float disruptScale = 0.3f;
			float disruptMovementScale = 1.0f;
			float randomGrainStrengthScale = 0.3f;
			float randomFadeStrengthScale = 0.4f;
			float noiseStrength = 0.2f;
			float chromaShiftDist = 1.0f;
			float chromaShiftStrength = 0.25f;
			float randFrequency = 0.055f;
			float itemOverrideStrength = 0.0f;
			float depthOfFieldStrength = 0.8f;
			float barScale = 0.8f;
			float barColorMultiplier = 5.0f;
			Vec3 barColor(0.39f, 0.6f, 1.0f);

			I3DEngine* p3DEngine = gEnv->p3DEngine;
			p3DEngine->SetPostEffectParam("HUD3D_Interference", interferenceScale);
			p3DEngine->SetPostEffectParamVec4("HUD3D_InterferenceParams0", Vec4(itemOverrideStrength, randFrequency, depthOfFieldStrength, 0.0f), true);
			p3DEngine->SetPostEffectParamVec4("HUD3D_InterferenceParams1", Vec4(randomGrainStrengthScale, randomFadeStrengthScale, chromaShiftStrength, chromaShiftDist), true);
			p3DEngine->SetPostEffectParamVec4("HUD3D_InterferenceParams2", Vec4(disruptScale, disruptMovementScale, noiseStrength, barScale), true);
			p3DEngine->SetPostEffectParamVec4("HUD3D_InterferenceParams3", Vec4(barColor.x, barColor.y, barColor.z, barColorMultiplier), true);
		}
	}
};

struct SEffectGhosting
{
	static void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<bool>("Enabled",               false, "Enables node",            "Enabled"),
			InputPortConfig<bool>("Disabled",              true,  "Disables node",           "Disabled"),
			InputPortConfig<float>("ImageGhosting_Amount", 0.0f,  "Enables ghosting effect", "GhostingAmount"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Ghosting effect");
	}

	static void Disable()
	{
		gEnv->p3DEngine->SetPostEffectParam("ImageGhosting_Amount", 0.0f);
	}

	static bool CustomSetData(const SFlowNodeConfig& config, TBaseNodeClass* pFlowBaseNode, bool bIsEnabled, IFlowNode::SActivationInfo* pActInfo) { return false; }
	static void Serialize(IFlowNode::SActivationInfo* pActInfo, TSerialize ser)                                                                    {}
};
//////////////////////////////////////////////////////////////////////////

template<class T>
class CFlowImageNode : public TBaseNodeClass
{
public:
	CFlowImageNode(SActivationInfo* pActInfo)
	{
	}

	~CFlowImageNode()
	{
	}

	/* It's now a singleton node, so clone MUST not be implemented
	   IFlowNodePtr Clone( SActivationInfo * pActInfo )
	   {
	   return this;
	   }
	 */

	enum EInputPorts
	{
		EIP_Enabled = 0,
		EIP_Disabled
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		T::GetConfiguration(config);
		if (!(config.nFlags & EFLN_OBSOLETE))
			config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
		{
			return;
		}

		if (pActInfo->pEntity)
		{
			if (!InputEntityIsLocalPlayer(pActInfo))
			{
				return;
			}
		}
		else
		{
			if (!gEnv->IsClient())
			{
				return;
			}
		}

		bool bIsEnabled = IsPortActive(pActInfo, EIP_Enabled) && GetPortBool(pActInfo, EIP_Enabled);
		const bool bIsDisabled = IsPortActive(pActInfo, EIP_Disabled) && GetPortBool(pActInfo, EIP_Disabled);

		if (!bIsDisabled && GetPortBool(pActInfo, EIP_Enabled))    // this is to support when effects parameters are changed after the effect was activated. would be easier to just make sure that the bool value of both inputs is always complementary, but that conflicts on how triggers work, and other nodes already rely on that way of working.
			bIsEnabled = true;

		if (!bIsEnabled)
		{
			T::Disable();
			return;
		}

		SFlowNodeConfig config;
		T::GetConfiguration(config);
		if (T::CustomSetData(config, this, bIsEnabled, pActInfo) == false)
		{
			I3DEngine* pEngine = gEnv->p3DEngine;

			for (int i(2); config.pInputPorts[i].name; ++i)
			{
				const TFlowInputData& anyVal = GetPortAny(pActInfo, i);

				switch (anyVal.GetType())
				{

				case eFDT_Vec3:
					{
						Vec3 pVal(GetPortVec3(pActInfo, i));
						if (!bIsEnabled)
							config.pInputPorts[i].defaultData.GetValueWithConversion(pVal);
						pEngine->SetPostEffectParamVec4(config.pInputPorts[i].name, Vec4(pVal, 1));

						break;
					}

				case eFDT_String:
					{
						const string& pszString = GetPortString(pActInfo, i);
						pEngine->SetPostEffectParamString(config.pInputPorts[i].name, pszString.c_str());

						break;
					}

				default:
					{
						float fVal;
						bool ok(anyVal.GetValueWithConversion(fVal));

						if (!bIsEnabled)
							ok = config.pInputPorts[i].defaultData.GetValueWithConversion(fVal);

						if (ok)
						{
							pEngine->SetPostEffectParam(config.pInputPorts[i].name, fVal);
						}

						break;
					}
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(IFlowNode::SActivationInfo* actInfo, TSerialize ser)
	{
		T::Serialize(actInfo, ser);
	}
};

//////////////////////////////////////////////////////////////////////////////////////

class CFlowNode_SetShadowMode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SetShadowMode(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eIn_Activate = 0,
		eIn_Mode
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_Void("Activate",   _HELP("Sets the specified shadow mode")),
			InputPortConfig<int>("ShadowMode", 0,                                       _HELP("Shadow mode"),_HELP("ShadowMode"), _UICONFIG("enum_int:Normal=0,HighQuality=1")), // the values needs to coincide with EShadowMode
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Sets a shadow mode for the 3dengine");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIn_Activate))
				{
					EShadowMode shadowMode = EShadowMode(GetPortInt(pActInfo, eIn_Mode));
					gEnv->p3DEngine->SetShadowMode(shadowMode);
				}
			}
			break;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// these macros expose the nodes in the FlowSystem, otherwise they're not available in the system
REGISTER_FLOW_NODE_EX("Image:ColorCorrection", CFlowImageNode<SColorCorrection>, SColorCorrection);
REGISTER_FLOW_NODE_EX("Image:FilterBlur", CFlowImageNode<SFilterBlur>, SFilterBlur);
REGISTER_FLOW_NODE_EX("Image:FilterRadialBlur", CFlowImageNode<SFilterRadialBlur>, SFilterRadialBlur);
REGISTER_FLOW_NODE_EX("Image:FilterSharpen", CFlowImageNode<SFilterSharpen>, SFilterSharpen);
REGISTER_FLOW_NODE_EX("Image:FilterChromaShift", CFlowImageNode<SFilterChromaShift>, SFilterChromaShift);
REGISTER_FLOW_NODE_EX("Image:EffectFrost", CFlowImageNode<SEffectFrost>, SEffectFrost);
REGISTER_FLOW_NODE_EX("Image:FilterGrain", CFlowImageNode<SFilterGrain>, SFilterGrain);
REGISTER_FLOW_NODE_EX("Image:EffectWaterDroplets", CFlowImageNode<SEffectWaterDroplets>, SEffectWaterDroplets);
REGISTER_FLOW_NODE_EX("Image:EffectWaterFlow", CFlowImageNode<SEffectWaterFlow>, SEffectWaterFlow);
REGISTER_FLOW_NODE_EX("Image:EffectBloodSplats", CFlowImageNode<SEffectBloodSplats>, SEffectBloodSplats);
REGISTER_FLOW_NODE_EX("Image:EffectGhosting", CFlowImageNode<SEffectGhosting>, SEffectGhosting);
REGISTER_FLOW_NODE_EX("Image:EffectGhostVision", CFlowImageNode<SEffectGhostVision>, SEffectGhostVision);
REGISTER_FLOW_NODE_EX("Image:EffectDepthOfField", CFlowImageNode<SEffectDof>, SEffectDof);
REGISTER_FLOW_NODE_EX("Image:EffectVolumetricScattering", CFlowImageNode<SEffectVolumetricScattering>, SEffectVolumetricScattering);
REGISTER_FLOW_NODE_EX("Image:FilterDirectionalBlur", CFlowImageNode<SEffectDirectionalBlur>, SEffectDirectionalBlur);
REGISTER_FLOW_NODE_EX("Image:EffectAlienInterference", CFlowImageNode<SEffectAlienInterference>, SEffectAlienInterference);
REGISTER_FLOW_NODE_EX("Image:EffectRainDrops", CFlowImageNode<SEffectRainDrops>, SEffectRainDrops);
REGISTER_FLOW_NODE_EX("Image:FilterVisualArtifacts", CFlowImageNode<SFilterVisualArtifacts>, SFilterVisualArtifacts);
REGISTER_FLOW_NODE_EX("Image:3DHudInterference", CFlowImageNode<SEffect3DHudInterference>, SEffect3DHudInterference);
REGISTER_FLOW_NODE("Image:SetShadowMode", CFlowNode_SetShadowMode);
