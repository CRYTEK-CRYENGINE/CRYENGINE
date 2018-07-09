// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
- 28:05:2010  Created by Jens Schöbel

*************************************************************************/

#include "StdAfx.h"

#include "GameXmlParamReader.h"
#include "Stereo3D/StereoZoom.h"
#include <CryRenderer/IRenderAuxGeom.h>

#define DEFAULT_ZOOM_EYE_DISTANCE         0.f   // default stereo off
#define DEFAULT_ZOOM_CONVERGENCE_DISTANCE 0.01f

using namespace Stereo3D::Zoom;

//////////////////////////////////////////////////////////////////////////
void Parameters::Reset(const XmlNodeRef& paramsNode, bool defaultInit /*true*/)
{
  if (defaultInit)
  {
    eyeDistance         = DEFAULT_ZOOM_EYE_DISTANCE;
    convergenceDistance = DEFAULT_ZOOM_CONVERGENCE_DISTANCE;
  }

  if (paramsNode)
  {
    CGameXmlParamReader reader(paramsNode);

    reader.ReadParamValue<float>("eyeDistance", eyeDistance);
    reader.ReadParamValue<float>("convergenceDistance", convergenceDistance);
  }
}

//////////////////////////////////////////////////////////////////////////
void Parameters::GetMemoryUsage(ICrySizer * s) const
{
  s->AddObject(eyeDistance);
  s->AddObject(convergenceDistance);
}

//////////////////////////////////////////////////////////////////////////
CStereoZoom::CStereoZoom()
: m_isPlaneZooming(false)
, m_isPlaneReturningToNormal(false)
, m_isEyeZooming(false)
, m_isEyeReturningToNormal(false)
{}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::RememberCurrentEyeDistance()
{
  ICVar* pICVar = gEnv->pConsole->GetCVar("r_StereoEyeDist");
  if ( pICVar != NULL)
  {
    m_distanceOfEyesBeforeChanging = pICVar->GetFVal();
  }
  else
  {
    m_distanceOfEyesBeforeChanging = 0.06f;
  }
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::RememberCurrentPlaneDistance()
{
  ICVar* pICVar = gEnv->pConsole->GetCVar("r_StereoScreenDist");
  if ( pICVar != NULL)
  {
    m_distanceOfPlaneBeforeChanging = pICVar->GetFVal();
  }
  else
  {
    m_distanceOfPlaneBeforeChanging = 10.f;
  }
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::UpdateStereoScreenDistance()
{
  //SetStereoPlaneDistCVAR(GetCurrentPlaneDist());
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::UpdateStereoEyeDistance()
{
  SetStereoEyeDistCVAR(GetCurrentEyeDist());
}


//////////////////////////////////////////////////////////////////////////
bool CStereoZoom::IsStereoEnabled() const
{
	ICVar* pICVar = gEnv->pConsole->GetCVar("r_StereoMode");
	if ( pICVar != NULL)
	{
		return pICVar->GetIVal() != 0;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CStereoZoom::IsPlaneZooming() const
{
  return m_isPlaneZooming;
}

//////////////////////////////////////////////////////////////////////////
bool CStereoZoom::IsEyeZooming() const
{
  return m_isEyeZooming;
}

//////////////////////////////////////////////////////////////////////////
bool CStereoZoom::IsPlaneTransitionFinished() const
{
  return m_timeOfPlaneCurrentlyPast > m_timeOfPlaneWhenTransitionFinishes;
}

//////////////////////////////////////////////////////////////////////////
bool CStereoZoom::IsEyeTransitionFinished() const
{
  return m_timeOfEyesCurrentlyPast > m_timeOfEyesWhenTransitionFinishes;
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::Update(float deltaTime)
{
	if ( !IsStereoEnabled() )
		return;

	UpdatePlaneZooming(deltaTime);
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::PrintDebugOutput()
{

  ICVar* pICVar = gEnv->pConsole->GetCVar("r_StereoScreenDist");
  float dist;
  if ( pICVar != NULL)
  {
    dist = pICVar->GetFVal();
  }
  else
  {
    dist = -10.f;
  }

  pICVar = gEnv->pConsole->GetCVar("r_StereoEyeDist");
  float eyedist;
  if ( pICVar != NULL)
  {
    eyedist = pICVar->GetFVal();
  }
  else
  {
    eyedist = -10.f;
  }


  static float color[4] = {1,1,1,1};
  float y=50.f, step1=15.f, step2=20.f, size1=1.3f, size2=1.5f;

  IRenderAuxText::Draw2dLabel(5.0f,  y         , size2, color, false, "Current PlaneDist: %f", dist);
  IRenderAuxText::Draw2dLabel(5.0f,  y += step1, size2, color, false, "Current EyeDist  : %f", eyedist);
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::SetStereoEyeDistCVAR(float eyeDist)
{
  ICVar* pICVar = gEnv->pConsole->GetCVar("r_StereoEyeDist");
  if ( pICVar != NULL)
  {
    pICVar->Set(eyeDist);
  }
}
 
//////////////////////////////////////////////////////////////////////////
void CStereoZoom::SetStereoPlaneDistCVAR(float planeDist)
{
  ICVar* pICVar = gEnv->pConsole->GetCVar("r_StereoScreenDist");
  if ( pICVar != NULL)
  {
    pICVar->Set(planeDist);
  }
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::SetEyeDistAndTransitionTime(float eyeDist, float transitionTime)
{
  if ( !IsEyeZooming() )
  {
    RememberCurrentEyeDistance();
    m_distanceOfEyesAtTransitionStart  = m_distanceOfEyesBeforeChanging;
  }
  else
  {
    m_distanceOfEyesAtTransitionStart  = GetCurrentEyeDist();
  }

  m_timeOfEyesCurrentlyPast          = 0;
  m_timeOfEyesWhenTransitionFinishes = transitionTime;
  m_distanceOfEyesAtTransitionEnd    = eyeDist;
  m_isEyeZooming                     = true;
  m_isEyeReturningToNormal          = false;
}

//////////////////////////////////////////////////////////////////////////
float CStereoZoom::GetCurrentClampedEyeTValue(void) const
{
  if ( m_timeOfEyesWhenTransitionFinishes > 0.0001f )
    return CLAMP(m_timeOfEyesCurrentlyPast / m_timeOfEyesWhenTransitionFinishes, 0, 1);
  else
    return 1.f;
}

//////////////////////////////////////////////////////////////////////////
float CStereoZoom::GetCurrentEyeDist(void) const
{
  return GetCurrentClampedEyeTValue()
    * (m_distanceOfEyesAtTransitionEnd - m_distanceOfEyesAtTransitionStart)
    + m_distanceOfEyesAtTransitionStart;
}

//////////////////////////////////////////////////////////////////////////
float CStereoZoom::GetCurrentClampedPlaneTValue(void) const
{
  if ( m_timeOfPlaneWhenTransitionFinishes > 0.0001f )
    return CLAMP(m_timeOfPlaneCurrentlyPast / m_timeOfPlaneWhenTransitionFinishes, 0, 1);
  else
    return 1.f;
}

//////////////////////////////////////////////////////////////////////////
float CStereoZoom::GetCurrentPlaneDist(void) const
{
  return GetCurrentClampedPlaneTValue()
         * (m_distanceOfPlaneAtTransitionEnd - m_distanceOfPlaneAtTransitionStart)
         + m_distanceOfPlaneAtTransitionStart;
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::SetPlaneDistAndTransitionTime(float planeDist, float transitionTime)
{
  if ( !IsPlaneZooming() )
  {
    RememberCurrentPlaneDistance();
    m_distanceOfPlaneAtTransitionStart  = m_distanceOfPlaneBeforeChanging;
  }
  else
  {
    m_distanceOfPlaneAtTransitionStart  = GetCurrentPlaneDist();
  }

  m_timeOfPlaneCurrentlyPast          = 0;
  m_timeOfPlaneWhenTransitionFinishes = transitionTime;
  m_distanceOfPlaneAtTransitionEnd    = planeDist;
  m_isPlaneZooming                    = true;
  m_isPlaneReturningToNormal          = false;
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::ReturnEyeToNormalSetting(float transitionTime)
{
  SetEyeDistAndTransitionTime(m_distanceOfEyesBeforeChanging, transitionTime);
  m_isEyeReturningToNormal = true;
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::ReturnPlaneToNormalSetting(float transitionTime)
{
  SetPlaneDistAndTransitionTime(m_distanceOfPlaneBeforeChanging, transitionTime);
  m_isPlaneReturningToNormal = true;
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::UpdatePlaneZooming( float deltaTime )
{
	if ( IsPlaneZooming() )
	{
		m_timeOfPlaneCurrentlyPast += deltaTime;
		UpdateStereoScreenDistance();
		if ( m_isPlaneReturningToNormal && IsPlaneTransitionFinished() )
		{
			m_isPlaneZooming = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CStereoZoom::UpdateEyeZooming( float deltaTime )
{
  if ( IsEyeZooming() )
  {
    m_timeOfEyesCurrentlyPast  += deltaTime;
    UpdateStereoEyeDistance();
    if ( m_isEyeReturningToNormal && IsEyeTransitionFinished() )
    {
      m_isEyeZooming = false;
    }
  }
}
