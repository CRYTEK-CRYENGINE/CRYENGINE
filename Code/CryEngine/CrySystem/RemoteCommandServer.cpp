// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:  Remote command system implementation (server)
   -------------------------------------------------------------------------
   History:
   - 10/04/2013   : Tomasz Jonarski, Created

*************************************************************************/
#include "StdAfx.h"
#include <CryNetwork/IServiceNetwork.h>
#include "RemoteCommand.h"
#include "RemoteCommandHelpers.h"

//-----------------------------------------------------------------------------

// remote system internal logging
#ifdef RELEASE
	#define LOG_VERBOSE(level, txt, ...)
#else
	#define LOG_VERBOSE(level, txt, ...) if (GetManager()->CheckVerbose(level)) { GetManager()->Log(txt, __VA_ARGS__); }
#endif

//-----------------------------------------------------------------------------

CRemoteCommandServer::WrappedCommand::WrappedCommand(IRemoteCommand* pCommand, const uint32 commandId)
	: m_pCommand(pCommand)
	, m_refCount(1)
	, m_commandID(commandId)
{
}

CRemoteCommandServer::WrappedCommand::~WrappedCommand()
{
	CRY_ASSERT(m_refCount == 0);
	m_pCommand->Delete();
}

void CRemoteCommandServer::WrappedCommand::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CRemoteCommandServer::WrappedCommand::Release()
{
	if (0 == CryInterlockedDecrement(&m_refCount))
	{
		delete this;
	}
}

//-----------------------------------------------------------------------------

CRemoteCommandServer::Endpoint::Endpoint(CRemoteCommandManager* pManager, class CRemoteCommandServer* pServer, IServiceNetworkConnection* pConnection)
	: m_pConnection(pConnection)
	, m_pManager(pManager)
	, m_pServer(pServer)
	, m_lastReceivedCommand(0)
	, m_lastExecutedCommand(0)
	, m_lastReceivedCommandACKed(0)
	, m_lastExecutedCommandACKed(0)
	, m_bHasReceivedClassList(false)
{
}

CRemoteCommandServer::Endpoint::~Endpoint()
{
	// release commands that were not yet executed
	// this will release the command memory buffers (if they are not referenced elsewhere)
	while (!m_pCommandsToExecute.empty())
	{
		WrappedCommand* pCommand = m_pCommandsToExecute.pop();
		pCommand->Release();
	}

	// make sure the network connection is closed
	if (NULL != m_pConnection)
	{
		// send the disconnect message
		{
			CDataWriteStreamBuffer writer;

			// format messages
			PackedHeader header;
			header.magic = PackedHeader::kMagic;
			header.msgType = PackedHeader::eCommand_Disconnect;
			header.count = 0;
			writer << header;

			// Send the message
			IServiceNetworkMessage* pMessage = writer.BuildMessage();
			if (NULL != pMessage)
			{
				m_pConnection->SendMsg(pMessage);
				pMessage->Release();
			}
		}

		// close the connection (but try to send messages out)
		m_pConnection->FlushAndClose(IServiceNetworkConnection::kDefaultFlushTime);
		m_pConnection->Release();
		m_pConnection = NULL;
	}
}

const char* CRemoteCommandServer::Endpoint::GetClassName(const uint32 classId) const
{
	// class index is out of bounds
	if (classId >= m_pLocalClassFactories.size())
	{
		return "InvalidClassID";
	}

	// get class factory for the class ID
	IRemoteCommandClass* theClass = m_pLocalClassFactories[classId];
	if (NULL == theClass)
	{
		// ID is valid but we do not support this class
		// Can happen, usually due to version mismatch between client and server binaries
		return "UnsupportedClassID";
	}

	return theClass->GetName();
}

IRemoteCommand* CRemoteCommandServer::Endpoint::CreateObject(const uint32 classId) const
{
	// class index is out of bounds
	if (classId >= m_pLocalClassFactories.size())
	{
		return NULL;
	}

	// get class factory for given class index
	IRemoteCommandClass* theClass = m_pLocalClassFactories[classId];
	if (NULL == theClass)
	{
		// ID is valid but we do not support this class
		// Can happen, usually due to version mismatch between client and server binaries
		return NULL;
	}

	// use the class definition to create the instance of the remote command object
	return theClass->CreateObject();
}

void CRemoteCommandServer::Endpoint::Execute()
{
	uint32 idOfLastExecutedCommand = 0;

	// Process the commands on the execution list
	while (!m_pCommandsToExecute.empty())
	{
		// Pop the command from the stack
		WrappedCommand* pCommand = m_pCommandsToExecute.pop();

		LOG_VERBOSE(3, "Executing command '%s', ID %d",
		            pCommand->GetCommand()->GetClass()->GetName(),
		            pCommand->GetId());

		// Here is where the magic happens
		{
			pCommand->GetCommand()->Execute();
		}

		// Keep track of the command ID executed so far (so we can update the ACK later)
		CRY_ASSERT(pCommand->GetId() > idOfLastExecutedCommand);
		idOfLastExecutedCommand = pCommand->GetId();

		// Command was executed, we can release it
		pCommand->Release();
	}

	// Update the ACK data (if it's needed)
	if (idOfLastExecutedCommand != 0)
	{
		CryAutoLock<CryMutex> lock(m_accessLock);

		LOG_VERBOSE(3, "Updating LastExecutedCommandID %d->%d",
		            m_lastExecutedCommand,
		            idOfLastExecutedCommand);

		// Well, it only makes sens if the current command ID is greater that the last one executed
		CRY_ASSERT(idOfLastExecutedCommand > m_lastExecutedCommand);
		if (idOfLastExecutedCommand > m_lastExecutedCommand)
		{
			m_lastExecutedCommand = idOfLastExecutedCommand;
		}
	}
}

bool CRemoteCommandServer::Endpoint::Update()
{
	// Check connection status
	if (!m_pConnection->IsAlive())
	{
		// Signal the owner that this endpoint should be deleted
		return false;
	}

	// Receive and deserialize the commands
	// Note that this is done asynchronously so commands can be decoded even if the main thread is busy
	// Note that execution is DEFERRED to the main thread (I wouldn't risk doing it from this thread ;-))
	bool bDisconnectReceived = false;
	IServiceNetworkMessage* pMsg = m_pConnection->ReceiveMsg();
	while (NULL != pMsg && !bDisconnectReceived)
	{
		CDataReadStreamFormMessage reader(pMsg);

		// read back the packet header
		PackedHeader packetHeader;
		reader << packetHeader;

		// Is this a command system messages ?
		if (packetHeader.magic == PackedHeader::kMagic)
		{
			switch (packetHeader.msgType)
			{
			// Class list, usually sent as first thing after connection
			case PackedHeader::eCommand_ClassList:
				{
					// deserialize class names
					std::vector<string> classNames;
					reader << classNames;

					// sync the command ID to the current value on the client
					const uint32 firstCommandID = packetHeader.count;
					m_lastExecutedCommand = firstCommandID;
					m_lastExecutedCommandACKed = firstCommandID;
					m_lastReceivedCommand = firstCommandID;
					m_lastReceivedCommandACKed = firstCommandID;
					m_bHasReceivedClassList = true;

					LOG_VERBOSE(3, "Received class list packet, count=%d, first message=%d from '%s'",
					            classNames.size(),
					            packetHeader.count,
					            m_pConnection->GetRemoteAddress().ToString().c_str());

					// create class mapping between remote client and this server
					GetManager()->BuildClassMapping(classNames, m_pLocalClassFactories);
					break;
				}

			// Actual command packets
			case PackedHeader::eCommand_Command:
				{
					LOG_VERBOSE(3, "Received packet, count=%d from '%s'",
					            packetHeader.count,
					            m_pConnection->GetRemoteAddress().ToString().c_str());

					// load the serialized commands
					const uint32 numCommands = packetHeader.count;
					for (uint32 i = 0; i < numCommands; ++i)
					{
						// Each command is prefixed with header
						CommandHeader header;
						reader << header;

						// We must be able to skip to the end of the command data because sometimes
						// some data can be omitted - either by dropping the command altogether or
						// by faulty deserialization. Don't trust the user.
						const uint32 offset = reader.GetOffset();
						const uint32 endOffset = offset + header.size; // here is where we can skip

						LOG_VERBOSE(3, "Received command ID=%d (class id=%d, size=%d) from '%s'",
						            header.commandId,
						            header.classId,
						            header.size,
						            m_pConnection->GetRemoteAddress().ToString().c_str());

						// Do not process commands out of order.
						// This should not happen if network is in good health, but we cannot assume that, never-ever.
						// This code will cause our side to stop executing new commands until the remote side to resend the missing ones.
						// Typically it is better than executing commands out of order.
						const uint32 expectedNextCommand = m_lastReceivedCommand + 1;
						if (header.commandId > expectedNextCommand)
						{
							LOG_VERBOSE(0, "Out of order command ID (%d > %d) received from '%s'",
							            header.commandId,
							            expectedNextCommand,
							            m_pConnection->GetRemoteAddress().ToString().c_str());

							// next commands will be even older, no need to check them
							break;
						}

						// Do not process the old commands
						// This may happen pretty often when command is resent while the ACK is "in-flight"
						// Just drop the data and go on.
						if (header.commandId <= m_lastReceivedCommand)
						{
							// getting old command is not an error, it just means that we have large enough lag
							// that the client started resending old commands.
							LOG_VERBOSE(1, "Old command (%d <= %d) received from '%s'",
							            header.commandId,
							            m_lastReceivedCommand,
							            m_pConnection->GetRemoteAddress().ToString().c_str());
						}
						else
						{
							// next command received, we are very strict about matchig the command IDs here
							CRY_ASSERT(header.commandId == expectedNextCommand);
							m_lastReceivedCommand = expectedNextCommand;

							// create the command
							IRemoteCommand* pCommand = CreateObject(header.classId);
							if (NULL != pCommand)
							{
								// Fine-grain logging
								LOG_VERBOSE(3, "Received command '%s', classId=%d, commandId=%d from '%s'",
								            pCommand->GetClass()->GetName(),
								            header.classId,
								            header.commandId,
								            m_pConnection->GetRemoteAddress().ToString().c_str());

								// Deserialize the command data from network message
								pCommand->LoadFromStream(reader);

								// Add to list of commands to execute
								{
									m_pCommandsToExecute.push(new WrappedCommand(pCommand, header.commandId));
								}
							}
							else
							{
								LOG_VERBOSE(0, "ClassId %d not recognized. Skipping command ID%d from '%s'",
								            header.classId,
								            header.commandId,
								            m_pConnection->GetRemoteAddress().ToString().c_str());
							}

							// Update the last command ID
							m_lastReceivedCommand = header.commandId;
						}

						// Sync the message stream to popper position
						CRY_ASSERT(reader.GetOffset() <= endOffset);
						reader.SetPosition(endOffset);
					}

					break;
				}

			// request to disconnect (graceful)
			case PackedHeader::eCommand_Disconnect:
				{
					LOG_VERBOSE(3, "Received disconnect request from '%s'",
					            m_pConnection->GetRemoteAddress().ToString().c_str());

					m_pConnection->Close();
					bDisconnectReceived = true;

					break;
				}

			// should not happen
			default:
				{
					LOG_VERBOSE(0, "Invalid message type '%s' received from '%s'",
					            packetHeader.msgType,
					            m_pConnection->GetRemoteAddress().ToString().c_str());

					break;
				}
			}
		}
		else
		{
			// This is a raw message, try to process immediately using async listeners.
			// If it fails, add to the queue for processing on the main thread by sync listeners.
			m_pServer->ProcessRawMessageAsync(pMsg, m_pConnection);
		}

		// Release the message data
		pMsg->Release();

		// Get the next message from network
		if (!bDisconnectReceived)
		{
			pMsg = m_pConnection->ReceiveMsg();
		}
	}

	// The value of lastExecutedCommand can change outside this thread,
	// so capture it one and keep it constant for the duration of the logic in this function.
	const uint32 snapshotLastExecutedCommand = m_lastExecutedCommand;

	// Determine if we should send generate the ACK signal
	if ((snapshotLastExecutedCommand != m_lastExecutedCommandACKed) ||
	    (m_lastReceivedCommand != m_lastReceivedCommandACKed)) // this can
	{
		ResponseHeader header;
		header.magic = PackedHeader::kMagic;
		header.msgType = PackedHeader::eCommand_ACK;
		header.lastCommandReceived = m_lastReceivedCommand;
		header.lastCommandExecuted = snapshotLastExecutedCommand; // note that we use the captured values

		LOG_VERBOSE(3, "Sending ACK to '%s' with LastReceived=%d, LastExecuted=%d",
		            m_pConnection->GetRemoteAddress().ToString().c_str(),
		            header.lastCommandReceived,
		            header.lastCommandExecuted);

		// Write header into the message
		CDataWriteStreamBuffer writer;
		writer << header;

		// Extract the message
		IServiceNetworkMessage* pMessage = writer.BuildMessage();
		if (NULL != pMessage)
		{
			// Send it back over the connection (works as ACK)
			if (m_pConnection->SendMsg(pMessage))
			{
				// Only after the message is accepted by the network we can assume that we have ACKed it properly
				// This can still leave a possibility that this message gets eaten in the network but we will resend newer ACK
				// soon enough that we don't need to bother with this.
				m_lastExecutedCommandACKed = header.lastCommandExecuted;
				m_lastReceivedCommandACKed = header.lastCommandReceived;
			}

			pMessage->Release();
		}
	}

	// Continue
	return true;
}

//-----------------------------------------------------------------------------

CRemoteCommandServer::CRemoteCommandServer(CRemoteCommandManager* pManager, IServiceNetworkListener* pListener)
	: m_pManager(pManager)
	, m_pListener(pListener)
	, m_bCloseThread(false)
	, m_suppressionCounter(0)
	, m_bIsSuppressed(false)
{
	// Start processing thread (receiving from network, deserialization, etc)
	if (!gEnv->pThreadManager->SpawnThread(this, "RemoteCommandServer"))
	{
		CryFatalError("Error spawning \"RemoteCommandServer\" thread.");
	}
}

CRemoteCommandServer::~CRemoteCommandServer()
{
	// Stop the thread, assumes that thread is responsive
	SignalStopWorker();
	gEnv->pThreadManager->JoinThread(this, eJM_Join);

	// Cleanup the clients endpoints
	for (TEndpoints::const_iterator it = m_pEndpoints.begin(); it != m_pEndpoints.end(); ++it)
	{
		delete (*it);
	}
	m_pEndpoints.clear();

	// Cleanup the clients that were not yet deleted but are dead
	for (TEndpoints::const_iterator it = m_pEndpointToDelete.begin(); it != m_pEndpointToDelete.end(); ++it)
	{
		delete (*it);
	}
	m_pEndpointToDelete.clear();

	// Cleanup the raw messages
	while (!m_pRawMessages.empty())
	{
		delete m_pRawMessages.pop();
	}

	// Properly close the listening socket
	if (m_pListener != NULL)
	{
		m_pListener->Close();
		m_pListener->Release();
		m_pListener = NULL;
	}
}

void CRemoteCommandServer::ProcessRawMessageAsync(IServiceNetworkMessage* pMessage, IServiceNetworkConnection* pConnection)
{
	// we lock for the whole duration of the function - I think that's the safest.
	// this function is being called from remote command server thread and even if it locks for a moment that's not a tragic situation.
	CryAutoLock<CryMutex> lock(m_rawMessagesLock);

	// Process the message using async listeners
	bool bWasProcessed = false;
	for (TRawMessageListenersAsync::const_iterator it = m_pRawListenersAsync.begin();
	     it != m_pRawListenersAsync.end(); ++it)
	{
		CDataReadStreamFormMessage reader(pMessage);
		CDataWriteStreamBuffer writer;

		// Request the listener to process this message
		if ((*it)->OnRawMessageAsync(pConnection->GetRemoteAddress(), reader, writer))
		{
			// Send response back using the source connection
			if (writer.GetSize() > 0)
			{
				IServiceNetworkMessage* pNewMessage = writer.BuildMessage();
				if (NULL != pNewMessage)
				{
					pConnection->SendMsg(pNewMessage);
					pNewMessage->Release();
				}
			}

			// mark as processed
			bWasProcessed = true;
			break;
		}
	}

	// Stats
	if (bWasProcessed)
	{
		LOG_VERBOSE(3, "Raw message from '%s', size %d ASYNC, PROCESSED",
		            pConnection->GetRemoteAddress().ToString().c_str(),
		            pMessage->GetSize());
	}
	else
	{
		LOG_VERBOSE(3, "Raw message from '%s', size %d ASYNC, NOT PROCESSED",
		            pConnection->GetRemoteAddress().ToString().c_str(),
		            pMessage->GetSize());
	}

	// If we have sync listeners add the raw message for processing on the main thread
	if (!bWasProcessed && !m_pRawListenersSync.empty())
	{
		m_pRawMessages.push(new RawMessage(pConnection, pMessage));
	}
}

void CRemoteCommandServer::ProcessRawMessagesSync()
{
	// get messages
	TRawMessageListenersSync listeners;
	{
		CryAutoLock<CryMutex> lock(m_rawMessagesLock);
		listeners = m_pRawListenersSync;
	}

	// process each message
	while (!m_pRawMessages.empty())
	{
		RawMessage* pMsg = m_pRawMessages.pop();

		// Process messages only from alive connection (they could die before we got a chance to process the message)
		if (pMsg && pMsg->m_pConnection->IsAlive())
		{
			// Try to process by on of the listeners
			bool bWasProcessed = false;
			for (TRawMessageListenersSync::const_iterator jt = listeners.begin();
			     jt != listeners.end(); ++jt)
			{
				CDataReadStreamFormMessage reader(pMsg->m_pMessage);
				CDataWriteStreamBuffer writer;

				// Request the listener to process this message
				if ((*jt)->OnRawMessageSync(pMsg->m_pConnection->GetRemoteAddress(), reader, writer))
				{
					// Send response back using the source connection
					if (writer.GetSize() > 0)
					{
						IServiceNetworkMessage* pMessage = writer.BuildMessage();
						if (NULL != pMessage)
						{
							pMsg->m_pConnection->SendMsg(pMessage);
							pMessage->Release();
						}
					}

					// mark as processed
					bWasProcessed = true;
					break;
				}
			}

			// Stats
			if (bWasProcessed)
			{
				LOG_VERBOSE(3, "Raw message from '%s', size %d SYNC PROCESSED",
				            pMsg->m_pConnection->GetRemoteAddress().ToString().c_str(),
				            pMsg->m_pMessage->GetSize());
			}
			else
			{
				LOG_VERBOSE(3, "Raw message from '%s', size %d SYNC NOT PROCESSED",
				            pMsg->m_pConnection->GetRemoteAddress().ToString().c_str(),
				            pMsg->m_pMessage->GetSize());
			}
		}

		/// Cleanup
		delete pMsg;
	}
}

void CRemoteCommandServer::Delete()
{
	delete this;
}

void CRemoteCommandServer::FlushCommandQueue()
{
	// Always process raw messages, even if commands are suspended
	ProcessRawMessagesSync();

	// When the command server is suppressed externally, well, then don't execute any commands
	// This is usually used when the main thread is doing some heavy stuff.
	// TODO: Consider signaling the clients about this condition.
	if (m_bIsSuppressed)
	{
		LOG_VERBOSE(4, "FlushCommandQueue: suppressed (counter=%d)",
		            m_suppressionCounter);

		return;
	}

	// Update the endpoints from a copy of the list
	{
		CryAutoLock<CryMutex> lock(m_accessLock);
		m_pUpdateEndpoints = m_pEndpoints;
	}

	// Execute the commands for each endpoint
	for (TEndpoints::const_iterator it = m_pUpdateEndpoints.begin();
	     it != m_pUpdateEndpoints.end(); ++it)
	{
		(*it)->Execute();
	}

	// Delete endpoints that were discarded within the thread (due to network errors)
	// We couldn't do that there because we would need to lock to much inside the mutex (bad idea)
	if (!m_pEndpointToDelete.empty())
	{
		// TODO: consider using different CS for pDeletedEnpoints array
		CryAutoLock<CryMutex> lock(m_accessLock);

		// delete the endpoint structured (deferred)
		for (size_t i = 0; i < m_pEndpointToDelete.size(); ++i)
		{
			delete m_pEndpointToDelete[i];
		}

		m_pEndpointToDelete.clear();
	}
}

void CRemoteCommandServer::SuppressCommands()
{
	if (CryInterlockedIncrement(&m_suppressionCounter) > 0)
	{
		m_bIsSuppressed = true;
	}
}

void CRemoteCommandServer::ResumeCommands()
{
	if (CryInterlockedDecrement(&m_suppressionCounter) == 0)
	{
		m_bIsSuppressed = false;
	}
}

void CRemoteCommandServer::ThreadEntry()
{
	TEndpoints updateList;
	while (!m_bCloseThread)
	{
		// Accept new connections
		{
			IServiceNetworkConnection* pNewConnection = m_pListener->Accept();
			if (NULL != pNewConnection)
			{
				LOG_VERBOSE(2, "New endpoint created with connection '%s'", pNewConnection->GetRemoteAddress().ToString().c_str());

				// Create endpoint wrapper
				Endpoint* pEndPoint = new Endpoint(GetManager(), this, pNewConnection);

				// Add to endpoint list
				{
					CryAutoLock<CryMutex> lock(m_accessLock);
					m_pEndpoints.push_back(pEndPoint);
				}
			}
		}

		// Get the current endpoint table (for update)
		{
			CryAutoLock<CryMutex> lock(m_accessLock);
			updateList = m_pEndpoints;
		}

		// Update endpoints
		for (TEndpoints::iterator it = updateList.begin(); it != updateList.end(); ++it)
		{
			Endpoint* ep = (*it);
			if (!ep->Update())
			{
				LOG_VERBOSE(2, "RemoteCommand endpoint '%s' closed", ep->GetConnection()->GetRemoteAddress().ToString().c_str());

				// remove the endpoint from the original list
				{
					CryAutoLock<CryMutex> lock(m_accessLock);

					// it's safe to remove from the endpoints list - we are iterating over a copy
					m_pEndpoints.erase(std::find(m_pEndpoints.begin(), m_pEndpoints.end(), ep));

					// don't delete the endpoint structure now (it may still be executed on main thread)
					// instead add it to a list that will be processed at the end of execution so this endpoint can get deleted
					m_pEndpointToDelete.push_back(ep);
				}
			}
		}
		// Limit the CPU usage
		// TODO: consider using some event based mechanism since the only source of
		// work for this thread is the network we can esily be triggered by that.
		CrySleep(5);
	}
}

void CRemoteCommandServer::SignalStopWorker()
{
	m_bCloseThread = true;
}

void CRemoteCommandServer::RegisterSyncMessageListener(IRemoteCommandListenerSync* pListener)
{
	CryAutoLock<CryMutex> lock(m_rawMessagesLock);
	m_pRawListenersSync.push_back(pListener);
}

void CRemoteCommandServer::UnregisterSyncMessageListener(IRemoteCommandListenerSync* pListener)
{
	CryAutoLock<CryMutex> lock(m_rawMessagesLock);

	TRawMessageListenersSync::iterator it = std::find(m_pRawListenersSync.begin(), m_pRawListenersSync.end(), pListener);
	if (it != m_pRawListenersSync.end())
	{
		m_pRawListenersSync.erase(it);
	}
}

void CRemoteCommandServer::RegisterAsyncMessageListener(IRemoteCommandListenerAsync* pListener)
{
	CryAutoLock<CryMutex> lock(m_rawMessagesLock);
	m_pRawListenersAsync.push_back(pListener);
}

void CRemoteCommandServer::UnregisterAsyncMessageListener(IRemoteCommandListenerAsync* pListener)
{
	CryAutoLock<CryMutex> lock(m_rawMessagesLock);

	TRawMessageListenersAsync::iterator it = std::find(m_pRawListenersAsync.begin(), m_pRawListenersAsync.end(), pListener);
	if (it != m_pRawListenersAsync.end())
	{
		m_pRawListenersAsync.erase(it);
	}
}

void CRemoteCommandServer::Broadcast(IServiceNetworkMessage* pMessage)
{
	if (NULL != pMessage && pMessage->GetSize() > 0)
	{
		CryAutoLock<CryMutex> lock(m_rawMessagesLock);
		for (TEndpoints::const_iterator jt = m_pEndpoints.begin();
		     jt != m_pEndpoints.end(); ++jt)
		{
			Endpoint* pEndpoint = (*jt);
			if (pEndpoint->HasReceivedClassList())
			{
				IServiceNetworkConnection* pConnection = pEndpoint->GetConnection();
				if (NULL != pConnection)
				{
					pConnection->SendMsg(pMessage);
				}
			}
		}
	}
}

bool CRemoteCommandServer::HasConnectedClients() const
{
	CryAutoLock<CryMutex> lock(m_rawMessagesLock);

	for (TEndpoints::const_iterator jt = m_pEndpoints.begin();
	     jt != m_pEndpoints.end(); ++jt)
	{
		Endpoint* pEndpoint = (*jt);
		if (pEndpoint->HasReceivedClassList())
		{
			IServiceNetworkConnection* pConnection = pEndpoint->GetConnection();
			if (pConnection->IsAlive())
			{
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------

// Do not remove (can mess up the uber file builds)
#undef LOG_VERBOSE

//-----------------------------------------------------------------------------
