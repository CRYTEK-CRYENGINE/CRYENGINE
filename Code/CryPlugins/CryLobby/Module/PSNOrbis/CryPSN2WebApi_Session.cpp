// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if CRY_PLATFORM_ORBIS

	#include "CryPSN2Lobby.h"

	#if USE_PSN

		#include "CryPSN2Support.h"
		#include "CryPSN2WebApi.h"
		#include "CryPSN2WebApi_Session.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// CreateSession
// Worker function that runs on the WebApi thread to create a session advert on PSN.
// Creator is automatically added to the session, no need to Join.

void CCryPSNOrbisWebApiThread::CreateSession(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiCreateSessionInput* pCreateData = (SCryPSNOrbisWebApiCreateSessionInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pCreateData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	sce::Json::Array names;
	sce::Json::Array statuses;
	if (pCreateData->advertisement.m_pLocalisedLanguages && pCreateData->advertisement.m_pLocalisedSessionNames && pCreateData->advertisement.m_pLocalisedSessionStatus)
	{
		for (uint32 i = 0; i < pCreateData->advertisement.m_numLanguages; ++i)
		{
			sce::Json::Object name;
			sce::Json::Object status;
			name["npLanguage"] = (sce::Json::String)pCreateData->advertisement.m_pLocalisedLanguages[i];
			name["sessionName"] = (sce::Json::String)pCreateData->advertisement.m_pLocalisedSessionNames[i];
			status["npLanguage"] = (sce::Json::String)pCreateData->advertisement.m_pLocalisedLanguages[i];
			status["sessionStatus"] = (sce::Json::String)pCreateData->advertisement.m_pLocalisedSessionStatus[i];
			names.push_back(name);
			statuses.push_back(status);
		}
	}

	sce::Json::Array platforms;
	platforms.push_back((sce::Json::String)"PS4");

	sce::Json::Object json;
	json["availablePlatforms"] = platforms;
	json["sessionType"] = (sce::Json::String)((pCreateData->advertisement.m_bIsEditableByAll) ? "owner-migration" : "owner-bind");
	json["sessionPrivacy"] = (sce::Json::String)((pCreateData->advertisement.m_bIsPrivate) ? "private" : "public");
	json["sessionMaxUser"] = (int64_t)pCreateData->advertisement.m_numSlots;
	json["localizedSessionNames"] = names;
	json["localizedSessionStatus"] = statuses;
	json["sessionName"] = (sce::Json::String)"DefaultName";
	json["sessionStatus"] = (sce::Json::String)"Default status";

	sce::Json::Value advertRoot;
	advertRoot.clear();
	advertRoot.set(json);

	sce::Json::String jsonString;
	advertRoot.serialize(jsonString);

	size_t length = 0;
	char bodyStr[CRY_WEBAPI_TEMP_BUFFER_SIZE];

	cry_sprintf(pCreateData->body,
	            CRY_WEBAPI_CREATESESSION_POST_BODY_JSON,
	            (uint32)jsonString.length(), jsonString.c_str());

	length = strlen(pCreateData->body);

	cry_sprintf(&pCreateData->body[length], sizeof(pCreateData->body) - length,
	            CRY_WEBAPI_CREATESESSION_POST_BODY_JPG,
	            pCreateData->advertisement.m_sizeofJPGImage);

	length = strlen(pCreateData->body);

	memcpy(&pCreateData->body[length], pCreateData->advertisement.m_pJPGImage, pCreateData->advertisement.m_sizeofJPGImage);

	length += pCreateData->advertisement.m_sizeofJPGImage;

	cry_sprintf(bodyStr,
	            CRY_WEBAPI_CREATESESSION_POST_BODY_DATA,
	            pCreateData->advertisement.m_sizeofData);
	cry_sprintf(&pCreateData->body[length], sizeof(pCreateData->body) - length, "%s", bodyStr);

	length += strlen(bodyStr);

	memcpy(&pCreateData->body[length], pCreateData->advertisement.m_pData, pCreateData->advertisement.m_sizeofData);

	length += pCreateData->advertisement.m_sizeofData;

	cry_sprintf(bodyStr,
	            CRY_WEBAPI_CREATESESSION_POST_BODY_END);
	cry_sprintf(&pCreateData->body[length], sizeof(pCreateData->body) - length, "%s", bodyStr);

	length += strlen(bodyStr);

	ret = _this->SendRequestData(CRY_WEBAPI_CREATESESSION_HTTP_METHOD, CRY_WEBAPI_CREATESESSION_CONTENT_TYPE, CRY_WEBAPI_CREATESESSION_API_GROUP, CRY_WEBAPI_CREATESESSION_REQUEST_PATH, pCreateData->body, length);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("createsession");
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
// DeleteSession
// Worker function that runs on the WebApi thread to delete a session advert on PSN.

void CCryPSNOrbisWebApiThread::DeleteSession(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiDeleteSessionInput* pDeleteData = (SCryPSNOrbisWebApiDeleteSessionInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pDeleteData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	char pathStr[CRY_WEBAPI_TEMP_PATH_SIZE];
	cry_sprintf(pathStr, CRY_WEBAPI_DELETESESSION_REQUEST_PATH, pDeleteData->sessionId.data);

	ret = _this->SendRequest(CRY_WEBAPI_DELETESESSION_HTTP_METHOD, CRY_WEBAPI_DELETESESSION_CONTENT_TYPE, CRY_WEBAPI_DELETESESSION_API_GROUP, pathStr);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("deletesession");
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
// UpdateSession
// Worker function that runs on the WebApi thread to update an existing advertised session's details on PSN.
// This is actually 3 jobs in 1, which is likely to be relatively slow.
void CCryPSNOrbisWebApiThread::UpdateSession(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiUpdateSessionInput* pUpdateData = (SCryPSNOrbisWebApiUpdateSessionInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pUpdateData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("updatesession");
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

	// UPDATE JSON
	if (pUpdateData->advertisement.m_numLanguages > 0)
	{
		sce::Json::Array names;
		sce::Json::Array statuses;
		if (pUpdateData->advertisement.m_pLocalisedLanguages && pUpdateData->advertisement.m_pLocalisedSessionNames && pUpdateData->advertisement.m_pLocalisedSessionStatus)
		{
			for (uint32 i = 0; i < pUpdateData->advertisement.m_numLanguages; ++i)
			{
				sce::Json::Object name;
				sce::Json::Object status;
				name["npLanguage"] = (sce::Json::String)pUpdateData->advertisement.m_pLocalisedLanguages[i];
				name["sessionName"] = (sce::Json::String)pUpdateData->advertisement.m_pLocalisedSessionNames[i];
				status["npLanguage"] = (sce::Json::String)pUpdateData->advertisement.m_pLocalisedLanguages[i];
				status["sessionStatus"] = (sce::Json::String)pUpdateData->advertisement.m_pLocalisedSessionStatus[i];
				names.push_back(name);
				statuses.push_back(status);
			}
		}

		sce::Json::Object json;
		json["localizedSessionNames"] = names;
		json["localizedSessionStatus"] = statuses;
		json["sessionName"] = (sce::Json::String)"DefaultName";
		json["sessionStatus"] = (sce::Json::String)"Default status";

		sce::Json::Value advertRoot;
		advertRoot.clear();
		advertRoot.set(json);

		sce::Json::String jsonString;
		advertRoot.serialize(jsonString);

		char pathStr[CRY_WEBAPI_TEMP_PATH_SIZE];
		cry_sprintf(pathStr, CRY_WEBAPI_UPDATESESSION_JSON_REQUEST_PATH, pUpdateData->sessionId.data);

		cry_strcpy(pUpdateData->jsonBody, jsonString.c_str());

		ret = _this->SendRequestText(CRY_WEBAPI_UPDATESESSION_JSON_HTTP_METHOD, CRY_WEBAPI_UPDATESESSION_JSON_CONTENT_TYPE, CRY_WEBAPI_UPDATESESSION_API_GROUP, pathStr, pUpdateData->jsonBody);
		if (ret < PSN_OK)
		{
			// failed
			pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, responseBodyHdl);
			return;
		}
		ret = _this->ReadResponse(pResponseBody);
		if (ret < PSN_OK)
		{
			// failed
			pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, responseBodyHdl);
			return;
		}
		// updating the json should have returned 204, and not set anything inside the response body,
		// so type should still be E_UNKNOWN
		if (pResponseBody->eType != SCryPSNWebApiResponseBody::E_UNKNOWN)
		{
			// should be unset! Error!
			pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, responseBodyHdl);
			return;
		}
	}
	// END JSON UPDATE

	// UPDATE JPG
	if (pUpdateData->advertisement.m_pJPGImage && (pUpdateData->advertisement.m_sizeofJPGImage > 0))
	{
		char pathStr[CRY_WEBAPI_TEMP_PATH_SIZE];
		cry_sprintf(pathStr, CRY_WEBAPI_UPDATESESSION_JPG_REQUEST_PATH, pUpdateData->sessionId.data);

		ret = _this->SendRequestData(CRY_WEBAPI_UPDATESESSION_JPG_HTTP_METHOD, CRY_WEBAPI_UPDATESESSION_JPG_CONTENT_TYPE, CRY_WEBAPI_UPDATESESSION_API_GROUP, pathStr, pUpdateData->advertisement.m_pJPGImage, pUpdateData->advertisement.m_sizeofJPGImage);
		if (ret < PSN_OK)
		{
			// failed
			pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, responseBodyHdl);
			return;
		}
		ret = _this->ReadResponse(pResponseBody);
		if (ret < PSN_OK)
		{
			// failed
			pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, responseBodyHdl);
			return;
		}
		// updating the jpg should have returned 204, and not set anything inside the response body,
		// so type should still be E_UNKNOWN
		if (pResponseBody->eType != SCryPSNWebApiResponseBody::E_UNKNOWN)
		{
			// should be unset! Error!
			pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, responseBodyHdl);
			return;
		}
	}
	// END JPEG UPDATE

	// TODO - UPDATE LINKDATA - DOESN'T EXIST YET, due SDK 1.700
	/*
	   TMemHdl dataStrHdl = TMemInvalidHdl;
	   char* pDataStr = pController->GetUtility()->Base64Encode(pUpdateData->advertisement.m_pData, pUpdateData->advertisement.m_sizeofData, dataStrHdl);

	   cry_strcpy(pCreateData->body, pDataStr);

	   pController->GetUtility()->FreeMem(dataStrHdl);

	   ret = _this->SendRequestData(CRY_WEBAPI_UPDATESESSION_LINKDATA_HTTP_METHOD, CRY_WEBAPI_UPDATESESSION_LINKDATA_CONTENT_TYPE, CRY_WEBAPI_UPDATESESSION_API_GROUP, CRY_WEBAPI_UPDATESESSION_LINKDATA_REQUEST_PATH, pUpdateData->advertisement.m_pData, pUpdateData->advertisement.m_sizeofData);
	   if (ret < PSN_OK)
	   {
	    // failed
	    pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, responseBodyHdl);
	    return;
	   }
	   ret = _this->ReadResponse(pResponseBody);
	   if (ret < PSN_OK)
	   {
	    // failed
	    pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, responseBodyHdl);
	    return;
	   }
	 */
	// all complete!
	pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, PSN_OK, responseBodyHdl);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// JoinSession
// Worker function that runs on the WebApi thread to join an existing advertised session on PSN.

void CCryPSNOrbisWebApiThread::JoinSession(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiJoinSessionInput* pJoinData = (SCryPSNOrbisWebApiJoinSessionInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pJoinData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	char pathStr[CRY_WEBAPI_TEMP_PATH_SIZE];
	cry_sprintf(pathStr, CRY_WEBAPI_JOINSESSION_REQUEST_PATH, pJoinData->sessionId.data);

	ret = _this->SendRequest(CRY_WEBAPI_JOINSESSION_HTTP_METHOD, CRY_WEBAPI_JOINSESSION_CONTENT_TYPE, CRY_WEBAPI_JOINSESSION_API_GROUP, pathStr);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("joinsession");
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
// LeaveSession
// Worker function that runs on the WebApi thread to leave an existing advertised session on PSN.

void CCryPSNOrbisWebApiThread::LeaveSession(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiLeaveSessionInput* pLeaveData = (SCryPSNOrbisWebApiLeaveSessionInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pLeaveData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	char pathStr[CRY_WEBAPI_TEMP_PATH_SIZE];
	cry_sprintf(pathStr, CRY_WEBAPI_LEAVESESSION_REQUEST_PATH, pLeaveData->sessionId.data, pLeaveData->playerId.data);

	ret = _this->SendRequest(CRY_WEBAPI_LEAVESESSION_HTTP_METHOD, CRY_WEBAPI_LEAVESESSION_CONTENT_TYPE, CRY_WEBAPI_LEAVESESSION_API_GROUP, pathStr);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("leavesession");
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
// GetSessionData
// Worker function that runs on the WebApi thread to get the linkdata attached to a session advertisement

void CCryPSNOrbisWebApiThread::GetSessionLinkData(CryWebApiJobId jobId, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this)
{
	int ret = PSN_OK;

	if (paramsHdl == TMemInvalidHdl)
	{
		// no params! Error!
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	SCryPSNOrbisWebApiGetSessionLinkDataInput* pGetData = (SCryPSNOrbisWebApiGetSessionLinkDataInput*)pController->GetUtility()->GetLobby()->MemGetPtr(paramsHdl);
	if (!pGetData)
	{
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, SCE_NP_ERROR_INVALID_ARGUMENT, TMemInvalidHdl);
		return;
	}

	char pathStr[CRY_WEBAPI_TEMP_PATH_SIZE];
	cry_sprintf(pathStr, CRY_WEBAPI_GETSESSIONDATA_REQUEST_PATH, pGetData->sessionId.data);

	ret = _this->SendRequest(CRY_WEBAPI_GETSESSIONDATA_HTTP_METHOD, CRY_WEBAPI_GETSESSIONDATA_CONTENT_TYPE, CRY_WEBAPI_GETSESSIONDATA_API_GROUP, pathStr);
	if (ret < PSN_OK)
	{
		// failed
		pController->GetUtility()->GetPSNSupport()->ReturnWebApiEvent(jobId, ret, TMemInvalidHdl);
		return;
	}

	TMemHdl responseBodyHdl = pController->GetUtility()->NewResponseBody("getsessiondata");
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
