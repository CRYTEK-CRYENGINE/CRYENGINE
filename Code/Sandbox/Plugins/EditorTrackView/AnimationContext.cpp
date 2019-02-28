// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "AnimationContext.h"

#include "Objects/SelectionGroup.h"
#include "Objects/EntityObject.h"
#include "Objects/ObjectManager.h"
#include "Nodes/TrackViewSequence.h"
#include "Animators/CommentNodeAnimator.h"

#include "RenderViewport.h"
#include "Viewport.h"
#include "ViewManager.h"
#include "IPostRenderer.h"
#include "TrackViewPlugin.h"

#include <CryMovie/IMovieSystem.h>
#include <CrySystem/ITimer.h>

class CMovieCallback : public IMovieCallback, public ICameraDelegate
{
public:
	CMovieCallback() : m_cameraObjectGUID(CryGUID::Null()) {}
	void OnSetCamera(const SCameraParams& cameraParams)
	{
		CEntityObject* pEditorEntity = CEntityObject::FindFromEntityId(cameraParams.cameraEntityId);
		if (pEditorEntity)
		{
			m_cameraObjectGUID = pEditorEntity->GetId();
		}
		else
		{
			m_cameraObjectGUID = CryGUID::Null();
		}
	};

	void ResetCamera()
	{
		m_cameraObjectGUID = CryGUID::Null();
	}

	virtual CryGUID GetActiveCameraObjectGUID() const
	{
		return m_cameraObjectGUID;
	}

	virtual const char* GetName() const
	{
		return "TrackView";
	}

private:
	CryGUID m_cameraObjectGUID;
};

static CMovieCallback s_movieCallback;

class CAnimationContextPostRender : public IPostRenderer
{
public:
	CAnimationContextPostRender(CAnimationContext* pAC)
		: m_pAC(pAC)
	{
	}

	void OnPostRender() const { assert(m_pAC); m_pAC->OnPostRender(); }

protected:
	CAnimationContext* m_pAC;
};

CAnimationContext::CAnimationContext()
{
	m_bPlaying = false;
	m_bPaused = false;
	m_bRecording = false;
	m_bSavedRecordingState = false;
	m_timeRange.Set(SAnimTime(0.0f), SAnimTime(0.0f));
	m_playbackRange.Set(SAnimTime(0.0f), SAnimTime(0.0f));
	m_currTime = SAnimTime(0.0f);
	m_bForceUpdateInNextFrame = false;
	m_fTimeScale = 1.0f;
	m_pSequence = nullptr;
	m_bLooping = false;
	m_bAutoRecording = false;
	m_recordingTimeStep = SAnimTime(0.0f);
	m_bSingleFrame = false;
	m_bPostRenderRegistered = false;
	m_bWasRecording = false;

	GetIEditor()->GetIUndoManager()->AddListener(this);
	CTrackViewPlugin::GetSequenceManager()->AddListener(this);
	GetIEditor()->RegisterNotifyListener(this);
	GetIEditor()->GetGameEngine()->AddListener(this);
	GetIEditor()->GetViewManager()->RegisterCameraDelegate(&s_movieCallback);

	gEnv->pMovieSystem->SetCallback(&s_movieCallback);
	gEnv->pMovieSystem->AddMovieListener(nullptr, this);
	REGISTER_COMMAND("mov_goToFrameEditor", (ConsoleCommandFunc)GoToFrameCmd, 0, "Make a specified sequence go to a given frame time in the editor.");
}

CAnimationContext::~CAnimationContext()
{
	gEnv->pMovieSystem->SetCallback(NULL);
	gEnv->pMovieSystem->RemoveMovieListener(nullptr, this);

	GetIEditor()->GetViewManager()->UnregisterCameraDelegate(&s_movieCallback);
	GetIEditor()->GetGameEngine()->RemoveListener(this);
	CTrackViewPlugin::GetSequenceManager()->RemoveListener(this);
	GetIEditor()->GetIUndoManager()->RemoveListener(this);
	GetIEditor()->UnregisterNotifyListener(this);
}

void CAnimationContext::AddListener(IAnimationContextListener* pListener)
{
	stl::push_back_unique(m_contextListeners, pListener);
}

void CAnimationContext::RemoveListener(IAnimationContextListener* pListener)
{
	stl::find_and_erase(m_contextListeners, pListener);
}

void CAnimationContext::SetSequence(CTrackViewSequence* pSequence, bool bForce, bool bNoNotify)
{
	CTrackViewSequence* pCurrentSequence = m_pSequence;

	if (!bForce && pSequence == pCurrentSequence)
	{
		return;
	}

	// Prevent keys being created from time change
	const bool bRecording = m_bRecording;
	m_bRecording = false;

	m_currTime = m_recordingCurrTime = SAnimTime(0.0f);

	if (!m_bPostRenderRegistered)
	{
		if (GetIEditor() && GetIEditor()->GetViewManager())
		{
			CViewport* pViewport = GetIEditor()->GetViewManager()->GetViewport(ET_ViewportCamera);

			if (pViewport)
			{
				pViewport->AddPostRenderer(new CAnimationContextPostRender(this));
				m_bPostRenderRegistered = true;
			}
		}
	}

	if (m_pSequence)
	{
		m_pSequence->Deactivate();

		if (m_bPlaying)
		{
			m_pSequence->EndCutScene();
		}

		m_pSequence->UnBindFromEditorObjects();
		s_movieCallback.ResetCamera();
	}

	m_pSequence = pSequence;

	if (m_pSequence)
	{
		if (m_bPlaying)
		{
			m_pSequence->BeginCutScene(true);
		}

		UpdateTimeRange();
		m_pSequence->Activate();
		m_pSequence->PrecacheData(SAnimTime(0.0f));

		m_pSequence->BindToEditorObjects();
	}

	ForceAnimation();

	if (!bNoNotify)
	{
		for (size_t i = 0; i < m_contextListeners.size(); ++i)
		{
			if (!pSequence)
				m_contextListeners[i]->OnTimeChanged(SAnimTime(0));
			else
				m_contextListeners[i]->OnTimeChanged(pSequence->GetTimeRange().start);

			m_contextListeners[i]->OnSequenceChanged(m_pSequence);
		}
	}

	m_bRecording = bRecording;
}

void CAnimationContext::UpdateTimeRange()
{
	if (m_pSequence)
	{
		m_timeRange = m_pSequence->GetTimeRange();
		auto seqPlaybackRange = m_pSequence->GetPlaybackRange();
		m_playbackRange.start = seqPlaybackRange.start != SAnimTime(-1) ? std::max(seqPlaybackRange.start, m_timeRange.start) : m_timeRange.start;
		m_playbackRange.start = std::min(m_playbackRange.start, m_timeRange.end);
		m_playbackRange.end = seqPlaybackRange.end != SAnimTime(-1) ? std::min(seqPlaybackRange.end, m_timeRange.end) : m_timeRange.end;
		m_playbackRange.end = std::max(m_playbackRange.end, m_playbackRange.start);
		m_playbackRange.end = std::max(m_playbackRange.end, m_timeRange.start);
	}
}

void CAnimationContext::SetLoopMode(bool bLooping)
{
	if (m_bLooping != bLooping)
	{
		m_bLooping = bLooping;

		for (size_t i = 0; i < m_contextListeners.size(); ++i)
			m_contextListeners[i]->OnLoopingStateChanged(m_bLooping);
	}
}

void CAnimationContext::SetTime(SAnimTime t)
{
	if (t < m_timeRange.start)
	{
		t = m_timeRange.start;
	}

	if (t > m_timeRange.end)
	{
		t = m_timeRange.end;
	}

	if (m_currTime == t)
	{
		return;
	}

	m_currTime = t;
	m_recordingCurrTime = t;
	ForceAnimation();
	UpdateAnimatedLights();

	for (size_t i = 0; i < m_contextListeners.size(); ++i)
	{
		m_contextListeners[i]->OnTimeChanged(m_currTime);
	}
}

void CAnimationContext::SetRecording(bool bRecording)
{
	if (bRecording == m_bRecording)
	{
		return;
	}

	m_bRecording = bRecording;
	m_bPlaying = false;

	if (!bRecording && m_recordingTimeStep != SAnimTime(0.0f))
	{
		SetAutoRecording(false, SAnimTime(0));
	}

	// If started recording, assume we have modified the document.
	GetIEditor()->SetModifiedFlag();

	for (size_t i = 0; i < m_contextListeners.size(); ++i)
	{
		m_contextListeners[i]->OnRecordingStateChanged(bRecording);
	}
}

void CAnimationContext::PlayPause(bool playOrPause)
{
	if (m_bRecording)
	{
		m_bWasRecording = true;
		SetRecording(false);
	}

	if (m_pSequence == nullptr || !m_bPlaying && !playOrPause || m_bPlaying && playOrPause == !m_bPaused)
	{
		return;
	}

	if (playOrPause)
	{
		UpdateTimeRange();
		// We need to deactivate/activate here to ensure all nodes update
		// to the latest changes done in editor.
		//
		// TODO: Nicer would be a proper way just to refresh.
		m_pSequence->Deactivate();
		m_pSequence->Activate();
	}

	if (!m_bPlaying && !m_bWasRecording)
	{
		m_currTime = m_playbackRange.start;
	}

	if (!m_bPlaying)
	{
		m_bPlaying = true;
	}

	if (!m_bPaused && !playOrPause)
	{
		GetIEditor()->GetMovieSystem()->Pause();

		if (m_pSequence)
		{
			m_pSequence->Pause();
		}

		m_bPaused = true;
	}
	else if (m_bPaused && playOrPause)
	{
		GetIEditor()->GetMovieSystem()->Resume();

		if (m_pSequence)
		{
			m_pSequence->Resume();
		}

		m_bPaused = false;
	}

	for (size_t i = 0; i < m_contextListeners.size(); ++i)
	{
		m_contextListeners[i]->OnPlaybackStateChanged(m_bPlaying, m_bPaused);
	}

	if (m_bPaused && m_bWasRecording)
	{
		m_bWasRecording = false;
		SetRecording(true);
	}
}

void CAnimationContext::Stop()
{
	if (m_bWasRecording)
	{
		m_bWasRecording = false;
		SetRecording(true);
	}

	m_bPlaying = false;
	m_bPaused = false;
	m_bSingleFrame = true; //still need one frame update

	for (size_t i = 0; i < m_contextListeners.size(); ++i)
	{
		m_contextListeners[i]->OnPlaybackStateChanged(m_bPlaying, m_bPaused);
	}
}

void CAnimationContext::Update()
{
	const SAnimTime lastTime = m_currTime;
	bool forceNotify = false;

	if (m_bForceUpdateInNextFrame)
	{
		ForceAnimation();
		m_bForceUpdateInNextFrame = false;
	}

	// If looking through camera object and recording animation, do not allow camera shake
	if ((GetIEditor()->GetViewManager()->GetCameraObjectId() != CryGUID::Null()) && IsRecording())
	{
		GetIEditor()->GetMovieSystem()->EnableCameraShake(false);
	}
	else
	{
		GetIEditor()->GetMovieSystem()->EnableCameraShake(true);
	}

	if ((!m_bPlaying && !m_bAutoRecording && !m_bSingleFrame) || (m_bPlaying && m_bPaused && !m_bSingleFrame))
	{
		if (m_pSequence)
		{
			m_pSequence->StillUpdate();
		}

		if (!m_bRecording)
		{
			GetIEditor()->GetMovieSystem()->StillUpdate();
		}

		return;
	}

	if (!m_bPlaying || m_bPaused)
	{
		return;
	}

	ITimer* pTimer = GetIEditor()->GetSystem()->GetITimer();
	const float frameTime = pTimer->GetFrameTime();

	if (m_bAutoRecording)
	{
		m_recordingCurrTime += SAnimTime(frameTime * m_fTimeScale);
		if ((m_recordingCurrTime - m_currTime) > m_recordingTimeStep)
		{
			m_currTime += m_recordingTimeStep;
		}

		if (m_currTime > m_playbackRange.end)
		{
			SetAutoRecording(false, SAnimTime(0));
		}

		// Send sync with physics event to all selected entities.
		GetIEditor()->GetISelectionGroup()->SendEvent(EVENT_PHYSICS_GETSTATE);
	}
	else //normal playing code path
	{
		bool updateTime = true;

		if (m_pSequence != NULL)
		{
			SAnimContext ac;
			ac.dt = SAnimTime(0);
			ac.time = m_currTime;
			ac.bSingleFrame = m_bSingleFrame;
			ac.m_activeCameraEntity = GetActiveCameraEntityId();

			if (m_bSingleFrame)
			{
				m_bSingleFrame = false;
				forceNotify = true;
				updateTime = false;
			}

			ac.bForcePlay = false;
			m_pSequence->Animate(ac);
			m_pSequence->SyncToConsole(ac);
		}

		if (!m_bRecording)
		{
			GetIEditor()->GetMovieSystem()->PreUpdate(frameTime);
			GetIEditor()->GetMovieSystem()->PostUpdate(frameTime);
		}

		if (updateTime)
		{
			SAnimTime futureTime = m_currTime + SAnimTime(frameTime * m_fTimeScale);
			if (futureTime > m_playbackRange.end)
			{
				if (m_bLooping)
				{
					m_currTime = m_playbackRange.start + futureTime - m_playbackRange.end;
				}
				else
				{
					m_currTime = m_playbackRange.end;
					Stop();
				}
			}
			else
			{
				m_currTime = futureTime;
			}
		}
	}

	if (lastTime != m_currTime || forceNotify)
	{
		for (size_t i = 0; i < m_contextListeners.size(); ++i)
		{
			m_contextListeners[i]->OnTimeChanged(m_currTime);
		}
	}

	UpdateAnimatedLights();
}

void CAnimationContext::ForceAnimation()
{
	// For force animation, disable auto recording
	const bool bAutoRecording = m_bAutoRecording;

	if (bAutoRecording)
	{
		SetAutoRecording(false, m_recordingTimeStep);
	}

	if (m_pSequence)
	{
		SAnimContext ac;
		ac.time = m_currTime;
		ac.bSingleFrame = true;
		ac.bForcePlay = true;
		ac.m_activeCameraEntity = GetActiveCameraEntityId();

		m_pSequence->Animate(ac);
		m_pSequence->SyncToConsole(ac);
	}

	if (bAutoRecording)
	{
		SetAutoRecording(true, m_recordingTimeStep);
	}

	GetIEditor()->UpdateViews();
}

EntityId CAnimationContext::GetActiveCameraEntityId() const
{
	if (GetIEditor() && GetIEditor()->GetViewManager())
	{
		CViewManager* pViewManager = GetIEditor()->GetViewManager();
		const CViewport* pViewport = pViewManager->GetActiveViewport();
		if (pViewport && pViewport->IsSequenceCamera())
		{
			if (pViewManager->GetCameraObjectId() != s_movieCallback.GetActiveCameraObjectGUID())
			{
				pViewManager->SetCameraObjectId(s_movieCallback.GetActiveCameraObjectGUID());
			}
		}

		const CBaseObject* pCameraObject = GetIEditor()->GetObjectManager()->FindObject(pViewManager->GetCameraObjectId());
		const IEntity* pCameraEntity = pCameraObject ? ((CEntityObject*)pCameraObject)->GetIEntity() : nullptr;
		if (pCameraEntity)
		{
			return pCameraEntity->GetId();
		}
	}

	return 0;
}

void CAnimationContext::SetAutoRecording(bool bEnable, SAnimTime fTimeStep)
{
	if (bEnable)
	{
		m_bAutoRecording = true;
		m_recordingTimeStep = fTimeStep;

		// Enables physics/ai.
		GetIEditor()->GetGameEngine()->SetSimulationMode(true);
		SetRecording(bEnable);
	}
	else
	{
		m_bAutoRecording = false;
		m_recordingTimeStep = SAnimTime(0);

		// Disables physics/ai.
		GetIEditor()->GetGameEngine()->SetSimulationMode(false);
	}
}

void CAnimationContext::GoToFrameCmd(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() < 3)
	{
		gEnv->pLog->LogError("GoToFrame failed! You should provide two arguments of the 'sequence name' & 'frame time'.");
		return;
	}

	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();

	assert(pAnimationContext);
	CTrackViewSequence* pSeq = pAnimationContext->GetSequence();
	string fullname = pSeq->GetName();
	assert(pSeq && strcmp(fullname.c_str(), pArgs->GetArg(1)) == 0);
	float targetFrame = (float)atof(pArgs->GetArg(2));
	assert(pSeq->GetTimeRange().start <= SAnimTime(targetFrame) && SAnimTime(targetFrame) <= pSeq->GetTimeRange().end);
	CTrackViewPlugin::GetAnimationContext()->m_currTime = SAnimTime(targetFrame);
	pAnimationContext->m_bSingleFrame = true;

	pAnimationContext->ForceAnimation();
}

void CAnimationContext::OnPostRender()
{
	if (m_pSequence)
	{
		SAnimContext ac;
		ac.time = m_currTime;
		ac.bSingleFrame = true;
		ac.bForcePlay = true;
		ac.m_activeCameraEntity = GetActiveCameraEntityId();
		m_pSequence->Render(ac);
	}
}

void CAnimationContext::UpdateAnimatedLights()
{
	bool bLightAnimationSetActive = m_pSequence && (m_pSequence->GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet);
	if (bLightAnimationSetActive == false)
		return;

	std::vector<CBaseObject*> entityObjects;
	GetIEditor()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CEntityObject), entityObjects);
	std::for_each(std::begin(entityObjects), std::end(entityObjects), [this](CBaseObject* pBaseObject)
	{
		CEntityObject* pEntityObject = static_cast<CEntityObject*>(pBaseObject);
		bool bLight = pEntityObject && (pEntityObject->GetEntityClass().Compare("Light") == 0);
		if (bLight)
		{
		  bool bTimeScrubbing = pEntityObject->GetEntityPropertyBool("bTimeScrubbingInTrackView");
		  if (bTimeScrubbing)
		  {
		    pEntityObject->SetEntityPropertyFloat("_fTimeScrubbed", m_currTime);
		  }
		}
	});
}

void CAnimationContext::BeginUndoTransaction()
{
	m_bSavedRecordingState = m_bRecording;
}

void CAnimationContext::EndUndoTransaction()
{
	if (m_pSequence)
	{
		m_pSequence->BindToEditorObjects();
	}
}

void CAnimationContext::OnSequenceRemoved(CTrackViewSequence* pSequence)
{
	if (m_pSequence == pSequence)
	{
		SetSequence(nullptr, true, false);
	}
}
void CAnimationContext::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginGameMode:
		if (m_pSequence)
		{
			m_pSequence->Resume();
		}

	case eNotify_OnBeginSceneSave:
	case eNotify_OnBeginLayerExport:
		if (m_pSequence)
		{
			m_sequenceName = m_pSequence->GetName();
		}
		else
		{
			m_sequenceName = "";
		}

		m_sequenceTime = GetTime();

		m_bSavedRecordingState = m_bRecording;
		SetSequence(nullptr, true, true);
		break;

	case eNotify_OnEndGameMode:
	case eNotify_OnEndSceneSave:
	case eNotify_OnEndLayerExport:
		m_currTime = m_sequenceTime;
		SetSequence(CTrackViewPlugin::GetSequenceManager()->GetSequenceByName(m_sequenceName), true, true);
		SetTime(m_sequenceTime);
		break;

	case eNotify_OnClearLevelContents:
		SetSequence(nullptr, true, false);
		break;

	case eNotify_OnBeginNewScene:
		SetSequence(nullptr, false, false);
		break;

	case eNotify_OnBeginLoad:
		m_bSavedRecordingState = m_bRecording;
		CTrackViewPlugin::GetAnimationContext()->SetSequence(nullptr, false, false);
		break;

	case eNotify_CameraChanged:
		ForceAnimation();
		break;
	}
}

void CAnimationContext::OnMovieEvent(EMovieEvent movieEvent, IAnimSequence* pAnimSequence)
{
	if (gEnv->IsEditorGameMode())
		return;

	switch (movieEvent)
	{
	case eMovieEvent_Started:
		{
			CTrackViewSequence* pTrackViewSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByName(pAnimSequence->GetName());
			SetSequence(pTrackViewSequence, true, false);
		}
		break;
	case eMovieEvent_Stopped:
		SetSequence(nullptr, true, false);
		break;
	default:
		break;
	}
}

