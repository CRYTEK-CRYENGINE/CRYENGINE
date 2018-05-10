// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 12:08:2010    : Created by Filipe Amim

*************************************************************************/

#ifndef __ITEM_PACKAGES_H__
#define __ITEM_PACKAGES_H__

#if _MSC_VER > 1000
# pragma once
#endif



class CItemPackages
{
public:

	typedef std::vector<IEntityClass*> TSetup;

	struct SPackage
	{
		ItemString m_displayName;
		IEntityClass* m_pItemClass;
		TSetup m_setup;
	};

	typedef std::vector<SPackage> TPackages;

public:
	void Load();
	const char* GetFullItemName(const CItem* pItem) const;

private:
	TPackages m_packages;
};


#endif
