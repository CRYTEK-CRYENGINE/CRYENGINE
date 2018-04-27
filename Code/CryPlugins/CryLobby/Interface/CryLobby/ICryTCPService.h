// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryRenderer/Tarray.h>

enum ECryTCPServiceConnectionStatus
{
	eCTCPSCS_Pending,
	eCTCPSCS_Connected,
	eCTCPSCS_NotConnected,
	eCTCPSCS_NotConnectedUserNotSignedIn,
	eCTCPSCS_NotConnectedDNSFailed,
	eCTCPSCS_NotConnectedConnectionFailed
};

//! May be expanded in future, at present when callback fires data will be de-queued immediately after.
enum ECryTCPServiceResult
{
	eCTCPSR_Ok,       //!< No errors in sending data to external site.
	eCTCPSR_Failed    //!< Some sort of error occurred (likely to be a fail in the socket send).
};

struct STCPServiceData;
typedef _smart_ptr<STCPServiceData> STCPServiceDataPtr;

// Only one of the callback members of an STCPServiceData should be non-NULL.

//! Callback for simple TCP data transfers that can be held in a single buffer and do not require a reply.
//! This will be called when all data has been sent or when an error occurs.
//! \param res result.
//! \param pArg user data.
typedef void (* CryTCPServiceCallback)(ECryTCPServiceResult res, void* pArg);

//! Callback for TCP data transfers that can be held in a single buffer and do
//! require a reply. This will be called when all data has been sent, when an
//! error occurs, or when data is received.
//! \param res result.
//! \param pArg user data.
//! \param pData received data.
//! \param dataLen length of received data.
//! \param endOfStream has the end of the reply been reached?.
//! \return Ignored when called on error or completion of send. When called on data
//!         received, return false to keep the socket open and wait for more data or
//!         true to close the socket.
typedef bool (* CryTCPServiceReplyCallback)(ECryTCPServiceResult res, void* pArg, STCPServiceDataPtr pUploadData, const char* pReplyData, size_t replyDataLen, bool endOfStream);

struct STCPServiceData : public CMultiThreadRefCount
{
	STCPServiceData()
		: tcpServCb(NULL),
		tcpServReplyCb(NULL),
		m_quietTimer(0.0f),
		m_socketIdx(-1),
		pUserArg(NULL),
		pData(NULL),
		length(0),
		sent(0),
		ownsData(true)
	{};

	~STCPServiceData()
	{
		if (ownsData)
		{
			SAFE_DELETE_ARRAY(pData);
		}
	};

	CryTCPServiceCallback      tcpServCb;      //!< Callback function to indicate success/failure of posting.
	CryTCPServiceReplyCallback tcpServReplyCb; //!< Callback function to receive reply.
	float                      m_quietTimer;   //!< Time in seconds since data was last sent or received for this data packet. timer is only incremented once a socket is allocated and the transaction begins.
	int32                      m_socketIdx;
	void*                      pUserArg;       //!< Application specific callback data.
	char*                      pData;          //!< Pointer to data to upload.
	size_t                     length;         //!< Length of data.
	size_t                     sent;           //!< Data sent.
	bool                       ownsData;       //!< should pData be deleted when we're finished with it.
};

struct ICryTCPService
{
	// <interfuscator:shuffle>
	virtual ~ICryTCPService(){}

	//! \param isDestructing - is this objects destructor running.
	//! \return Result code.
	virtual ECryTCPServiceResult Terminate(bool isDestructing) = 0;

	//! Has the address resolved?
	//! \return true if the address has resolved, otherwise false.
	virtual bool HasResolved() = 0;

	//! Queue a transaction.
	//! \param pData - transaction.
	//! \return true if transaction was successfully queued, otherwise false
	virtual bool UploadData(STCPServiceDataPtr pData) = 0;

	//! \param delta - time delta from last tick.
	virtual void Tick(CTimeValue delta) = 0;

	//! Increment reference count.
	//! \return Reference count.
	virtual int AddRef() = 0;

	//! Decrement reference count.
	//! \return Reference count.
	virtual int Release() = 0;

	//! Get the current connection status.
	//! \return Status.
	virtual ECryTCPServiceConnectionStatus GetConnectionStatus() = 0;

	//! Get the number of items currently in the data queue.
	//! \return Number of items.
	virtual uint32 GetDataQueueLength() = 0;

	//! Get the total data size currently in the data queue.
	//! \return Number of bytes.
	virtual uint32 GetDataQueueSize() = 0;

	//! Get the server name.
	//! \return Server name.
	virtual const char* GetServerName() = 0;

	//! Get the port.
	//! \return Port
	virtual uint16 GetPort() = 0;

	//! Get the server path.
	//! \return Server path.
	virtual const char* GetURLPrefix() = 0;
	// </interfuscator:shuffle>
};

typedef _smart_ptr<ICryTCPService> ICryTCPServicePtr;
