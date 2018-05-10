// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   Handles Orbis Web API. Creates a worker thread to process blocking GET requests, also registers PushEvent listener.
   All responses get pushed onto the m_lobbyThreadQueueBuilding queue to be processed on the network thread.
 */

#ifndef __CRYPSN2_WEBAPI_H__
#define __CRYPSN2_WEBAPI_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

		#include <json2.h>

class CCryPSNSupport;
class CCryPSNOrbisWebApiThread;
class CCryPSNWebApiUtility;
class CCryPSNWebApiJobController;

		#define INVALID_PSN_WEBAPI_PUSHEVENT_ID           (-1)
		#define INVALID_PSN_WEBAPI_SENDREQUEST_ID         (-1)
		#define INVALID_WEBAPI_JOB_ID                     (-1)

		#define CRY_WEBAPI_ERROR_FAIL                     (-1)
		#define CRY_WEBAPI_ERROR_END_OF_BODY              (-2)
		#define CRY_WEBAPI_ERROR_SEND_ALREADY_IN_PROGRESS (-3)
		#define CRY_WEBAPI_ERROR_NO_SEND_IN_PROGRESS      (-4)
		#define CRY_WEBAPI_ERROR_BAD_CONTENTLENGTH        (-5)

		#define CRY_WEBAPI_HEADER_CONTENT_TYPE            "Content-Type"
		#define CRY_WEBAPI_HEADER_CONTENT_LENGTH          "Content-Length"
		#define CRY_WEBAPI_CONTENT_TYPE_JSON              "application/json"

		#define CRY_WEBAPI_HEADER_VALUE_SIZE              (64)
		#define CRY_WEBAPI_TEMP_PATH_SIZE                 (256)
		#define CRY_WEBAPI_TEMP_BUFFER_SIZE               (256)

		#if !defined(_RELEASE)
			#define CRY_WEBAPI_RESPONSE_DEBUG_STR_LEN (64)
		#endif

struct SCryPSNWebApiResponseBody
{
	enum EResponseBodyType
	{
		E_UNKNOWN = 0,
		E_JSON,
		E_RAW,
	};

	sce::Json::Value  jsonTreeRoot;
	TMemHdl           rawResponseBodyHdl;
	size_t            rawResponseBodySize;
	EResponseBodyType eType;

		#if !defined(_RELEASE)
	char debugStr[CRY_WEBAPI_RESPONSE_DEBUG_STR_LEN];
		#endif
};

// WebAPI has its own threads, with queues, because all calls are blocking.
typedef int32   CryWebApiJobId;
typedef void (* TCryPSNWebApiThreadJobFunc)(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);

struct SCryPSNWebApiThreadJobInfo
{
	TCryPSNWebApiThreadJobFunc m_pJobFunc;
	TMemHdl                    m_paramsHdl;
	CryWebApiJobId             m_id;

	SCryPSNWebApiThreadJobInfo(CryWebApiJobId id, TCryPSNWebApiThreadJobFunc pFunc, TMemHdl paramsHdl)
	{
		m_id = id;
		m_pJobFunc = pFunc;
		m_paramsHdl = paramsHdl;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////

		#define WEBAPI_WORKBUFFER_MAX_SIZE (4 * 1024)

class CCryPSNOrbisWebApiThread
	: public IThread
{
public:
	CCryPSNOrbisWebApiThread(CCryPSNWebApiJobController* pController)
		: m_pController(pController)
		, m_bAlive(true)
	{
		memset(&m_request, 0, sizeof(m_request));
		m_request.requestId = INVALID_PSN_WEBAPI_SENDREQUEST_ID;
		m_serverError.Reset();
	}

	// Start accepting work on thread
	virtual void   ThreadEntry();       // IThread
	void           SignalStopWork();

	CryWebApiJobId Push(TCryPSNWebApiThreadJobFunc pFunc, TMemHdl paramsHdl);
	void           Flush();

	// Add new WebApi request jobs below here
	// FriendsList
	static void GetFriendsList(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	// Presence
	static void SetPresence(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	// RecentPlayers
	static void AddRecentPlayers(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	// ActivityFeed
	static void PostActivity(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	// Session advertising
	static void CreateSession(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	static void DeleteSession(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	static void UpdateSession(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	static void JoinSession(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	static void LeaveSession(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	static void GetSessionLinkData(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	// PSN Store
	static void GetCommerceList(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	static void GetEntitlementList(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);
	static void ConsumeEntitlement(CryWebApiJobId id, TMemHdl paramsHdl, CCryPSNWebApiJobController* pController, CCryPSNOrbisWebApiThread* _this);

private:

	struct SRequestAndResponse
	{
		int64  requestId;
		char   workBuffer[WEBAPI_WORKBUFFER_MAX_SIZE];
		uint32 nRecvBufferUsed;
		uint32 nRecvBufferParsed;
	};

	struct SServerError
	{
		SceNpWebApiResponseInformationOption serverError;
		#ifdef _DEBUG
		static const int                     kErrorBufSize = 256;
		char serverErrorBuffer[kErrorBufSize]; // This buffer can be inspected in the debugger, it will contain JSON error data
		#endif

		void Reset()
		{
			memset(this, 0, sizeof(SServerError));
		#ifdef _DEBUG
			serverError.pErrorObject = serverErrorBuffer;
			serverError.errorObjectSize = sizeof(serverErrorBuffer);
		#endif
		}
	};

	static CryWebApiJobId                  sm_nextFreeJobId;

	sce::Json::Parser                      Parser;
	SRequestAndResponse                    m_request;
	SServerError                           m_serverError;
	CCryPSNWebApiJobController*            m_pController;
	CryMutex                               m_mtx;
	CryConditionVariable                   m_cond;
	std::deque<SCryPSNWebApiThreadJobInfo> m_jobQueue;
	bool m_bAlive;

	int            SendRequest(SceNpWebApiHttpMethod httpMethod, const char* pContentType, const char* pAPIGroup, const char* pPath);
	int            SendRequestText(SceNpWebApiHttpMethod httpMethod, const char* pContentType, const char* pAPIGroup, const char* pPath, const char* pBody);
	int            SendRequestData(SceNpWebApiHttpMethod httpMethod, const char* pContentType, const char* pAPIGroup, const char* pPath, const void* pData, size_t sizeofData);
	int            StoreRawResponseBody(SCryPSNWebApiResponseBody* pResponseBody);
	int            ReadResponse(SCryPSNWebApiResponseBody* pResponseBody);

	static int     DataProvideCallback(char& data, void* pWebAPIJobThread);

	CryWebApiJobId GetUniqueJobId();
};

//////////////////////////////////////////////////////////////////////////////////////////////

		#define WEBAPI_JSON_DEBUG_INDENTS (64)

class CCryPSNWebApiUtility : public sce::Json::MemAllocator
{
protected:

	struct SPrintJSONIndent
	{
		uint32 m_debugIndent;
		char   m_debugIndentString[WEBAPI_JSON_DEBUG_INDENTS + 1];

		SPrintJSONIndent()
			: m_debugIndent(0)
		{
			memset(m_debugIndentString, ' ', WEBAPI_JSON_DEBUG_INDENTS);
			m_debugIndentString[m_debugIndent] = '\0';
			m_debugIndentString[WEBAPI_JSON_DEBUG_INDENTS] = '\0';
		}
		void operator++(void)
		{
			if (m_debugIndent <= WEBAPI_JSON_DEBUG_INDENTS - 2)
			{
				m_debugIndentString[m_debugIndent] = ' ';
				m_debugIndent += 2;
				m_debugIndentString[m_debugIndent] = '\0';
			}
		}
		void operator--(void)
		{
			if (m_debugIndent >= 2)
			{
				m_debugIndentString[m_debugIndent] = ' ';
				m_debugIndent -= 2;
				m_debugIndentString[m_debugIndent] = '\0';
			}
		}
		const char* c_str() const
		{
			return m_debugIndentString;
		}
	};

	sce::Json::Initializer Initializer;
	CCryPSNSupport*        m_pSupport;
	CCryLobby*             m_pLobby;

	int                    m_webApiCtxId;
	int                    m_pushEventFilterId;
	int                    m_servicePushEventFilterId;

	bool                   m_bIsInitialised;

	int                    m_debugJsonAllocs;

	// utility for printing a json tree
	void DebugPrintJSONObject(const sce::Json::Object& obj, SPrintJSONIndent& indent);
	void DebugPrintJSONArray(const sce::Json::Array& ary, SPrintJSONIndent& indent);
	void DebugPrintJSONValue(const sce::Json::Value& val, SPrintJSONIndent& indent);

public:

	CCryPSNWebApiUtility()
		: m_pSupport(nullptr)
		, m_pLobby(nullptr)
		, m_webApiCtxId(0)
		, m_pushEventFilterId(0)
		, m_servicePushEventFilterId(0)
		, m_bIsInitialised(false)
		, m_debugJsonAllocs(0)
	{
	}
	~CCryPSNWebApiUtility() { Terminate(); }

	int             Initialise(int webApiCtxId, CCryPSNSupport* pSupport);
	void            Terminate();
	bool            IsInitialised()                           { return m_bIsInitialised; }

	int             GetWebApiContextId() const                { return m_webApiCtxId; }
	int             GetWebApiPushEventFilterId() const        { return m_pushEventFilterId; }
	int             GetWebApiServicePushEventFilterId() const { return m_servicePushEventFilterId; }

	CCryPSNSupport* GetPSNSupport() const                     { return m_pSupport; }
	CCryLobby*      GetLobby() const                          { return m_pLobby; }

	// utility for printing a json tree
	int     PrintResponseJSONTree(const SCryPSNWebApiResponseBody* pResponseBody);

	TMemHdl NewResponseBody(char* debugString);
	int     FreeResponseBody(TMemHdl responseBodyHdl);

	TMemHdl AllocMem(size_t numBytes);
	void    FreeMem(TMemHdl hdl);

	// From sce::Json::MemAllocator - Only used by Json library
	virtual void* allocate(size_t numBytes, void* pUserData);
	virtual void  deallocate(void* ptr, void* pUserData);

	// utility for encoding and decoding base64
	char*  Base64Encode(const uint8* buffer, int len, TMemHdl& hdl);
	uint8* Base64Decode(const char* buffer, int len, TMemHdl& hdl);
};

//////////////////////////////////////////////////////////////////////////////////////////////

class CCryPSNWebApiJobController
{
protected:

	CCryPSNWebApiUtility*     m_pWebApiUtility;
	CCryPSNOrbisWebApiThread* m_pWebApiThread;        // 1 for now

	int                       m_webApiUserCtxId;
	int                       m_pushEventCallbackId;
	int                       m_servicePushEventCallbackId;

	bool                      m_bIsInitialised;

	static void               PushEventCallback(int, int, const SceNpPeerAddressA*, const SceNpPeerAddressA*, const SceNpWebApiPushEventDataType*, const char*, size_t, void*);
	static void               ServicePushEventCallback(int, int, const char*, SceNpServiceLabel, const SceNpPeerAddressA*, const SceNpPeerAddressA*, const SceNpWebApiPushEventDataType*, const char*, size_t, void*);

public:

	CCryPSNWebApiJobController()
		: m_pWebApiUtility(nullptr)
		, m_pWebApiThread(nullptr)
		, m_webApiUserCtxId(0)
		, m_pushEventCallbackId(0)
		, m_servicePushEventCallbackId(0)
		, m_bIsInitialised(false)
	{
	}
	~CCryPSNWebApiJobController() { Terminate(); }

	int                   Initialise(CCryPSNWebApiUtility* pUtility, SceNpId npId);
	int                   Terminate();
	bool                  IsInitialised()      { return m_bIsInitialised; }

	int                   GetContextId() const { return m_webApiUserCtxId; }
	CCryPSNWebApiUtility* GetUtility() const   { return m_pWebApiUtility; }

	CryWebApiJobId        AddJob(TCryPSNWebApiThreadJobFunc job, TMemHdl paramsHdl);
};

//////////////////////////////////////////////////////////////////////////////////////////////

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS

#endif // __CRYPSN2_WEBAPI_H__
