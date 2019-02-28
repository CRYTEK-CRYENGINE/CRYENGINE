// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------

   Description: Utilities for debugging input synchronization problems

   -------------------------------------------------------------------------
   History:
   -	30:03:2007  : Created by Craig TillerNetInputChainPrint( const char *

*************************************************************************/

#include "StdAfx.h"
#include "NetInputChainDebug.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ITextModeConsole.h>
#include <CryRenderer/IRenderAuxGeom.h>

#if ENABLE_NETINPUTCHAINDEBUG
	#include <CryCore/BoostHelpers.h>
	#include <CryCore/Platform/CryWindows.h>

EntityId _netinputchain_debugentity = 0;
static int lastFrame = -100;
static int ypos = 0;
static int dump = 0;
static uint64 tstamp;

typedef CryVariant<float, Vec3> TNetInputValue;

static const char* GetEntityName()
{
	IEntity* pEnt = gEnv->pEntitySystem->GetEntity(_netinputchain_debugentity);
	assert(pEnt);
	if (pEnt)
		return pEnt->GetName();
	else
		return "<<unknown>>";
}

static void Put(const char* name, const TNetInputValue& value)
{
	FILE* fout = 0;
	if (dump) fout = fopen("netinput.log", "at");
	#if CRY_PLATFORM_WINDOWS
	FILETIME tm;
	GetSystemTimeAsFileTime(&tm);
	#endif
	ITextModeConsole* pTMC = gEnv->pSystem->GetITextModeConsole();

	if (lastFrame != gEnv->pRenderer->GetFrameID())
	{
		ypos = 0;
		lastFrame = gEnv->pRenderer->GetFrameID();
	#if CRY_PLATFORM_WINDOWS
		tstamp = (uint64(tm.dwHighDateTime) << 32) | tm.dwLowDateTime;
	#endif
	}
	float white[] = { 1, 1, 1, 1 };
	char buf[256];

	if (const Vec3* pVec = stl::get_if<const Vec3>(&value))
	{
		cry_sprintf(buf, "%s: %f %f %f", name, pVec->x, pVec->y, pVec->z);
		IRenderAuxText::Draw2dLabel(10.f, (float)(ypos+=20), 2.f, white, false, "%s", buf);
		if (pTMC)
			pTMC->PutText(0, ypos / 20, buf);
	#ifndef _RELEASE
		if (fout) fprintf(fout, "%" PRIu64 " %s %s %f %f %f\n", tstamp, GetEntityName(), name, pVec->x, pVec->y, pVec->z);
	#endif
	}
	else if (const float* pFloat = stl::get_if<const float>(&value))
	{
		cry_sprintf(buf, "%s: %f", name, *pFloat);
		IRenderAuxText::Draw2dLabel(10.f, (float)(ypos+=20), 2, white, false, "%s", buf);
		if (pTMC)
			pTMC->PutText(0, ypos / 20, buf);
	#ifndef _RELEASE
		if (fout) fprintf(fout, "%" PRIu64 " %s %s %f\n", tstamp, GetEntityName(), name, *pFloat);
	#endif
	}
	if (fout)
		fclose(fout);
}

static void OnChangeEntity(ICVar* pVar)
{
	const char* entName = pVar->GetString();
	if (IEntity* pEnt = gEnv->pEntitySystem->FindEntityByName(entName))
		_netinputchain_debugentity = pEnt->GetId();
	else
		_netinputchain_debugentity = 0;
}

void NetInputChainPrint(const char* name, const Vec3& val)
{
	Put(name, TNetInputValue(val));
}

void NetInputChainPrint(const char* name, float val)
{
	Put(name, TNetInputValue(val));
}

void NetInputChainInitCVars()
{
	REGISTER_STRING_CB("net_input_trace", "0", VF_CHEAT, "trace an entities input processing to assist finding sync errors", OnChangeEntity);

	REGISTER_CVAR2("net_input_dump", &dump, 0, VF_CHEAT, "write net_input_trace output to a file (netinput.log)");
}

void NetInputChainUnregisterCVars()
{
	gEnv->pConsole->UnregisterVariable("net_input_trace");
	gEnv->pConsole->UnregisterVariable("net_input_dump");
}
#else

void NetInputChainInitCVars()       {}
void NetInputChainUnregisterCVars() {}

#endif
