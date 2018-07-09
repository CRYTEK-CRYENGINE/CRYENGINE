// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
	
	-------------------------------------------------------------------------
	History:
	- 20:10:2009  : Created by Mark Tully

*************************************************************************/

#ifndef _GAME_RULES_STATS_RECORDING_MODULE_H_
#define _GAME_RULES_STATS_RECORDING_MODULE_H_

struct HitInfo;
class XmlNodeRef;
struct IStatsTracker;

#include "../GameRulesTypes.h"

class IGameRulesStatsRecording
{
	public:
		virtual			~IGameRulesStatsRecording() {}

		virtual void	Init(
							XmlNodeRef		inXML)=0;

		virtual void	OnInGameBegin()=0;
		virtual void	OnPostGameBegin()=0;
		virtual void	OnGameEnd()=0;
		virtual void	OnPlayerRevived(
							IActor			*inActor)=0;
		virtual void	OnPlayerKilled(
							IActor			*inActor)=0;
		 
#if USE_PC_PREMATCH
		virtual void OnPrematchEnd(
							IActor			*inActor)=0;
#endif
};

#endif // _GAME_RULES_STATS_RECORDING_MODULE_H_
