// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

- 16:09:2008   Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "GameParameters.h"
#include "ItemSharedParams.h"
#include "WeaponSharedParams.h"
#include "ItemResourceCache.h"
#include "FireModeParams.h"

CGameSharedParametersStorage::CGameSharedParametersStorage()
: m_pItemResourceCache(NULL)
{
	m_pItemResourceCache = new CItemResourceCache();
}

CGameSharedParametersStorage::~CGameSharedParametersStorage()
{
	SAFE_DELETE(m_pItemResourceCache);

	m_itemParametersMap.clear();
	m_weaponParametersMap.clear();
}

//================================================================
void CGameSharedParametersStorage::GetMemoryStatistics(ICrySizer *s)
{	
	s->AddObject(m_itemParametersMap);
	s->AddObject(m_weaponParametersMap);
	m_pItemResourceCache->GetMemoryStatistics(s);
}

//========================================================
template <typename TSharedParams>
TSharedParams* GetSharedParameters(CGameSharedParametersStorage::TSharedParamsMap &paramsMap, const char* className, bool create)
{
	CGameSharedParametersStorage::TSharedParamsMap::iterator it=paramsMap.find(CONST_TEMP_STRING(className));
	if (it!=paramsMap.end())
		return static_cast<TSharedParams*>(it->second.get());

	if (create)
	{
		TSharedParams *params = new TSharedParams();
		paramsMap.insert(CGameSharedParametersStorage::TSharedParamsMap::value_type(className, params));

		return params;
	}

	return 0;
}

//========================================================
CItemSharedParams *CGameSharedParametersStorage::GetItemSharedParameters(const char *className, bool create)
{
	return GetSharedParameters<CItemSharedParams>(m_itemParametersMap, className, create);
}

//========================================================
CWeaponSharedParams *CGameSharedParametersStorage::GetWeaponSharedParameters(const char *className, bool create)
{
	return GetSharedParameters<CWeaponSharedParams>(m_weaponParametersMap, className, create);
}


//========================================================
int CGameSharedParametersStorage::GetWeaponSharedParametersCount() const
{
	return m_weaponParametersMap.size();
}

//========================================================
const char* CGameSharedParametersStorage::GetWeaponSharedParametersName(int index) const
{
	CRY_ASSERT(index >= 0 && index < (int)m_weaponParametersMap.size());
	TSharedParamsMap::const_iterator iter = m_weaponParametersMap.begin();
	std::advance(iter, index);
	return iter->first.c_str();
}

//========================================================
void CGameSharedParametersStorage::ReleaseLevelResources()
{
	ReleaseLevelResources(m_itemParametersMap);
	ReleaseLevelResources(m_weaponParametersMap);
}

//========================================================
void CGameSharedParametersStorage::ReleaseLevelResources( TSharedParamsMap& paramsMap )
{
	TSharedParamsMap::iterator paramsMapEnd = paramsMap.end();
	for (TSharedParamsMap::iterator paramsIt = paramsMap.begin(); paramsIt != paramsMapEnd; ++paramsIt)
	{
		paramsIt->second->ReleaseLevelResources();
	}
}

//========================================================
void CGameSharedParametersStorage::GetDetailedMemoryStatics(TSharedParamsMap* paramsMap, const char* typeName)
{
	const float kbInvert = 1.0f/1024.0f;

	int totalSize = 0;

	float white[4] = {1.f, 1.f, 1.f, 1.f};
	float purple[4] = {1.f, 0.f, 1.f, 1.f};

	float posY = 50.f;
	float posX = 50.f;

	int num = 0;
	
	for (TSharedParamsMap::iterator iter = paramsMap->begin(); iter != paramsMap->end(); ++iter)
	{
		ICrySizer *pSizer = gEnv->pSystem->CreateSizer();

		iter->second->GetMemoryUsage(pSizer);

		int paramSize = pSizer->GetTotalSize();
		totalSize += paramSize;

		IRenderAuxText::Draw2dLabel(posX, posY, 1.5f, purple, false, "%s Params: %s. Mem: %.3f Kb", typeName, iter->first.c_str(), paramSize*kbInvert);
		posY += 15.f;

		if(posY > 600.f)
		{
			posX += 500.f;
			posY = 50.f;
		}

		num++;

		pSizer->Release();
	}

	IRenderAuxText::Draw2dLabel(50.0f, 30.0f, 1.5f, white, false, "%s Params Total: %d. Mem: %.2f Kb.", typeName, num, totalSize*kbInvert);
}

void CGameSharedParametersStorage::ClearItemParamSets()
{
	ITEM_PARAM_STRUCT_TYPES(CLEAR_SETS)
}

ITEM_PARAM_STRUCT_TYPES(IMPLEMENT_SET_FUNCS)
