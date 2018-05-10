// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
DesignerWarning.h
	- basic message box in windows to allow designers to be told something is wrong with their setup

-	[10/11/2009] : Created by James Bamford

*************************************************************************/

#include "StdAfx.h"
#include "DesignerWarning.h"
#include "GameCVars.h"
#include "Testing/FeatureTester.h"
#include "Testing/AutoTester.h"

#if DESIGNER_WARNING_ENABLED

static int s_numDesignerWarningsHit=0;

int DesignerWarningFunc(const char * message)
{
	if (g_pGameCVars->designer_warning_enabled && (!gEnv->IsDedicated()))
	{
		GameWarning("!DESIGNER WARNING\n%s", message);
	}

	// kept because autotests gather all designer warnings out of logs with this form
	CryLogAlways("---DESIGNER_WARNING: %s", message);
	CryLogAlways("----------------------------------------");

#if ENABLE_FEATURE_TESTER
	// If feature testing is in progress, write each designer warning out as a failed feature test

	CFeatureTester * featureTester = CFeatureTester::GetInstance();
	if (featureTester)
	{
		CAutoTester * autoTestResultWriter = featureTester->GetAutoTesterIfActive();
		if (autoTestResultWriter)
		{
			CryFixedStringT<32> warningName;
			warningName.Format("DesignerWarning%04u", s_numDesignerWarningsHit);
			autoTestResultWriter->AddSimpleTestCase("DesignerWarnings", warningName.c_str(), 0.1f, message);
		}
	}
#endif

	s_numDesignerWarningsHit++;
	return 0;
}

int GetNumDesignerWarningsHit()
{
	return s_numDesignerWarningsHit;
}

#endif // DESIGNER_WARNING_ENABLED