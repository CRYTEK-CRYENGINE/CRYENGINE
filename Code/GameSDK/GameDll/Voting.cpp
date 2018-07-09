// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 4:23:2007   : Created by Stas Spivakov

*************************************************************************/

#include "StdAfx.h"
#include "Voting.h"

CVotingSystem::CVotingSystem():
m_state(eVS_none),
m_id(0)
{

}

CVotingSystem::~CVotingSystem()
{

}

bool  CVotingSystem::StartVoting(EntityId id, const CTimeValue& start, EVotingState st, EntityId eid, const char* subj)
{
  if(st == eVS_none)
    return false;
  if(IsInProgress())
    return false;
  SVoting v;
  v.initiator = id;
  v.startTime = start;
  m_votings.push_back(v);
  m_votesFor.resize(0);
  m_votesAgainst.resize(0);

	m_subject = subj?subj:"";
  m_state = st;
  m_id = eid;

  m_startTime = start;
  return true;
}

void  CVotingSystem::EndVoting()
{
  m_votesFor.resize(0);
  m_votesAgainst.resize(0);
  m_subject.resize(0);
  m_state = eVS_none;
  m_id = 0;
}

bool  CVotingSystem::GetCooldownTime(EntityId id, CTimeValue& v)
{
  for(int i=m_votings.size()-1;i>=0;--i)
    if(m_votings[i].initiator==id)
    {
      v = m_votings[i].startTime;
      return true;
    }
  return false;
}

bool  CVotingSystem::IsInProgress()const
{
  return m_state!=eVS_none;
}

int   CVotingSystem::GetNumVotesFor()const
{
  return m_votesFor.size();
}

int   CVotingSystem::GetNumVotesAgainst()const
{
	return m_votesAgainst.size();
}

const string& CVotingSystem::GetSubject()const
{
  return m_subject;
}

EntityId CVotingSystem::GetEntityId()const
{
  return m_id;
}

EVotingState CVotingSystem::GetType()const
{
  return m_state;
}

CTimeValue CVotingSystem::GetVotingTime()const
{
  return gEnv->pTimer->GetFrameStartTime()-m_startTime;
}

CTimeValue CVotingSystem::GetVotingStartTime()const
{
	return m_startTime;
}

void  CVotingSystem::Reset()
{
  EndVoting();
  m_votings.resize(0);
}

void	CVotingSystem::GetPlayerVoteBreakdown(EntityId kickTargetId, TVoteDataList& voteDataList) const
{
	CRY_ASSERT_MESSAGE(kickTargetId == GetEntityId(), "The requested vote breakdown and the target vote are for two different entities!");

	for( TVoteEntitiesVec::const_iterator iter = m_votesFor.begin(), end = m_votesFor.end(); iter != end; ++iter)
	{
		voteDataList.push_back(SVoteData(*iter, true));
	}

	for( TVoteEntitiesVec::const_iterator iter = m_votesAgainst.begin(), end = m_votesAgainst.end(); iter != end; ++iter)
	{
		voteDataList.push_back(SVoteData(*iter, false));
	}
}

bool  CVotingSystem::EntityLeftGame(EntityId entityId)
{
	TVoteEntitiesVec::iterator itFor = m_votesFor.begin();
	TVoteEntitiesVec::const_iterator itForEnd = m_votesFor.end();
	for(;itFor!=itForEnd;++itFor)
	{
		if (entityId == (*itFor))
		{
			m_votesFor.erase(itFor);

			return true;	
		}
	}

	TVoteEntitiesVec::iterator itAgainst = m_votesAgainst.begin();
	TVoteEntitiesVec::const_iterator itAgainstEnd = m_votesAgainst.end();
	for(;itAgainst!=itAgainstEnd;++itAgainst)
	{
		if (entityId == (*itAgainst))
		{
			m_votesAgainst.erase(itAgainst);

			return true;	
		}
	}

	return false;
}

//clients can vote
void  CVotingSystem::Vote(EntityId id, int team, bool yes)
{
  if(CanVote(id))
  {
		if(yes)
		{
			m_votesFor.push_back(id);
		}
		else
		{
			m_votesAgainst.push_back(id);
		}
  }
}

bool  CVotingSystem::CanVote(EntityId id)const
{
	size_t numVotesFor = m_votesFor.size();
  for(size_t i=0;i<numVotesFor; ++i)
	{
    if(m_votesFor[i] == id)
		{
			return false;
		}
	}

	size_t numVotesAgainst = m_votesAgainst.size();
	for(size_t i=0;i<numVotesAgainst; ++i)
	{
		if(m_votesAgainst[i] == id)
		{
			return false;
		}
	}

  return true;
}