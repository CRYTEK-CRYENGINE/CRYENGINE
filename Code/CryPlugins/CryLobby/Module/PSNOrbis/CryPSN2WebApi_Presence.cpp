// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

//#pragma optimize("", off)

#if CRY_PLATFORM_ORBIS

	#include "CryPSN2Lobby.h"

	#if USE_PSN

		#include "CryPSN2Support.h"
		#include "CryPSN2WebApi.h"
		#include "CryPSN2WebApi_Presence.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// SetPresence
// Worker function that runs on the WebApi thread to set the rich presence for a user on PSN.
void CCryPSNOrbisWebApiThread::SetPresence(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiSetPresenceInput* pPresenceData = (SCryPSNOrbisWebApiSetPresenceInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pPresenceData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	pPresenceData->presence[CRY_WEBAPI_SETPRESENCE_MAX_SIZE - 1] = 0;

	char pathStr[CRY_WEBAPI_TEMP_PATH_SIZE];
	cry_sprintf(pathStr, CRY_WEBAPI_SETPRESENCE_REQUEST_PATH, pPresenceData->id.data);

	cry_sprintf(_this->m_request.workBuffer, CRY_WEBAPI_SETPRESENCE_PUT_BODY, pPresenceData->presence);

	ret = _this->SendRequestText(CRY_WEBAPI_SETPRESENCE_HTTP_METHOD, CRY_WEBAPI_SETPRESENCE_CONTENT_TYPE, CRY_WEBAPI_SETPRESENCE_API_GROUP, pathStr, _this->m_request.workBuffer);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("setpresence");
	if (responseBodyHdl == TMemInvalidHdl)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_OUT_OF_MEMORY, TMemInvalidHdl);
		return;
	}

	SCryPSNWebApiResponseBody* pResponseBody = (SCryPSNWebApiResponseBody*)pController->GetUtility()->GetLobby()->MemGetPtr(responseBodyHdl);
	if (!pResponseBody)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_OUT_OF_MEMORY, responseBodyHdl);
		return;
	}

	ret = _this->ReadResponse(pResponseBody);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, responseBodyHdl);
		return;
	}

	// all complete!
	pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, PSN_OK, responseBodyHdl);
}

//////////////////////////////////////////////////////////////////////////////////////////////

	#endif //USE_PSN
#endif   // CRY_PLATFORM_ORBIS
