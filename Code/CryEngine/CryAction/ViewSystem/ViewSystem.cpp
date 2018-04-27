// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ViewSystem.h"
#include <CryMath/Cry_Camera.h>
#include <ILevelSystem.h>
#include "GameObjects/GameObject.h"
#include "CryAction.h"
#include "IActorSystem.h"
#include <CryMath/PNoise3.h>
#include <CryAISystem/IAISystem.h>

#define VS_CALL_LISTENERS(func)                                                                               \
  {                                                                                                           \
    size_t count = m_listeners.size();                                                                        \
    if (count > 0)                                                                                            \
    {                                                                                                         \
      const size_t memSize = count * sizeof(IViewSystemListener*);                                            \
      PREFAST_SUPPRESS_WARNING(6255) IViewSystemListener * *pArray = (IViewSystemListener**) alloca(memSize); \
      memcpy(pArray, &*m_listeners.begin(), memSize);                                                         \
      while (count--)                                                                                         \
      {                                                                                                       \
        (*pArray)->func; ++pArray;                                                                            \
      }                                                                                                       \
    }                                                                                                         \
  }

//------------------------------------------------------------------------
CViewSystem::CViewSystem(ISystem* const pSystem) :
	m_pSystem(pSystem),
	m_activeViewId(0),
	m_nextViewIdToAssign(1000),
	m_preSequenceViewId(0),
	m_cutsceneViewId(0),
	m_cutsceneCount(0),
	m_bOverridenCameraRotation(false),
	m_bActiveViewFromSequence(false),
	m_fBlendInPosSpeed(0.0f),
	m_fBlendInRotSpeed(0.0f),
	m_bPerformBlendOut(false),
	m_useDeferredViewSystemUpdate(false)
{
	REGISTER_CVAR2("cl_camera_noise", &m_fCameraNoise, -1, 0,
	               "Adds hand-held like camera noise to the camera view. \n The higher the value, the higher the noise.\n A value <= 0 disables it.");
	REGISTER_CVAR2("cl_camera_noise_freq", &m_fCameraNoiseFrequency, 2.5326173f, 0,
	               "Defines camera noise frequency for the camera view. \n The higher the value, the higher the noise.");

	REGISTER_CVAR2("cl_ViewSystemDebug", &m_nViewSystemDebug, 0, VF_CHEAT,
	               "Sets Debug information of the ViewSystem.");

	REGISTER_CVAR2("cl_DefaultNearPlane", &m_fDefaultCameraNearZ, DEFAULT_NEAR, 0,
		"The default camera near plane. ");

	REGISTER_CVAR2("cl_ViewApplyHmdOffset", &m_bApplyHmdOffset, 1, 0,
		"Enables default engine HMD positional / rotational offset");

	//Register as level system listener
	if (CCryAction::GetCryAction()->GetILevelSystem())
		CCryAction::GetCryAction()->GetILevelSystem()->AddListener(this);
}

//------------------------------------------------------------------------
CViewSystem::~CViewSystem()
{
	ClearAllViews();

	IConsole* pConsole = gEnv->pConsole;
	CRY_ASSERT(pConsole);
	pConsole->UnregisterVariable("cl_camera_noise", true);
	pConsole->UnregisterVariable("cl_camera_noise_freq", true);
	pConsole->UnregisterVariable("cl_ViewSystemDebug", true);
	pConsole->UnregisterVariable("cl_DefaultNearPlane", true);

	//Remove as level system listener
	if (CCryAction::GetCryAction()->GetILevelSystem())
		CCryAction::GetCryAction()->GetILevelSystem()->RemoveListener(this);
}

//------------------------------------------------------------------------
void CViewSystem::Update(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (gEnv->IsDedicated())
		return;

	CView* const __restrict pActiveView = static_cast<CView*>(GetActiveView());

	for (auto const& view : m_views)
	{
		CView* const __restrict pView = view.second;

		bool const bIsActive = (pView == pActiveView);

		pView->Update(frameTime, bIsActive);

		if (bIsActive)
		{
			CCamera& camera = pView->GetCamera();
			SViewParams currentParams = *(pView->GetCurrentParams());

			camera.SetJustActivated(currentParams.justActivated);

			currentParams.justActivated = false;
			pView->SetCurrentParams(currentParams);

			if (m_bOverridenCameraRotation)
			{
				// When camera rotation is overridden.
				Vec3 pos = camera.GetMatrix().GetTranslation();
				Matrix34 camTM(m_overridenCameraRotation);
				camTM.SetTranslation(pos);
				camera.SetMatrix(camTM);
			}
			else
			{
				// Normal setting of the camera

				if (m_fCameraNoise > 0)
				{
					Matrix33 m = Matrix33(camera.GetMatrix());
					m.OrthonormalizeFast();
					Ang3 aAng1 = Ang3::GetAnglesXYZ(m);
					//Ang3 aAng2 = RAD2DEG(aAng1);

					Matrix34 camTM = camera.GetMatrix();
					Vec3 pos = camTM.GetTranslation();
					camTM.SetIdentity();

					const float fScale = 0.1f;
					CPNoise3* pNoise = m_pSystem->GetNoiseGen();
					float fRes = pNoise->Noise1D(gEnv->pTimer->GetCurrTime() * m_fCameraNoiseFrequency);
					aAng1.x += fRes * m_fCameraNoise * fScale;
					pos.z -= fRes * m_fCameraNoise * fScale;
					fRes = pNoise->Noise1D(17 + gEnv->pTimer->GetCurrTime() * m_fCameraNoiseFrequency);
					aAng1.y -= fRes * m_fCameraNoise * fScale;

					//aAng1.z+=fRes*0.025f; // left / right movement should be much less visible

					camTM.SetRotationXYZ(aAng1);
					camTM.SetTranslation(pos);
					camera.SetMatrix(camTM);
				}
			}

			m_pSystem->SetViewCamera(camera);
		}
	}

	// Display debug info on screen
	if (m_nViewSystemDebug)
	{
		DebugDraw();
	}

	// perform dynamic color grading
	// Beni - Commented for console demo stuff
	/******************************************************************************************
	   if (!IsPlayingCutScene() && gEnv->pAISystem)
	   {
	   SAIDetectionLevels detection;
	   gEnv->pAISystem->GetDetectionLevels(nullptr, detection);
	   float factor = detection.puppetThreat;

	   // marcok: magic numbers taken from Maciej's tweaked values (final - initial)
	   {
	    float percentage = (0.0851411f - 0.0509998f)/0.0509998f * factor;
	    float amount = 0.0f;
	    gEnv->p3DEngine->GetPostEffectParam("ColorGrading_GrainAmount", amount);
	    gEnv->p3DEngine->SetPostEffectParam("ColorGrading_GrainAmount_Offset", amount*percentage);
	   }
	   {
	    float percentage = (0.405521f - 0.256739f)/0.256739f * factor;
	    float amount = 0.0f;
	    gEnv->p3DEngine->GetPostEffectParam("ColorGrading_SharpenAmount", amount);
	    //gEnv->p3DEngine->SetPostEffectParam("ColorGrading_SharpenAmount_Offset", amount*percentage);
	   }
	   {
	    float percentage = (0.14f - 0.11f)/0.11f * factor;
	    float amount = 0.0f;
	    gEnv->p3DEngine->GetPostEffectParam("ColorGrading_PhotoFilterColorDensity", amount);
	    gEnv->p3DEngine->SetPostEffectParam("ColorGrading_PhotoFilterColorDensity_Offset", amount*percentage);
	   }
	   {
	    float percentage = (234.984f - 244.983f)/244.983f * factor;
	    float amount = 0.0f;
	    gEnv->p3DEngine->GetPostEffectParam("ColorGrading_maxInput", amount);
	    //gEnv->p3DEngine->SetPostEffectParam("ColorGrading_maxInput_Offset", amount*percentage);
	   }
	   {
	    float percentage = (239.984f - 247.209f)/247.209f * factor;
	    float amount = 0.0f;
	    gEnv->p3DEngine->GetPostEffectParam("ColorGrading_maxOutput", amount);
	    //gEnv->p3DEngine->SetPostEffectParam("ColorGrading_maxOutput_Offset", amount*percentage);
	   }
	   {
	    Vec4 dest(0.0f/255.0f, 22.0f/255.0f, 33.0f/255.0f, 1.0f);
	    Vec4 initial(2.0f/255.0f, 154.0f/255.0f, 226.0f/255.0f, 1.0f);
	    Vec4 percentage = (dest - initial)/initial * factor;
	    Vec4 amount;
	    gEnv->p3DEngine->GetPostEffectParamVec4("clr_ColorGrading_SelectiveColor", amount);
	    //gEnv->p3DEngine->SetPostEffectParamVec4("clr_ColorGrading_SelectiveColor_Offset", amount*percentage);
	   }
	   {
	    float percentage = (-5.0083 - -1.99999)/-1.99999 * factor;
	    float amount = 0.0f;
	    gEnv->p3DEngine->GetPostEffectParam("ColorGrading_SelectiveColorCyans", amount);
	    //gEnv->p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorCyans_Offset", amount*percentage);
	   }
	   {
	    float percentage = (10.0166 - -5.99999)/-5.99999 * factor;
	    float amount = 0.0f;
	    gEnv->p3DEngine->GetPostEffectParam("ColorGrading_SelectiveColorMagentas", amount);
	    //gEnv->p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorMagentas_Offset", amount*percentage);
	   }
	   {
	    float percentage = (10.0166 - 1.99999)/1.99999 * factor;
	    float amount = 0.0f;
	    gEnv->p3DEngine->GetPostEffectParam("ColorGrading_SelectiveColorYellows", amount);
	    //gEnv->p3DEngine->SetPostEffectParam("ColorGrading_SelectiveColorYellows_Offset", amount*percentage);
	   }
	   }
	 ********************************************************************************************/
}

//------------------------------------------------------------------------
IView* CViewSystem::CreateView()
{
	CView* newView = new CView(m_pSystem);

	if (newView)
	{
		m_views.insert(TViewMap::value_type(m_nextViewIdToAssign, newView));
		if (IsPlayingCutScene())
		{
			m_cutsceneViewIdVector.push_back(m_nextViewIdToAssign);
		}
		++m_nextViewIdToAssign;
	}

	return newView;
}

void CViewSystem::RemoveView(IView* pView)
{
	RemoveViewById(GetViewId(pView));
}

void CViewSystem::RemoveView(unsigned int viewId)
{
	RemoveViewById(viewId);
}

void CViewSystem::RemoveViewById(unsigned int viewId)
{
	TViewMap::iterator iter = m_views.find(viewId);

	if (iter != m_views.end())
	{
		SAFE_RELEASE(iter->second);
		m_views.erase(iter);
	}
}

//------------------------------------------------------------------------
void CViewSystem::SetActiveView(IView* pView)
{
	if (pView != nullptr)
	{
		IView* const __restrict pPrevView = GetView(m_activeViewId);

		if (pPrevView != pView)
		{
			if (pPrevView != nullptr)
			{
				pPrevView->SetActive(false);
			}

			pView->SetActive(true);
			m_activeViewId = GetViewId(pView);
		}
	}
	else
	{
		m_activeViewId = ~0;
	}

	m_bActiveViewFromSequence = false;
}

//------------------------------------------------------------------------
void CViewSystem::SetActiveView(unsigned int viewId)
{
	IView* const __restrict pPrevView = GetView(m_activeViewId);

	if (pPrevView != nullptr)
	{
		pPrevView->SetActive(false);
	}

	IView* const __restrict pView = GetView(viewId);

	if (pView != nullptr)
	{
		pView->SetActive(true);
		m_activeViewId = viewId;
		m_bActiveViewFromSequence = false;
	}
}

//------------------------------------------------------------------------
IView* CViewSystem::GetView(unsigned int viewId) const
{
	TViewMap::const_iterator it = m_views.find(viewId);

	if (it != m_views.end())
	{
		return it->second;
	}

	return nullptr;
}

//------------------------------------------------------------------------
IView* CViewSystem::GetActiveView() const
{
	return GetView(m_activeViewId);
}

//------------------------------------------------------------------------
bool CViewSystem::IsClientActorViewActive() const
{
	IView* pActiveView = GetActiveView();
	if (!pActiveView)
	{
		return true;
	}
	return pActiveView->GetLinkedId() == CCryAction::GetCryAction()->GetClientActorId();
}

//------------------------------------------------------------------------
unsigned int CViewSystem::GetViewId(IView* pView) const
{
	for (auto const& view : m_views)
	{
		IView* tView = view.second;

		if (tView == pView)
			return view.first;
	}

	return 0;
}

//------------------------------------------------------------------------
unsigned int CViewSystem::GetActiveViewId() const
{
	// cutscene can override the games id of the active view
	if (m_cutsceneCount && m_cutsceneViewId)
		return m_cutsceneViewId;
	return m_activeViewId;
}

//------------------------------------------------------------------------
IView* CViewSystem::GetViewByEntityId(EntityId id, bool forceCreate)
{
	for (auto const& view : m_views)
	{
		IView* const pIView = view.second;

		if (pIView != nullptr && pIView->GetLinkedId() == id)
		{
			return pIView;
		}
	}

	if (forceCreate)
	{
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
		{
			if (CGameObject* pGameObject = static_cast<CGameObject*>(pEntity->GetProxy(ENTITY_PROXY_USER)))
			{
				if (IView* pNew = CreateView())
				{
					pNew->LinkTo(pGameObject);
					return pNew;
				}
			}
			else
			{
				if (IView* pNew = CreateView())
				{
					pNew->LinkTo(pEntity);
					return pNew;
				}
			}
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------
void CViewSystem::SetActiveCamera(const SCameraParams& params)
{
	IView* pView = nullptr;
	if (params.cameraEntityId)
	{
		pView = GetViewByEntityId(params.cameraEntityId, true);
		if (pView)
		{
			SViewParams viewParams = *pView->GetCurrentParams();
			viewParams.fov = params.fFOV;
			viewParams.nearplane = params.fNearZ;

			//char szDebug[256];
			//cry_sprintf(szDebug,"pos=%0.2f,%0.2f,%0.2f,rot=%0.2f,%0.2f,%0.2f\n",viewParams.position.x,viewParams.position.y,viewParams.position.z,viewParams.rotation.v.x,viewParams.rotation.v.y,viewParams.rotation.v.z,viewParams.rotation.w);
			//OutputDebugString(szDebug);

			if (m_bActiveViewFromSequence == false && m_preSequenceViewId == 0)
			{
				m_preSequenceViewId = m_activeViewId;
				IView* pPrevView = GetView(m_activeViewId);
				if (pPrevView && m_fBlendInPosSpeed > 0.0f && m_fBlendInRotSpeed > 0.0f)
				{
					viewParams.blendPosSpeed = m_fBlendInPosSpeed;
					viewParams.blendRotSpeed = m_fBlendInRotSpeed;
					viewParams.BlendFrom(*pPrevView->GetCurrentParams());
				}
			}

			if (m_activeViewId != GetViewId(pView) && params.justActivated)
			{
				viewParams.justActivated = true;
			}

			pView->SetCurrentParams(viewParams);
			// make this one the active view
			SetActiveView(pView);
			m_bActiveViewFromSequence = true;
		}
	}
	else
	{
		if (m_preSequenceViewId != 0)
		{
			IView* pActiveView = GetView(m_activeViewId);
			IView* pNewView = GetView(m_preSequenceViewId);
			if (pActiveView && pNewView && m_bPerformBlendOut)
			{
				SViewParams activeViewParams = *pActiveView->GetCurrentParams();
				SViewParams newViewParams = *pNewView->GetCurrentParams();
				newViewParams.BlendFrom(activeViewParams);
				newViewParams.blendPosSpeed = activeViewParams.blendPosSpeed;
				newViewParams.blendRotSpeed = activeViewParams.blendRotSpeed;

				if (m_activeViewId != m_preSequenceViewId && params.justActivated)
				{
					newViewParams.justActivated = true;
				}

				pNewView->SetCurrentParams(newViewParams);
				SetActiveView(m_preSequenceViewId);

			}
			else if (pActiveView && m_activeViewId != m_preSequenceViewId && params.justActivated)
			{
				SViewParams activeViewParams = *pActiveView->GetCurrentParams();
				activeViewParams.justActivated = true;
				if (m_nViewSystemDebug)
				{
					GameWarning("CViewSystem::SetActiveCamera(): %d", m_preSequenceViewId);
					if (pNewView == nullptr)
						GameWarning("CViewSystem::SetActiveCamera(): Switching to pre-sequence view failed!");
					else
						GameWarning("CViewSystem::SetActiveCamera(): Switching to pre-sequence view OK!");
				}

				if (pNewView)
				{
					pNewView->SetCurrentParams(activeViewParams);
					SetActiveView(m_preSequenceViewId);
				}

			}

			m_preSequenceViewId = 0;
			m_bActiveViewFromSequence = false;
		}
	}
	m_cutsceneViewId = GetViewId(pView);

	VS_CALL_LISTENERS(OnCameraChange(params));
}

//------------------------------------------------------------------------
void CViewSystem::BeginCutScene(IAnimSequence* pSeq, unsigned long dwFlags, bool bResetFX)
{
	m_cutsceneCount++;

	if (m_cutsceneCount == 1)
	{
		gEnv->p3DEngine->ResetPostEffects();
	}

	VS_CALL_LISTENERS(OnBeginCutScene(pSeq, bResetFX));

	/* TODO: how do we pause the game
	   IScriptSystem *pSS=m_pGame->GetScriptSystem();
	   _SmartScriptObject pClientStuff(pSS,true);
	   if(pSS->GetGlobalValue("ClientStuff",pClientStuff)){
	    pSS->BeginCall("ClientStuff","OnPauseGame");
	    pSS->PushFuncParam(pClientStuff);
	    pSS->EndCall();
	   }
	 */

	/* TODO: how do we block keys
	   // do not allow the player to mess around with player's keys
	   // during a cutscene
	   GetISystem()->GetIInput()->GetIKeyboard()->ClearKeyState();
	   m_pGame->m_pIActionMapManager->SetActionMap("player_dead");
	   m_pGame->AllowQuicksave(false);
	 */

	/* TODO: how do we reset FX?
	   if (bResetFx)
	   {
	    m_pGame->m_p3DEngine->ResetScreenFx();
	    ICVar *pResetScreenEffects=pCon->GetCVar("r_ResetScreenFx");
	    if(pResetScreenEffects)
	    {
	    pResetScreenEffects->Set(1);
	    }
	   }
	 */

	//  if(gEnv->p3DEngine)
	//  gEnv->p3DEngine->ProposeContentPrecache();
}

//------------------------------------------------------------------------
void CViewSystem::EndCutScene(IAnimSequence* pSeq, unsigned long dwFlags)
{
	m_cutsceneCount -= (m_cutsceneCount > 0);

	if (m_cutsceneCount == 0)
	{
		gEnv->p3DEngine->ResetPostEffects();
	}

	ClearCutsceneViews();

	VS_CALL_LISTENERS(OnEndCutScene(pSeq));

	// TODO: reimplement
	//	m_pGame->AllowQuicksave(true);

	/* TODO: how to resume game
	   if (!m_pGame->IsServer())
	   {
	    IScriptSystem *pSS=m_pGame->GetScriptSystem();
	    _SmartScriptObject pClientStuff(pSS,true);
	    if(pSS->GetGlobalValue("ClientStuff",pClientStuff)){
	      pSS->BeginCall("ClientStuff","OnResumeGame");
	      pSS->PushFuncParam(pClientStuff);
	      pSS->EndCall();
	    }
	   }
	 */

	/* TODO: resolve input difficulties
	   m_pGame->m_pIActionMapManager->SetActionMap("default");
	   GetISystem()->GetIInput()->GetIKeyboard()->ClearKeyState();
	 */

	/* TODO: weird game related stuff
	   // we regenerate stamina fpr the local payer on cutsceen end - supposendly he was idle long enough to get it restored
	   if (m_pGame->GetMyPlayer())
	   {
	    CPlayer *pPlayer;
	    if (m_pGame->GetMyPlayer()->GetContainer()->QueryContainerInterface(CIT_IPLAYER,(void**)&pPlayer))
	    {
	      pPlayer->m_stats.stamina = 100;
	    }
	   }

	   // reset subtitles
	   m_pGame->m_pClient->ResetSubtitles();
	 */
}

void CViewSystem::SendGlobalEvent(const char* pszEvent)
{
	// TODO: broadcast to flowgraph/script system
}

//////////////////////////////////////////////////////////////////////////
void CViewSystem::SetOverrideCameraRotation(bool bOverride, Quat rotation)
{
	m_bOverridenCameraRotation = bOverride;
	m_overridenCameraRotation = rotation;
}

//////////////////////////////////////////////////////////////////
void CViewSystem::OnLoadingStart(ILevelInfo* pLevel)
{
	//If the level is being restarted (IsSerializingFile() == 1)
	//views should not be cleared, because the main view (player one) won't be recreated in this case
	//Views will only be cleared when loading a new map, or loading a saved game (IsSerizlizingFile() == 2)
	bool shouldClearViews = gEnv->pSystem ? (gEnv->pSystem->IsSerializingFile() != 1) : false;

	if (shouldClearViews)
		ClearAllViews();
}

/////////////////////////////////////////////////////////////////////
void CViewSystem::OnUnloadComplete(ILevelInfo* pLevel)
{
	bool shouldClearViews = gEnv->pSystem ? (gEnv->pSystem->IsSerializingFile() != 1) : false;

	if (shouldClearViews)
		ClearAllViews();

	assert(m_listeners.empty());
	stl::free_container(m_listeners);
}

/////////////////////////////////////////////////////////////////////
void CViewSystem::ClearCutsceneViews()
{
	//First switch to previous camera if available
	//In practice, the camera should be already restored before reaching this point, but just in case.
	if (m_preSequenceViewId != 0)
	{
		SCameraParams camParams;
		camParams.cameraEntityId = 0; //Setting to 0, will try to switch to previous camera
		camParams.fFOV = 60.0f;
		camParams.fNearZ = DEFAULT_NEAR;
		camParams.justActivated = true;
		SetActiveCamera(camParams);
	}

	//Delete views created during the cut-scene
	const int count = m_cutsceneViewIdVector.size();
	for (int i = 0; i < count; ++i)
	{
		TViewMap::iterator it = m_views.find(m_cutsceneViewIdVector[i]);
		if (it != m_views.end())
		{
			it->second->Release();
			if (it->first == m_activeViewId)
				m_activeViewId = 0;
			if (it->first == m_preSequenceViewId)
				m_preSequenceViewId = 0;
			m_views.erase(it);
		}
	}
	stl::free_container(m_cutsceneViewIdVector);
}

///////////////////////////////////////////
void CViewSystem::ClearAllViews()
{
	TViewMap::iterator end = m_views.end();
	for (TViewMap::iterator it = m_views.begin(); it != end; ++it)
	{
		SAFE_RELEASE(it->second);
	}
	stl::free_container(m_views);
	stl::free_container(m_cutsceneViewIdVector);
	m_preSequenceViewId = 0;
	m_activeViewId = 0;
}

////////////////////////////////////////////////////////////////////
void CViewSystem::DebugDraw()
{
	float xpos = 20;
	float ypos = 15;
	float fColor[4] = { 1.0f, 1.0f, 1.0f, 0.7f };
	float fColorRed[4] = { 1.0f, 0.0f, 0.0f, 0.7f };
	//		float fColorYellow[4]	={1.0f, 1.0f, 0.0f, 0.7f};
	float fColorGreen[4] = { 0.0f, 1.0f, 0.0f, 0.7f };
	//		float fColorOrange[4]	={1.0f, 0.5f, 0.0f, 0.7f};
	//		float fColorBlue[4]		={0.4f, 0.4f, 7.0f, 0.7f};

	IRenderAuxText::Draw2dLabel(xpos, 5, 1.35f, fColor, false, "ViewSystem Stats: %" PRISIZE_T " Views ", m_views.size());

	IView* pActiveView = GetActiveView();
	for (auto const& view : m_views)
	{
		CView* pView = view.second;
		const CCamera& cam = pView->GetCamera();
		bool isActive = (pView == pActiveView);
		bool cutSceneCamera = false;
		for (auto const i : m_cutsceneViewIdVector)
		{
			if (i == view.first)
			{
				cutSceneCamera = true;
				break;
			}
		}

		Vec3 pos = cam.GetPosition();
		Ang3 ang = cam.GetAngles();
		if (!cutSceneCamera)
			IRenderAuxText::Draw2dLabel(xpos, ypos, 1.35f, isActive ? fColorGreen : fColorRed, false, "View Camera: %p . View Id: %d, pos (%f, %f, %f), ang (%f, %f, %f)", &cam, view.first, pos.x, pos.y, pos.z, ang.x, ang.y, ang.z);
		else
			IRenderAuxText::Draw2dLabel(xpos, ypos, 1.35f, isActive ? fColorGreen : fColorRed, false, "View Camera: %p . View Id: %d, pos (%f, %f, %f), ang (%f, %f, %f) - Created during Cut-Scene", &cam, view.first, pos.x, pos.y, pos.z, ang.x, ang.y, ang.z);

		ypos += 11;
	}
}

//////////////////////////////////////////////////////////////////////////
void CViewSystem::GetMemoryUsage(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "ViewSystem");
	s->Add(*this);
	s->AddObject(m_views);
}

//////////////////////////////////////////////////////////////////////////
void CViewSystem::Serialize(TSerialize ser)
{
	for (auto const& view : m_views)
	{
		view.second->Serialize(ser);
	}
}

//////////////////////////////////////////////////////////////////////////
void CViewSystem::PostSerialize()
{
	for (auto const& view : m_views)
	{
		view.second->PostSerialize();
	}
}

///////////////////////////////////////////////////////////////////////////
void CViewSystem::SetControlAudioListeners(bool bActive)
{
	for (auto const& view : m_views)
	{
		view.second->SetActive(bActive);
	}
}

//////////////////////////////////////////////////////////////////////////
void CViewSystem::UpdateAudioListeners()
{
  CView* const __restrict pActiveView = static_cast<CView*>(GetActiveView());

  for (auto const& view : m_views)
  {
    CView* const __restrict pView = view.second;
    bool const bIsActive = (pView == pActiveView);
    CCamera const& camera = bIsActive ? gEnv->pSystem->GetViewCamera() : pView->GetCamera();
    pView->UpdateAudioListener(camera.GetMatrix());
  }
}
