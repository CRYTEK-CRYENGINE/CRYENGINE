// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialHelpers.h"
#include <CryRenderer/IShader.h>

namespace MaterialHelpers
{
//////////////////////////////////////////////////////////////////////////
static void ParsePublicParamsScript(const char* sUIScript, IVariable* pVar)
{
	string uiscript = sUIScript;
	string element[3];
	int p1 = 0;
	string itemToken = uiscript.Tokenize(";", p1);
	while (!itemToken.empty())
	{
		int nElements = 0;
		int p2 = 0;
		string token = itemToken.Tokenize(" \t\r\n=", p2);
		while (!token.empty())
		{
			element[nElements++] = token;
			if (nElements == 2)
			{
				element[nElements] = itemToken.substr(p2);
				element[nElements].Trim(" =\t\"");
				break;
			}
			token = itemToken.Tokenize(" \t\r\n=", p2);
		}

		float minLimit, maxLimit;
		pVar->GetLimits(minLimit, maxLimit);

		if (stricmp(element[1], "UIWidget") == 0)
		{
			if (stricmp(element[2], "Color") == 0)
				pVar->SetDataType(IVariable::DT_COLOR);
		}
		else if (stricmp(element[1], "UIHelp") == 0)
		{
			string help = element[2];
			help.replace("\\n", "\n");
			pVar->SetDescription(help);
		}
		else if (stricmp(element[1], "UIName") == 0)
		{
			pVar->SetHumanName(element[2].c_str());
		}
		else if (stricmp(element[1], "UIMin") == 0)
		{
			pVar->SetLimits(atof(element[2]), maxLimit);
		}
		else if (stricmp(element[1], "UIMax") == 0)
		{
			pVar->SetLimits(minLimit, atof(element[2]));
		}
		else if (stricmp(element[1], "UIStep") == 0)
		{

		}

		itemToken = uiscript.Tokenize(";", p1);
	}
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* GetPublicVars(SInputShaderResources& pShaderResources)
{
	if (pShaderResources.m_ShaderParams.empty())
		return 0;

	CVarBlock* pPublicVars = new CVarBlock;
	for (int i = 0; i < pShaderResources.m_ShaderParams.size(); i++)
	{
		IVariable* pIVar = NULL;
		SShaderParam* pParam = &pShaderResources.m_ShaderParams[i];

		switch (pParam->m_Type)
		{
		case eType_BYTE:
			pIVar = new CVariable<int>(pParam->m_Value.m_Byte);
			break;
		case eType_SHORT:
			pIVar = new CVariable<int>(pParam->m_Value.m_Short);
			break;
		case eType_INT:
			pIVar = new CVariable<int>(pParam->m_Value.m_Int);
			break;
		case eType_FLOAT:
			pIVar = new CVariable<float>(pParam->m_Value.m_Float);
			break;
		/*	case eType_STRING:
		    pIVar = new CVariable<string>(pParam->m_Value.m_String);
		    break;	*/
		case eType_FCOLOR:
			pIVar = new CVariable<Vec3>(Vec3(pParam->m_Value.m_Color[0], pParam->m_Value.m_Color[1], pParam->m_Value.m_Color[2]));
			pIVar->SetDataType(IVariable::DT_COLOR);
			break;
		case eType_VECTOR:
			pIVar = new CVariable<Vec3>(Vec3(pParam->m_Value.m_Vector[0], pParam->m_Value.m_Vector[1], pParam->m_Value.m_Vector[2]));
			break;
		default:
			break;
		}

		if (pIVar)
		{
			pIVar->SetName(pParam->m_Name);
			pPublicVars->AddVariable(pIVar);

			if (pParam->m_Script.size())
			{
				ParsePublicParamsScript(pParam->m_Script.c_str(), pIVar);
			}
		}
	}

	return pPublicVars;
}

void SetPublicVars(CVarBlock* pPublicVars, SInputShaderResources& pInputShaderResources)
{
	assert(pPublicVars);

	if (pInputShaderResources.m_ShaderParams.empty())
		return;

	int numVars = pPublicVars->GetNumVariables();

	for (int i = 0; i < numVars; i++)
	{
		if (i >= numVars)
			break;

		IVariable* pVar = pPublicVars->GetVariable(i);
		SShaderParam* pParam = NULL;
		for (int j = 0; j < pInputShaderResources.m_ShaderParams.size(); j++)
		{
			if (strcmp(pVar->GetName(), pInputShaderResources.m_ShaderParams[j].m_Name) == 0)
			{
				pParam = &pInputShaderResources.m_ShaderParams[j];
				break;
			}
		}
		if (!pParam)
			continue;

		switch (pParam->m_Type)
		{
		case eType_BYTE:
			if (pVar->GetType() == IVariable::INT)
			{
				int val = 0;
				pVar->Get(val);
				pParam->m_Value.m_Byte = val;
			}
			break;
		case eType_SHORT:
			if (pVar->GetType() == IVariable::INT)
			{
				int val = 0;
				pVar->Get(val);
				pParam->m_Value.m_Short = val;
			}
			break;
		case eType_INT:
			if (pVar->GetType() == IVariable::INT)
			{
				int val = 0;
				pVar->Get(val);
				pParam->m_Value.m_Int = val;
			}
			break;
		case eType_FLOAT:
			if (pVar->GetType() == IVariable::FLOAT)
			{
				float val = 0;
				pVar->Get(val);
				pParam->m_Value.m_Float = val;
			}
			break;
		/*
		   case eType_STRING:
		   if (pVar->GetType() == IVariable::STRING)
		   {
		   CString str;
		   int val = 0;
		   pVar->Get(val);
		   pParam->m_Value.m_Byte = val;
		   }
		   break;
		 */
		case eType_FCOLOR:
			if (pVar->GetType() == IVariable::VECTOR && pVar->GetDataType() == IVariable::DT_COLOR)
			{
				Vec3 val(0, 0, 0);
				pVar->Get(val);
				pParam->m_Value.m_Color[0] = val.x;
				pParam->m_Value.m_Color[1] = val.y;
				pParam->m_Value.m_Color[2] = val.z;
			}
			break;
		case eType_VECTOR:
			if (pVar->GetType() == IVariable::VECTOR)
			{
				Vec3 val(0, 0, 0);
				pVar->Get(val);
				pParam->m_Value.m_Vector[0] = val.x;
				pParam->m_Value.m_Vector[1] = val.y;
				pParam->m_Value.m_Vector[2] = val.z;
			}
			break;
		default:
			break;
		}
	}
}

void SetPublicVars(CVarBlock* pPublicVars, SInputShaderResources& pInputShaderResources, IRenderShaderResources* pRenderShaderResources, IShader* pShader)
{
	SetPublicVars(pPublicVars, pInputShaderResources);

	// Set shader params.
	if (pRenderShaderResources)
		pRenderShaderResources->SetShaderParams(&pInputShaderResources, pShader);
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* GetShaderGenParamsVars(IShader* pShader, uint64 nShaderGenMask)
{
	IShader* pTemplShader = pShader;
	if (!pTemplShader)
		return 0;

	SShaderGen* pShaderGen = pTemplShader->GetGenerationParams();
	if (!pShaderGen)
		return 0;

	CVarBlock* pBlock = new CVarBlock;
	for (int i = 0; i < pShaderGen->m_BitMask.size(); i++)
	{
		SShaderGenBit* pGenBit = pShaderGen->m_BitMask[i];
		if (pGenBit->m_Flags & SHGF_HIDDEN)
			continue;
		if (!pGenBit->m_ParamProp.empty())
		{
			CVariable<bool>* pVar = new CVariable<bool>;
			pBlock->AddVariable(pVar);
			pVar->SetName(pGenBit->m_ParamProp.c_str());
			*pVar = (pGenBit->m_Mask & nShaderGenMask) != 0;
			pVar->SetDescription(pGenBit->m_ParamDesc.c_str());
		}
	}

	return pBlock;
}

//////////////////////////////////////////////////////////////////////////
uint64 SetShaderGenParamsVars(IShader* pShader, CVarBlock* pBlock)
{
	IShader* pTemplShader = pShader;
	if (!pTemplShader)
		return 0;

	SShaderGen* pShaderGen = pTemplShader->GetGenerationParams();
	if (!pShaderGen)
		return 0;

	uint64 nGenMask = 0;

	for (int i = 0; i < pShaderGen->m_BitMask.size(); i++)
	{
		SShaderGenBit* pGenBit = pShaderGen->m_BitMask[i];
		if (pGenBit->m_Flags & SHGF_HIDDEN)
			continue;

		if (!pGenBit->m_ParamProp.empty())
		{
			IVariable* pVar = pBlock->FindVariable(pGenBit->m_ParamProp.c_str());
			if (!pVar)
				continue;
			bool bFlagOn = false;
			pVar->Get(bFlagOn);
			if (bFlagOn)
				nGenMask |= pGenBit->m_Mask;
		}
	}

	return nGenMask;
}
}

