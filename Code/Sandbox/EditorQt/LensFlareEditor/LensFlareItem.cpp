// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LensFlareItem.h"
#include "LensFlareUtil.h"
#include <CryRenderer/IFlares.h>
#include "Objects/EntityObject.h"
#include "LensFlareEditor.h"
#include "LensFlareManager.h"
#include "LensFlareUndo.h"

CLensFlareItem::CLensFlareItem()
{
	m_pOptics = NULL;
	CreateOptics();
}

CLensFlareItem::~CLensFlareItem()
{
}

void CLensFlareItem::Serialize(SerializeContext& ctx)
{
	if (ctx.bLoading)
	{
		CreateOptics();
		LensFlareUtil::FillOpticsFromXML(m_pOptics, ctx.node);

		for (int i = 0, iChildCount(ctx.node->getChildCount()); i < iChildCount; ++i)
			AddChildOptics(m_pOptics, ctx.node->getChild(i));

		__super::Serialize(ctx);
	}
	else
	{
		__super::Serialize(ctx);
	}
}

void CLensFlareItem::AddChildOptics(IOpticsElementBasePtr pParentOptics, XmlNodeRef& pNode)
{
	if (pNode == NULL)
		return;

	if (pNode->getTag() && strcmp(pNode->getTag(), "FlareItem"))
		return;

	IOpticsElementBasePtr pOptics = LensFlareUtil::CreateOptics(pNode);
	if (pOptics == NULL)
		return;

	pParentOptics->AddElement(pOptics);

	for (int i = 0, iChildCount(pNode->getChildCount()); i < iChildCount; ++i)
		AddChildOptics(pOptics, pNode->getChild(i));
}

void CLensFlareItem::CreateOptics()
{
	m_pOptics = gEnv->pOpticsManager->Create(eFT_Root);
}

XmlNodeRef CLensFlareItem::CreateXmlData() const
{
	XmlNodeRef pRootNode = NULL;
	if (LensFlareUtil::CreateXmlData(m_pOptics, pRootNode))
	{
		pRootNode->setAttr("Name", m_pOptics->GetName());
		return pRootNode;
	}
	return NULL;
}

void CLensFlareItem::SetName(const string& name)
{
	SetName(name, false, false);
}

void CLensFlareItem::SetName(const string& name, bool bRefreshWhenUndo, bool bRefreshWhenRedo)
{
	string newNameWithGroup;
	string newFullName;
	LensFlareUtil::GetExpandedItemNames(this, LensFlareUtil::GetGroupNameFromName(name), LensFlareUtil::GetShortName(name), newNameWithGroup, newFullName);

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoRenameLensFlareItem(GetFullName(), newFullName, bRefreshWhenUndo, bRefreshWhenRedo));

	CBaseLibraryItem::SetName(name, true);

	if (m_pOptics)
		m_pOptics->SetName(GetShortName());
}

void CLensFlareItem::UpdateLights(IOpticsElementBasePtr pSrcOptics)
{
	string srcFullOpticsName = GetFullName();
	bool bUpdateChildren = false;
	if (pSrcOptics == NULL)
	{
		if (m_pOptics == NULL)
			return;
		pSrcOptics = m_pOptics;
		bUpdateChildren = true;
	}

	std::vector<CEntityObject*> lightEntities;
	LensFlareUtil::GetLightEntityObjects(lightEntities);

	for (int i = 0, iLightSize(lightEntities.size()); i < iLightSize; ++i)
	{
		CEntityObject* pLightEntity = lightEntities[i];
		if (pLightEntity == NULL)
			continue;

		IOpticsElementBasePtr pTargetOptics = pLightEntity->GetOpticsElement();
		if (pTargetOptics == NULL)
			continue;

		string targetFullOpticsName(pTargetOptics->GetName());
		if (srcFullOpticsName != targetFullOpticsName)
			continue;

		string srcOpticsName(pSrcOptics->GetName());
		IOpticsElementBasePtr pFoundOptics = NULL;
		if (LensFlareUtil::GetShortName(targetFullOpticsName) == srcOpticsName)
			pFoundOptics = pTargetOptics;
		else
			pFoundOptics = LensFlareUtil::FindOptics(pTargetOptics, srcOpticsName);

		if (pFoundOptics == NULL)
			continue;
		if (pFoundOptics->GetType() != pSrcOptics->GetType())
			continue;

		LensFlareUtil::CopyOptics(pSrcOptics, pFoundOptics, bUpdateChildren);
	}
}

void CLensFlareItem::ReplaceOptics(IOpticsElementBasePtr pNewData)
{
	if (pNewData == NULL)
		return;
	if (pNewData->GetType() != eFT_Root)
		return;
	CreateOptics();
	LensFlareUtil::CopyOptics(pNewData, m_pOptics);
	m_pOptics->SetName(GetFullName());
	UpdateLights();

	CLensFlareEditor* pLensFlareEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pLensFlareEditor == NULL)
		return;
	if (this != pLensFlareEditor->GetSelectedLensFlareItem())
		return;
	pLensFlareEditor->UpdateLensFlareItem(this);
	pLensFlareEditor->RemovePropertyItems();
}

