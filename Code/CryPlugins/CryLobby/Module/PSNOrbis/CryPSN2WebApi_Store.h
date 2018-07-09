// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYPSN2_WEBAPI_STORE_H__
#define __CRYPSN2_WEBAPI_STORE_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

//////////////////////////////////////////////////////////////////////////////////////////////

		#define CRY_WEBAPI_COMMERCE_HTTP_METHOD  SCE_NP_WEBAPI_HTTP_METHOD_GET
		#define CRY_WEBAPI_COMMERCE_CONTENT_TYPE SCE_NP_WEBAPI_CONTENT_TYPE_APPLICATION_JSON_UTF8
		#define CRY_WEBAPI_COMMERCE_API_GROUP    "commerce"
		#define CRY_WEBAPI_COMMERCE_REQUEST_PATH "/v1/users/me/container/%s?start=0&size=50&sort=price&useFree=true&useCurrencySymbol=true"
// This is an example for activity feed JSON document formatting.

// note: if CRY_WEBAPI_COMMERCE_MAX_PRODUCTS or CRY_WEBAPI_COMMERCE_PRODUCT_ID_LENGTH changes, the buffer used to concatenate ids into a string may not be long enough
// (see CCryPSNOrbisWebApiThread::GetCommerceList)
		#define CRY_WEBAPI_COMMERCE_MAX_PRODUCTS (10)

struct SCryPSNOrbisWebApiGetCommerceInput
{
	uint32        numProducts;
	TStoreOfferID products[CRY_WEBAPI_COMMERCE_MAX_PRODUCTS];
};

		#define CRY_WEBAPI_ENTITLEMENT_LIST_HTTP_METHOD     SCE_NP_WEBAPI_HTTP_METHOD_GET
		#define CRY_WEBAPI_ENTITLEMENT_LIST_CONTENT_TYPE    SCE_NP_WEBAPI_CONTENT_TYPE_APPLICATION_JSON_UTF8
		#define CRY_WEBAPI_ENTITLEMENT_LIST_API_GROUP       "entitlement"
		#define CRY_WEBAPI_ENTITLEMENT_LIST_REQUEST_PATH    "/v1/users/me/entitlements?entitlement_type=service&start=0&size=50"

		#define CRY_WEBAPI_ENTITLEMENT_CONSUME_HTTP_METHOD  SCE_NP_WEBAPI_HTTP_METHOD_PUT
		#define CRY_WEBAPI_ENTITLEMENT_CONSUME_CONTENT_TYPE SCE_NP_WEBAPI_CONTENT_TYPE_APPLICATION_JSON_UTF8
		#define CRY_WEBAPI_ENTITLEMENT_CONSUME_API_GROUP    "entitlement"
		#define CRY_WEBAPI_ENTITLEMENT_CONSUME_REQUEST_PATH "/v1/users/me/entitlements/%s"

		#define CRY_WEBAPI_ENTITLEMENT_CONSUME_PUT_BODY \
		  "{"                                           \
		  "\"use_count\":%d"                            \
		  "}"

struct SCryPSNOrbisWebApiConsumeEntitlementInput
{
	uint32        consumption;
	TStoreAssetID entitlementLabel;
};

//////////////////////////////////////////////////////////////////////////////////////////////

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS

#endif // __CRYPSN2_WEBAPI_ACTIVITYFEED_H__
