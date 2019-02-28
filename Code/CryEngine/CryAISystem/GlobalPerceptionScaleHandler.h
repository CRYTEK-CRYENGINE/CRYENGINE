// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   GlobalPerceptionScaleHandler.h
   $Id$
   Description: Handler to manage global perception scale values

   -------------------------------------------------------------------------
   History:
   22:12:2011 -  Created by Francesco Roccucci

 *********************************************************************/

#ifndef __GlobalPerceptionScale_h__
#define __GlobalPerceptionScale_h__

#pragma once

#include <CryCore/Containers/CryListenerSet.h>

struct IAIObject;

class CGlobalPerceptionScaleHandler
{
public:
	CGlobalPerceptionScaleHandler();

	void SetGlobalPerception(const float visual, const float audio, const EAIFilterType filterType, uint8 factionID);
	void ResetGlobalPerception();

	// Return the specific visual/audio perception scale value for the pObject
	// in relation of the global settings
	float GetGlobalVisualScale(const IAIObject* pAIObject) const;
	float GetGlobalAudioScale(const IAIObject* pAIObject) const;

	void  RegisterListener(IAIGlobalPerceptionListener* plistener);
	void  UnregisterListener(IAIGlobalPerceptionListener* plistener);

	void  Reload();
	void  Serialize(TSerialize ser);

	void  DebugDraw() const;

private:

	bool IsObjectAffected(const IAIObject* pAIObject) const;
	void NotifyEvent(const IAIGlobalPerceptionListener::EGlobalPerceptionScaleEvent event);

	float         m_visual;
	float         m_audio;
	EAIFilterType m_filterType;
	uint8         m_factionID;
	uint8         m_playerFactionID;

	typedef CListenerSet<IAIGlobalPerceptionListener*> Listeners;
	Listeners m_listenersList;
};

#endif // __GlobalPerceptionScale_h__
