// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File: System.cpp
//  Description: Handle raising/lowering the frame rate on server
//               based upon CPU usage
//
//	History:
//	-May 28,2007: Originally Created by Craig Tiller
//
//////////////////////////////////////////////////////////////////////

#ifndef __SERVERTHROTTLE_H__
#define __SERVERTHROTTLE_H__

#pragma once

struct ISystem;

class CCPUMonitor;

class CServerThrottle
{
public:
	CServerThrottle(ISystem* pSys, int nCPUs);
	~CServerThrottle();
	void Update();

private:
	std::unique_ptr<CCPUMonitor> m_pCPUMonitor;

	void SetStep(int step, float* dueToCPU);

	float  m_minFPS;
	float  m_maxFPS;
	int    m_nSteps;
	int    m_nCurStep;
	ICVar* m_pDedicatedMaxRate;
	ICVar* m_pDedicatedCPU;
	ICVar* m_pDedicatedCPUVariance;
};

#endif
