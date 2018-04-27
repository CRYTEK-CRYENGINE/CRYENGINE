// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIDbgRecorder.cpp
   Description: Simple text AI debugging event recorder

   -------------------------------------------------------------------------
   History:
   -01:07:2005 : Created by Kirill Bulatsev
   -19:11:2008 : Separated out by Matthew

   Notes:        Don't include the header in CAISystem - below it is redundant

 *********************************************************************/

#include "StdAfx.h"
#include "AIDbgRecorder.h"

#ifdef CRYAISYSTEM_DEBUG

	#define AIRECORDER_FILENAME          "AILog.log"
	#define AIRECORDER_SECONDARYFILENAME "AISignals.csv"

	#define BUFFER_SIZE                  256

//
//----------------------------------------------------------------------------------------------
bool CAIDbgRecorder::IsRecording(const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event) const
{
	if (!m_sFile.empty())
		return false;

	if (event == IAIRecordable::E_RESET)
		return true;

	if (!pTarget)
		return false;

	return !strcmp(gAIEnv.CVars.StatsTarget, "all")
	       || !strcmp(gAIEnv.CVars.StatsTarget, pTarget->GetName());
}

//
//----------------------------------------------------------------------------------------------
void CAIDbgRecorder::Record(const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event, const char* pString) const
{
	if (event == IAIRecordable::E_RESET)
	{
		LogString("\n\n--------------------------------------------------------------------------------------------\n");
		LogString("<RESETTING AI SYSTEM>\n");
		LogString("aiTick:startTime:<entity> ai_event    details\n");
		LogString("--------------------------------------------------------------------------------------------\n");
		// (MATT) Since some gAIEnv settings change only on a reset, this would be the time to list them {2008/11/20}
		return;
	}
	#ifdef AI_LOG_SIGNALS
	else if (event == IAIRecordable::E_SIGNALEXECUTEDWARNING)
	{
		char bufferString[BUFFER_SIZE];
		cry_sprintf(bufferString, "%s\n", pString);
		LogStringSecondary(bufferString);
		return;
	}
	#endif

	if (!pTarget)
		return;

	// Filter to only log the targets we are interested in
	// I.e. if not "all" and if not our current target, return
	const char* sStatsTarget = gAIEnv.CVars.StatsTarget;
	if ((strcmp(sStatsTarget, "all") != 0) && (strcmp(sStatsTarget, pTarget->GetName()) != 0))
		return;

	const char* pEventString("");
	switch (event)
	{
	case IAIRecordable::E_SIGNALRECIEVED:
		pEventString = "signal_recieved      ";
		break;
	case IAIRecordable::E_SIGNALRECIEVEDAUX:
		pEventString = "auxsignal_recieved   ";
		break;
	case IAIRecordable::E_SIGNALEXECUTING:
		pEventString = "signal_executing     ";
		break;
	case IAIRecordable::E_GOALPIPEINSERTED:
		pEventString = "goalpipe_inserted    ";
		break;
	case IAIRecordable::E_GOALPIPESELECTED:
		pEventString = "goalpipe_selected    ";
		break;
	case IAIRecordable::E_GOALPIPERESETED:
		pEventString = "goalpipe_reseted     ";
		break;
	case IAIRecordable::E_BEHAVIORSELECTED:
		pEventString = "behaviour_selected   ";
		break;
	case IAIRecordable::E_BEHAVIORDESTRUCTOR:
		pEventString = "behaviour_destructor ";
		break;
	case IAIRecordable::E_BEHAVIORCONSTRUCTOR:
		pEventString = "behaviour_constructor";
		break;
	case IAIRecordable::E_ATTENTIONTARGET:
		pEventString = "atttarget_change     ";
		break;
	case IAIRecordable::E_REGISTERSTIMULUS:
		pEventString = "register_stimulus    ";
		break;
	case IAIRecordable::E_HANDLERNEVENT:
		pEventString = "handler_event        ";
		break;
	case IAIRecordable::E_ACTIONSTART:
		pEventString = "action_start         ";
		break;
	case IAIRecordable::E_ACTIONSUSPEND:
		pEventString = "action_suspend       ";
		break;
	case IAIRecordable::E_ACTIONRESUME:
		pEventString = "action_resume        ";
		break;
	case IAIRecordable::E_ACTIONEND:
		pEventString = "action_end           ";
		break;
	case IAIRecordable::E_REFPOINTPOS:
		pEventString = "refpoint_pos         ";
		break;
	case IAIRecordable::E_EVENT:
		pEventString = "triggering event     ";
		break;
	case IAIRecordable::E_LUACOMMENT:
		pEventString = "lua comment          ";
		break;
	default:
		pEventString = "undefined            ";
		break;
	}

	// Fetch the current AI tick and the time that tick started
	int frame = GetAISystem()->GetAITickCount();
	float time = GetAISystem()->GetFrameStartTimeSeconds();

	char bufferString[BUFFER_SIZE];

	if (!pString)
		pString = "<null>";
	cry_sprintf(bufferString, "%6d:%9.3f: <%s> %s\t\t\t%s\n", frame, time, pTarget->GetName(), pEventString, pString);
	LogString(bufferString);
}

//
//----------------------------------------------------------------------------------------------
void CAIDbgRecorder::InitFile() const
{
	// Set the string
	m_sFile = PathUtil::Make(gEnv->pSystem->GetRootFolder(), AIRECORDER_FILENAME);

	// Open to wipe and write any preamble
	FILE* pFile = fxopen(m_sFile.c_str(), "wt");
	fputs("CryAISystem text debugging log\n", pFile);

	#if CRY_PLATFORM_WINDOWS
	// Get time.
	time_t ltime;
	time(&ltime);
	tm* today = localtime(&ltime);

	char s[1024];
	strftime(s, 128, "Date(%d %b %Y) Time(%H %M %S)\n\n", today);
	fputs(s, pFile);
	#endif

	fclose(pFile);
}

//
//----------------------------------------------------------------------------------------------
void CAIDbgRecorder::InitFileSecondary() const
{
	m_sFileSecondary = PathUtil::Make(gEnv->pSystem->GetRootFolder(), AIRECORDER_SECONDARYFILENAME);

	FILE* pFileSecondary = fxopen(m_sFileSecondary.c_str(), "wt");
	fputs("Function,Time,Page faults,Entity\n", pFileSecondary);
	fclose(pFileSecondary);
}

//----------------------------------------------------------------------------------------------
void CAIDbgRecorder::LogString(const char* pString) const
{
	bool mergeWithLog(gAIEnv.CVars.RecordLog != 0);
	if (!mergeWithLog)
	{
		if (m_sFile.empty())
			InitFile();

		FILE* pFile = fxopen(m_sFile.c_str(), "at");
		if (!pFile)
			return;

		fputs(pString, pFile);
		fclose(pFile);
	}
	else
	{
		GetAISystem()->LogEvent("<AIrec>", "%s", pString);
	}
}

//
//----------------------------------------------------------------------------------------------
void CAIDbgRecorder::LogStringSecondary(const char* pString) const
{
	if (m_sFile.empty())
		InitFileSecondary();

	FILE* pFileSecondary = fxopen(m_sFileSecondary.c_str(), "at");
	if (!pFileSecondary)
		return;

	fputs(pString, pFileSecondary);
	fclose(pFileSecondary);
}

#endif //CRYAISYSTEM_DEBUG
