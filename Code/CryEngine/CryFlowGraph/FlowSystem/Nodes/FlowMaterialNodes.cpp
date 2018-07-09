// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowMaterialNodes.cpp
//  Version:     v1.00
//  Created:     04/11/2013 by Marco H.
//  Description: flownodes copied and updated from FlowEngineNodes.cpp
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNodeEntityMaterial : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNodeEntityMaterial(SActivationInfo* pActInfo) : m_bMaterialChanged(false) {}
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNodeEntityMaterial(pActInfo);
	}

	enum EInputs
	{
		IN_SET,
		IN_GET,
		IN_SLOT,
		IN_MATERIAL,
		IN_RESET,
		IN_SERIALIZE,
	};

	enum EOutputs
	{
		OUT_VALUE,
	};

	_smart_ptr<IMaterial> m_pMaterial;
	_smart_ptr<IMaterial> m_pEditorMaterial;

	bool m_bMaterialChanged;

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (GetPortBool(pActInfo, IN_SERIALIZE))
		{
			IEntity* pEntity = pActInfo->pEntity;

			if (ser.IsReading())
			{
				if (pEntity)
				{
					string matSavedName;
					ser.Value("material", matSavedName);

					if (matSavedName.empty())
						pEntity->SetMaterial(NULL);
					else
					{
						IMaterial* pMatCurr = pEntity->GetMaterial();
						if (!pMatCurr || matSavedName != pMatCurr->GetName())
						{
							IMaterial* pMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(matSavedName.c_str());
							pEntity->SetMaterial(pMtl);
						}
					}
				}
			}
			else
			{
				string matName;
				if (pEntity)
				{
					IMaterial* pMat = pEntity->GetMaterial();
					if (pMat)
						matName = pMat->GetName();
				}
				ser.Value("material", matName);
			}
		}
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_Void("Set", _HELP("Trigger to set the material")),
			InputPortConfig_Void("Get", _HELP("Trigger to get the material name")),
			InputPortConfig<int>("slot", -1, _HELP("Slot on which to apply the material. -1 if it should be applied to the whole entity.")),
			InputPortConfig<string>("mat_Material", _HELP("Name of material to apply")),
			InputPortConfig_Void("Reset", _HELP("Trigger to reset the original material")),
			InputPortConfig<bool>("Serialize", _HELP("Serialize this change")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<string>("Value", _HELP("Returns the name of the material"), _HELP("Name")),
			{ 0 }
		};

		config.sDescription = _HELP("Apply material to the entity");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	};

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		IEntity* pEntity = pActInfo->pEntity;
		if (pEntity == 0)
			return;

		switch (event)
		{
		case eFE_Initialize:
			{
				// workaround to the fact that the entity does not clear the material when going back to editor mode,
				// and to the fact that there is not any special event to signal when its going back to editor mode
				if (gEnv->IsEditor())
				{
					//if the material has been changed revert it to the original
					if (pEntity != nullptr && m_bMaterialChanged)
					{
						ApplyMaterialChange(pActInfo, pEntity, m_pEditorMaterial);
						m_bMaterialChanged = false;
					}
				}
				break;
			}

		case eFE_PrecacheResources:
			{
				// Preload target material.
				const string& mtlName = GetPortString(pActInfo, IN_MATERIAL);
				m_pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(mtlName.c_str());
				break;
			}

		case eFE_Activate:
			{
				if (pEntity)
				{
					if (IsPortActive(pActInfo, IN_SET))
					{
						ChangeMat(pActInfo, pEntity);
					}

					if (IsPortActive(pActInfo, IN_RESET))
					{
						ApplyMaterialChange(pActInfo, pEntity, m_pEditorMaterial);
					}

					if (!IsPortActive(pActInfo, IN_MATERIAL) && !IsPortActive(pActInfo, IN_SERIALIZE))
					{
						if (pEntity->GetMaterial())
						{
							ActivateOutput(pActInfo, OUT_VALUE, string(pEntity->GetMaterial()->GetName()));
						}
						else
						{
							IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
							
							{
								IMaterial* pMtl = pIEntityRender->GetRenderMaterial(0);
								if (pMtl)
									ActivateOutput(pActInfo, OUT_VALUE, string(pMtl->GetName()));
							}
						}
					}
				}
			}
			break;
		}
	}

	void ChangeMat(SActivationInfo* pActInfo, IEntity* pEntity)
	{
		//saving the original material before change it
		if (!m_bMaterialChanged)
		{
			m_pEditorMaterial = pEntity->GetMaterial();
			m_bMaterialChanged = true;
		}

		const string& mtlName = GetPortString(pActInfo, IN_MATERIAL);
		_smart_ptr<IMaterial> pMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(mtlName.c_str());
		if (pMtl != NULL)
		{
			ApplyMaterialChange(pActInfo, pEntity, pMtl);
		}
	}

	void ApplyMaterialChange(SActivationInfo* pActInfo, IEntity* pEntity, IMaterial* pMtl)
	{
		const int slot = GetPortInt(pActInfo, IN_SLOT);
		if (slot == -1)
		{
			pEntity->SetMaterial(pMtl);
		}
		else
		{
			if (IEntityRender* pIEntityRender = pEntity->GetRenderInterface())
			{
				pIEntityRender->SetSlotMaterial(slot, pMtl);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

////////////////////////////////////////////////////////////////////////////////////////
// Flow node for setting entity render parameters (opacity, glow, motionblur, etc). Same than CFlowNodeMaterialShaderParam, but it serializes the changes
////////////////////////////////////////////////////////////////////////////////////////
class CFlowNodeMaterialShaderParams : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNodeMaterialShaderParams(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		EIP_Get = 0,
		EIP_Name,
		EIP_SubMtlId,
		EIP_ParamFloat,
		EIP_Float,
		EIP_ParamColor,
		EIP_Color,
		EIP_Serialize
	};

	enum EOutputPorts
	{
		EOP_Float = 0,
		EOP_Color,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Get",               _HELP("Trigger to get current value")),
			InputPortConfig<string>("mat_Material",   _HELP("Material Name")),
			InputPortConfig<int>("SubMtlId",          0,                                                     _HELP("Sub Material Id, starting at 0"),                 _HELP("SubMaterialId")),
			InputPortConfig<string>("ParamFloat",     _HELP("Float Parameter to be set/get"),                0,                                                       _UICONFIG("dt=matparamname,name_ref=mat_Material,sub_ref=SubMtlId,param=float")),
			InputPortConfig<float>("ValueFloat",      0.0f,                                                  _HELP("Trigger and value to set the Float parameter")),
			InputPortConfig<string>("ParamColor",     _HELP("Color Parameter to be set/get"),                0,                                                       _UICONFIG("dt=matparamname,name_ref=mat_Material,sub_ref=SubMtlId,param=vec")),
			InputPortConfig<Vec3>("color_ValueColor", _HELP("Trigger and value to set the Color parameter")),
			InputPortConfig<bool>("Serialize",        _HELP("Serialize this change")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("ValueFloat", _HELP("Current Float Value")),
			OutputPortConfig<Vec3>("ValueColor",  _HELP("Current Color Value")),
			{ 0 }
		};

		config.sDescription = _HELP("Set/Get Material's Shader Parameters, with 'Set' serialization");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (GetPortBool(pActInfo, EIP_Serialize))
		{
			if (ser.IsWriting())
			{
				bool validFloat = false;
				bool validColor = false;
				float floatValue = 0;
				Vec3 colorValue(0.f, 0.f, 0.f);

				GetValues(pActInfo, floatValue, colorValue, validFloat, validColor);
				ser.Value("validFloat", validFloat);
				ser.Value("validColor", validColor);
				ser.Value("floatValue", floatValue);
				ser.Value("colorValue", colorValue);
			}

			if (ser.IsReading())
			{
				bool validFloat = false;
				bool validColor = false;
				float floatValue = 0;
				Vec3 colorValue(0.f, 0.f, 0.f);

				ser.Value("validFloat", validFloat);
				ser.Value("validColor", validColor);
				ser.Value("floatValue", floatValue);
				ser.Value("colorValue", colorValue);

				SetValues(pActInfo, validFloat, validColor, floatValue, colorValue);
			}
		}
	}

	IMaterial* ObtainMaterial(SActivationInfo* pActInfo)
	{
		const string& matName = GetPortString(pActInfo, EIP_Name);
		IMaterial* pMtl = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(matName);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeMaterialShaderParamSerialize: Material '%s' not found.", matName.c_str());
			return NULL;
		}

		const int& subMtlId = GetPortInt(pActInfo, EIP_SubMtlId);
		pMtl = pMtl->GetSafeSubMtl(subMtlId);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeMaterialShaderParamSerialize: Material '%s' has no sub-material %d", matName.c_str(), subMtlId);
			return NULL;
		}
		return pMtl;
	}

	IRenderShaderResources* ObtainRendRes(SActivationInfo* pActInfo, IMaterial* pMtl)
	{
		assert(pMtl);
		const SShaderItem& shaderItem = pMtl->GetShaderItem();
		IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
		if (pRendRes == 0)
		{
			const string& matName = GetPortString(pActInfo, EIP_Name);
			const int& subMtlId = GetPortInt(pActInfo, EIP_SubMtlId);
			GameWarning("[flow] CFlowNodeMaterialShaderParamSerialize: Material '%s' (submtl %d) has no shader-resources.", matName.c_str(), subMtlId);
		}
		return pRendRes;
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_Activate)
			return;

		const bool bGet = IsPortActive(pActInfo, EIP_Get);
		const bool bSetFloat = IsPortActive(pActInfo, EIP_Float);
		const bool bSetColor = IsPortActive(pActInfo, EIP_Color);

		if (!bGet && !bSetFloat && !bSetColor)
			return;

		if (bSetFloat || bSetColor)
		{
			float floatValue = GetPortFloat(pActInfo, EIP_Float);
			Vec3 colorValue = GetPortVec3(pActInfo, EIP_Color);

			SetValues(pActInfo, bSetFloat, bSetColor, floatValue, colorValue);
		}

		if (bGet)
		{
			bool validFloat = false;
			bool validColor = false;
			float floatValue = 0;
			Vec3 colorValue(0.f, 0.f, 0.f);

			GetValues(pActInfo, floatValue, colorValue, validFloat, validColor);

			if (validFloat)
				ActivateOutput(pActInfo, EOP_Float, floatValue);
			if (validColor)
				ActivateOutput(pActInfo, EOP_Color, colorValue);
		}
	}

	void GetValues(SActivationInfo* pActInfo, float& floatValue, Vec3& colorValue, bool& validFloat, bool& validColor)
	{
		validFloat = false;
		validColor = false;
		const string& paramNameFloat = GetPortString(pActInfo, EIP_ParamFloat);
		const string& paramNameColor = GetPortString(pActInfo, EIP_ParamColor);

		IMaterial* pMtl = ObtainMaterial(pActInfo);
		if (!pMtl)
			return;
		IRenderShaderResources* pRendRes = ObtainRendRes(pActInfo, pMtl);
		if (!pRendRes)
			return;
		DynArrayRef<SShaderParam>& params = pRendRes->GetParameters();

		validFloat = pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, true);
		validColor = pMtl->SetGetMaterialParamVec3(paramNameColor, colorValue, true);

		if (!validFloat)
		{
			for (int i = 0; i < params.size(); ++i)
			{
				SShaderParam& param = params[i];
				if (stricmp(param.m_Name, paramNameFloat) == 0)
				{
					switch (param.m_Type)
					{
					case eType_BOOL:
						floatValue = param.m_Value.m_Bool;
						validFloat = true;
						break;
					case eType_SHORT:
						floatValue = param.m_Value.m_Short;
						validFloat = true;
						break;
					case eType_INT:
						floatValue = (float)param.m_Value.m_Int;
						validFloat = true;
						break;
					case eType_FLOAT:
						floatValue = param.m_Value.m_Float;
						validFloat = true;
						break;
					default:
						break;
					}
				}
			}
		}

		if (!validColor)
		{
			for (int i = 0; i < params.size(); ++i)
			{
				SShaderParam& param = params[i];

				if (stricmp(param.m_Name, paramNameColor) == 0)
				{
					if (param.m_Type == eType_VECTOR)
					{
						colorValue.Set(param.m_Value.m_Vector[0], param.m_Value.m_Vector[1], param.m_Value.m_Vector[2]);
						validColor = true;
					}
					if (param.m_Type == eType_FCOLOR)
					{
						colorValue.Set(param.m_Value.m_Color[0], param.m_Value.m_Color[1], param.m_Value.m_Color[2]);
						validColor = true;
					}
					break;
				}
			}
		}
	}

	void SetValues(SActivationInfo* pActInfo, bool bSetFloat, bool bSetColor, float floatValue, Vec3& colorValue)
	{
		IMaterial* pMtl = ObtainMaterial(pActInfo);
		if (!pMtl)
			return;

		IRenderShaderResources* pRendRes = ObtainRendRes(pActInfo, pMtl);
		if (!pRendRes)
			return;
		DynArrayRef<SShaderParam>& params = pRendRes->GetParameters();
		const SShaderItem& shaderItem = pMtl->GetShaderItem();

		const string& paramNameFloat = GetPortString(pActInfo, EIP_ParamFloat);
		const string& paramNameColor = GetPortString(pActInfo, EIP_ParamColor);

		bool bUpdateShaderConstants = false;
		if (bSetFloat)
		{
			if (pMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, false) == false)
			{
				UParamVal val;
				val.m_Float = floatValue;
				SShaderParam::SetParam(paramNameFloat, &params, val);
				bUpdateShaderConstants = true;
			}
		}
		if (bSetColor)
		{
			if (pMtl->SetGetMaterialParamVec3(paramNameColor, colorValue, false) == false)
			{
				UParamVal val;
				val.m_Color[0] = colorValue[0];
				val.m_Color[1] = colorValue[1];
				val.m_Color[2] = colorValue[2];
				val.m_Color[3] = 1.0f;
				SShaderParam::SetParam(paramNameColor, &params, val);
				bUpdateShaderConstants = true;
			}
		}
		if (bUpdateShaderConstants)
		{
			shaderItem.m_pShaderResources->UpdateConstants(shaderItem.m_pShader);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

////////////////////////////////////////////////////////////////////////////////////////
// Flow node for setting entity render parameters (opacity, glow, motionblur, etc)
////////////////////////////////////////////////////////////////////////////////////////

class CFlowNodeEntityMaterialShaderParams : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNodeEntityMaterialShaderParams(SActivationInfo* pActInfo)
	{
		//m_entityId = (EntityId)(UINT_PTR)pActInfo->m_pUserData;
		m_anyChangeDone = false;
		m_pMaterial = NULL;
	}

	virtual IFlowNodePtr Clone(SActivationInfo *pActInfo)
	{
		return new CFlowNodeEntityMaterialShaderParams(pActInfo);
	}

	enum EInputPorts
	{
		EIP_Get = 0,
		EIP_Slot,
		EIP_SubMtlId,
		EIP_ParamFloat,
		EIP_Float,
		EIP_ParamColor,
		EIP_Color,
	};

	enum EOutputPorts
	{
		EOP_Float = 0,
		EOP_Color,
	};

	//////////////////////////////////////////////////////////////////////////
	/// this serialize is *NOT* perfect. If any of the following inputs: "slot, submtlid, paramFloat, paramColor" do change and the node is triggered more than once  with different values for them, the serializing wont be 100% correct.
	/// this does not happens in crysis2, so is good enough for now.
	/// to make it perfect, either the materials should be serialized at engine level, or either we would need to create an intermediate module to handle changes on materials from game side,
	/// make the flownodes to use that module instead of the materials directly, and serialize the changes there instead of in the flownodes.

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (!pActInfo->pEntity)
			return;

		// we do assume that those inputs never change, which is ok for crysis2, but of course is not totally right
		const int slot = GetPortInt(pActInfo, EIP_Slot);
		const int subMtlId = GetPortInt(pActInfo, EIP_SubMtlId);
		const string& paramNameFloat = GetPortString(pActInfo, EIP_ParamFloat);
		const string& paramNameColor = GetPortString(pActInfo, EIP_ParamColor);

		ser.Value("m_anyChangeDone", m_anyChangeDone);

		//---------------------
		if (ser.IsWriting())
		{
			IMaterial* pSubMtl;
			IRenderShaderResources* pRendShaderRes;
			if (!ObtainMaterialPtrs(pActInfo->pEntity, slot, subMtlId, false, pSubMtl, pRendShaderRes))
				return;
			DynArrayRef<SShaderParam>& shaderParams = pRendShaderRes->GetParameters();

			if (!paramNameFloat.empty())
			{
				float floatValue = 0;
				ReadFloatValueFromMat(pSubMtl, pRendShaderRes, paramNameFloat, floatValue);
				ser.Value("floatValue", floatValue);
			}

			if (!paramNameColor.empty())
			{
				Vec3 colorValue(ZERO);
				ReadColorValueFromMat(pSubMtl, pRendShaderRes, paramNameColor, colorValue);
				ser.Value("colorValue", colorValue);
			}
		}

		//----------------
		if (ser.IsReading())
		{
			// if there was no change done, we still need to set values to the material, in case it was changed after the savegame.
			// But in that case, we dont need to create a clone, we just use the current material
			bool cloneMaterial = m_anyChangeDone;
			IMaterial* pSubMtl;
			IRenderShaderResources* pRendShaderRes;
			if (!ObtainMaterialPtrs(pActInfo->pEntity, slot, subMtlId, cloneMaterial, pSubMtl, pRendShaderRes))
				return;
			DynArrayRef<SShaderParam>& shaderParams = pRendShaderRes->GetParameters();

			// if we *potentially* can change one of them, we have to restore them, even if we in theory didnt change any. that is why this checks the name of the parameters, instead of actualy changedone
			{
				bool bUpdateShaderConstants = false;
				if (!paramNameFloat.empty())
				{
					float floatValue;
					ser.Value("floatValue", floatValue);
					if (pSubMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, false) == false)
					{
						UParamVal val;
						val.m_Float = floatValue;
						SShaderParam::SetParam(paramNameFloat, &shaderParams, val);
						bUpdateShaderConstants = true;
					}
				}
				if (!paramNameColor.empty())
				{
					Vec3 colorValue;
					ser.Value("colorValue", colorValue);
					if (pSubMtl->SetGetMaterialParamVec3(paramNameColor, colorValue, false) == false)
					{
						UParamVal val;
						val.m_Color[0] = colorValue[0];
						val.m_Color[1] = colorValue[1];
						val.m_Color[2] = colorValue[2];
						val.m_Color[3] = 1.0f;
						SShaderParam::SetParam(paramNameColor, &shaderParams, val);
						bUpdateShaderConstants = true;
					}
				}

				if (bUpdateShaderConstants)
				{
					const SShaderItem& shaderItem = pSubMtl->GetShaderItem();
					shaderItem.m_pShaderResources->UpdateConstants(shaderItem.m_pShader);
				}
			}
		}
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_Void("Get",               _HELP("Trigger to get current value")),
			InputPortConfig<int>("Slot",              0,                                                 _HELP("Material Slot")),
			InputPortConfig<int>("SubMtlId",          0,                                                 _HELP("Sub Material Id, starting at 0"),                _HELP("SubMaterialId")),
			InputPortConfig<string>("ParamFloat",     _HELP("Float Parameter to be set/get"),            0,                                                      _UICONFIG("dt=matparamslot,slot_ref=Slot,sub_ref=SubMtlId,param=float")),
			InputPortConfig<float>("ValueFloat",      0.0f,                                              _HELP("Trigger and value to set the Float parameter")),
			InputPortConfig<string>("ParamColor",     _HELP("Color Parameter to be set/get"),            0,                                                      _UICONFIG("dt=matparamslot,slot_ref=Slot,sub_ref=SubMtlId,param=vec")),
			InputPortConfig<Vec3>("color_ValueColor", _HELP("Trigger and value to set the Color value")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<float>("ValueFloat", _HELP("Current Float Value")),
			OutputPortConfig<Vec3>("ValueColor",  _HELP("Current Color Value")),
			{ 0 }
		};

		config.sDescription = _HELP("Set/Get Entity's Material Shader Parameters");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (!pActInfo->pEntity)
			return;

		if (event != eFE_Activate)
			return;

		const bool bGet = IsPortActive(pActInfo, EIP_Get);
		const bool bSetFloat = IsPortActive(pActInfo, EIP_Float);
		const bool bSetColor = IsPortActive(pActInfo, EIP_Color);
		const int slot = GetPortInt(pActInfo, EIP_Slot);
		const int subMtlId = GetPortInt(pActInfo, EIP_SubMtlId);
		const string& paramNameFloat = GetPortString(pActInfo, EIP_ParamFloat);
		const string& paramNameColor = GetPortString(pActInfo, EIP_ParamColor);

		if (!bGet && !bSetFloat && !bSetColor)
			return;

		bool cloneMaterial = bSetFloat || bSetColor;
		IMaterial* pSubMtl;
		IRenderShaderResources* pRendShaderRes;

		if (!ObtainMaterialPtrs(pActInfo->pEntity, slot, subMtlId, cloneMaterial, pSubMtl, pRendShaderRes))
			return;

		DynArrayRef<SShaderParam>& shaderParams = pRendShaderRes->GetParameters();

		//..........sets.......
		{
			bool bUpdateShaderConstants = false;
			if (bSetFloat)
			{
				m_anyChangeDone = true;
				float floatValue = GetPortFloat(pActInfo, EIP_Float);
				if (pSubMtl->SetGetMaterialParamFloat(paramNameFloat, floatValue, false) == false)
				{
					UParamVal val;
					val.m_Float = floatValue;
					SShaderParam::SetParam(paramNameFloat, &shaderParams, val);
					bUpdateShaderConstants = true;
				}
			}
			if (bSetColor)
			{
				m_anyChangeDone = true;
				Vec3 colorValue = GetPortVec3(pActInfo, EIP_Color);
				if (pSubMtl->SetGetMaterialParamVec3(paramNameColor, colorValue, false) == false)
				{
					UParamVal val;
					val.m_Color[0] = colorValue[0];
					val.m_Color[1] = colorValue[1];
					val.m_Color[2] = colorValue[2];
					val.m_Color[3] = 1.0f;
					SShaderParam::SetParam(paramNameColor, &shaderParams, val);
					bUpdateShaderConstants = true;
				}
			}

			if (bUpdateShaderConstants)
			{
				const SShaderItem& shaderItem = pSubMtl->GetShaderItem();
				shaderItem.m_pShaderResources->UpdateConstants(shaderItem.m_pShader);
			}
		}

		//........get
		if (bGet)
		{
			float floatValue;
			if (ReadFloatValueFromMat(pSubMtl, pRendShaderRes, paramNameFloat, floatValue))
				ActivateOutput(pActInfo, EOP_Float, floatValue);

			Vec3 colorValue;
			if (ReadColorValueFromMat(pSubMtl, pRendShaderRes, paramNameColor, colorValue))
				ActivateOutput(pActInfo, EOP_Color, colorValue);
		}
	}

	bool ReadFloatValueFromMat(IMaterial* pSubMtl, IRenderShaderResources* pRendShaderRes, const char* paramName, float& floatValue)
	{
		if (pSubMtl->SetGetMaterialParamFloat(paramName, floatValue, true))
			return true;

		DynArrayRef<SShaderParam>& shaderParams = pRendShaderRes->GetParameters();

		for (int i = 0; i < shaderParams.size(); ++i)
		{
			SShaderParam& param = shaderParams[i];
			if (stricmp(param.m_Name, paramName) == 0)
			{
				floatValue = 0.0f;
				switch (param.m_Type)
				{
				case eType_BOOL:
					floatValue = param.m_Value.m_Bool;
					break;
				case eType_SHORT:
					floatValue = param.m_Value.m_Short;
					break;
				case eType_INT:
					floatValue = (float)param.m_Value.m_Int;
					break;
				case eType_FLOAT:
					floatValue = param.m_Value.m_Float;
					break;
				}
				return true;
			}
		}
		return false;
	}

	bool ReadColorValueFromMat(IMaterial* pSubMtl, IRenderShaderResources* pRendShaderRes, const char* paramName, Vec3& colorValue)
	{
		if (pSubMtl->SetGetMaterialParamVec3(paramName, colorValue, true))
			return true;

		DynArrayRef<SShaderParam>& shaderParams = pRendShaderRes->GetParameters();

		for (int i = 0; i < shaderParams.size(); ++i)
		{
			SShaderParam& param = shaderParams[i];
			if (stricmp(param.m_Name, paramName) == 0)
			{
				colorValue.Set(0, 0, 0);
				if (param.m_Type == eType_VECTOR)
				{
					colorValue.Set(param.m_Value.m_Vector[0], param.m_Value.m_Vector[1], param.m_Value.m_Vector[2]);
				}
				if (param.m_Type == eType_FCOLOR)
				{
					colorValue.Set(param.m_Value.m_Color[0], param.m_Value.m_Color[1], param.m_Value.m_Color[2]);
				}
				return true;
			}
		}
		return false;
	}

	/*
	   void OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
	   {
	   }
	 */
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	//////////////////////////////////////////////////////////////////////////
	bool ObtainMaterialPtrs(IEntity* pEntity, int slot, int subMtlId, bool cloneMaterial, IMaterial*& pSubMtl, IRenderShaderResources*& pRenderShaderRes)
	{
		//const bool RETFAIL = false;
		//const bool RETOK = true;

		//IEntity* pEntity = GetEntity();
		if (!pEntity)
			return false;

		IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
		if (pIEntityRender == 0)
			return false;

		IMaterial* pMaterial = pIEntityRender->GetRenderMaterial(slot);
		if (pMaterial == 0)
		{
			GameWarning("[flow] CFlowNodeEntityMaterialShaderParam: Entity '%s' [%d] has no material at slot %d", pEntity->GetName(), pEntity->GetId(), slot);
			return false;
		}

		if (cloneMaterial && m_pMaterial != pMaterial)
		{
			pMaterial = gEnv->p3DEngine->GetMaterialManager()->CloneMultiMaterial(pMaterial);
			if (!pMaterial)
			{
				GameWarning("[flow] CFlowNodeEntityMaterialShaderParam: Entity '%s' [%d] Can't clone material at slot %d", pEntity->GetName(), pEntity->GetId(), slot);
				return false;
			}
			pIEntityRender->SetSlotMaterial(slot, pMaterial);
			m_pMaterial = pMaterial;
		}

		pSubMtl = pMaterial->GetSafeSubMtl(subMtlId);
		if (pSubMtl == 0)
		{
			GameWarning("[flow] CFlowNodeEntityMaterialShaderParam: Entity '%s' [%d] has no sub-material %d at slot %d", pEntity->GetName(), pEntity->GetId(), subMtlId, slot);
			return false;
		}

		const SShaderItem& shaderItem = pSubMtl->GetShaderItem();
		pRenderShaderRes = shaderItem.m_pShaderResources;
		if (!pRenderShaderRes)
		{
			GameWarning("[flow] CFlowNodeEntityMaterialShaderParam: Entity '%s' [%d] Material's '%s' sub-material [%d] '%s' has not shader-resources", pEntity->GetName(), pEntity->GetId(), pMaterial->GetName(), subMtlId, pSubMtl->GetName());
			return false;
		}
		return true;
	}

protected:
	bool                  m_anyChangeDone;
	_smart_ptr<IMaterial> m_pMaterial;// this is only used to know if we need to clone or not. And is in purpose not reseted on flow initialize, and also not serialized.
	// the situation is:
	// if we already cloned the material, it will stay cloned even if we load a checkpoint previous to the point where the change happened, so we dont need to clone it again.
	// On the other side, if we are doing a 'resumegame", we need to clone the material even if we already cloned it when the checkpoint happened.
	// By not serializing or reseting this member, we cover those cases correctly
};

////////////////////////////////////////////////////////////////////////////////////////
// Flow node for cloning an entity's material
////////////////////////////////////////////////////////////////////////////////////////
class CFlowNodeEntityCloneMaterial : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNodeEntityCloneMaterial(SActivationInfo* pActInfo)
	{
	}

	enum EInputPorts
	{
		EIP_Clone = 0,
		EIP_Reset,
		EIP_Slot,
	};

	enum EOutputPorts
	{
		EOP_Cloned = 0,
		EOP_Reset
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Clone", _HELP("Trigger to clone material")),
			InputPortConfig_Void("Reset", _HELP("Trigger to reset material")),
			InputPortConfig<int>("Slot",  0,                                  _HELP("Material Slot")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Cloned", _HELP("Triggered when cloned.")),
			OutputPortConfig_Void("Reset",  _HELP("Triggered when reset.")),
			{ 0 }
		};

		config.sDescription = _HELP("Clone an Entity's Material and Reset it back to original.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				const int slot = GetPortInt(pActInfo, EIP_Slot);
				UnApplyMaterial(pActInfo->pEntity, slot);
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Reset))
				{
					const int slot = GetPortInt(pActInfo, EIP_Slot);
					UnApplyMaterial(pActInfo->pEntity, slot);
					ActivateOutput(pActInfo, EOP_Reset, true);
				}
				if (IsPortActive(pActInfo, EIP_Clone))
				{
					const int slot = GetPortInt(pActInfo, EIP_Slot);
					UnApplyMaterial(pActInfo->pEntity, slot);
					CloneAndApplyMaterial(pActInfo->pEntity, slot);
					ActivateOutput(pActInfo, EOP_Cloned, true);
				}
			}
			break;
		default:
			break;
		}
	}

	void UnApplyMaterial(IEntity* pEntity, int slot)
	{
		if (pEntity == 0)
			return;
		IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
		if (pIEntityRender == 0)
			return;
		pIEntityRender->SetSlotMaterial(slot, 0);
	}

	void CloneAndApplyMaterial(IEntity* pEntity, int slot)
	{
		if (pEntity == 0)
			return;

		IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
		if (pIEntityRender == 0)
			return;

		IMaterial* pMtl = pIEntityRender->GetSlotMaterial(slot);
		if (pMtl)
			return;

		pMtl = pIEntityRender->GetRenderMaterial(slot);
		if (pMtl == 0)
		{
			GameWarning("[flow] CFlowNodeEntityCloneMaterial: Entity '%s' [%d] has no material at slot %d", pEntity->GetName(), pEntity->GetId(), slot);
			return;
		}
		pMtl = gEnv->p3DEngine->GetMaterialManager()->CloneMultiMaterial(pMtl);
		pIEntityRender->SetSlotMaterial(slot, pMtl);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Material:EntityMaterialChange", CFlowNodeEntityMaterial)
REGISTER_FLOW_NODE("Material:MaterialParams", CFlowNodeMaterialShaderParams)
REGISTER_FLOW_NODE("Material:EntityMaterialParams", CFlowNodeEntityMaterialShaderParams)
REGISTER_FLOW_NODE("Material:MaterialClone", CFlowNodeEntityCloneMaterial)
