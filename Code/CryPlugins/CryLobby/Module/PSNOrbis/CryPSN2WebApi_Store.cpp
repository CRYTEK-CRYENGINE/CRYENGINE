// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

//#pragma optimize("", off)

#if CRY_PLATFORM_ORBIS

	#include "CryPSN2Lobby.h"

	#if USE_PSN

		#include "CryPSN2Support.h"
		#include "CryPSN2WebApi.h"
		#include "CryPSN2WebApi_Store.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// GetCommerceList
// Worker function that runs on the WebApi thread to send a request to list PSN store products
// filtered by product ID

void CCryPSNOrbisWebApiThread::GetCommerceList(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiGetCommerceInput* pCommerceData = (SCryPSNOrbisWebApiGetCommerceInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pCommerceData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	if (pCommerceData->numProducts > CRY_WEBAPI_COMMERCE_MAX_PRODUCTS)
	{
		pCommerceData->numProducts = CRY_WEBAPI_COMMERCE_MAX_PRODUCTS;
	}

	char pathStr[CRY_WEBAPI_TEMP_PATH_SIZE + CRY_WEBAPI_TEMP_BUFFER_SIZE];
	char workStr[CRY_WEBAPI_TEMP_BUFFER_SIZE];
	workStr[0] = 0;
	for (uint32 i = 0; i < pCommerceData->numProducts; ++i)
	{
		if (i > 0)
		{
			cry_strcat(workStr, ":");
		}
		cry_strcat(workStr, pCommerceData->products[i].productId.c_str());
	}
	cry_sprintf(pathStr, CRY_WEBAPI_COMMERCE_REQUEST_PATH, workStr);

	ret = _this->SendRequest(CRY_WEBAPI_COMMERCE_HTTP_METHOD, CRY_WEBAPI_COMMERCE_CONTENT_TYPE, CRY_WEBAPI_COMMERCE_API_GROUP, pathStr);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("commerce list");
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
// GetEntitlementList
// Worker function that runs on the WebApi thread to send a request to list PSN store
// "service" entitlements (consumables)

void CCryPSNOrbisWebApiThread::GetEntitlementList(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl != TMemInvalidHdl)
	{
		// There should be no params for this! If there are params, it is wrong!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	ret = _this->SendRequest(CRY_WEBAPI_ENTITLEMENT_LIST_HTTP_METHOD, CRY_WEBAPI_ENTITLEMENT_LIST_CONTENT_TYPE, CRY_WEBAPI_ENTITLEMENT_LIST_API_GROUP, CRY_WEBAPI_ENTITLEMENT_LIST_REQUEST_PATH);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("entitlement list");
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
// ConsumeEntitlement
// Worker function that runs on the WebApi thread to inform PSN that an entitlement has
// been consumed.

void CCryPSNOrbisWebApiThread::ConsumeEntitlement(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiConsumeEntitlementInput* pConsumeData = (SCryPSNOrbisWebApiConsumeEntitlementInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pConsumeData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	char pathStr[CRY_WEBAPI_TEMP_PATH_SIZE];
	cry_sprintf(pathStr, CRY_WEBAPI_ENTITLEMENT_CONSUME_REQUEST_PATH, pConsumeData->entitlementLabel.c_str());

	char tempStr[CRY_WEBAPI_TEMP_BUFFER_SIZE];
	cry_sprintf(tempStr, CRY_WEBAPI_ENTITLEMENT_CONSUME_PUT_BODY, pConsumeData->consumption);

	ret = _this->SendRequestText(CRY_WEBAPI_ENTITLEMENT_CONSUME_HTTP_METHOD, CRY_WEBAPI_ENTITLEMENT_CONSUME_CONTENT_TYPE, CRY_WEBAPI_ENTITLEMENT_CONSUME_API_GROUP, pathStr, tempStr);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("consume entitlement");
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
