// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 12:08:2010    : Created by Filipe Amim

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemPackages.h"
#include "GameXmlParamReader.h"

#include "ItemSharedParams.h"

namespace
{


	const char* itemPackageFilePath = "Scripts/Entities/Items/ItemPackages.xml";



	void ParseAccessoryString(CItemPackages::TSetup* pSetup, const char* setupString)
	{
		const int stringSize = 128;
		if(setupString)
		{
			char subString[stringSize];
			for (int idx = 0; ; ++setupString)
			{
				if (isspace((unsigned char)*setupString))
					continue;

				if (!*setupString || *setupString == ';')
				{
					subString[idx] = 0;

					IEntityClass* pAccessoryClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(&subString[0]);
					if (pAccessoryClass)
						pSetup->push_back(pAccessoryClass);

					idx = 0;
					if (!*setupString)
						break;
				}
				else
				{
					subString[idx] = *setupString;
					++idx;
				}
			}
		}
	}



	void ProcessPackages(CItemPackages::TPackages* pPackages, XmlNodeRef itemPackageFile)
	{
		int numEntries = itemPackageFile->getChildCount();

		for (int i = 0; i < numEntries; ++i)
		{
			XmlNodeRef entry = itemPackageFile->getChild(i);
			if (strcmp(entry->getTag(), "package") != 0)
				continue;

			ItemString displayName = entry->getAttr("display_name");
			const char* itemName = entry->getAttr("item");
			const char* setupString = entry->getAttr("setup");

			IEntityClass* pItemClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(itemName);
			if (!pItemClass)
				continue;

			CItemPackages::SPackage package;
			package.m_displayName = displayName;
			package.m_pItemClass = pItemClass;

			ParseAccessoryString(&package.m_setup, setupString);

			pPackages->push_back(package);
		}
	}



	bool SetupsAreIdentical(const CItem::TAccessoryArray& itemAccessoires, const CItemPackages::TSetup& setup)
	{
		if (setup.size() != itemAccessoires.size())
			return false;

		CItemPackages::TSetup::const_iterator begin = setup.begin();
		CItemPackages::TSetup::const_iterator end = setup.end();

		for (CItemPackages::TSetup::const_iterator it = begin; it != end; ++it)
		{
			IEntityClass* pThisClass = *it;
			bool otherAlsoHasIt = false;
			for (unsigned int j = 0; j < itemAccessoires.size(); ++j)
			{
				IEntityClass* pOtherClass = itemAccessoires[j].pClass;
				if (pOtherClass == pThisClass)
				{
					otherAlsoHasIt = true;
					break;
				}
			}
			if (!otherAlsoHasIt)
				return false;
		}
		return true;
	}


}



void CItemPackages::Load()
{
	XmlNodeRef itemPackageFile = gEnv->pSystem->LoadXmlFromFile(itemPackageFilePath);
	if (!itemPackageFile)
		return;

	m_packages.clear();

	ProcessPackages(&m_packages, itemPackageFile);
}



const char* CItemPackages::GetFullItemName(const CItem* pItem) const
{
	IEntityClass* pItemClass = pItem->GetEntity()->GetClass();
	const CItem::TAccessoryArray& itemAccessoires = pItem->GetAccessories();

	TPackages::const_iterator begin = m_packages.begin();
	TPackages::const_iterator end = m_packages.end();
	for (TPackages::const_iterator it = begin; it != end; ++it)
	{
		if (it->m_pItemClass == pItemClass && SetupsAreIdentical(itemAccessoires, it->m_setup))
			return it->m_displayName.c_str();
	}

	return pItem->GetSharedItemParams()->params.display_name.c_str();
}
