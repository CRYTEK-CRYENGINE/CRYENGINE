// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LensFlareElement.h"
#include "LensFlareElementTree.h"
#include "LensFlareUtil.h"
#include "LensFlareElement.h"
#include "LensFlareItem.h"
#include "LensFlareEditor.h"
#include "LensFlareView.h"

CLensFlareElement::CLensFlareElement() :
	m_vars(NULL),
	m_pOpticsElement(NULL)
{
}

CLensFlareElement::~CLensFlareElement()
{
}

void CLensFlareElement::OnInternalVariableChange(IVariable* pVar)
{
	IOpticsElementBasePtr pOptics = GetOpticsElement();
	if (pOptics == NULL)
		return;

	IFuncVariable* pFuncVar = LensFlareUtil::GetFuncVariable(pOptics, (int)(intptr_t)pVar->GetUserData());
	if (pFuncVar == NULL)
		return;

	switch (pVar->GetType())
	{
	case IVariable::INT:
		{
			int var(0);
			pVar->Get(var);
			if (pFuncVar->paramType == e_COLOR)
			{
				ColorF color(pFuncVar->GetColorF());
				color.a = (float)var / 255.0f;
				pFuncVar->InvokeSetter((void*)&color);
			}
			else if (pFuncVar->paramType == e_INT)
			{
				if (LensFlareUtil::HaveParameterLowBoundary(pFuncVar->name.c_str()))
					LensFlareUtil::BoundaryProcess(var);
				pFuncVar->InvokeSetter((void*)&var);
				if (pFuncVar->GetInt() != var)
					pVar->Set(pFuncVar->GetInt());
			}
		}
		break;
	case IVariable::BOOL:
		{
			if (pFuncVar->paramType == e_BOOL)
			{
				bool var;
				pVar->Get(var);
				pFuncVar->InvokeSetter((void*)&var);
			}
		}
		break;
	case IVariable::FLOAT:
		{
			if (pFuncVar->paramType == e_FLOAT)
			{
				float var;
				pVar->Get(var);
				pFuncVar->InvokeSetter((void*)&var);
			}
		}
		break;
	case IVariable::VECTOR2:
		{
			if (pFuncVar->paramType == e_VEC2)
			{
				Vec2 var;
				pVar->Get(var);
				pFuncVar->InvokeSetter((void*)&var);
			}
		}
		break;
	case IVariable::VECTOR:
		{
			Vec3 var;
			pVar->Get(var);
			if (pFuncVar->paramType == e_COLOR)
			{
				ColorF color(pFuncVar->GetColorF());
				color.r = var.x;
				color.g = var.y;
				color.b = var.z;
				pFuncVar->InvokeSetter((void*)&color);
			}
			else if (pFuncVar->paramType == e_VEC3)
			{
				pFuncVar->InvokeSetter((void*)&var);
			}
		}
		break;
	case IVariable::VECTOR4:
		{
			if (pFuncVar->paramType == e_VEC4)
			{
				Vec4 var;
				pVar->Get(var);
				pFuncVar->InvokeSetter((void*)&var);
			}
		}
		break;
	case IVariable::STRING:
		{
			string var;
			pVar->Get(var);
			if (pFuncVar->paramType == e_TEXTURE2D || pFuncVar->paramType == e_TEXTURE3D || pFuncVar->paramType == e_TEXTURE_CUBE)
			{
				var.Trim(" ");
				ITexture* pTexture = NULL;
				if (!var.IsEmpty())
				{
					pTexture = GetIEditorImpl()->GetRenderer()->EF_LoadTexture(var);
				}
				pFuncVar->InvokeSetter((void*)pTexture);
				if (pTexture)
				{
					pTexture->Release();
				}
			}
		}
		break;
	}

	UpdateLights();
}

bool CLensFlareElement::IsEnable()
{
	IOpticsElementBasePtr pOptics = GetOpticsElement();
	if (pOptics == NULL)
		return false;
	return pOptics->IsEnabled();
}

void CLensFlareElement::SetEnable(bool bEnable)
{
	IOpticsElementBasePtr pOptics = GetOpticsElement();
	if (pOptics == NULL)
		return;
	pOptics->SetEnabled(bEnable);
	UpdateLights();
}

EFlareType CLensFlareElement::GetOpticsType()
{
	IOpticsElementBasePtr pOptics = GetOpticsElement();
	if (pOptics == NULL)
		return eFT__Base__;
	return pOptics->GetType();
}

bool CLensFlareElement::GetShortName(string& outName) const
{
	string fullName;
	if (!GetName(fullName))
		return false;
	int nPos = fullName.ReverseFind('.');
	if (nPos == -1)
	{
		outName = fullName;
		return true;
	}
	outName = fullName.Right(fullName.GetLength() - nPos - 1);
	return true;
}

void CLensFlareElement::UpdateLights()
{
	IOpticsElementBasePtr pOptics = GetOpticsElement();
	if (pOptics == NULL)
		return;
	if (GetLensFlareTree())
	{
		if (GetLensFlareTree()->GetLensFlareItem())
			GetLensFlareTree()->GetLensFlareItem()->UpdateLights(pOptics);
	}
}

void CLensFlareElement::UpdateProperty(IOpticsElementBasePtr pOptics)
{
	if (GetLensFlareTree())
	{
		std::vector<IVariable::OnSetCallback> funcs;
		funcs.push_back(functor(*GetLensFlareTree(), &CLensFlareElementTree::OnInternalVariableChange));
		funcs.push_back(functor(*GetLensFlareView(), &CLensFlareView::OnInternalVariableChange));
		LensFlareUtil::SetVariablesTemplateFromOptics(pOptics, m_vars, funcs);
	}
	else
	{
		LensFlareUtil::SetVariablesTemplateFromOptics(pOptics, m_vars);
	}
}

CLensFlareElementTree* CLensFlareElement::GetLensFlareTree() const
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pEditor == NULL)
		return NULL;
	return pEditor->GetLensFlareElementTree();
}

CLensFlareView* CLensFlareElement::GetLensFlareView() const
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pEditor == NULL)
		return NULL;
	return pEditor->GetLensFlareView();
}

