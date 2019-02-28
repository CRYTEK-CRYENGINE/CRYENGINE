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

#ifndef __VOTING_H__
#define __VOTING_H__

#pragma once

#include "GameRulesTypes.h"

// Summary
//  Types for the different vote states
enum EVotingState
{
  eVS_none=0,//no voting is currently held
  eVS_kick,//kick vote
  eVS_nextMap,//next map vote
  eVS_changeMap,//change map vote
  eVS_consoleCmd,//execute arbitrary console cmd
  eVS_last//this should be always last!
};

enum EKickState
{
	eKS_None = 0,
	eKS_StartVote,
	eKS_VoteProgress,
	eKS_VoteEnd_Success,
	eKS_VoteEnd_Failure,
	eKS_Num
};

struct SVotingParams
{
  SVotingParams();

};

class CVotingSystem
{
  struct SVoting//once it is initiated
  {
    EntityId    initiator;
    CTimeValue  startTime;
  };
public:
  CVotingSystem();
  ~CVotingSystem();
  bool  StartVoting(EntityId id, const CTimeValue& start, EVotingState st, EntityId eid, const char* subj);
  void  EndVoting();
  bool  GetCooldownTime(EntityId id, CTimeValue& v);
  
  bool  IsInProgress()const;
  int   GetNumVotesFor()const;
  int   GetNumVotesAgainst()const;

	void	GetPlayerVoteBreakdown(EntityId kickTargetId, TVoteDataList& voteDataList) const;

  const string& GetSubject()const;
  EntityId GetEntityId()const;
  EVotingState GetType()const;
  
	CTimeValue GetVotingTime()const;
	CTimeValue GetVotingStartTime()const;

  void  Reset();
	bool  EntityLeftGame(EntityId entityId);

  //clients can vote
  void  Vote(EntityId id, int team, bool yes);
  bool  CanVote(EntityId id)const;
private:
	typedef std::vector<EntityId> TVoteEntitiesVec;

  CTimeValue            m_startTime;
  
  EVotingState          m_state;
  string                m_subject;
  EntityId              m_id;

  TVoteEntitiesVec      m_votesFor;
  TVoteEntitiesVec      m_votesAgainst;

  //recent voting
  std::vector<SVoting>  m_votings;
};

#endif // #ifndef __VOTING_H__