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

private:
	// TODO Can these CVars be handled in some other way? Currently live in CryAISystem where they are used for other things as well
	const ICVar *m_pCVar_CloakMinDist;
	const ICVar *m_pCVar_CloakMaxDist;
	const ICVar *m_pCVar_SOMSpeedRelaxed;
	const ICVar *m_pCVar_SOMSpeedCombat;
};

#endif //__TARGET_TRACK_THREAT_MODIFIER_H__
