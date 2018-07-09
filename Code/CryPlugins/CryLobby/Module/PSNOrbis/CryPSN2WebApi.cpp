// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if CRY_PLATFORM_ORBIS

	#include "CryPSN2Lobby.h"

	#if USE_PSN

		#include "CryPSN2Support.h"
		#include "CryPSN2WebApi.h"

//////////////////////////////////////////////////////////////////////////////////////////////

CryWebApiJobId CCryPSNOrbisWebApiThread::sm_nextFreeJobId = 1;
void CCryPSNOrbisWebApiThread::ThreadEntry()
{
	while (true)
	{
		m_mtx.Lock();

		while (m_bAlive && m_jobQueue.empty())
		{
			m_cond.Wait(m_mtx);
		}

		if (!m_bAlive)
		{
			break;
		}

		SCryPSNWebApiThreadJobInfo info(INVALID_WEBAPI_JOB_ID, NULL, TMemInvalidHdl);
		info = m_jobQueue.front();
		m_jobQueue.pop_front();

		if (m_pController && info.m_pJobFunc)
		{
			info.m_pJobFunc(info.m_id, info.m_paramsHdl, m_pController, this);
		}

		m_mtx.Unlock();
	}

	// thread is stopped, clear remaining queue jobs
	{
		std::deque<SCryPSNWebApiThreadJobInfo> tempQueue;
		m_jobQueue.swap(tempQueue);
	}
	m_mtx.Unlock();
}

void CCryPSNOrbisWebApiThread::SignalStopWork()
{
	CryAutoLock<CryMutex> lk(m_mtx);

	m_bAlive = false;
	m_cond.Notify();
}

CryWebApiJobId CCryPSNOrbisWebApiThread::Push(TCryPSNWebApiThreadJobFunc pFunc, TMemHdl paramsHdl)
{
	CryAutoLock<CryMutex> lk(m_mtx);

	CryWebApiJobId id = GetUniqueJobId();

	m_jobQueue.push_back(SCryPSNWebApiThreadJobInfo(id, pFunc, paramsHdl));

	m_cond.Notify();

	return id;
}

void CCryPSNOrbisWebApiThread::Flush()
{
	CryAutoLock<CryMutex> lk(m_mtx);

	{
		std::deque<SCryPSNWebApiThreadJobInfo> tempQueue;
		m_jobQueue.swap(tempQueue);
	}
	m_cond.Notify();

	if (m_request.requestId != INVALID_PSN_WEBAPI_SENDREQUEST_ID)
	{
		sceNpWebApiAbortRequest(m_request.requestId);
		sceNpWebApiDeleteRequest(m_request.requestId);
		m_request.requestId = INVALID_PSN_WEBAPI_SENDREQUEST_ID;
	}
}

int CCryPSNOrbisWebApiThread::SendRequest(SceNpWebApiHttpMethod httpMethod, const char* pContentType, const char* pAPIGroup, const char* pPath)
{
	return SendRequestData(httpMethod, pContentType, pAPIGroup, pPath, NULL, 0);
}

int CCryPSNOrbisWebApiThread::SendRequestText(SceNpWebApiHttpMethod httpMethod, const char* pContentType, const char* pAPIGroup, const char* pPath, const char* pBody)
{
	return SendRequestData(httpMethod, pContentType, pAPIGroup, pPath, (const void*)pBody, strlen(pBody));
}

int CCryPSNOrbisWebApiThread::SendRequestData(SceNpWebApiHttpMethod httpMethod, const char* pContentType, const char* pAPIGroup, const char* pPath, const void* pData, size_t sizeofData)
{
	int ret;

	if (m_request.requestId != INVALID_PSN_WEBAPI_SENDREQUEST_ID)
	{
		return CRY_WEBAPI_ERROR_SEND_ALREADY_IN_PROGRESS;
	}

	switch (httpMethod)
	{
	case SCE_NP_WEBAPI_HTTP_METHOD_GET:
		{
			NetLog("[PSN WEBAPI] HTTP GET %s%s", pAPIGroup, pPath);
		}
		break;
	case SCE_NP_WEBAPI_HTTP_METHOD_POST:
		{
			NetLog("[PSN WEBAPI] HTTP POST %s%s", pAPIGroup, pPath);
		}
		break;
	case SCE_NP_WEBAPI_HTTP_METHOD_PUT:
		{
			NetLog("[PSN WEBAPI] HTTP PUT %s%s", pAPIGroup, pPath);
		}
		break;
	case SCE_NP_WEBAPI_HTTP_METHOD_DELETE:
		{
			NetLog("[PSN WEBAPI] HTTP DELETE %s%s", pAPIGroup, pPath);
		}
		break;
	default:
		{
			NetLog("[PSN WEBAPI] HTTP UNKNOWN %s%s", pAPIGroup, pPath);
		}
		break;
	}

	if (pData && (sizeofData > 0))
	{
		SceNpWebApiContentParameter contentParam;
		contentParam.contentLength = sizeofData;
		contentParam.pContentType = pContentType;

		ret = sceNpWebApiCreateRequest(m_pController->GetContextId(),
		                               pAPIGroup, pPath, httpMethod,
		                               &contentParam,
		                               (int64_t*)&m_request.requestId);
		if (ret < PSN_OK)
		{
			return ret;
		}

		m_serverError.Reset();
		ret = sceNpWebApiSendRequest2(m_request.requestId, pData, contentParam.contentLength, &m_serverError.serverError);
		if (ret < PSN_OK)
		{
			sceNpWebApiDeleteRequest(m_request.requestId);
			m_request.requestId = INVALID_PSN_WEBAPI_SENDREQUEST_ID;
			return ret;
		}
	}
	else
	{
		ret = sceNpWebApiCreateRequest(m_pController->GetContextId(),
		                               pAPIGroup, pPath, httpMethod,
		                               NULL,
		                               (int64_t*)&m_request.requestId);
		if (ret < PSN_OK)
		{
			return ret;
		}

		m_serverError.Reset();
		ret = sceNpWebApiSendRequest2(m_request.requestId, NULL, 0, &m_serverError.serverError);
		if (ret < PSN_OK)
		{
			sceNpWebApiDeleteRequest(m_request.requestId);
			m_request.requestId = INVALID_PSN_WEBAPI_SENDREQUEST_ID;
			return ret;
		}
	}

	return PSN_OK;
}

int CCryPSNOrbisWebApiThread::DataProvideCallback(char& data, void* pWebAPIJobThread)
{
	int ret = PSN_OK;

	CCryPSNOrbisWebApiThread* _this = reinterpret_cast<CCryPSNOrbisWebApiThread*>(pWebAPIJobThread);

	if (_this->m_request.nRecvBufferParsed == _this->m_request.nRecvBufferUsed)
	{
		//-- refill the buffer
		ret = sceNpWebApiReadData(_this->m_request.requestId, _this->m_request.workBuffer, WEBAPI_WORKBUFFER_MAX_SIZE);
		if (ret > PSN_OK)
		{
			// Normal operation
			_this->m_request.nRecvBufferParsed = 0;
			_this->m_request.nRecvBufferUsed = ret;

			_this->m_request.workBuffer[ret] = 0;
			// NetLog("> %s", _this->m_request.workBuffer);
		}
		else if (ret < PSN_OK)
		{
			// failed
			NetLog("sceNpWebApiReadData() failed. ret = 0x%x\n", ret);
			return ret;
		}
		else
		{
			// end of body. Time to quit.
			return CRY_WEBAPI_ERROR_END_OF_BODY;
		}
	}

	// Supply 1 byte to the JSON parser.
	data = _this->m_request.workBuffer[_this->m_request.nRecvBufferParsed++];

	return PSN_OK;
}

int CCryPSNOrbisWebApiThread::StoreRawResponseBody(SCryPSNWebApiResponseBody* pResponseBody)
{
	char contentLengthValue[CRY_WEBAPI_HEADER_VALUE_SIZE];
	memset(contentLengthValue, 0, CRY_WEBAPI_HEADER_VALUE_SIZE);

	int ret = sceNpWebApiGetHttpResponseHeaderValue(m_request.requestId, CRY_WEBAPI_HEADER_CONTENT_LENGTH, contentLengthValue, CRY_WEBAPI_HEADER_VALUE_SIZE);
	if (ret < PSN_OK)
	{
		// error, header not found
		NetLog("sceNpWebApiGetHttpResponseHeaderValue(): \"%s\") not found", CRY_WEBAPI_HEADER_CONTENT_LENGTH);
		return ret;
	}

	size_t contentLength = atol(contentLengthValue);

	if (contentLength <= 0)
	{
		return CRY_WEBAPI_ERROR_BAD_CONTENTLENGTH;
	}

	pResponseBody->rawResponseBodyHdl = m_pController->GetUtility()->AllocMem(contentLength);
	if (pResponseBody->rawResponseBodyHdl == TMemInvalidHdl)
	{
		return SCE_NP_ERROR_OUT_OF_MEMORY;
	}

	pResponseBody->rawResponseBodySize = contentLength;
	char* pRawBody = (char*)m_pController->GetUtility()->GetLobby()->MemGetPtr(pResponseBody->rawResponseBodyHdl);

	while (contentLength)
	{
		ret = sceNpWebApiReadData(m_request.requestId, m_request.workBuffer, WEBAPI_WORKBUFFER_MAX_SIZE);
		if (ret > PSN_OK)
		{
			// Normal operation
			memcpy(pRawBody, m_request.workBuffer, ret);
			pRawBody += ret;
			contentLength -= ret;
		}
		else if (ret < PSN_OK)
		{
			// failed
			NetLog("sceNpWebApiReadData() failed. ret = 0x%x\n", ret);
			return ret;
		}
		else
		{
			// end of body. Time to quit.
			break;
		}
	}

	return PSN_OK;
}

int CCryPSNOrbisWebApiThread::ReadResponse(SCryPSNWebApiResponseBody* pResponseBody)
{
	if (m_request.requestId == INVALID_PSN_WEBAPI_SENDREQUEST_ID)
	{
		return CRY_WEBAPI_ERROR_NO_SEND_IN_PROGRESS;
	}

	m_request.workBuffer[0] = 0;
	m_request.nRecvBufferUsed = 0;
	m_request.nRecvBufferParsed = 0;

	const int httpStatusCode = m_serverError.serverError.httpStatus;
	int ret;

	if ((httpStatusCode >= 200) && (httpStatusCode < 300))
	{
		// Request success
		// Attempt to read out response data
		if (httpStatusCode == 204)
		{
			// 204 is a special case - it means there shouldn't be any response data.
			ret = PSN_OK;
			NetLog("[PSN WEBAPI] HTTP RESPONSE %d: (no body)", httpStatusCode);
		}
		else
		{
			// check the response type
			char contentTypeValue[CRY_WEBAPI_HEADER_VALUE_SIZE];
			memset(contentTypeValue, 0, CRY_WEBAPI_HEADER_VALUE_SIZE);

			ret = sceNpWebApiGetHttpResponseHeaderValue(m_request.requestId, CRY_WEBAPI_HEADER_CONTENT_TYPE, contentTypeValue, CRY_WEBAPI_HEADER_VALUE_SIZE);
			if (ret >= PSN_OK)
			{
				if (!strncmp(CRY_WEBAPI_CONTENT_TYPE_JSON, contentTypeValue, strlen(CRY_WEBAPI_CONTENT_TYPE_JSON)))
				{
					// response is in JSON format
					ret = Parser.parse(pResponseBody->jsonTreeRoot, CCryPSNOrbisWebApiThread::DataProvideCallback, this);
					if (ret == PSN_OK)
					{
						NetLog("[PSN WEBAPI] HTTP RESPONSE %d: %s %s (JSON body)", httpStatusCode, CRY_WEBAPI_HEADER_CONTENT_TYPE, contentTypeValue);
						m_pController->GetUtility()->PrintResponseJSONTree(pResponseBody);
						pResponseBody->eType = SCryPSNWebApiResponseBody::E_JSON;
					}
					else
					{
						// Couldn't parse the JSON for some reason.
						if (m_request.nRecvBufferUsed > 0)
						{
							NetLog("[PSN WEBAPI] HTTP RESPONSE %d: %s %s (JSON body parse failed) %s", httpStatusCode, CRY_WEBAPI_HEADER_CONTENT_TYPE, contentTypeValue, m_request.workBuffer);
							pResponseBody->rawResponseBodyHdl = m_pController->GetUtility()->AllocMem(m_request.nRecvBufferUsed);
							if (pResponseBody->rawResponseBodyHdl != TMemInvalidHdl)
							{
								pResponseBody->rawResponseBodySize = m_request.nRecvBufferUsed;
								char* pRawBodyCopy = (char*)m_pController->GetUtility()->GetLobby()->MemGetPtr(pResponseBody->rawResponseBodyHdl);
								memcpy(pRawBodyCopy, m_request.workBuffer, m_request.nRecvBufferUsed);
								pResponseBody->eType = SCryPSNWebApiResponseBody::E_RAW;
							}
						}
						else
						{
							NetLog("[PSN WEBAPI] HTTP RESPONSE %d: %s %s (no body)", httpStatusCode, CRY_WEBAPI_HEADER_CONTENT_TYPE, contentTypeValue);
						}
					}
				}
				else
				{
					// response is not JSON format
					ret = StoreRawResponseBody(pResponseBody);
					if (ret == PSN_OK)
					{
						NetLog("[PSN WEBAPI] HTTP RESPONSE %d: %s %s (raw body)", httpStatusCode, CRY_WEBAPI_HEADER_CONTENT_TYPE, contentTypeValue);
						pResponseBody->eType = SCryPSNWebApiResponseBody::E_RAW;
					}
					else
					{
						// Couldn't copy raw response body
						NetLog("[PSN WEBAPI] HTTP RESPONSE %d: %s %s (couldn't copy raw response body)", httpStatusCode, CRY_WEBAPI_HEADER_CONTENT_TYPE, contentTypeValue);
					}
				}
			}
			else
			{
				// "Content-Type" header not found.
				NetLog("[PSN WEBAPI] HTTP RESPONSE %d: (%s not found - cannot determine body)", httpStatusCode, CRY_WEBAPI_HEADER_CONTENT_TYPE);
			}
		}
	}
	else
	{
		// Unexpected server error
		NetLog("[PSN WEBAPI] HTTP RESPONSE %d: UNKNOWN ERROR", httpStatusCode);
		ret = SCE_NP_WEBAPI_ERROR_INVALID_HTTP_STATUS_CODE;
	}

	sceNpWebApiDeleteRequest(m_request.requestId);
	m_request.requestId = INVALID_PSN_WEBAPI_SENDREQUEST_ID;

	return ret;
}

// Theoretically, only the network thread will ever call this function, so it doesn't need to have a mutex lock to ensure unique ids.
CryWebApiJobId CCryPSNOrbisWebApiThread::GetUniqueJobId()
{
	CryWebApiJobId id = sm_nextFreeJobId++;
	return id;
}

//////////////////////////////////////////////////////////////////////////////////////////////

int CCryPSNWebApiUtility::Initialise(int webApiCtxId, CCryPSNSupport* pSupport)
{
	m_pSupport = pSupport;
	m_pLobby = pSupport->GetLobby();
	m_debugJsonAllocs = 0;

	sce::Json::InitParameter jsonInitParams((sce::Json::MemAllocator*)this, this, 512);
	Initializer.initialize(&jsonInitParams);

	m_webApiCtxId = webApiCtxId;
	m_pushEventFilterId = INVALID_PSN_WEBAPI_PUSHEVENT_ID;
	m_servicePushEventFilterId = INVALID_PSN_WEBAPI_PUSHEVENT_ID;

	int ret;

	int numPushFilters = 2;
	SceNpWebApiPushEventDataType pushFilterInfo[numPushFilters];
	memset(pushFilterInfo, 0, sizeof(SceNpWebApiPushEventDataType) * numPushFilters);
	cry_sprintf(pushFilterInfo[0].val, "np:service:friendlist:friend");
	cry_sprintf(pushFilterInfo[1].val, "np:service:presence:onlineStatus");

	ret = sceNpWebApiCreatePushEventFilter(webApiCtxId, pushFilterInfo, numPushFilters);
	if (ret >= PSN_OK)
	{
		m_pushEventFilterId = ret;
	}
	else
	{
		NetLog("sceNpWebApiCreatePushEventFilter : Error %08X", ret);
		return ret;
	}

	ret = sceNpWebApiCreateHandle(webApiCtxId);
	if (ret >= PSN_OK)
	{
		int servicePushHandle = ret;
		int numServicePushFilters = 1;
		SceNpWebApiPushEventDataType servicePushFilterInfo[numServicePushFilters];
		memset(servicePushFilterInfo, 0, sizeof(SceNpWebApiPushEventDataType) * numServicePushFilters);
		cry_sprintf(servicePushFilterInfo[0].val, "np:service:invitation");

		/*		ret = sceNpWebApiCreateServicePushEventFilter(webApiCtxId, servicePushHandle, "", SCE_NP_DEFAULT_SERVICE_LABEL, servicePushFilterInfo, 1);
		    if (ret >= PSN_OK)
		    {
		      m_servicePushEventFilterId = ret;		}
		    else
		    {
		      NetLog("sceNpWebApiCreateServicePushEventFilter : Error %08X", ret);
		    }
		 */
		sceNpWebApiDeleteHandle(webApiCtxId, servicePushHandle);
	}
	else
	{
		NetLog("sceNpWebApiCreateHandle : Error %08X", ret);
		return ret;
	}

	m_bIsInitialised = true;

	return PSN_OK;
}

void CCryPSNWebApiUtility::Terminate()
{
	int ret;

	if (m_servicePushEventFilterId != INVALID_PSN_WEBAPI_PUSHEVENT_ID)
	{
		ret = sceNpWebApiDeleteServicePushEventFilter(m_webApiCtxId, m_servicePushEventFilterId);
		m_servicePushEventFilterId = INVALID_PSN_WEBAPI_PUSHEVENT_ID;
	}
	if (m_pushEventFilterId != INVALID_PSN_WEBAPI_PUSHEVENT_ID)
	{
		ret = sceNpWebApiDeletePushEventFilter(m_webApiCtxId, m_pushEventFilterId);
		m_pushEventFilterId = INVALID_PSN_WEBAPI_PUSHEVENT_ID;
	}

	Initializer.terminate();
	m_bIsInitialised = false;
}

TMemHdl CCryPSNWebApiUtility::NewResponseBody(char* debugString)
{
	TMemHdl returnHdl = TMemInvalidHdl;

	if (m_pLobby)
	{
		returnHdl = m_pLobby->MemAlloc(sizeof(SCryPSNWebApiResponseBody));
		if (returnHdl != TMemInvalidHdl)
		{
			SCryPSNWebApiResponseBody* pResponseBody = (SCryPSNWebApiResponseBody*)m_pLobby->MemGetPtr(returnHdl);
			new(&pResponseBody->jsonTreeRoot) sce::Json::Value;
			pResponseBody->jsonTreeRoot.clear();
			pResponseBody->rawResponseBodyHdl = TMemInvalidHdl;
			pResponseBody->rawResponseBodySize = 0;
			pResponseBody->eType = SCryPSNWebApiResponseBody::E_UNKNOWN;

		#if !defined(_RELEASE)
			cry_strcpy(pResponseBody->debugStr, debugString);
		#endif

			CryLog("[WEBAPI] NEW WEBAPI RESPONSE BODY(initialised=%s) - Hdl %p, Ptr %p, \"%s\"",
			       m_bIsInitialised ? "true" : "false",
			       returnHdl, pResponseBody,
		#if !defined(_RELEASE)
			       pResponseBody->debugStr
		#else
			       ""
		#endif
			       );
		}
	}

	return returnHdl;
}

int CCryPSNWebApiUtility::FreeResponseBody(TMemHdl responseBodyHdl)
{
	if (m_pLobby)
	{
		if (responseBodyHdl != TMemInvalidHdl)
		{
			SCryPSNWebApiResponseBody* pResponseBody = (SCryPSNWebApiResponseBody*)m_pLobby->MemGetPtr(responseBodyHdl);
			if (pResponseBody)
			{
				CryLog("[WEBAPI] FREE WEBAPI RESPONSE BODY(initialised=%s) - Hdl %p, Ptr %p \"%s\" - type %d has jsonroot %s, raw %s (%d bytes)",
				       m_bIsInitialised ? "true" : "false",
				       responseBodyHdl, pResponseBody,
		#if !defined(_RELEASE)
				       pResponseBody->debugStr,
		#else
				       "",
		#endif
				       pResponseBody->eType,
				       (pResponseBody->jsonTreeRoot.getType() == sce::Json::kValueTypeObject) ? "object" : "null",
				       (pResponseBody->rawResponseBodyHdl != TMemInvalidHdl) ? "pointer" : "null",
				       (uint32)pResponseBody->rawResponseBodySize
				       );

				if (m_bIsInitialised)
				{
					// don't try to delete json if allocator is not initialised.
					// Might happen if utility was reset while a response was still in the work queue.
					// Should be ok because the reset will have released all the json allocations already.
					pResponseBody->jsonTreeRoot.clear();
					pResponseBody->jsonTreeRoot.~Value();
				}

				if (pResponseBody->rawResponseBodyHdl != TMemInvalidHdl)
				{
					m_pLobby->MemFree(pResponseBody->rawResponseBodyHdl);
					pResponseBody->rawResponseBodyHdl = TMemInvalidHdl;
					pResponseBody->rawResponseBodySize = 0;
				}
			}

			m_pLobby->MemFree(responseBodyHdl);

			return PSN_OK;
		}
	}

	return CRY_WEBAPI_ERROR_FAIL;
}

TMemHdl CCryPSNWebApiUtility::AllocMem(size_t numBytes)
{
	if (m_pLobby)
	{
		return m_pLobby->MemAlloc(numBytes);
	}
	return TMemInvalidHdl;
}

void CCryPSNWebApiUtility::FreeMem(TMemHdl hdl)
{
	if (m_pLobby)
	{
		return m_pLobby->MemFree(hdl);
	}
}

// From sce::Json::MemAllocator - Only used by Json library
void* CCryPSNWebApiUtility::allocate(size_t numBytes, void* pUserData)
{
	CCryPSNWebApiUtility* _this = static_cast<CCryPSNWebApiUtility*>(pUserData);

	if (_this->m_pLobby)
	{
		TMemHdl hdl = _this->AllocMem(numBytes);
		if (hdl != TMemInvalidHdl)
		{
			//		  CryLog("Json allocated 0x%08X %d bytes. (%d)", hdl, numBytes, ++_this->m_debugJsonAllocs);
			return _this->m_pLobby->MemGetPtr(hdl);
		}
	}
	return NULL;
}

// From sce::Json::MemAllocator
void CCryPSNWebApiUtility::deallocate(void* ptr, void* pUserData)
{
	CCryPSNWebApiUtility* _this = static_cast<CCryPSNWebApiUtility*>(pUserData);

	if (_this->m_pLobby)
	{
		_this->FreeMem(_this->m_pLobby->MemGetHdl(ptr));
		//		CryLog("Json free 0x%08X. (%d)", ptr, --_this->m_debugJsonAllocs);
	}
}

int CCryPSNWebApiUtility::PrintResponseJSONTree(const SCryPSNWebApiResponseBody* pResponseBody)
{
	SPrintJSONIndent indent;
	DebugPrintJSONValue(pResponseBody->jsonTreeRoot, indent);

	return PSN_OK;
}

void CCryPSNWebApiUtility::DebugPrintJSONObject(const sce::Json::Object& obj, SPrintJSONIndent& indent)
{
	sce::Json::String str;
	sce::Json::Value val;

	NetLog("[JSON]%s (Object) { %s", indent.c_str(), str.c_str());
	++indent;

	for (sce::Json::Object::const_iterator it = obj.begin(); it != obj.end(); ++it)
	{
		str = it->first;
		val = it->second;

		NetLog("[JSON]%s (Key) \"%s\"", indent.c_str(), str.c_str());
		DebugPrintJSONValue(val, indent);
	}

	--indent;
	NetLog("[JSON]%s }", indent.c_str());
}

void CCryPSNWebApiUtility::DebugPrintJSONArray(const sce::Json::Array& ary, SPrintJSONIndent& indent)
{
	NetLog("[JSON]%s (Array) [", indent.c_str());
	++indent;

	int n = 0;
	sce::Json::Value val;
	for (sce::Json::Array::const_iterator it = ary.begin(); it != ary.end(); ++it)
	{
		val = (*it);
		DebugPrintJSONValue(val, indent);
	}

	--indent;
	NetLog("[JSON]%s ]", indent.c_str());
}

void CCryPSNWebApiUtility::DebugPrintJSONValue(const sce::Json::Value& val, SPrintJSONIndent& indent)
{
	switch (val.getType())
	{
	case sce::Json::kValueTypeObject:
		{
			DebugPrintJSONObject(val.getObject(), indent);
		}
		break;
	case sce::Json::kValueTypeArray:
		{
			DebugPrintJSONArray(val.getArray(), indent);
		}
		break;
	case sce::Json::kValueTypeInteger:
		{
			NetLog("[JSON]%s (Int) %ld", indent.c_str(), val.getInteger());
		}
		break;
	case sce::Json::kValueTypeUInteger:
		{
			NetLog("[JSON]%s (Uint) %ld", indent.c_str(), val.getUInteger());
		}
		break;
	case sce::Json::kValueTypeReal:
		{
			NetLog("[JSON]%s (Real) %f", indent.c_str(), val.getReal());
		}
		break;
	default:
		{
			NetLog("[JSON]%s (String) \"%s\"", indent.c_str(), val.toString().c_str());
		}
		break;
	}
}

char* CCryPSNWebApiUtility::Base64Encode(const uint8* buffer, int len, TMemHdl& hdl)
{
	if (!buffer || (len <= 0))
	{
		return "";
	}

	static const char base64Dict[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	int b64Len = ((len + 2) / 3) * 4;
	hdl = AllocMem(b64Len + 1);
	char* b64Buf = (char*)m_pLobby->MemGetPtr(hdl);
	memset(b64Buf, '=', b64Len);
	int byteCount = 0;

	for (int i = 0; i < b64Len; i += 4)
	{
		size_t remainder = len - byteCount;
		uint8 temp[3];
		temp[0] = (byteCount < len) ? buffer[byteCount++] : 0;
		temp[1] = (byteCount < len) ? buffer[byteCount++] : 0;
		temp[2] = (byteCount < len) ? buffer[byteCount++] : 0;

		uint8 val = 0;

		val = temp[0] >> 2;
		b64Buf[i] = base64Dict[val];

		val = (temp[0] & 0x03) << 4;
		val |= temp[1] >> 4;
		b64Buf[i + 1] = base64Dict[val];

		val = (temp[1] & 0x0f) << 2;
		val |= temp[2] >> 6;

		if (remainder >= 2)
		{
			b64Buf[i + 2] = base64Dict[val];

			val = temp[2] & 0x3f;

			if (remainder >= 3)
			{
				b64Buf[i + 3] = base64Dict[val];
			}
		}
	}

	b64Buf[b64Len] = '\0';

	return b64Buf;
}

uint8* CCryPSNWebApiUtility::Base64Decode(const char* buffer, int b64Len, TMemHdl& hdl)
{
	if (!buffer || (b64Len <= 0))
	{
		return NULL;
	}

	static const std::string base64Dict = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	int len = (b64Len / 4) * 3;
	hdl = AllocMem(len);
	uint8* uBuf = (uint8*)m_pLobby->MemGetPtr(hdl);
	memset(uBuf, 0, len);

	int byteCount = 0;

	for (int i = 0; i < len; i += 3)
	{
		char temp[4];
		temp[0] = (byteCount < b64Len) ? base64Dict.find(buffer[byteCount++]) : -1;
		temp[1] = (byteCount < b64Len) ? base64Dict.find(buffer[byteCount++]) : -1;
		temp[2] = (byteCount < b64Len) ? base64Dict.find(buffer[byteCount++]) : -1;
		temp[3] = (byteCount < b64Len) ? base64Dict.find(buffer[byteCount++]) : -1;

		char c = 0;

		if (temp[0] != -1 && temp[1] != -1)
		{
			c = (temp[0] << 2);
			c |= (temp[1] & 0x30) >> 4;
			uBuf[i] = c;

			if (temp[2] != -1)
			{
				c = (temp[1] & 0x0f) << 4;
				c |= (temp[2] & 0x3c) >> 2;
				uBuf[i + 1] = c;

				if (temp[3] != -1)
				{
					c = (temp[2] & 0x03) << 6;
					c |= temp[3];
					uBuf[i + 2] = c;
				}
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	return uBuf;
}

//////////////////////////////////////////////////////////////////////////////////////////////

int CCryPSNWebApiJobController::Initialise(CCryPSNWebApiUtility* pUtility, SceNpId npId)
{
	m_pWebApiUtility = pUtility;
	m_pWebApiThread = NULL;

	m_webApiUserCtxId = INVALID_PSN_WEBAPI_CONTEXT;
	m_pushEventCallbackId = INVALID_PSN_WEBAPI_PUSHEVENT_ID;
	m_servicePushEventCallbackId = INVALID_PSN_WEBAPI_PUSHEVENT_ID;

	if (!m_pWebApiUtility)
	{
		return CRY_WEBAPI_ERROR_FAIL;
	}

	if (!m_pWebApiThread)
	{
		m_pWebApiThread = new CCryPSNOrbisWebApiThread(this);
		if (m_pWebApiThread)
		{
			if (!gEnv->pThreadManager->SpawnThread(m_pWebApiThread, "COrbisWebApiThread"))
			{
				CRY_ASSERT_MESSAGE(false, "Error spawning \"COrbisWebApiThread\" thread.");
				__debugbreak();
				delete m_pWebApiThread;
				m_pWebApiThread = NULL;
				return CRY_WEBAPI_ERROR_FAIL;
			}
		}
		else
		{
			NetLog("Cannot create new COrbisWebApiThread");
			return CRY_WEBAPI_ERROR_FAIL;
		}
	}

	int ret;
	int webApiCtxId = m_pWebApiUtility->GetWebApiContextId();
	int pushEventFilterId = m_pWebApiUtility->GetWebApiPushEventFilterId();
	int servicePushEventFilterId = m_pWebApiUtility->GetWebApiServicePushEventFilterId();

	ret = sceNpWebApiCreateContext(webApiCtxId, &npId.handle);
	if (ret >= PSN_OK)
	{
		m_webApiUserCtxId = ret;
	}
	else
	{
		NetLog("sceNpWebApiCreateContext : Error %08X", ret);
		return ret;
	}

	ret = sceNpWebApiRegisterPushEventCallback(m_webApiUserCtxId, pushEventFilterId, PushEventCallback, this);
	if (ret >= PSN_OK)
	{
		m_pushEventCallbackId = ret;
	}
	else
	{
		NetLog("sceNpWebApiRegisterPushEventCallback : Error %08X", ret);
		return ret;
	}

	ret = sceNpWebApiRegisterServicePushEventCallback(m_webApiUserCtxId, servicePushEventFilterId, ServicePushEventCallback, this);
	if (ret >= PSN_OK)
	{
		m_servicePushEventCallbackId = ret;
	}
	else
	{
		NetLog("sceNpWebApiRegisterServicePushEventCallback : Error %08X", ret);
		return ret;
	}

	m_bIsInitialised = true;

	return PSN_OK;
}

int CCryPSNWebApiJobController::Terminate()
{
	int ret;

	if (m_pWebApiThread)
	{
		m_pWebApiThread->SignalStopWork();
		gEnv->pThreadManager->JoinThread(m_pWebApiThread, eJM_Join);

		m_pWebApiThread->Flush();
		SAFE_DELETE(m_pWebApiThread);
	}

	if (m_servicePushEventCallbackId != INVALID_PSN_WEBAPI_PUSHEVENT_ID)
	{
		ret = sceNpWebApiUnregisterServicePushEventCallback(m_webApiUserCtxId, m_servicePushEventCallbackId);
		m_servicePushEventCallbackId = INVALID_PSN_WEBAPI_PUSHEVENT_ID;
	}
	if (m_pushEventCallbackId != INVALID_PSN_WEBAPI_PUSHEVENT_ID)
	{
		ret = sceNpWebApiUnregisterPushEventCallback(m_webApiUserCtxId, m_pushEventCallbackId);
		m_pushEventCallbackId = INVALID_PSN_WEBAPI_PUSHEVENT_ID;
	}
	if (m_webApiUserCtxId != INVALID_PSN_WEBAPI_CONTEXT)
	{
		ret = sceNpWebApiDeleteContext(m_webApiUserCtxId);
		m_webApiUserCtxId = INVALID_PSN_WEBAPI_CONTEXT;
	}

	m_pWebApiUtility = NULL;
	m_bIsInitialised = false;

	return ret;
}

CryWebApiJobId CCryPSNWebApiJobController::AddJob(TCryPSNWebApiThreadJobFunc job, TMemHdl paramsHdl)
{
	// For now, there's only one work thread per Controller, so just push to that thread
	// In the future, if multiple threads, spread load over the available threads.
	if (m_bIsInitialised && m_pWebApiThread && m_pWebApiUtility)
	{
		return m_pWebApiThread->Push(job, paramsHdl);
	}

	return INVALID_WEBAPI_JOB_ID;
}

void CCryPSNWebApiJobController::PushEventCallback(int webApiUserCtxId, int callbackId,
                                                   const SceNpPeerAddressA* pTo, const SceNpPeerAddressA* pFrom,
                                                   const SceNpWebApiPushEventDataType* pDataType,
                                                   const char* pData, size_t dataSize, void* pArg)
{
	CCryPSNWebApiJobController* _this = static_cast<CCryPSNWebApiJobController*>(pArg);
	NetLog("Incoming PushEventCallback for user %d, type %s, from %s", webApiUserCtxId, pDataType->val, pFrom->onlineId.data);

	if (_this->m_pWebApiThread)
	{
		//		_this->m_pWebApiThread->Push(GetFriendsList, TMemInvalidHdl);
	}
}

void CCryPSNWebApiJobController::ServicePushEventCallback(int webApiUserCtxId, int callbackId, const char* pNpServiceName, SceNpServiceLabel npServiceLabel,
                                                          const SceNpPeerAddressA* pTo, const SceNpPeerAddressA* pFrom,
                                                          const SceNpWebApiPushEventDataType* pDataType,
                                                          const char* pData, size_t dataSize, void* pArg)
{
	CCryPSNWebApiJobController* _this = static_cast<CCryPSNWebApiJobController*>(pArg);
	NetLog("Incoming ServicePushEventCallback for user %d, type %s, from %s", webApiUserCtxId, pDataType->val, pFrom->onlineId.data);
}

//////////////////////////////////////////////////////////////////////////////////////////////

	#endif //USE_PSN
#endif   // CRY_PLATFORM_ORBIS
