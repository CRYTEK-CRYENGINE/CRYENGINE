// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DesignerEditor.h"
#include "Core/Helper.h"

#include <CrySystem/ICryLink.h>

namespace Designer
{

template<class T>
void DesignerBaseObject<T >::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	if (m_pCompiler)
	{
		m_pCompiler->DeleteAllRenderNodes();
		m_pCompiler = NULL;
	}

	__super::Done();

	if (!IsEmpty() && DesignerSession::GetInstance()->GetBaseObject() == this)
		GetIEditor()->SetEditTool(NULL);
}

template<class T>
void DesignerBaseObject<T >::SetCompiler(ModelCompiler* pCompiler)
{
	DESIGNER_ASSERT(pCompiler);
	m_pCompiler = pCompiler;
}

template<class T>
ModelCompiler* DesignerBaseObject<T >::GetCompiler() const
{
	if (!m_pCompiler)
		m_pCompiler = new ModelCompiler(eCompiler_General);
	return m_pCompiler;
}

template<class T>
void DesignerBaseObject<T >::SetModel(Model* pModel)
{
	DESIGNER_ASSERT(pModel);
	m_pModel = pModel;
}

template<class T>
void DesignerBaseObject<T >::UpdateEngineNode()
{
	if (GetCompiler() == NULL)
		return;
	GetCompiler()->Compile(this, GetModel(), eShelf_Base, true);
}

template<class T>
bool DesignerBaseObject<T >::QueryNearestPos(const BrushVec3& worldPos, BrushVec3& outPos) const
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Base);

	BrushVec3 modelPos = GetWorldTM().GetInverted().TransformPoint(worldPos);
	BrushVec3 outModelPos;
	if (GetModel()->QueryNearestPos(modelPos, outModelPos))
	{
		outPos = GetWorldTM().TransformPoint(outModelPos);
		return true;
	}
	return false;
}

template<class T>
void DesignerBaseObject<T >::Validate()
{
	CBaseObject::Validate();
	if (!GetModel() || GetModel()->IsEmpty(eShelf_Base))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Empty Designer Object: %s %s", GetName(),
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));

	}
	else if (!CheckIfHasValidModel(this))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "This designer object consists of only edges not polygons: %s %s", GetName(),
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
	}
}

template<class T>
void DesignerBaseObject<T >::UpdateHighlightPassState(bool bSelected, bool bHighlighted)
{
	GetCompiler()->UpdateHighlightPassState(bSelected, bHighlighted);
}

template<class T>
IStatObj* DesignerBaseObject<T >::GetIStatObj()
{
	if (!GetCompiler())
		return NULL;

	_smart_ptr<IStatObj> pStatObj = NULL;
	if (!GetCompiler()->GetIStatObj(&pStatObj))
		return NULL;

	return pStatObj;
}

template<class T>
void DesignerBaseObject<T >::UpdateHiddenIStatObjState()
{
	ModelCompiler* pCompiler = GetCompiler();

	if (!pCompiler)
		return;

	bool bHidden = IsHiddenByOption() || IsHidden() || IsHiddenBySpec() || GetIEditor()->IsInGameMode();

	if (bHidden)
	{
		pCompiler->SetRenderFlags(GetCompiler()->GetRenderFlags() | ERF_HIDDEN);
	}
	else
	{
		pCompiler->SetRenderFlags(GetCompiler()->GetRenderFlags() & (~ERF_HIDDEN));
	}

	pCompiler->Compile(this, GetModel(), eShelf_Any, true);
}

template<class T>
void DesignerBaseObject<T >::UpdateVisibility(bool visible)
{
	bool bShouldBeUpdated = visible == CheckFlags(OBJFLAG_INVISIBLE);

	CBaseObject::UpdateVisibility(visible);
	ModelCompiler* pCompiler = GetCompiler();

	if (pCompiler->GetRenderNode())
	{
		int renderFlag = pCompiler->GetRenderFlags();
		if (!visible || IsHiddenBySpec() || IsHiddenByOption() || IsHidden())
			renderFlag |= ERF_HIDDEN;
		else
			renderFlag &= ~ERF_HIDDEN;

		pCompiler->SetRenderFlags(renderFlag);

		_smart_ptr<IStatObj> pStatObj;
		if (pCompiler->GetIStatObj(&pStatObj))
		{
			int flag = pStatObj->GetFlags();
			if (visible)
				flag &= (~STATIC_OBJECT_HIDDEN);
			else
				flag |= STATIC_OBJECT_HIDDEN;
			pStatObj->SetFlags(flag);
		}
	}

	if (visible && bShouldBeUpdated)
		pCompiler->Compile(this, GetModel());
}

}

