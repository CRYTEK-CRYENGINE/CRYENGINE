// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleLibrary.h"
#include "ParticleItem.h"

//////////////////////////////////////////////////////////////////////////
// CParticleLibrary implementation.
//////////////////////////////////////////////////////////////////////////
bool CParticleLibrary::Save()
{
	return SaveLibrary("ParticleLibrary");
}

//////////////////////////////////////////////////////////////////////////
bool CParticleLibrary::Load(const string& filename)
{
	if (filename.IsEmpty())
		return false;

	SetFilename(filename);
	XmlNodeRef root = XmlHelpers::LoadXmlFromFile(filename);
	if (!root)
		return false;

	Serialize(root, true);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CParticleLibrary::Serialize(XmlNodeRef& root, bool bLoading)
{
	if (bLoading)
	{
		// Loading.
		RemoveAllItems();
		string name = GetName();
		root->getAttr("Name", name);
		SetName(name);
		for (int i = 0; i < root->getChildCount(); i++)
		{
			XmlNodeRef itemNode = root->getChild(i);
			// Only accept nodes with correct name.
			if (stricmp(itemNode->getTag(), "Particles") != 0)
				continue;
			string effectName = name + "." + itemNode->getAttr("Name");
			IParticleEffect* pEffect = gEnv->pParticleManager->LoadEffect(effectName, itemNode, false);
			if (pEffect)
			{
				CParticleItem* pItem = new CParticleItem(pEffect);
				AddItem(pItem);
				pItem->AddAllChildren();
			}
			else
			{
				CParticleItem* pItem = new CParticleItem;
				AddItem(pItem);
				CBaseLibraryItem::SerializeContext ctx(itemNode, bLoading);
				pItem->Serialize(ctx);
			}
		}
		SetModified(false);
	}
	else
	{
		// Saving.
		root->setAttr("Name", GetName());
		root->setAttr("SandboxVersion", (const char*)GetIEditorImpl()->GetFileVersion().ToFullString());
		// Serialize prototypes.
		for (int i = 0; i < GetItemCount(); i++)
		{
			CParticleItem* pItem = (CParticleItem*)GetItem(i);
			// Save materials with childs under thier parent table.
			if (pItem->GetParent())
				continue;

			XmlNodeRef itemNode = root->newChild("Particles");
			CBaseLibraryItem::SerializeContext ctx(itemNode, bLoading);
			GetItem(i)->Serialize(ctx);
		}
	}
}

