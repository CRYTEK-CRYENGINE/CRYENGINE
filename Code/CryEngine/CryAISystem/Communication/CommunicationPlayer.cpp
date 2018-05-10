// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CommunicationPlayer.h"
#include "CommunicationManager.h"
#include "Puppet.h"

void CommunicationPlayer::PlayState::OnCommunicationEvent(
  IAICommunicationHandler::ECommunicationHandlerEvent event, EntityId actorID)
{
	const uint8 prevFlags = finishedFlags;

	switch (event)
	{
	case IAICommunicationHandler::ActionStarted:
	case IAICommunicationHandler::SignalStarted:
	case IAICommunicationHandler::SoundStarted:
	case IAICommunicationHandler::VoiceStarted:
		//case IAICommunicationHandler::ActionCancelled:
		break;
	case IAICommunicationHandler::ActionFailed:
	case IAICommunicationHandler::SignalFailed:
	case IAICommunicationHandler::SignalCancelled:
	case IAICommunicationHandler::SignalFinished:
		finishedFlags |= FinishedAnimation;
		break;
	case IAICommunicationHandler::SoundFailed:
	case IAICommunicationHandler::SoundFinished:
	case IAICommunicationHandler::SoundCancelled:
		finishedFlags |= FinishedSound;
		break;
	case IAICommunicationHandler::VoiceFailed:
	case IAICommunicationHandler::VoiceFinished:
	case IAICommunicationHandler::VoiceCancelled:
		finishedFlags |= FinishedVoice;
		break;
	default:
		break;
	}

	if (gAIEnv.CVars.DebugDrawCommunication == 5 && prevFlags != finishedFlags)
	{
		if (finishedFlags != FinishedAll)
		{
			CryLogAlways("CommunicationPlayer::PlayState: Still waiting for %x to finish commID[%u]", ~finishedFlags & FinishedAll, commID.id);
		}
		else
		{
			CryLogAlways("CommunicationPlayer::PlayState: All finished! commID[%u]", commID.id);
		}
	}
}

void CommunicationPlayer::Reset()
{
	PlayingCommunications::iterator it = m_playing.begin();
	PlayingCommunications::iterator end = m_playing.end();

	for (; it != end; ++it)
	{
		PlayState& playState = it->second;

		IEntity* entity = gEnv->pEntitySystem->GetEntity(playState.actorID);
		if (!entity)
			continue;

		IAIObject* aiObject = entity->GetAI();
		if (!aiObject)
			continue;

		IAIActorProxy* aiProxy = aiObject->GetProxy();
		if (!aiProxy)
			continue;

		IAICommunicationHandler* commHandler = aiProxy->GetCommunicationHandler();

		if (!playState.animationName.empty())
		{
			IAICommunicationHandler::EAnimationMethod method = (playState.flags & SCommunication::AnimationAction)
			                                                   ? IAICommunicationHandler::AnimationMethodAction : IAICommunicationHandler::AnimationMethodSignal;

			commHandler->StopAnimation(it->first, playState.animationName.c_str(), method);
		}

		if (playState.soundInfo.playSoundControlId != CryAudio::InvalidControlId)
			commHandler->StopSound(playState.soundInfo);

		if (playState.voiceInfo.playSoundControlId != CryAudio::InvalidControlId)
			commHandler->StopVoice(playState.voiceInfo);
	}

	m_playing.clear();
}

void CommunicationPlayer::Clear()
{
	Reset();
}

void CommunicationPlayer::OnCommunicationHandlerEvent(
  IAICommunicationHandler::ECommunicationHandlerEvent event, CommPlayID playID, EntityId actorID)
{
	PlayingCommunications::iterator stateIt = m_playing.find(playID);
	if (stateIt != m_playing.end())
	{
		stateIt->second.OnCommunicationEvent(event, actorID);
	}
}

void CommunicationPlayer::BlockingSettings(IAIObject* aiObject, PlayState& playState, bool set)
{
	if (aiObject)
	{
		if (playState.flags & SCommunication::BlockAll)
		{
			if (playState.flags & SCommunication::BlockMovement)
			{
				if (CPipeUser* pipeUser = aiObject->CastToCPipeUser())
				{
					pipeUser->Pause(set);
				}
			}
			if (playState.flags & SCommunication::BlockFire)
			{
				if (CPuppet* puppet = aiObject->CastToCPuppet())
				{
					puppet->EnableFire(!set);
				}
			}
		}
	}
}

bool CommunicationPlayer::Play(const CommPlayID& playID, const SCommunicationRequest& request,
                               const SCommunication& comm, uint32 variationIdx, ICommunicationFinishedListener* listener,
                               void* param)
{
	// Only return false when playing could not happen due to congestion
	// If case of unrecoverable error return true to avoid the queue getting full
	bool failed = true;

	IAIObject* aiObject = 0;
	IAIActorProxy* aiProxy = 0;

	if (IEntity* entity = gEnv->pEntitySystem->GetEntity(request.actorID))
		if (aiObject = entity->GetAI())
			if (aiProxy = aiObject->GetProxy())
				failed = false;

	assert(variationIdx < comm.variations.size());
	const SCommunicationVariation& variation = comm.variations[variationIdx];

	PlayState& playState = stl::map_insert_or_get(m_playing, playID,
	                                              PlayState(request, variation, listener, param));

	if (failed)
	{
		playState.finishedFlags = PlayState::FinishedAll;
		return false;
	}

	assert(playState.animationName.empty());

	assert(aiProxy);//shut up SCA
	IAICommunicationHandler* commHandler = aiProxy->GetCommunicationHandler();

	if (!variation.soundName.empty() && !request.skipCommSound)
	{
		playState.soundInfo = commHandler->PlaySound(playID, variation.soundName.c_str(),
		                                             (variation.flags & SCommunication::FinishSound) ? this : 0);

		if (playState.soundInfo.playSoundControlId == CryAudio::InvalidControlId)
		{
			AIWarning("Failed to play communication sound '%s'...", variation.soundName.c_str());

			playState.finishedFlags |= PlayState::FinishedSound;
		}
	}
	else
		playState.finishedFlags |= PlayState::FinishedSound;

	if (!variation.voiceName.empty() && !request.skipCommSound)
	{
		playState.voiceInfo = commHandler->PlayVoice(playID, variation.voiceName.c_str(),
		                                             (variation.flags & SCommunication::FinishVoice) ? this : 0);

		if (playState.voiceInfo.playSoundControlId == CryAudio::InvalidControlId)
		{
			AIWarning("Failed to play communication voice '%s'...", variation.voiceName.c_str());

			playState.finishedFlags |= PlayState::FinishedVoice;
		}
	}
	else
		playState.finishedFlags |= PlayState::FinishedVoice;

	if (!variation.animationName.empty() && !request.skipCommAnimation)
	{
		playState.animationName = variation.animationName;

		IAICommunicationHandler::EAnimationMethod method = (variation.flags & SCommunication::AnimationAction)
		                                                   ? IAICommunicationHandler::AnimationMethodAction : IAICommunicationHandler::AnimationMethodSignal;

		IAICommunicationHandler::IEventListener* listener2 = (playState.flags & SCommunication::FinishAnimation) ? this : 0;
		commHandler->PlayAnimation(playID, variation.animationName.c_str(), method, listener2);
	}
	else
		playState.finishedFlags |= PlayState::FinishedAnimation;

	if (playState.timeout <= 0.000001f)
		playState.finishedFlags |= PlayState::FinishedTimeout;

	BlockingSettings(aiObject, playState, true);

	return true;
}

void CommunicationPlayer::Update(float updateTime)
{
	PlayingCommunications::iterator it = m_playing.begin();
	PlayingCommunications::iterator end = m_playing.end();

	for (; it != end; )
	{
		PlayState& playState = it->second;
		const CommPlayID& playId = it->first;

		if (!UpdatePlaying(playId, playState, updateTime))
		{
			CleanUpPlayState(playId, playState);

			PlayingCommunications::iterator erased = it++;

			if (gAIEnv.CVars.DebugDrawCommunication == 5)
				CryLogAlways("CommunicationPlayer removed finished: %s[%u] as playID[%u] with listener[%p]", gAIEnv.pCommunicationManager->GetCommunicationName(erased->second.commID), erased->second.commID.id, erased->first.id, erased->second.listener);

			m_playing.erase(erased);

			continue;
		}
		++it;
	}
}

bool CommunicationPlayer::UpdatePlaying(const CommPlayID& playId, PlayState& playState, float updateTime)
{

	// Only return false when playing could not happen due to congestion
	// If case of unrecoverable error return true to avoid the queue getting full
	IEntity* entity = gEnv->pEntitySystem->GetEntity(playState.actorID);
	if (!entity)
		return false;

	IAIObject* aiObject = entity->GetAI();
	if (!aiObject)
		return false;

	if (playState.flags & SCommunication::LookAtTarget)
	{
		if (playState.targetID)
		{
			if (IEntity* targetEntity = gEnv->pEntitySystem->GetEntity(playState.targetID))
				playState.target = targetEntity->GetWorldPos();
		}

		if (!playState.target.IsZeroFast())
		{
			if (CPipeUser* pipeUser = aiObject->CastToCPipeUser())
			{
				pipeUser->SetLookStyle(LOOKSTYLE_HARD_NOLOWER);
				pipeUser->SetLookAtPointPos(playState.target, true);
			}
		}
	}

	if (playState.timeout > 0.0f)
	{
		playState.timeout -= updateTime;
		if (playState.timeout <= 0.0f)
			playState.finishedFlags |= PlayState::FinishedTimeout;
	}

	bool finished = ((playState.flags & SCommunication::FinishAnimation) == 0)
	                || (playState.finishedFlags & PlayState::FinishedAnimation);

	finished = finished && (((playState.flags & SCommunication::FinishSound) == 0)
	                        || (playState.finishedFlags & PlayState::FinishedSound));

	finished = finished && (((playState.flags & SCommunication::FinishVoice) == 0)
	                        || (playState.finishedFlags & PlayState::FinishedVoice));

	finished = finished && (((playState.flags & SCommunication::FinishTimeout) == 0)
	                        || (playState.finishedFlags & PlayState::FinishedTimeout));

	if (finished)
	{
		if ((playState.flags & SCommunication::AnimationAction) && ((playState.finishedFlags & PlayState::FinishedAnimation) == 0))
		{
			IAIActorProxy* aiProxy = aiObject->GetProxy();
			if (!aiProxy)
				return false;

			IAICommunicationHandler* commHandler = aiProxy->GetCommunicationHandler();

			commHandler->StopAnimation(playId, playState.animationName.c_str(), IAICommunicationHandler::AnimationMethodAction);

			playState.finishedFlags |= PlayState::FinishedAnimation;
		}

		BlockingSettings(aiObject, playState, false);

		return false;
	}

	return true;
}

void CommunicationPlayer::CleanUpPlayState(const CommPlayID& playID, PlayState& playState)
{
	bool finished = ((playState.flags & SCommunication::FinishAnimation) == 0)
	                || (playState.finishedFlags & PlayState::FinishedAnimation);
	finished = finished && (((playState.flags & SCommunication::FinishSound) == 0)
	                        || (playState.finishedFlags & PlayState::FinishedSound));
	finished = finished && (((playState.flags & SCommunication::FinishVoice) == 0)
	                        || (playState.finishedFlags & PlayState::FinishedVoice));
	finished = finished && (((playState.flags & SCommunication::FinishTimeout) == 0)
	                        || (playState.finishedFlags & PlayState::FinishedTimeout));

	if (!finished)
	{
		IEntity* entity = gEnv->pEntitySystem->GetEntity(playState.actorID);
		IAIObject* aiObject = entity ? entity->GetAI() : 0;

		if (IAIActorProxy* aiProxy = aiObject ? aiObject->GetProxy() : 0)
		{
			IAICommunicationHandler* commHandler = aiProxy->GetCommunicationHandler();
			//REINST(Stop currently playing comm sounds and voice lines)
			if ((playState.finishedFlags & PlayState::FinishedSound) == 0)
				commHandler->StopSound(playState.soundInfo);

			if ((playState.finishedFlags & PlayState::FinishedVoice) == 0)
				commHandler->StopVoice(playState.voiceInfo);

			if (!playState.animationName.empty() && ((playState.finishedFlags & PlayState::FinishedAnimation) == 0))
			{
				IAICommunicationHandler::EAnimationMethod method = (playState.flags & SCommunication::AnimationAction)
				                                                   ? IAICommunicationHandler::AnimationMethodAction : IAICommunicationHandler::AnimationMethodSignal;

				commHandler->StopAnimation(playID, playState.animationName.c_str(), method);
			}

			BlockingSettings(aiObject, playState, false);
		}
	}

	if (playState.listener)
		playState.listener->OnCommunicationFinished(playID, playState.finishedFlags);

}

void CommunicationPlayer::Stop(const CommPlayID& playID)
{
	PlayingCommunications::iterator it = m_playing.find(playID);
	if (it != m_playing.end())
	{
		PlayState& playState = it->second;

		CleanUpPlayState(playID, playState);

		if (gAIEnv.CVars.DebugDrawCommunication == 5)
		{
			CryLogAlways("CommunicationPlayer finished playing: %s[%u] as playID[%u]", gAIEnv.pCommunicationManager->GetCommunicationName(playState.commID), playState.commID.id, it->first.id);
		}

		m_playing.erase(it);
	}
}

bool CommunicationPlayer::IsPlaying(const CommPlayID& playID, float* remainingTime) const
{
	PlayingCommunications::const_iterator it = m_playing.find(playID);
	if (it != m_playing.end())
		return true;

	return false;
}

CommID CommunicationPlayer::GetCommunicationID(const CommPlayID& playID) const
{
	PlayingCommunications::const_iterator it = m_playing.find(playID);
	if (it != m_playing.end())
		return it->second.commID;

	return CommID(0);
}
