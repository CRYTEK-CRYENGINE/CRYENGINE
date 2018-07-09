// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIBattleFront.h"
#include <CryAISystem/IAgent.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "GameCVars.h"
#include <CryAISystem/IAIObject.h>


SBattleFrontMember::SBattleFrontMember( EntityId _entityID )
	: entityID(_entityID),
	paused(false)
{
}

CAIBattleFrontGroup::CAIBattleFrontGroup(GroupID groupID)
	: m_battleFrontPosition(0.0f, 0.0f, 0.0f)
	, m_averagePosition(0.0f, 0.0f, 0.0f)
	, m_battleFrontMode(Dynamic)
	, m_groupID(groupID)
{
}

void CAIBattleFrontGroup::AddEntity(EntityId entityID)
{
	m_members.insert(SBattleFrontMember(entityID));
}

void CAIBattleFrontGroup::RemoveEntity(EntityId entityID)
{
	std::set<SBattleFrontMember>::iterator it = m_members.find(entityID);
	if (it != m_members.end())
	{
		m_members.erase(it);
	}
}

void CAIBattleFrontGroup::PauseEntity( EntityId entityID )
{
	SetPause(entityID, true);
}

void CAIBattleFrontGroup::ResumeEntity( EntityId entityID )
{
	SetPause(entityID, false);
}

void CAIBattleFrontGroup::SetPause(EntityId entityID, bool paused)
{
	std::set<SBattleFrontMember>::iterator it = m_members.find(entityID);
	if (it != m_members.end())
	{
		(*it).paused = paused;
	}
}

void CAIBattleFrontGroup::Update()
{
	CalculateAveragePositionOfGroupMembers();

	if (m_battleFrontMode == Dynamic)
	{
		m_battleFrontPosition = m_averagePosition;
	}

	if (g_pGameCVars->ai_DebugBattleFront)
	{
		DrawDebugInfo();
	}
}

void CAIBattleFrontGroup::SetDesignerControlledBattleFrontAt(const Vec3& position)
{
	m_battleFrontMode = DesignerControlled;
	m_battleFrontPosition = position;
}

void CAIBattleFrontGroup::DisableDesignerControlledBattleFront()
{
	m_battleFrontMode = Dynamic;
}

void CAIBattleFrontGroup::CalculateAveragePositionOfGroupMembers()
{
	Vec3 accumulatedPositions(0.0f, 0.0f, 0.0f);
	float influences = 0.0f;

	std::set<SBattleFrontMember>::const_iterator it = m_members.begin();
	std::set<SBattleFrontMember>::const_iterator end = m_members.end();

	while (it != end)
	{
		SBattleFrontMember battleFrontMember = *it;
		if(!battleFrontMember.paused)
		{
			const IEntity* entity = gEnv->pEntitySystem->GetEntity(battleFrontMember.entityID);
			CRY_ASSERT_MESSAGE(entity, "Somehow there is an invalid entity in a battlefront group");
			if (!entity)
				break;

			influences += 1.0f;
			accumulatedPositions += entity->GetWorldPos();
		}
		++it;
	}

	if (influences > 0.0)
	{
		m_averagePosition = accumulatedPositions / influences;
	}
}

void CAIBattleFrontGroup::DrawDebugInfo()
{
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_battleFrontPosition, 0.5f, ColorB(255, 0, 255));

	const float colorwhite[4]={1,1,1,1};

	IRenderAuxText::DrawLabelExF(m_battleFrontPosition, 1, colorwhite, true, false, "GroupID - %d", m_groupID);

 	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
 	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	std::set<SBattleFrontMember>::const_iterator it = m_members.begin();
	std::set<SBattleFrontMember>::const_iterator end = m_members.end();
	while (it != end)
	{
		SBattleFrontMember battleFrontMember = *it;
		if(!battleFrontMember.paused)
		{
			const IEntity* entity = gEnv->pEntitySystem->GetEntity(battleFrontMember.entityID);
			if (!entity)
				break;
		
 			pAuxGeom->DrawLine(m_battleFrontPosition,ColorB(70, 20, 135) , entity->GetWorldPos(), ColorB(100, 50, 165));
		}
		++it;
	}
}

void CAIBattleFrontModule::EntityEnter(EntityId entityID)
{
	EntityLeave(entityID);

	IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID);
	if(!entity)
	{
		GameWarning("Battlefront : Entity failed to enter."); 
		return;
	}

	IAIObject* aiObject = entity->GetAI();
	if(!aiObject)
	{
		GameWarning("Battlefront : Entity failed to enter."); 
		return;
	}

	CAIBattleFrontGroup::GroupID groupID = aiObject->GetGroupId();
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup>::iterator it = m_battleFrontGroups.find(groupID);
	if (it == m_battleFrontGroups.end())
	{
		m_battleFrontGroups.insert(std::make_pair(groupID, CAIBattleFrontGroup(groupID)));
		it = m_battleFrontGroups.find(groupID);
	}

	CAIBattleFrontGroup& group = it->second;
	group.AddEntity(entityID);
}

void CAIBattleFrontModule::EntityLeave(EntityId entityID)
{
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup>::iterator it = m_battleFrontGroups.begin();
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup>::iterator end = m_battleFrontGroups.end();

	while (it != end)
	{
		CAIBattleFrontGroup& group = it->second;
		group.RemoveEntity(entityID);
		++it;
	}
}

void CAIBattleFrontModule::EntityPause(EntityId entityID)
{
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup>::iterator it = m_battleFrontGroups.begin();
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup>::iterator end = m_battleFrontGroups.end();

	while (it != end)
	{
		CAIBattleFrontGroup& group = it->second;
		group.PauseEntity(entityID);
		++it;
	}
}

void CAIBattleFrontModule::EntityResume(EntityId entityID)
{
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup>::iterator it = m_battleFrontGroups.begin();
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup>::iterator end = m_battleFrontGroups.end();

	while (it != end)
	{
		CAIBattleFrontGroup& group = it->second;
		group.ResumeEntity(entityID);
		++it;
	}
}

void CAIBattleFrontModule::Reset(bool bUnload)
{
	m_battleFrontGroups.clear();
}

void CAIBattleFrontModule::Update(float dt)
{
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup>::iterator it = m_battleFrontGroups.begin();
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup>::iterator end = m_battleFrontGroups.end();
	while (it != end)
	{
		CAIBattleFrontGroup& group = it->second;
		group.Update();
		++it;
	}
}

const char* CAIBattleFrontModule::GetName() const
{
	return "BattleFront";
}

CAIBattleFrontGroup* CAIBattleFrontModule::GetGroupForEntity(EntityId entityID)
{
	CAIBattleFrontGroup::GroupID groupID = gEnv->pEntitySystem->GetEntity(entityID)->GetAI()->GetGroupId();
	return GetGroupByID(groupID);
}

CAIBattleFrontGroup* CAIBattleFrontModule::GetGroupByID( CAIBattleFrontGroup::GroupID groupID )
{
	std::map<CAIBattleFrontGroup::GroupID, CAIBattleFrontGroup>::iterator it = m_battleFrontGroups.find(groupID);
	if (it == m_battleFrontGroups.end())
	{
		return NULL;
	}

	return &it->second;
}
