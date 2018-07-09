// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYPSN2_WEBAPI_RECENTPLAYERS_H__
#define __CRYPSN2_WEBAPI_RECENTPLAYERS_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

//////////////////////////////////////////////////////////////////////////////////////////////

		#define CRY_WEBAPI_ADDRECENTPLAYERS_HTTP_METHOD  SCE_NP_WEBAPI_HTTP_METHOD_POST
		#define CRY_WEBAPI_ADDRECENTPLAYERS_CONTENT_TYPE SCE_NP_WEBAPI_CONTENT_TYPE_APPLICATION_JSON_UTF8
		#define CRY_WEBAPI_ADDRECENTPLAYERS_API_GROUP    "activityFeed"
		#define CRY_WEBAPI_ADDRECENTPLAYERS_REQUEST_PATH "/v1/users/me/feed"
		#define CRY_WEBAPI_ADDRECENTPLAYERS_POST_BODY_SAMPLE \
		  "{"                                                \
		  "\"source\":{"                                     \
		  "\"meta\":\"CryPlayer1\","                         \
		  "\"type\":\"ONLINE_ID\""                           \
		  "}, "                                              \
		  "\"storyType\":\"PLAYED_WITH\","                   \
		  "\"targets\":["                                    \
		  "{"                                                \
		  "\"meta\":\"CryPlayer2\","                         \
		  "\"type\":\"ONLINE_ID\""                           \
		  "},"                                               \
		  "{"                                                \
		  "\"meta\":\"CryPlayer3\","                         \
		  "\"type\":\"ONLINE_ID\""                           \
		  "},"                                               \
		  "{"                                                \
		  "\"meta\":\"NPXX12345_00\","                       \
		  "\"type\":\"TITLE_ID\""                            \
		  "},"                                               \
		  "{"                                                \
		  "\"meta\":\"Deathmatch PVP\","                     \
		  "\"type\":\"PLAYED_DESCRIPTION\""                  \
		  "}"                                                \
		  "]"                                                \
		  "}"

		#define CRY_WEBAPI_ADDRECENTPLAYERS_MAX_SIZE (4096)         // can change this if needed

struct SCryPSNOrbisWebApiAddRecentPlayersInput
{
	char feed[CRY_WEBAPI_ADDRECENTPLAYERS_MAX_SIZE];
};

//////////////////////////////////////////////////////////////////////////////////////////////

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS

#endif // __CRYPSN2_WEBAPI_RECENTPLAYERS_H__
