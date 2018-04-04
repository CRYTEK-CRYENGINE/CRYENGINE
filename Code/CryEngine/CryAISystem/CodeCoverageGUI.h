// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/********************************************************************
   -------------------------------------------------------------------------
   File name:   CodeCoverageGUI.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - ?
   - 2 Mar 2009	: Evgeny Adamenkov: Removed parameter of type IRenderer from DebugDraw

 *********************************************************************/
#ifndef _CODECOVERAGEGUI_H_
#define _CODECOVERAGEGUI_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#if !defined(_RELEASE)

const int CCLAST_ELEM = 3;

class CCodeCoverageGUI
{
	struct SStrAndTime
	{
		SStrAndTime(const char* str = NULL, float time = 0.f) : pStr(str), fTime(time) {}

		const char* pStr;
		float       fTime;
	};

public:   // Construction & destruction
	CCodeCoverageGUI(void);
	~CCodeCoverageGUI(void);

public:   // Operations
	void Reset(IAISystem::EResetReason reason);
	void Update(CTimeValue frameStartTime, float frameDeltaTime);
	void DebugDraw(int nMode);

private:  // Member data
	float                    m_fPercentageDone, m_fNewHit, m_fUnexpectedHit;
	ITexture*                m_pTex, * m_pTexHit, * m_pTexUnexpected;
	SStrAndTime              m_arrLast3[CCLAST_ELEM];
	std::vector<const char*> m_vecRemaining;
	int                      m_nListUnexpectedSize;
};

#endif //_RELEASE

#endif
