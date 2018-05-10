// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

//#pragma optimize("", off)

#if CRY_PLATFORM_ORBIS

	#include "CryPSN2Lobby.h"

	#if USE_PSN

		#include "CryPSN2Support.h"
		#include "CryPSN2WebApi.h"
		#include "CryPSN2WebApi_RecentPlayers.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// AddRecentPlayers
// Worker function that runs on the WebApi thread to send a recent players item to PSN.
void CCryPSNOrbisWebApiThread::AddRecentPlayers(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiAddRecentPlayersInput* pRecentPlayersData = (SCryPSNOrbisWebApiAddRecentPlayersInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pRecentPlayersData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	pRecentPlayersData->feed[CRY_WEBAPI_ADDRECENTPLAYERS_MAX_SIZE - 1] = 0;

	ret = _this->SendRequestText(CRY_WEBAPI_ADDRECENTPLAYERS_HTTP_METHOD, CRY_WEBAPI_ADDRECENTPLAYERS_CONTENT_TYPE, CRY_WEBAPI_ADDRECENTPLAYERS_API_GROUP, CRY_WEBAPI_ADDRECENTPLAYERS_REQUEST_PATH, pRecentPlayersData->feed);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("addrecentplayers");
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
