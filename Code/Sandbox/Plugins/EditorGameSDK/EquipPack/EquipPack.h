// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include <deque>

struct SEquipment
{
	SEquipment()
	{
		sName = "";
		sType = "";
		sSetup = "";
	}
	bool operator==(const SEquipment& e)
	{
		if (sName != e.sName)
			return false;
		return true;
	}
	bool operator<(const SEquipment& e)
	{
		if (sName < e.sName)
			return true;
		if (sName != e.sName)
			return false;
		return false;
	}
	string sName;
	string sType;
	string sSetup;
};

struct SAmmo
{
	bool operator==(const SAmmo& e)
	{
		if (sName != e.sName)
			return false;
		return true;
	}
	bool operator<(const SAmmo& e)
	{
		if (sName < e.sName)
			return true;
		if (sName != e.sName)
			return false;
		return false;
	}
	string sName;
	int     nAmount;
};

typedef std::deque<SEquipment> TEquipmentVec;
typedef std::vector<SAmmo>     TAmmoVec;

class CEquipPackLib;

class CEquipPack
{
public:
	void           Release();

	const string& GetName() const { return m_name; }

	bool           AddEquip(const SEquipment& equip);
	bool           RemoveEquip(const SEquipment& equip);
	bool           AddAmmo(const SAmmo& ammo);

	void           Clear();
	void           Load(XmlNodeRef node);
	bool           Save(XmlNodeRef node);
	int            Count() const               { return m_equipmentVec.size(); }

	TEquipmentVec& GetEquip()                  { return m_equipmentVec; }
	TAmmoVec&      GetAmmoVec()                { return m_ammoVec; }

	void           SetModified(bool bModified) { m_bModified = bModified; }
	bool           IsModified() const          { return m_bModified; }

protected:
	friend CEquipPackLib;

	CEquipPack(CEquipPackLib* pCreator);
	~CEquipPack();

	void SetName(const string& name)
	{
		m_name = name;
		SetModified(true);
	}

	bool InternalSetPrimary(const string& primary);

protected:
	string        m_name;
	CEquipPackLib* m_pCreator;
	TEquipmentVec  m_equipmentVec;
	TAmmoVec       m_ammoVec;
	bool           m_bModified;
};

