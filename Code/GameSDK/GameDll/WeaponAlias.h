// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 08:12:2010		Created by Ben Parbury
*************************************************************************/

#ifndef __WEAPONALIAS_H__
#define __WEAPONALIAS_H__

struct IEntityClass;

class CWeaponAlias
{
public:
	CWeaponAlias();
	virtual ~CWeaponAlias();

	void AddAlias(const char* pParentName, const char* pChildName);
	void Reset();

	//Will update pClass if they have a parent
	void UpdateClass(IEntityClass** ppClass) const;

	//Return pParentClass (or NULL if they don't have one)
	const IEntityClass* GetParentClass(const IEntityClass* pClass) const;
	const IEntityClass* GetParentClass(const char* pClassName) const;

	bool IsAlias(const char* pAliasName) const;

protected:
	struct SWeaponAlias
	{
		SWeaponAlias(const char* pParentName, const char* pName);

		IEntityClass* m_pParentClass;
		IEntityClass* m_pClass;
	};

	typedef std::vector<SWeaponAlias> TWeaponAliasVec;
	TWeaponAliasVec m_aliases;
};

#endif // __WEAPONALIAS_H__
