// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
- 28:05:2010  Created by Jens Schöbel

*************************************************************************/
#pragma once

#ifndef STEREOZOOM_H
#define STEREOZOOM_H

namespace Stereo3D
{
  namespace Zoom
  {
    struct Parameters
    {
      Parameters()
        : eyeDistance(0)
        , convergenceDistance(10)
      { 
        Reset(XmlNodeRef(NULL)); 
      };

      void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);
      void GetMemoryUsage(ICrySizer * s) const;

      float eyeDistance;
      float convergenceDistance;
    };

    class CStereoZoom
    {
    public:
      CStereoZoom();

      float GetCurrentPlaneDist() const;
      bool IsPlaneZooming() const;
      void Update(float deltaTime);
      void SetPlaneDistAndTransitionTime(float, float);
      void SetEyeDistAndTransitionTime(float, float);
      void ReturnEyeToNormalSetting(float);
      void ReturnPlaneToNormalSetting(float);

    protected:
    private:
      float GetCurrentClampedPlaneTValue() const;

      float GetCurrentClampedEyeTValue() const;
      float GetCurrentEyeDist() const;

			bool IsStereoEnabled() const;

      bool IsEyeZooming() const;
      bool IsPlaneTransitionFinished() const;
      bool IsEyeTransitionFinished()const;

      void UpdateStereoScreenDistance();
      void UpdateStereoEyeDistance();
      void RememberCurrentPlaneDistance();
      void RememberCurrentEyeDistance();
      void SetStereoPlaneDistCVAR(float);
      void SetStereoEyeDistCVAR(float);
      void PrintDebugOutput();

			void UpdatePlaneZooming(float deltaTime);
			void UpdateEyeZooming(float deltaTime);

    public:
    protected:
    private:
      float m_distanceOfPlaneBeforeChanging;
      float m_distanceOfPlaneAtTransitionStart;
      float m_distanceOfPlaneAtTransitionEnd;
      float m_timeOfPlaneWhenTransitionFinishes;
      float m_timeOfPlaneCurrentlyPast;

      float m_distanceOfEyesBeforeChanging;
      float m_distanceOfEyesAtTransitionStart;
      float m_distanceOfEyesAtTransitionEnd;
      float m_timeOfEyesWhenTransitionFinishes;
      float m_timeOfEyesCurrentlyPast;

      bool m_isEyeZooming;
      bool m_isPlaneZooming;

      bool m_isEyeReturningToNormal;
      bool m_isPlaneReturningToNormal;
    };
  } // namespace Zoom
} // namespace Stereo3D

#endif
