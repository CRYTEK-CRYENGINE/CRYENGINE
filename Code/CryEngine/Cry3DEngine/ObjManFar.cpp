// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   statobjmanfar.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: draw far objects as sprites
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "ObjMan.h"
#include "3dEngine.h"

static PodArray<SVegetationSpriteInfo> arrVegetationSprites[RT_COMMAND_BUF_COUNT][MAX_RECURSION_LEVELS];

void CObjManager::UnloadFarObjects()
{
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		for (int j = 0; j < MAX_RECURSION_LEVELS; ++j)
			stl::free_container(arrVegetationSprites[i][j]);
}

void CObjManager::RenderFarObjects(const SRenderingPassInfo& passInfo)
{

	FUNCTION_PROFILER_3DENGINE;

	int nCount = 0;
	for (int t = 0; t < nThreadsNum; t++)
		nCount += m_arrVegetationSprites[passInfo.GetRecursiveLevel()][t].size();

	if (!m_REFarTreeSprites)
	{
		m_REFarTreeSprites = (CREFarTreeSprites*)GetRenderer()->EF_CreateRE(eDATA_FarTreeSprites);
	}
	if (m_REFarTreeSprites && GetCVars()->e_VegetationSprites && nCount && !GetCVars()->e_DefaultMaterial)
	{
		arrVegetationSprites[passInfo.ThreadID()][passInfo.GetRecursiveLevel()].Clear();

		for (int t = 0; t < nThreadsNum; t++)
		{
			CThreadSafeRendererContainer<SVegetationSpriteInfo>& rList = m_arrVegetationSprites[passInfo.GetRecursiveLevel()][t];
			if (rList.size())
			{
				rList.CoalesceMemory();
				for (size_t i = 0; i < rList.size(); ++i)
					arrVegetationSprites[passInfo.ThreadID()][passInfo.GetRecursiveLevel()].Add(rList[i]);
			}
		}
		m_REFarTreeSprites->m_arrVegetationSprites[passInfo.ThreadID()][passInfo.GetRecursiveLevel()] = &arrVegetationSprites[passInfo.ThreadID()][passInfo.GetRecursiveLevel()];
		CRenderObject* pObj = GetRenderer()->EF_GetObject_Temp(passInfo.ThreadID());
		if (!pObj)
			return;
		pObj->m_II.m_Matrix.SetIdentity();
		SShaderItem shItem(m_p3DEngine->m_pFarTreeSprites);
		GetRenderer()->EF_AddEf(m_REFarTreeSprites, shItem, pObj, passInfo, EFSLIST_GENERAL, 1);
	}
}
/*
   static inline int Compare(CVegetation *& p1, CVegetation *& p2)
   {
   if(p1->m_fDistance > p2->m_fDistance)
    return 1;
   else
   if(p1->m_fDistance < p2->m_fDistance)
    return -1;

   return 0;
   } */

void CObjManager::DrawFarObjects(float fMaxViewDist, const SRenderingPassInfo& passInfo)
{
	if (!GetCVars()->e_VegetationSprites)
		return;

	FUNCTION_PROFILER_3DENGINE;

	if (passInfo.GetRecursiveLevel() >= MAX_RECURSION_LEVELS)
	{
		assert(!"Recursion depther than MAX_RECURSION_LEVELS is not supported");
		return;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	//  Draw all far
	//////////////////////////////////////////////////////////////////////////////////////

	//PodArray<SVegetationSpriteInfo> * pList = &arrVegetationSprites[m_nRenderStackLevel];
	//if (pList->Count())
	//  GetRenderer()->DrawObjSprites(pList);
}

void CObjManager::GenerateFarObjects(float fMaxViewDist, const SRenderingPassInfo& passInfo)
{
	if (!GetCVars()->e_VegetationSprites)
		return;

	FUNCTION_PROFILER_3DENGINE;

	if (passInfo.GetRecursiveLevel() >= MAX_RECURSION_LEVELS)
	{
		assert(!"Recursion depther than MAX_RECURSION_LEVELS is not supported");
		return;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	//  Draw all far
	//////////////////////////////////////////////////////////////////////////////////////

	/*arrVegetationSprites[m_nRenderStackLevel].Clear();

	   for(int t=0; t<nThreadsNum; t++)
	   {
	   PodArray<SVegetationSpriteInfo> * pList = &m_arrVegetationSprites[m_nRenderStackLevel][t];
	   if (pList->Count())
	    arrVegetationSprites[m_nRenderStackLevel].AddList(*pList);
	   }*/

	//PodArray<SVegetationSpriteInfo> * pList = &arrVegetationSprites[m_nRenderStackLevel];
	//if (pList->Count())
	//  GetRenderer()->GenerateObjSprites(pList);
}
