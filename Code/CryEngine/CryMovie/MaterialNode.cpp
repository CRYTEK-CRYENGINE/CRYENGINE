// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialNode.h"
#include "AnimTrack.h"

#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include "AnimSplineTrack.h"
#include "CompoundSplineTrack.h"

// Don't remove or the console builds will break!
#define s_nodeParamsInitialized s_nodeParamsInitializedMat
#define s_nodeParams            s_nodeParamsMat
#define AddSupportedParam       AddSupportedParamMat

namespace
{
bool s_nodeParamsInitialized = false;
std::vector<CAnimNode::SParamInfo> s_nodeParams;

void AddSupportedParam(const char* sName, int paramId, EAnimValue valueType)
{
	CAnimNode::SParamInfo param;
	param.name = sName;
	param.paramType = paramId;
	param.valueType = valueType;
	s_nodeParams.push_back(param);
}
}

enum EMaterialNodeParam
{
	MTL_PARAM_AMBIENT       = eAnimParamType_User + 2,
	MTL_PARAM_SHADER_PARAM1 = eAnimParamType_User + 100,
};

void CAnimMaterialNode::InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType)
{
	if (paramType == eAnimParamType_MaterialOpacity)
	{
		pTrack->SetKeyValueRange(0.0f, 100.f);
	}
	else if (paramType == eAnimParamType_MaterialSmoothness)
	{
		pTrack->SetKeyValueRange(0.0f, 255.f);
	}
}

CAnimMaterialNode::CAnimMaterialNode(const int id)
	: CAnimNode(id)
{
	SetFlags(GetFlags() | eAnimNodeFlags_CanChangeName);

	CAnimMaterialNode::Initialize();
}

void CAnimMaterialNode::Initialize()
{
	if (!s_nodeParamsInitialized)
	{
		s_nodeParamsInitialized = true;

		s_nodeParams.reserve(6);
		AddSupportedParam("Opacity", eAnimParamType_MaterialOpacity, eAnimValue_Float);
		AddSupportedParam("Ambient", MTL_PARAM_AMBIENT, eAnimValue_Vector);
		AddSupportedParam("Diffuse", eAnimParamType_MaterialDiffuse, eAnimValue_RGB);
		AddSupportedParam("Specular", eAnimParamType_MaterialSpecular, eAnimValue_RGB);
		AddSupportedParam("Emission", eAnimParamType_MaterialEmissive, eAnimValue_RGB);
		AddSupportedParam("Glossiness", eAnimParamType_MaterialSmoothness, eAnimValue_Float);
	}
}

void CAnimMaterialNode::SetName(const char* name)
{
	CAnimNode::SetName(name);
	UpdateDynamicParams();
}

void CAnimMaterialNode::UpdateDynamicParams()
{
	m_dynamicShaderParamInfos.clear();
	m_nameToDynamicShaderParam.clear();

	const char* pName = GetName();
	IMaterial* pMtl = GetMaterialByName(pName);

	if (!pMtl)
	{
		return;
	}

	const SShaderItem& shaderItem = pMtl->GetShaderItem();
	IRenderShaderResources* pShaderResources = shaderItem.m_pShaderResources;

	if (!pShaderResources)
	{
		return;
	}

	DynArrayRef<SShaderParam>& shaderParams = pShaderResources->GetParameters();

	for (int i = 0; i < shaderParams.size(); ++i)
	{
		SShaderParam& shaderParam = shaderParams[i];
		m_nameToDynamicShaderParam[shaderParams[i].m_Name] = i;

		CAnimNode::SParamInfo paramInfo;

		switch (shaderParam.m_Type)
		{
		case eType_FLOAT:

		// Fall through
		case eType_HALF:
			paramInfo.valueType = eAnimValue_Float;
			break;

		case eType_VECTOR:
			paramInfo.valueType = eAnimValue_Vector;
			break;

		case eType_FCOLOR:
			paramInfo.valueType = eAnimValue_RGB;
			break;

		case eType_BOOL:
			paramInfo.valueType = eAnimValue_Bool;
			break;

		default:
			continue;
		}

		paramInfo.name = shaderParam.m_Name;
		paramInfo.paramType = shaderParam.m_Name;
		paramInfo.flags = IAnimNode::ESupportedParamFlags(0);

		m_dynamicShaderParamInfos.push_back(paramInfo);
	}
}

unsigned int CAnimMaterialNode::GetParamCount() const
{
	return s_nodeParams.size() + m_dynamicShaderParamInfos.size();
}

CAnimParamType CAnimMaterialNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex < s_nodeParams.size())
	{
		return s_nodeParams[nIndex].paramType;
	}
	else if (nIndex >= (unsigned int)s_nodeParams.size() &&
	         nIndex < ((unsigned int)s_nodeParams.size() + (unsigned int)m_dynamicShaderParamInfos.size()))
	{
		return m_dynamicShaderParamInfos[nIndex - (int)s_nodeParams.size()].paramType;
	}

	return eAnimParamType_Invalid;
}

bool CAnimMaterialNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
	for (size_t i = 0; i < s_nodeParams.size(); ++i)
	{
		if (s_nodeParams[i].paramType == paramId)
		{
			info = s_nodeParams[i];
			return true;
		}
	}

	for (size_t i = 0; i < m_dynamicShaderParamInfos.size(); ++i)
	{
		if (m_dynamicShaderParamInfos[i].paramType == paramId)
		{
			info = m_dynamicShaderParamInfos[i];
			return true;
		}
	}

	return false;
}

const char* CAnimMaterialNode::GetParamName(const CAnimParamType& param) const
{
	if (param.GetType() == eAnimParamType_ByString)
	{
		return param.GetName();
	}
	else if ((int)param.GetType() >= (int)MTL_PARAM_SHADER_PARAM1)
	{
		switch ((int)param.GetType())
		{
		case MTL_PARAM_SHADER_PARAM1:
			return "Shader Param 1";

		case MTL_PARAM_SHADER_PARAM1 + 1:
			return "Shader Param 2";

		case MTL_PARAM_SHADER_PARAM1 + 2:
			return "Shader Param 3";

		case MTL_PARAM_SHADER_PARAM1 + 3:
			return "Shader Param 4";

		case MTL_PARAM_SHADER_PARAM1 + 4:
			return "Shader Param 5";

		case MTL_PARAM_SHADER_PARAM1 + 5:
			return "Shader Param 6";

		case MTL_PARAM_SHADER_PARAM1 + 6:
			return "Shader Param 7";

		case MTL_PARAM_SHADER_PARAM1 + 7:
			return "Shader Param 8";

		case MTL_PARAM_SHADER_PARAM1 + 8:
			return "Shader Param 9";

		default:
			return "Unknown Shader Param";
		}
	}

	return CAnimNode::GetParamName(param);
}

void CAnimMaterialNode::Animate(SAnimContext& animContext)
{
	int paramCount = NumTracks();

	if (paramCount <= 0)
	{
		return;
	}

	// Find material.
	const char* pName = GetName();
	IMaterial* pMtl = GetMaterialByName(pName);

	if (!pMtl)
	{
		return;
	}

	const SShaderItem& shaderItem = pMtl->GetShaderItem();
	IRenderShaderResources* pShaderResources = shaderItem.m_pShaderResources;

	if (!pShaderResources)
	{
		return;
	}

	float fValue;
	Vec3 vValue;

	int trackCount = NumTracks();

	for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
	{
		CAnimParamType paramId = m_tracks[paramIndex]->GetParameterType();
		IAnimTrack* pTrack = m_tracks[paramIndex];

		if (pTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled)
		{
			continue;
		}

		const TMovieSystemValue value = pTrack->GetValue(animContext.time);

		switch (paramId.GetType())
		{
		case eAnimParamType_MaterialOpacity:
			pShaderResources->SetStrengthValue(EFTT_OPACITY, stl::get<float>(value));
			break;

		case eAnimParamType_MaterialDiffuse:
			vValue = stl::get<Vec3>(value);
			{
				pShaderResources->SetColorValue(EFTT_DIFFUSE, vValue / 255.0f);
			}
			break;

		case eAnimParamType_MaterialSpecular:
			vValue = stl::get<Vec3>(value);
			{
				pShaderResources->SetColorValue(EFTT_SPECULAR, vValue / 255.0f);
			}
			break;

		case eAnimParamType_MaterialEmissive:
			vValue = stl::get<Vec3>(value);
			{
				pShaderResources->SetColorValue(EFTT_EMITTANCE, vValue / 255.0f);
			}
			break;

		case eAnimParamType_MaterialSmoothness:
			fValue = stl::get<float>(value);
			{
				pShaderResources->SetStrengthValue(EFTT_SMOOTHNESS, fValue / 255.0f);
			}
			break;

		case eAnimParamType_ByString:
			AnimateNamedParameter(animContext, pShaderResources, paramId.GetName(), pTrack);
			break;

		default:

			// Legacy support code
			if (paramId.GetType() >= (EAnimParamType)MTL_PARAM_SHADER_PARAM1)
			{
				int id = paramId.GetType() - (EAnimParamType)MTL_PARAM_SHADER_PARAM1;

				if (id < (int)pShaderResources->GetParameters().size())
				{
					pShaderResources->GetParameters()[id].m_Value.m_Float = stl::get<float>(value);
				}
			}
		}
	}

	IShader* pShader = shaderItem.m_pShader;

	if (pShader)
	{
		shaderItem.m_pShaderResources->UpdateConstants(pShader);
	}
}

void CAnimMaterialNode::AnimateNamedParameter(SAnimContext& animContext, IRenderShaderResources* pShaderResources, const char* name, IAnimTrack* pTrack)
{
	TDynamicShaderParamsMap::iterator findIter = m_nameToDynamicShaderParam.find(name);

	if (findIter != m_nameToDynamicShaderParam.end())
	{
		SShaderParam& param = pShaderResources->GetParameters()[findIter->second];

		const TMovieSystemValue value = pTrack->GetValue(animContext.time);

		Vec3 vecValue;
		Vec3 colorValue;

		switch (pTrack->GetValueType())
		{
		case eAnimValue_Float:
			param.m_Value.m_Float = stl::get<float>(value);
			break;

		case eAnimValue_Vector:
			vecValue = stl::get<Vec3>(value);
			param.m_Value.m_Vector[0] = vecValue[0];
			param.m_Value.m_Vector[1] = vecValue[1];
			param.m_Value.m_Vector[2] = vecValue[2];
			break;

		case eAnimValue_RGB:
			colorValue = stl::get<Vec3>(value);
			param.m_Value.m_Color[0] = colorValue[0];
			param.m_Value.m_Color[1] = colorValue[1];
			param.m_Value.m_Color[2] = colorValue[2];
			param.m_Value.m_Color[3] = 0.0f;
			break;

		case eAnimValue_Bool:
			param.m_Value.m_Bool = stl::get<bool>(value);
			break;
		}
	}
}

IMaterial* CAnimMaterialNode::GetMaterialByName(const char* pName)
{
	const char* pCh;

	if (pCh = strstr(pName, ".["))
	{
		char MatName[256];
		cry_strcpy(MatName, pName, pCh - pName);
		IMaterial* pMat = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(MatName);

		if (!pMat)
		{
			return NULL;
		}

		pCh += 2;

		if (!(*pCh))
		{
			return NULL;
		}

		int index = atoi(pCh) - 1;

		if (index < 0 || index >= pMat->GetSubMtlCount())
		{
			return NULL;
		}

		return pMat->GetSubMtl(index);
	}
	else
	{
		return gEnv->p3DEngine->GetMaterialManager()->FindMaterial(GetName());
	}
}

#undef s_nodeParamsInitialized
#undef s_nodeParams
#undef AddSupportedParam
