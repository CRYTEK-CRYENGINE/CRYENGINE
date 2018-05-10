// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CommunicationTestManager.h"
#include "CommunicationManager.h"

CommunicationTestManager::CommunicationTestManager()
	: m_playGenID(0)
{
}

void CommunicationTestManager::Reset()
{
	while (!m_playingActors.empty())
		Stop(m_playingActors.begin()->first);
}

void CommunicationTestManager::StartFor(EntityId actorID, const char* commName, int variationNumber)
{
	IEntity* entity = gEnv->pEntitySystem->GetEntity(actorID);

	if (IAIObject* aiObject = entity->GetAI())
	{
		if (IAIActorProxy* proxy = aiObject->GetProxy())
		{
			CryLogAlways("Playing communications test for '%s'...", entity ? entity->GetName() : "<null>");

			std::pair<PlayingActors::iterator, bool> iresult = m_playingActors.insert(
			  PlayingActors::value_type(actorID, PlayingActor()));

			if (!iresult.second)
			{
				// already playing
				return;
			}

			PlayingActor& playingActor = iresult.first->second;
			playingActor.configID = gAIEnv.pCommunicationManager->GetConfigID(proxy->GetCommunicationConfigName());
			playingActor.onlyOne = commName && *commName;

			if (playingActor.onlyOne)
			{
				playingActor.commID = gAIEnv.pCommunicationManager->GetCommunicationID(commName);
				playingActor.variation = variationNumber;
			}

			PlayNext(actorID);
		}
	}
}

void CommunicationTestManager::Stop(EntityId actorID)
{
	PlayingActors::iterator it = m_playingActors.find(actorID);

	if (it != m_playingActors.end())
	{
		PlayingActor& playingActor = it->second;
		CommPlayID playID = playingActor.playID;
		m_playing.erase(playingActor.playID);
		m_playingActors.erase(it);

		m_player.Stop(playID);

		IEntity* entity = gEnv->pEntitySystem->GetEntity(actorID);
		CryLogAlways("Cancelled communications test for '%s'...", entity ? entity->GetName() : "<null>");
	}
}

void CommunicationTestManager::Update(float updateTime)
{
	m_player.Update(updateTime);
}

void CommunicationTestManager::OnCommunicationFinished(const CommPlayID& playID, uint8 stateFlags)
{
	PlayingCommunications::iterator it = m_playing.find(playID);
	if (it != m_playing.end())
	{
		EntityId actorID = it->second;
		m_playing.erase(playID);

		PlayingActors::iterator pit = m_playingActors.find(actorID);
		if (pit != m_playingActors.end())
		{
			PlayingActor& playingActor = pit->second;

			if (playingActor.onlyOne)
			{
				const CCommunicationManager::CommunicationsConfig& config =
				  gAIEnv.pCommunicationManager->GetConfig(playingActor.configID);

				Report(actorID, playingActor, config.name.c_str());
				m_playingActors.erase(pit);
			}
			else
				PlayNext(actorID);
		}
	}
}

void CommunicationTestManager::PlayNext(EntityId actorID)
{
	PlayingActors::iterator pit = m_playingActors.find(actorID);
	if (pit != m_playingActors.end())
	{
		PlayingActor& playingActor = pit->second;

		const CCommunicationManager::CommunicationsConfig& config =
		  gAIEnv.pCommunicationManager->GetConfig(playingActor.configID);

		while (true)
		{
			if (!playingActor.commID && !config.comms.empty())
				playingActor.commID = config.comms.begin()->first;

			if (!playingActor.commID)
			{
				Report(actorID, playingActor, config.name.c_str());
				m_playingActors.erase(pit);

				return;
			}

			CCommunicationManager::Communications::const_iterator commIt = config.comms.find(playingActor.commID);
			if (commIt == config.comms.end())
			{
				++playingActor.failedCount;
				Report(actorID, playingActor, config.name.c_str());
				m_playingActors.erase(pit);

				return;
			}

			const SCommunication& prevComm = commIt->second;
			uint32 prevVarCount = prevComm.variations.size();

			if (playingActor.variation >= prevVarCount)
			{
				++commIt;

				if (commIt == config.comms.end())
				{
					Report(actorID, playingActor, config.name.c_str());
					m_playingActors.erase(pit);

					return;
				}

				playingActor.variation = 0;
				playingActor.commID = commIt->first;
			}

			SCommunicationRequest request;
			request.actorID = actorID;
			request.commID = playingActor.commID;
			request.configID = playingActor.configID;

			const SCommunication& comm = commIt->second;
			++playingActor.totalCount;

			SCommunication testComm(comm);
			SCommunicationVariation& variation = testComm.variations[playingActor.variation];
			variation.flags |= (SCommunication::FinishVoice | SCommunication::FinishSound);

			++m_playGenID.id;
			if (m_player.Play(m_playGenID, request, testComm, playingActor.variation++, this, 0))
			{
				CryLogAlways("Now playing communication test '%s' variation %u from '%s'...",
				             comm.name.c_str(), playingActor.variation - 1, config.name.c_str());
				m_playing.insert(PlayingCommunications::value_type(m_playGenID, actorID));
				playingActor.playID = m_playGenID;

				break;
			}
			else
			{
				CryLogAlways("Failed to play communication test '%s' variation '%u' from '%s'...",
				             comm.name.c_str(), playingActor.variation - 1, config.name.c_str());

				++playingActor.failedCount;

				if (playingActor.onlyOne)
				{
					Report(actorID, playingActor, config.name.c_str());
					m_playingActors.erase(pit);

					return;
				}
			}
		}
	}
}

void CommunicationTestManager::Report(EntityId actorID, const PlayingActor& playingActor, const char* configName)
{
	IEntity* entity = gEnv->pEntitySystem->GetEntity(actorID);

	CryLogAlways("Finished communication test for '%s' using '%s'...", entity ? entity->GetName() : "<null>", configName);
	CryLogAlways("Attempted: %u", playingActor.totalCount);
	CryLogAlways("Failed: %u", playingActor.failedCount);
}
