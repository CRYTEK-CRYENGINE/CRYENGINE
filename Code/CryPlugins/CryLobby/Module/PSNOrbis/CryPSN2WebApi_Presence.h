// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYPSN2_WEBAPI_PRESENCE_H__
#define __CRYPSN2_WEBAPI_PRESENCE_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

//////////////////////////////////////////////////////////////////////////////////////////////

		#define CRY_WEBAPI_SETPRESENCE_HTTP_METHOD  SCE_NP_WEBAPI_HTTP_METHOD_PUT
		#define CRY_WEBAPI_SETPRESENCE_CONTENT_TYPE SCE_NP_WEBAPI_CONTENT_TYPE_APPLICATION_JSON_UTF8
		#define CRY_WEBAPI_SETPRESENCE_API_GROUP    "userProfile"
		#define CRY_WEBAPI_SETPRESENCE_REQUEST_PATH "/v1/users/%s/presence/gameStatus"
		#define CRY_WEBAPI_SETPRESENCE_PUT_BODY \
		  "{"                                   \
		  "\"gameStatus\":\"%s\""               \
		  "}"

		#define CRY_WEBAPI_SETPRESENCE_MAX_SIZE (256)           // Sony docs say presence string should be NO MORE THAN 64 UTF-8 characters.

struct SCryPSNOrbisWebApiSetPresenceInput
{
	SceNpOnlineId id;
	char          presence[CRY_WEBAPI_SETPRESENCE_MAX_SIZE];
};

//////////////////////////////////////////////////////////////////////////////////////////////

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS

#endif // __CRYPSN2_WEBAPI_PRESENCE_H__
