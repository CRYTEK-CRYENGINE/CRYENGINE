// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannequinGoalOp.h"
#include "PipeUser.h"
#include "AIBubblesSystem/AIBubblesSystem.h"

//////////////////////////////////////////////////////////////////////////
CMannequinTagGoalOp::CMannequinTagGoalOp()
	: m_tagCrc(0)
{
}

CMannequinTagGoalOp::CMannequinTagGoalOp(const char* tagName)
	: m_tagCrc(0)
{
	assert(tagName);
	assert(tagName[0] != 0);

	m_tagCrc = CCrc32::ComputeLowercase(tagName);
}

CMannequinTagGoalOp::CMannequinTagGoalOp(const uint32 tagCrc)
	: m_tagCrc(tagCrc)
{
}

CMannequinTagGoalOp::CMannequinTagGoalOp(const XmlNodeRef& node)
	: m_tagCrc(0)
{
	assert(node != 0);

	const char* tagName = 0;
	if (node->getAttr("name", &tagName))
	{
		m_tagCrc = CCrc32::ComputeLowercase(tagName);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_ERROR, "Animation tag GoalOp doesn't have a 'name' attribute.");
	}
}

//////////////////////////////////////////////////////////////////////////
CSetAnimationTagGoalOp::CSetAnimationTagGoalOp()
	: CMannequinTagGoalOp()
{
}

CSetAnimationTagGoalOp::CSetAnimationTagGoalOp(const char* tagName)
	: CMannequinTagGoalOp(tagName)
{
}

CSetAnimationTagGoalOp::CSetAnimationTagGoalOp(const uint32 tagCrc)
	: CMannequinTagGoalOp(tagCrc)
{
}

CSetAnimationTagGoalOp::CSetAnimationTagGoalOp(const XmlNodeRef& node)
	: CMannequinTagGoalOp(node)
{
}

void CSetAnimationTagGoalOp::Update(CPipeUser& pipeUser)
{
	SOBJECTSTATE& state = pipeUser.GetState();
	const uint32 tagCrc = GetTagCrc();

	const aiMannequin::SCommand* pCommand = state.mannequinRequest.CreateSetTagCommand(tagCrc);
	if (pCommand != NULL)
	{
		GoalOpSucceeded();
	}
	else
	{
		const EntityId entityId = pipeUser.GetEntityID();
		AIQueueBubbleMessage("CMannequinSetTagGoalOp::Update", entityId, "Could not add a set tag command to the mannequin request this frame.", eBNS_LogWarning);
	}
}

//////////////////////////////////////////////////////////////////////////
CClearAnimationTagGoalOp::CClearAnimationTagGoalOp()
	: CMannequinTagGoalOp()
{
}

CClearAnimationTagGoalOp::CClearAnimationTagGoalOp(const char* tagName)
	: CMannequinTagGoalOp(tagName)
{
}

CClearAnimationTagGoalOp::CClearAnimationTagGoalOp(const uint32 tagCrc)
	: CMannequinTagGoalOp(tagCrc)
{
}

CClearAnimationTagGoalOp::CClearAnimationTagGoalOp(const XmlNodeRef& node)
	: CMannequinTagGoalOp(node)
{
}

void CClearAnimationTagGoalOp::Update(CPipeUser& pipeUser)
{
	SOBJECTSTATE& state = pipeUser.GetState();
	const uint32 tagCrc = GetTagCrc();

	const aiMannequin::SCommand* pCommand = state.mannequinRequest.CreateClearTagCommand(tagCrc);
	if (pCommand != NULL)
	{
		GoalOpSucceeded();
	}
	else
	{
		const EntityId entityId = pipeUser.GetEntityID();
		AIQueueBubbleMessage("CMannequinClearTagGoalOp::Update", entityId, "Could not add a clear tag command to the mannequin request this frame.", eBNS_LogWarning);
	}
}
