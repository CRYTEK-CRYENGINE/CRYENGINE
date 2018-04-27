// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   CryGame Source File.
   -------------------------------------------------------------------------
   File name:   AIFaceManager.cpp
   Version:     v1.00
   Description:

   -------------------------------------------------------------------------
   History:
   - 05:05:2007   12:05 : Created by Kirill Bulatsev

 *********************************************************************/
#include "StdAfx.h"
#include "AIFaceManager.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryNetwork/ISerialize.h>
#include <IActorSystem.h>

CAIFaceManager::TExprState CAIFaceManager::s_Expressions[EE_Count + 1];
//
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
CAIFaceManager::CAIFaceManager(IEntity* pEntity) :
	m_pEntity(pEntity),
	m_CurrentState(EE_IDLE),
	m_CurrentExprIdx(0),
	m_ExprStartTime(0.f),
	m_ChangeExpressionTimeMs(0)
{

}

//
//------------------------------------------------------------------------------
CAIFaceManager::~CAIFaceManager(void)
{

}

//
//------------------------------------------------------------------------------
void CAIFaceManager::OnReused(IEntity* pEntity)
{
	m_pEntity = pEntity;

	Reset();
}

//
//------------------------------------------------------------------------------
int CAIFaceManager::SelectExpressionTime() const
{
	return cry_random(30000, 50000);
}

//
//------------------------------------------------------------------------------
void CAIFaceManager::Reset()
{
	m_CurrentState = EE_IDLE;
	m_CurrentExprIdx = 0;
	m_ExprStartTime = 0.f;
}

//
//------------------------------------------------------------------------------
void CAIFaceManager::PrecacheSequences()
{
	IActorSystem* pASystem = gEnv->pGameFramework->GetIActorSystem();
	IActor* pActor = (pASystem && m_pEntity ? pASystem->GetActor(m_pEntity->GetId()) : 0);

	for (e_ExpressionEvent eventType = e_ExpressionEvent(0); eventType <= EE_Count; eventType = e_ExpressionEvent(eventType + 1))
	{
		const TExprState& stateSequences = s_Expressions[eventType];
		for (int stateSequenceIndex = 0, stateSequenceCount = stateSequences.size(); stateSequenceIndex < stateSequenceCount; ++stateSequenceIndex)
		{
			const string& expressionName = stateSequences[stateSequenceIndex];

			if (pActor)
				pActor->PrecacheFacialExpression(expressionName.c_str());
		}
	}
}

//
//------------------------------------------------------------------------------
bool CAIFaceManager::LoadStatic()
{
	stl::for_each_array(s_Expressions, stl::container_freer());
	string sPath = PathUtil::Make("Libs/Readability/Faces", "Faces.xml");
	return Load(sPath);
	/*
	   m_SoundPacks.clear();
	   //	Load("korean_01.xml");

	   string path( "Libs/Readability/Sound" );
	   ICryPak * pCryPak = gEnv->pCryPak;
	   _finddata_t fd;
	   string fileName;

	   string searchPath(path + "/*.xml");
	   intptr_t handle = pCryPak->FindFirst( searchPath.c_str(), &fd );
	   if (handle != -1)
	   {
	    do
	    {
	      fileName = path;
	      fileName += "/" ;
	      fileName += fd.name;
	      Load(fileName);
	    } while ( pCryPak->FindNext( handle, &fd ) >= 0 );
	    pCryPak->FindClose( handle );
	   }
	   m_reloadID++;
	 */
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIFaceManager::Load(const char* szPackName)
{
	string sPath = PathUtil::Make("Libs/Readability/Faces", "Faces.xml");

	XmlNodeRef root = GetISystem()->LoadXmlFromFile(sPath);
	if (!root)
		return false;

	XmlNodeRef nodeWorksheet = root->findChild("Worksheet");
	if (!nodeWorksheet)
		return false;

	XmlNodeRef nodeTable = nodeWorksheet->findChild("Table");
	if (!nodeTable)
		return false;

	e_ExpressionEvent curExpression(EE_NONE);
	const char* szSignal = 0;
	for (int rowCntr = 0, childN = 0; childN < nodeTable->getChildCount(); ++childN)
	{
		XmlNodeRef nodeRow = nodeTable->getChild(childN);
		if (!nodeRow->isTag("Row"))
			continue;
		++rowCntr;
		if (rowCntr == 1) // Skip the first row, it only have description/header.
			continue;
		const char* szFaceName = 0;

		for (int childrenCntr = 0, cellIndex = 1; childrenCntr < nodeRow->getChildCount(); ++childrenCntr, ++cellIndex)
		{
			XmlNodeRef nodeCell = nodeRow->getChild(childrenCntr);
			if (!nodeCell->isTag("Cell"))
				continue;

			if (nodeCell->haveAttr("ss:Index"))
			{
				cellIndex = 0;
				nodeCell->getAttr("ss:Index", cellIndex);
			}
			XmlNodeRef nodeCellData = nodeCell->findChild("Data");
			if (!nodeCellData)
				continue;

			switch (cellIndex)
			{
			case 1:   // Readability Signal name
				szSignal = nodeCellData->getContent();
				curExpression = StringToEnum(szSignal);
				continue;
			case 2:   // The expression name
				szFaceName = nodeCellData->getContent();
				break;
			}
		}
		if (szFaceName != NULL)
			s_Expressions[curExpression].push_back(szFaceName);
	}
	return true;
}

//
//------------------------------------------------------------------------------
CAIFaceManager::e_ExpressionEvent CAIFaceManager::StringToEnum(const char* szFaceName)
{
	if (!strcmp(szFaceName, "IDLE"))
		return EE_IDLE;
	else if (!strcmp(szFaceName, "ALERT"))
		return EE_ALERT;
	else if (!strcmp(szFaceName, "COMBAT"))
		return EE_COMBAT;

	return EE_NONE;
}

//
//------------------------------------------------------------------------------
void CAIFaceManager::Update()
{
	if (m_CurrentState == EE_NONE)
	{
		MakeFace(NULL);
		return;
	}
	int64 timePassedMs((gEnv->pTimer->GetFrameStartTime() - m_ExprStartTime).GetMilliSecondsAsInt64());
	if (timePassedMs > m_ChangeExpressionTimeMs)
		SetExpression(m_CurrentState, true);
}

//
//------------------------------------------------------------------------------
void CAIFaceManager::SetExpression(e_ExpressionEvent expression, bool forceChange)
{
	if (m_CurrentState == expression && !forceChange)
		return;

	assert(expression >= 0 && expression < EE_Count);
	TExprState& theState = s_Expressions[expression];
	if (!theState.empty())
	{
		m_CurrentExprIdx = cry_random(0, (int)theState.size() - 1);
		MakeFace(theState[m_CurrentExprIdx]);
	}
	m_ExprStartTime = gEnv->pTimer->GetFrameStartTime();
	m_CurrentState = expression;
}

//
//------------------------------------------------------------------------------
void CAIFaceManager::MakeFace(const char* pFaceName)
{
	IActorSystem* pASystem = gEnv->pGameFramework->GetIActorSystem();
	if (pASystem)
	{
		IActor* pActor = pASystem->GetActor(m_pEntity->GetId());
		if (pActor)
		{
			f32 length = -1.0f;
			pActor->RequestFacialExpression(pFaceName, &length);
			if (length > 0.0f)
				m_ChangeExpressionTimeMs = (uint32)(length * 1000.0f);
		}
	}
}

//----------------------------------------------------------------------------------------------------------
//
//
//----------------------------------------------------------------------------------------------------------
