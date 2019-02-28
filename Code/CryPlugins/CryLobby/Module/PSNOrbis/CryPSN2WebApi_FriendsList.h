// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYPSN2_WEBAPI_FRIENDSLIST_H__
#define __CRYPSN2_WEBAPI_FRIENDSLIST_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

//////////////////////////////////////////////////////////////////////////////////////////////

		#define CRY_WEBAPI_FRIENDSLIST_HTTP_METHOD  SCE_NP_WEBAPI_HTTP_METHOD_GET
		#define CRY_WEBAPI_FRIENDSLIST_CONTENT_TYPE SCE_NP_WEBAPI_CONTENT_TYPE_APPLICATION_JSON_UTF8
		#define CRY_WEBAPI_FRIENDSLIST_API_GROUP    "userProfile"
		#define CRY_WEBAPI_FRIENDSLIST_REQUEST_PATH "/v1/users/%s/friendList?friendStatus=friend&presenceType=primary&limit=%d&offset=%d"
		#define CRY_WEBAPI_FRIENDSLIST_PAGE_LIMIT   2000

struct SCryPSNOrbisWebApiGetFriendsListInput
{
	SceNpOnlineId id;
	uint32        startIndex;
	uint32        count;
};

//////////////////////////////////////////////////////////////////////////////////////////////

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS

#endif // __CRYPSN2_WEBAPI_FRIENDSLIST_H__
