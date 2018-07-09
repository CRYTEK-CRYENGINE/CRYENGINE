// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
- 31:05:2010  Created by Jens Schöbel

*************************************************************************/

#include "StdAfx.h"
#include "Stereo3D/StereoFramework.h"
#include "Player.h"

#include <CryRenderer/IStereoRenderer.h>

namespace
{
  Stereo3D::Zoom::CStereoZoom s_stereoZoomer;
  Stereo3D::Weapon::CWeaponCheck s_weaponCheck;
} // END OF ANONYMOUS NAMESPACE

namespace Stereo3D
{
  ICVar *s_cvScreenDist   = NULL;
  int   s_stereoFrameworkState;
  float s_defaultStereoDistance;


  void Update(float deltaTime)
  {
		IStereoRenderer* pStereoRenderer = gEnv->pRenderer->GetIStereoRenderer();
	  const bool stereoFrameWorkEnabled = (pStereoRenderer != NULL) && (pStereoRenderer->GetStereoEnabled()) && g_pGameCVars->g_stereoFrameworkEnable;
    if ( stereoFrameWorkEnabled == false )
    {
      s_cvScreenDist = NULL;
      return;
    }

    if ( !s_cvScreenDist )
    {
      s_cvScreenDist = gEnv->pConsole->GetCVar( "r_StereoScreenDist" );
      if ( !s_cvScreenDist )
        return;
      s_defaultStereoDistance = s_cvScreenDist->GetFVal();
    }

    s_stereoZoomer.Update(deltaTime);
    s_weaponCheck.Update(deltaTime);

    float zoomDist = s_defaultStereoDistance;
    if ( s_stereoZoomer.IsPlaneZooming() ) {
       zoomDist = s_stereoZoomer.GetCurrentPlaneDist();
    }
    s_cvScreenDist->Set( min(zoomDist, s_weaponCheck.GetCurrentPlaneDist()) );

#ifndef _RELEASE
    if ( g_pGameCVars->g_stereoFrameworkEnable == 2 ) {
      IRenderAuxText::Draw2dLabel(5.0f,  45.f, 1.5f, NULL, false, "cvar      : %f", s_cvScreenDist->GetFVal() );
      IRenderAuxText::Draw2dLabel(5.0f,  60.f, 1.5f, NULL, false, "zoomdist  : %f", zoomDist);
      IRenderAuxText::Draw2dLabel(5.0f,  75.f, 1.5f, NULL, false, "weaponDist: %f", s_weaponCheck.GetCurrentPlaneDist() );
      IRenderAuxText::Draw2dLabel(5.0f,  90.f, 1.5f, NULL, false, "PlaneDist : %f", min(zoomDist, s_weaponCheck.GetCurrentPlaneDist()));
      IRenderAuxText::Draw2dLabel(5.0f,  105.f, 1.5f, NULL, false, "enable   : %i", g_pGameCVars->g_stereoFrameworkEnable );
    }
#endif

  }

  namespace Zoom
  {
    //////////////////////////////////////////////////////////////////////////
    void SetFinalPlaneDist(float planeDist, float transitionTime)
    {
      s_stereoZoomer.SetPlaneDistAndTransitionTime(planeDist, transitionTime);
    }

    //////////////////////////////////////////////////////////////////////////
    void SetFinalEyeDist(float eyeDist, float transitionTime)
    {
      s_stereoZoomer.SetEyeDistAndTransitionTime(eyeDist, transitionTime);
    }

    //////////////////////////////////////////////////////////////////////////
    void ReturnToNormalSetting(float transitionTime)
    {
      s_stereoZoomer.ReturnEyeToNormalSetting(transitionTime);
      s_stereoZoomer.ReturnPlaneToNormalSetting(transitionTime);
    }
  } // namespace Zoom

  namespace Weapon
  {
    //////////////////////////////////////////////////////////////////////////
    CWeaponCheck::~CWeaponCheck() {
      if ( !g_pGame )
        return;

      for ( int i = 0; i < Stereo3D::Weapon::MAX_RAY_IDS; ++i )
      {
        if ( m_rayIDs[i] != Stereo3D::Weapon::INVALID_RAY_ID )
          g_pGame->GetRayCaster().Cancel(m_rayIDs[i]);
      }
    }

    //////////////////////////////////////////////////////////////////////////
    float CWeaponCheck::GetCurrentPlaneDist()
    {
      float distance = s_defaultStereoDistance;
      if ( 0.00001f < m_closestDist  && m_closestDist < 1.f ) {
        float t = m_closestDist;
        t = (t > 0.5f) ? (t * 0.5f) : (0);
        distance = s_defaultStereoDistance * t + (1-t) * 0.05f;
      }
      float curScreenDist = s_cvScreenDist->GetFVal();
      return (curScreenDist * 0.85f + distance * 0.15f);
    }

    //////////////////////////////////////////////////////////////////////////
    void CWeaponCheck::Update(float deltaTime)
    {
      if ( !s_cvScreenDist )
        return;

      if (m_numResults > 4 || m_numFramesWaiting++ > 19)
        CastRays();
    }

    //////////////////////////////////////////////////////////////////////////
    void CWeaponCheck::CastRays()
    {
      IActor* pActor = gEnv->pGameFramework->GetClientActor();
      CPlayer* pPlayer = pActor ? static_cast<CPlayer*>(pActor) : NULL;
      if( !pPlayer ) {
        m_closestDist = 10000.f;
        return;
      }

      m_closestDist = m_closestCastDist;
      m_closestCastDist = 10000.f;
      m_numFramesWaiting = m_numResults = 0;

      const CCamera& camera = GetISystem()->GetViewCamera();
      Vec3 camPos = camera.GetPosition();
      Ang3 angles = camera.GetAngles();
      Vec3 rayDirs[Stereo3D::Weapon::MAX_RAY_IDS] = {
        GetISystem()->GetViewCamera().GetViewdir(),
        Quat::CreateRotationXYZ(Ang3(angles.y - camera.GetFov() * 0.25f,angles.z - camera.GetHorizontalFov() * 0.25f,angles.x)).GetColumn1(),
        Quat::CreateRotationXYZ(Ang3(angles.y,angles.z + camera.GetHorizontalFov() * 0.25f,angles.x)).GetColumn1(),
        Quat::CreateRotationXYZ(Ang3(angles.y,angles.z - camera.GetHorizontalFov() * 0.25f,angles.x)).GetColumn1(),
        Quat::CreateRotationXYZ(Ang3(angles.y - camera.GetFov() * 0.25f,angles.z + camera.GetHorizontalFov() * 0.25f,angles.x)).GetColumn1(),
      };
      IPhysicalEntity *pTargetPhysics = pPlayer->GetEntity()->GetPhysics();
      for ( int i = 0; i < Stereo3D::Weapon::MAX_RAY_IDS; ++i )
      {
        if ( m_rayIDs[i] != Stereo3D::Weapon::INVALID_RAY_ID )
        {
          g_pGame->GetRayCaster().Cancel(m_rayIDs[i]);
        }

        m_rayIDs[i] =
          g_pGame->GetRayCaster().Queue(RayCastRequest::MediumPriority,
          RayCastRequest(camPos, rayDirs[i] * 3,
          ent_all, rwi_stop_at_pierceable|rwi_ignore_back_faces,
          pTargetPhysics ? &pTargetPhysics : 0,
          pTargetPhysics ? 1 : 0),
          functor(*this, &CWeaponCheck::OnRayCastResult));
      }
    }

    //////////////////////////////////////////////////////////////////////////
    void CWeaponCheck::OnRayCastResult(const QueuedRayID &rayID, const RayCastResult &result)
    {
      ++m_numResults;

      for ( int i = 0; i < Stereo3D::Weapon::MAX_RAY_IDS; ++i )
      {
        if ( m_rayIDs[i] == rayID )
        {
          m_rayIDs[i] = Stereo3D::Weapon::INVALID_RAY_ID;
          break;
        }
      }

      if ( result.hitCount > 0 )
      {
        m_closestCastDist = min( result->dist, m_closestCastDist );
      }
    }

  } // namespace Weapon
} // namespace Stere3D
