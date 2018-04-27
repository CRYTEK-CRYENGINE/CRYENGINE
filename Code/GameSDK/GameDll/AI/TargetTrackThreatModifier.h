// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Game-side logic for controlling the threat level of an AI
				agent's current target
  
 -------------------------------------------------------------------------
  History:
  - 09:08:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __TARGET_TRACK_THREAT_MODIFIER_H__
#define __TARGET_TRACK_THREAT_MODIFIER_H__

#include <CryAISystem/ITargetTrackManager.h>

class CTargetTrackThreatModifier : public ITargetTrackThreatModifier
{
public:
	CTargetTrackThreatModifier();
	virtual ~CTargetTrackThreatModifier();
	
	virtual void ModifyTargetThreat(IAIObject &ownerAI, IAIObject &targetAI, const ITargetTrack &track, float &outThreatRatio, EAITargetThreat &outThreat) const;
};

#endif //__TARGET_TRACK_THREAT_MODIFIER_H__
