// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementPlan.h"
#include <CryAISystem/MovementUpdateContext.h>

namespace Movement
{
Plan::Status Plan::Execute(const MovementUpdateContext& context)
{
	if (m_current == NoBlockIndex)
		ChangeToIndex(0, context.actor);

	m_lastStatus = Plan::Status::Running;

	while (true)
	{
		assert(m_current != NoBlockIndex);
		assert(m_current < m_blocks.size());

		const Block::Status blockStatus = m_blocks[m_current]->Update(context);
		switch (blockStatus)
		{
		case Block::Finished:
		{
			if (m_current + 1 < m_blocks.size())
			{
				ChangeToIndex(m_current + 1, context.actor);
				continue;
			}			
			ChangeToIndex(NoBlockIndex, context.actor);
			m_lastStatus = Plan::Status::Finished;
			break;
		}
		case Block::CantBeFinished:
			m_lastStatus = Plan::Status::CantBeFinished;
			break;
		case Block::Running:
			m_lastStatus = Plan::Status::Running;
			break;
		default:
			assert(0);
			break;
		}
		break;
	}
	return m_lastStatus;
}

void Plan::ChangeToIndex(const uint newIndex, IMovementActor& actor)
{
	const uint oldIndex = m_current;

	if (oldIndex != NoBlockIndex)
		m_blocks[oldIndex]->End(actor);

	if (newIndex != NoBlockIndex)
		m_blocks[newIndex]->Begin(actor);

	m_current = newIndex;
}

void Plan::Clear(IMovementActor& actor)
{
	ChangeToIndex(NoBlockIndex, actor);
	m_blocks.clear();
	m_requestId = MovementRequestID();
	m_lastStatus = Plan::Status::None;
}

void Plan::CutOffAfterCurrentBlock()
{
	if (m_current < m_blocks.size())
		m_blocks.resize(m_current + 1);
}

bool Plan::InterruptibleNow() const
{
	if (m_current == NoBlockIndex)
		return true;

	return m_blocks[m_current]->InterruptibleNow();
}

const Block* Plan::GetBlock(uint32 index) const
{
	return m_blocks[index].get();
}
}
