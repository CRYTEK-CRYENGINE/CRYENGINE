// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EquipPack.h"
#include "EquipPackLib.h"

CEquipPack::CEquipPack(CEquipPackLib* pCreator)
{
	m_pCreator = pCreator;
	m_bModified = false;
}

CEquipPack::~CEquipPack()
{
	m_pCreator = 0;
	Release();
}

void CEquipPack::Release()
{
	if (m_pCreator)
	{
		if (!m_pCreator->RemoveEquipPack(m_name))
			delete this;
	}
}

bool CEquipPack::AddEquip(const SEquipment& equip)
{
	bool bAdded = stl::push_back_unique(m_equipmentVec, equip);
	SetModified(bAdded);
	return bAdded;
}

bool CEquipPack::RemoveEquip(const SEquipment& equip)
{
	const bool bErased = stl::find_and_erase(m_equipmentVec, equip);
	SetModified(bErased);
	return bErased;
}

bool CEquipPack::AddAmmo(const SAmmo& ammo)
{
	TAmmoVec::iterator iter = std::find(m_ammoVec.begin(), m_ammoVec.end(), ammo);
	if (iter != m_ammoVec.end())
	{
		iter->nAmount = ammo.nAmount;
	}
	else
	{
		m_ammoVec.push_back(ammo);
	}
	SetModified(true);
	return true;
}

bool CEquipPack::InternalSetPrimary(const string& primary)
{
	SEquipment equip;
	equip.sName = primary;
	TEquipmentVec::iterator iter = std::find(m_equipmentVec.begin(), m_equipmentVec.end(), equip);
	if (iter == m_equipmentVec.end())
		return false;
	// move this item to the front
	equip = *iter;
	m_equipmentVec.erase(iter);
	m_equipmentVec.push_front(equip);
	SetModified(true);
	return true;
}

void CEquipPack::Clear()
{
	SetModified(true);
	m_equipmentVec.clear();
}

void CEquipPack::Load(XmlNodeRef node)
{
	for (int iChild = 0; iChild < node->getChildCount(); ++iChild)
	{
		const XmlNodeRef childNode = node->getChild(iChild);
		if (childNode == 0)
			continue;

		if (childNode->isTag("Items"))
		{
			for (int i = 0; i < childNode->getChildCount(); ++i)
			{
				XmlNodeRef itemNode = childNode->getChild(i);
				const char* itemName = itemNode->getTag();
				const char* itemType = itemNode->getAttr("type");
				const char* itemSetup = itemNode->getAttr("setup");
				SEquipment equip;
				equip.sName = itemName;
				equip.sType = itemType;
				equip.sSetup = itemSetup;
				AddEquip(equip);
			}
		}
		else if (childNode->isTag("Ammo")) // legacy
		{
			const char* ammoName = "";
			const char* ammoCount = "";
			int nAttr = childNode->getNumAttributes();
			for (int j = 0; j < nAttr; ++j)
			{
				if (childNode->getAttributeByIndex(j, &ammoName, &ammoCount))
				{
					int nAmmoCount = atoi(ammoCount);
					SAmmo ammo;
					ammo.sName = ammoName;
					ammo.nAmount = nAmmoCount;
					AddAmmo(ammo);
				}
			}
		}
		else if (childNode->isTag("Ammos"))
		{
			for (int i = 0; i < childNode->getChildCount(); ++i)
			{
				XmlNodeRef ammoNode = childNode->getChild(i);
				if (ammoNode->isTag("Ammo") == false)
					continue;
				const char* ammoName = ammoNode->getAttr("name");
				if (ammoName == 0 || ammoName[0] == '\0')
					continue;
				int nAmmoCount = 0;
				ammoNode->getAttr("amount", nAmmoCount);
				SAmmo ammo;
				ammo.sName = ammoName;
				ammo.nAmount = nAmmoCount;
				AddAmmo(ammo);
			}
		}
	}
	const char* primaryName = node->getAttr("primary");
	if (primaryName && *primaryName)
		InternalSetPrimary(primaryName);
}

bool CEquipPack::Save(XmlNodeRef node)
{
	node->setAttr("name", m_name);

	if (m_equipmentVec.empty() == false)
		node->setAttr("primary", m_equipmentVec.begin()->sName);
	XmlNodeRef itemsNode = node->newChild("Items");
	for (TEquipmentVec::iterator iter = m_equipmentVec.begin(); iter != m_equipmentVec.end(); ++iter)
	{
		const SEquipment& equip = *iter;
		XmlNodeRef equipNode = itemsNode->newChild(equip.sName);
		equipNode->setAttr("type", equip.sType);
		equipNode->setAttr("setup", equip.sSetup);
	}
	XmlNodeRef ammosNode = node->newChild("Ammos");
	for (TAmmoVec::iterator iter = m_ammoVec.begin(); iter != m_ammoVec.end(); ++iter)
	{
		const SAmmo& ammo = *iter;
		XmlNodeRef ammoNode = ammosNode->newChild("Ammo");
		ammoNode->setAttr("name", ammo.sName);
		ammoNode->setAttr("amount", ammo.nAmount);
	}
	return true;
}

