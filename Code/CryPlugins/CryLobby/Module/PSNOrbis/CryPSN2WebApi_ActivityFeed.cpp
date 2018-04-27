// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

//#pragma optimize("", off)

#if CRY_PLATFORM_ORBIS

	#include "CryPSN2Lobby.h"

	#if USE_PSN

		#include "CryPSN2Support.h"
		#include "CryPSN2WebApi.h"
		#include "CryPSN2WebApi_ActivityFeed.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// PostActivity
// Worker function that runs on the WebApi thread to send an activity feed item to PSN.

void CCryPSNOrbisWebApiThread::PostActivity(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiAddActivityFeedInput* pFeedData = (SCryPSNOrbisWebApiAddActivityFeedInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pFeedData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	pFeedData->feed[CRY_WEBAPI_ACTIVITYFEED_MAX_SIZE - 1] = 0;

	ret = _this->SendRequestText(CRY_WEBAPI_ACTIVITYFEED_HTTP_METHOD, CRY_WEBAPI_ACTIVITYFEED_CONTENT_TYPE, CRY_WEBAPI_ACTIVITYFEED_API_GROUP, CRY_WEBAPI_ACTIVITYFEED_REQUEST_PATH, pFeedData->feed);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("postactivity");
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
