// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 08:12:2010		Created by Ben Parbury
*************************************************************************/

#include "StdAfx.h"
#include "WeaponAlias.h"

//---------------------------------------
CWeaponAlias::CWeaponAlias()
{
	Reset();
}

//---------------------------------------
CWeaponAlias::~CWeaponAlias()
{
	m_aliases.clear();
}

//---------------------------------------
void CWeaponAlias::Reset()
{
	m_aliases.clear();
	m_aliases.reserve(16);
}

//---------------------------------------
void CWeaponAlias::AddAlias(const char* pParentName, const char* pChildName)
{
	SWeaponAlias alias(pParentName, pChildName);
	m_aliases.push_back(alias);
}

//---------------------------------------
const IEntityClass* CWeaponAlias::GetParentClass(const IEntityClass* pClass) const
{
	const size_t aliasCount = m_aliases.size();
	for(size_t i = 0; i < aliasCount; i++)
	{
		const SWeaponAlias& pAlias = m_aliases[i];
		if(pClass == pAlias.m_pClass)
		{
			return pAlias.m_pParentClass;
		}
	}

	return NULL;
}

//---------------------------------------
const IEntityClass* CWeaponAlias::GetParentClass( const char* pClassName ) const
{
	const IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	IEntityClass* pClass = pClassRegistry->FindClass(pClassName);
	if(pClass)
	{
		return GetParentClass(pClass);
	}

	return NULL;
}

//---------------------------------------
void CWeaponAlias::UpdateClass(IEntityClass** ppClass) const
{
	const IEntityClass* pClass = *ppClass;
	const size_t aliasCount = m_aliases.size();
	for(size_t i = 0; i < aliasCount; i++)
	{
		const SWeaponAlias& pAlias = m_aliases[i];
		if(pClass == pAlias.m_pClass)
		{
			*ppClass = pAlias.m_pParentClass;
			return;
		}
	}
}

//---------------------------------------
bool CWeaponAlias::IsAlias(const char* pAliasName) const
{
	const size_t aliasCount = m_aliases.size();
	for(size_t i = 0; i < aliasCount; i++)
	{
		const SWeaponAlias& pAlias = m_aliases[i];
		if(strcmpi(pAlias.m_pClass->GetName(), pAliasName) == 0)
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////
CWeaponAlias::SWeaponAlias::SWeaponAlias(const char* pParentName, const char* pName)
{
	const IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();

	m_pParentClass = pClassRegistry->FindClass(pParentName);
	CRY_ASSERT(m_pParentClass);

	m_pClass = pClassRegistry->FindClass(pName);
	CRY_ASSERT(m_pClass);
}
