// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:  Remote command system implementation
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

CRemoteCommandClient::Command::Command()
	: m_refCount(1)
	, m_szClassName(NULL)
	, m_id(0)
{
}

CRemoteCommandClient::Command::~Command()
{
	// Release message buffer with compiled command data
	if (m_pMessage != NULL)
	{
		m_pMessage->Release();
		m_pMessage = NULL;
	}
}

CRemoteCommandClient::Command* CRemoteCommandClient::Command::Compile(const IRemoteCommand& cmd, const uint32 commandId, const uint32 classId)
{
	// Build command header
	CommandHeader header;
	header.classId = classId;
	header.commandId = commandId;
	header.size = 0; // not known yet

	// Output stream builder
	CDataWriteStreamBuffer writer;

	// Start the packet with a command header (it will be later overwritten)
	writer << header;

	// Serialize command header and data
	const uint32 commandDataStart = writer.GetSize();
	cmd.SaveToStream(writer);
	const uint32 commandDataEnd = writer.GetSize();

	// Extract a message from the stream
	IServiceNetworkMessage* pMessage = writer.BuildMessage();
	if (NULL == pMessage)
	{
		// No message was generated (for some reason)
		// Do not allow this command to compile
		return NULL;
	}

	// Rewrite header with the proper command size
	// This is a little bit over-the-top because it uses another serializer created
	// on top of the message buffer. The advantage is that we have the endianess problem abstracted away.
	// TODO: consider writing the size directly
	{
		// update header with popper data size
		const uint32 dataSize = commandDataEnd - commandDataStart;
		header.size = dataSize;

		// rewrite the header in existing message
		CDataWriteStreamToMessage inPlaceWriter(pMessage);
		inPlaceWriter << header;
	}

	// Create command wrapper
	Command* pCommand = new Command();
	pCommand->m_id = commandId;
	pCommand->m_szClassName = cmd.GetClass()->GetName();
	pCommand->m_pMessage = pMessage;
	return pCommand;
}

void CRemoteCommandClient::Command::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CRemoteCommandClient::Command::Release()
{
	if (0 == CryInterlockedDecrement(&m_refCount))
	{
		delete this;
	}
}

//-----------------------------------------------------------------------------

CRemoteCommandClient::Connection::Connection(CRemoteCommandManager* pManager, IServiceNetworkConnection* pConnection, uint32 currentCommandId)
	: m_pConnection(pConnection)
	, m_pManager(pManager)
	, m_lastReceivedCommand(currentCommandId)
	, m_lastExecutedCommand(currentCommandId)
	, m_remoteAddress(pConnection->GetRemoteAddress())
	, m_refCount(1)
{
	// The first thing to do after the connection is initialized is to
	// send the message with list of classes supported by this side.
	{
		// Write the header
		PackedHeader header;
		header.magic = PackedHeader::kMagic;
		header.msgType = PackedHeader::eCommand_ClassList;
		header.count = currentCommandId; // send the intial command ID so we can be in sync

		// Get the class list for our local remote command manager
		std::vector<string> classList;
		GetManager()->GetClassList(classList);

		// Write the message
		CDataWriteStreamBuffer writer;
		writer << header;
		writer << classList;

		// Send the message to the remote side
		IServiceNetworkMessage* pMsg = writer.BuildMessage();
		if (NULL != pMsg)
		{
			LOG_VERBOSE(1, "Sent class list message (%d classes, size=%d) to '%s'",
			            classList.size(),
			            pMsg->GetSize(),
			            m_pConnection->GetRemoteAddress().ToString().c_str());

			// TODO: well, there is no reason this can fail since the connection is brand new, but...
			// We still relay on the service network to deliver this message unharmed.
			m_pConnection->SendMsg(pMsg);

			// cleanup
			pMsg->Release();
		}
	}
}

CRemoteCommandClient::Connection::~Connection()
{
	// Close the connection
	const bool bFlushBeforeClosing = false;
	Close(bFlushBeforeClosing);

	// Release any commands left over on the list
	for (TCommands::const_iterator it = m_pCommands.begin();
	     it != m_pCommands.end(); ++it)
	{
		(*it)->m_pCommand->Release();
		delete (*it);
	}
	m_pCommands.clear();

	// Release all of the raw messages that were not picked up
	while (!m_pRawMessages.empty())
	{
		IServiceNetworkMessage* pMessage = m_pRawMessages.pop();
		pMessage->Release();
	}

	// Release the connection object
	SAFE_RELEASE(m_pConnection);
}

void CRemoteCommandClient::Connection::SendDisconnectMessage()
{
	if (NULL != m_pConnection && m_pConnection->IsAlive())
	{
		IDataWriteStream* pWriter = gEnv->pServiceNetwork->CreateMessageWriter();
		if (NULL != pWriter)
		{
			// write header to message
			PackedHeader header;
			header.magic = PackedHeader::kMagic;
			header.count = 0;
			header.msgType = PackedHeader::eCommand_Disconnect;
			*pWriter << header;

			// Send the disconnect signal
			IServiceNetworkMessage* pMessage = pWriter->BuildMessage();
			if (NULL != pMessage)
			{
				m_pConnection->SendMsg(pMessage);
				pMessage->Release();
			}

			pWriter->Delete();
		}
	}
}

void CRemoteCommandClient::Connection::AddToSendQueue(Command* pCommand)
{
	// Do not add commands if the connection is closed
	if (m_pConnection == NULL)
	{
		return;
	}

	// Add command to local list
	// NOTE: this list always needs to be sorted in increasing command ID for various optimization reason.
	// This is achieved by resorting after pushing each element. Usually the cost of this is close to nothing
	// because incoming commands tend to be added with increasing command IDs.
	// The only case when something else can happen is when commands are added from different threads
	// and the one that was lower CommandID took longer to serialize and therefore is added later.
	// Anyway, this case is handled here.
	{
		CryAutoLock<CryMutex> lock(m_commandAccessMutex);

		// Always add to the end (don't try to guess position)
		// TODO: consider binary search
		m_pCommands.push_back(new CommandRef(pCommand));

		// Resort, NODE: This usually does not sort anything because the vector is already sorted
		std::sort(m_pCommands.begin(), m_pCommands.end(), CommandRef::CompareCommandRefs);
	}

	// Keep local reference to command (since we added it to our array)
	pCommand->AddRef();
}

bool CRemoteCommandClient::Connection::Update()
{
	// If the network connection got dead we should close this one to
	if ((NULL == m_pConnection) || !m_pConnection->IsAlive())
	{
		return false;
	}

	// Receive ACKs first so we have better view of what to send
	uint32 newLastExecutedCommand = m_lastExecutedCommand;
	uint32 newLastReceivedCommand = m_lastReceivedCommand;
	IServiceNetworkMessage* pMsg = m_pConnection->ReceiveMsg();
	while (pMsg != NULL)
	{
		// Deserialize the message
		{
			CDataReadStreamFormMessage reader(pMsg);
			ResponseHeader response;
			reader << response;

			// is this proper command system message ?
			if (response.magic == PackedHeader::kMagic)
			{
				if (response.msgType == PackedHeader::eCommand_ACK)
				{
					// Update internal ACK values
					// This code supports getting the ACK messages out of order.
					newLastExecutedCommand = max<uint32>(newLastExecutedCommand, response.lastCommandExecuted);
					newLastReceivedCommand = max<uint32>(newLastReceivedCommand, response.lastCommandReceived);

					LOG_VERBOSE(3, "ACK (rcv=%d, exe=%d) received from '%s'",
					            response.lastCommandReceived,
					            response.lastCommandExecuted,
					            m_pConnection->GetRemoteAddress().ToString().c_str());
				}
				else if (response.msgType == PackedHeader::eCommand_Disconnect)
				{
					// Disconnect request was received
					LOG_VERBOSE(3, "DISCONNECT (rcv=%d, exe=%d) received from '%s'",
					            response.lastCommandReceived,
					            response.lastCommandExecuted,
					            m_pConnection->GetRemoteAddress().ToString().c_str());

					// Close connection
					m_pConnection->Close();
					m_pConnection->Release();
					m_pConnection = NULL;

					// release the message
					pMsg->Release();

					// Signal manager to delete this object
					return false;
				}
			}
			else
			{
				// Keep an extra reference for the message in the raw message list
				pMsg->AddRef();

				// Assume it's a raw message, add it to the raw list
				m_pRawMessages.push(pMsg);
			}
		}

		// Release message data
		pMsg->Release();

		// Get next message from the network
		pMsg = m_pConnection->ReceiveMsg();
	}

	// ACK was updated
	if ((newLastExecutedCommand != m_lastExecutedCommand) ||
	    (newLastReceivedCommand != m_lastReceivedCommand))
	{
		m_lastExecutedCommand = newLastExecutedCommand;
		m_lastReceivedCommand = newLastReceivedCommand;

		// Drop commands that were ACKed as received (server has them and they will be executed soon)
		{
			CryAutoLock<CryMutex> lock(m_commandAccessMutex);

			// we use this to count how many elements we need to remove later from the command vector
			uint32 numCommandsToDelete = 0;

			for (TCommands::const_iterator it = m_pCommands.begin();
			     it != m_pCommands.end(); ++it)
			{
				CommandRef* cmdRef = *it;

				// Command is still needed because it was not yet received by the remote part
				if (cmdRef->m_pCommand->GetCommandId() > newLastReceivedCommand)
				{
					break;
				}

				// Drop the command data
				cmdRef->m_pCommand->Release();
				delete cmdRef;

				++numCommandsToDelete;
			}

			// Erase the command slots in the vector (in one batch)
			if (numCommandsToDelete > 0)
			{
				m_pCommands.erase(m_pCommands.begin(), m_pCommands.begin() + numCommandsToDelete);
			}
		}
	}

	// (Re)Send the commands
	{
		// Calculate the maximum command ID we can send, this depends on
		// the last command that was ACKed as executed on the remote side.
		// This effectively throttles the communication and prevents the
		// situation when remote side is flooded with unprocessed commands.
		// NOTE: the time when command is executed is different to the
		// time that command is received. Sometimes if the server is suppressed (level loading)
		// it can take a long time before commands begin to execute.
		const uint32 maxCommandIdToSend = m_lastExecutedCommand + kCommandSendLead;

		// Calculate the cutoff time for sending (all commands that were not send before this time will be sent again)
		// This assumes that the last sent time for new commands is 0 (so they will always got sent the first time)
		// This situation can only happen due to the network failure since RemoteCommand layer does not require the commands to be resent.
		const uint64 currentTime = gEnv->pTimer->GetAsyncTime().GetMilliSecondsAsInt64();
		const uint64 cutoffTime = currentTime - kCommandResendTime;

		std::vector<CommandRef*> commandsInPacket;   // temp array

		// Process until we send all that there is to send
		for (;; )
		{
			// When sending connections try to merge them in larger packets.
			// NOTE: this should not impact delivery time since we are not waiting
			// for pending commands to accumulate before sending them, it's just an optimization
			// to prevent may small messages from being sent.
			uint32 packetDataSizeSoFar = 0;
			{
				CryAutoLock<CryMutex> lock(m_commandAccessMutex);

				// fast local clear
				// TODO: do we have a good template alternative to temporary array on stack?
				packetDataSizeSoFar = 0;
				commandsInPacket.resize(0);

				for (TCommands::iterator it = m_pCommands.begin();
				     it != m_pCommands.end(); ++it)
				{
					CommandRef* commandRef = *it;

					// this command is to new, don't send it
					if (commandRef->m_pCommand->GetCommandId() >= maxCommandIdToSend)
					{
						break;
					}

					// should we send this command ?
					if (commandRef->m_lastSentTime < cutoffTime)
					{
						// will it fit into current packet ?
						const uint32 commandDataSize = commandRef->m_pCommand->GetMessage()->GetSize();
						if (packetDataSizeSoFar == 0 ||  // always add at least one command to the packet (no splitting)
						    (packetDataSizeSoFar + commandDataSize < kCommandMaxMergePacketSize))
						{
							if (commandRef->m_lastSentTime == 0)
							{
								LOG_VERBOSE(3, "Command ID=%d is sent FIRST TIME to '%s'",
								            commandRef->m_pCommand->GetCommandId(),
								            m_pConnection->GetRemoteAddress().ToString().c_str());
							}
							else
							{
								LOG_VERBOSE(3, "Command ID=%d is resent to '%s'",
								            commandRef->m_pCommand->GetCommandId(),
								            m_pConnection->GetRemoteAddress().ToString().c_str());
							}

							// will be sent
							commandsInPacket.push_back(commandRef);
							packetDataSizeSoFar += commandDataSize;
						}
						else
						{
							LOG_VERBOSE(3, "Command ID=%d is too big (%d) to fit packet size limit (%d)",
							            commandRef->m_pCommand->GetCommandId(),
							            commandDataSize,
							            kCommandMaxMergePacketSize);

							// no more commands will fit current packet
							break;
						}
					}
				}
			}

			// No new commands to be send
			if (commandsInPacket.empty())
			{
				break;
			}

			// Stats
			LOG_VERBOSE(3, "Sending %d commands in packet, total size=%d, maxID=%d, dest: %s",
			            commandsInPacket.size(),
			            packetDataSizeSoFar,
			            maxCommandIdToSend,
			            m_pConnection->GetRemoteAddress().ToString().c_str());

			// Estimate the size of the network packet
			const uint32 messageDataSize = packetDataSizeSoFar + PackedHeader::kSerializationSize;

			// Allocate and fill the message buffer
			IServiceNetworkMessage* pSendMsg = gEnv->pServiceNetwork->AllocMessageBuffer(messageDataSize);
			if (NULL != pSendMsg)
			{
				CDataWriteStreamToMessage writer(pSendMsg);

				// Packet header
				PackedHeader header;
				header.magic = PackedHeader::kMagic;
				header.msgType = PackedHeader::eCommand_Command;
				header.count = commandsInPacket.size(); // number commands to send in this packet
				writer << header;

				// Merge data of single commands
				for (size_t i = 0; i < commandsInPacket.size(); ++i)
				{
					const IServiceNetworkMessage* pCommandMsg = commandsInPacket[i]->m_pCommand->GetMessage();
					writer.Write(pCommandMsg->GetPointer(), pCommandMsg->GetSize());
				}

				// Schedule the packet for sending via our network connection
				if (m_pConnection->SendMsg(pSendMsg))
				{
					// Only after the network layer has accepted our message we can assume that the commands were sent
					for (size_t i = 0; i < commandsInPacket.size(); ++i)
					{
						CommandRef* cmdRef = commandsInPacket[i];
						cmdRef->m_lastSentTime = currentTime;
					}

					// Release temporary message memory
					pSendMsg->Release();
				}
				else
				{
					// We failed to send the message (possibly the send queue is full)
					pSendMsg->Release();
					break;
				}
			}
			else
			{
				// No message was created, stop sending
				break;
			}
		}
	}

	// Keep the connection alive
	return true;
}

bool CRemoteCommandClient::Connection::IsAlive() const
{
	return (NULL != m_pConnection) && (m_pConnection->IsAlive());
}

const ServiceNetworkAddress& CRemoteCommandClient::Connection::GetRemoteAddress() const
{
	return m_remoteAddress;
}

void CRemoteCommandClient::Connection::Close(bool bFlushQueueBeforeClosing /*= false*/)
{
	// Close the connection
	if (NULL != m_pConnection)
	{
		if (m_pConnection->IsAlive() && bFlushQueueBeforeClosing)
		{
			// We have a chance to send a graceful disconnect message, so send it
			SendDisconnectMessage();

			// Send all the messages from the send queue before closing this connection.
			// This does not block current thread.
			m_pConnection->FlushAndClose(IServiceNetworkConnection::kDefaultFlushTime);
		}
		else
		{
			// Just close the connection (hasher way)
			m_pConnection->Close();
		}
	}
}

bool CRemoteCommandClient::Connection::SendRawMessage(IServiceNetworkMessage* pMessage)
{
	// We can send the raw messages right away
	if (NULL != m_pConnection && m_pConnection->IsAlive())
	{
		return m_pConnection->SendMsg(pMessage);
	}
	else
	{
		return false;
	}
}

IServiceNetworkMessage* CRemoteCommandClient::Connection::ReceiveRawMessage()
{
	return m_pRawMessages.pop();
}

void CRemoteCommandClient::Connection::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CRemoteCommandClient::Connection::Release()
{
	if (0 == CryInterlockedDecrement(&m_refCount))
	{
		delete this;
	}
}

//-----------------------------------------------------------------------------

CRemoteCommandClient::CRemoteCommandClient(CRemoteCommandManager* pManager)
	: m_pManager(pManager)
	, m_commandId(0)
	, m_bCloseThread(false)
{
	if (!gEnv->pThreadManager->SpawnThread(this, "RemoteCommandClient"))
	{
		CryFatalError("Error spawning \"RemoteCommandClient\" thread.");
	}
}

CRemoteCommandClient::~CRemoteCommandClient()
{
	// Stop the thread
	SignalStopWork();
	gEnv->pThreadManager->JoinThread(this, eJM_Join);

	// Delete connections
	for (size_t i = 0; i < m_pConnections.size(); ++i)
	{
		m_pConnections[i]->Release();
	}
	m_pConnections.clear();
}

void CRemoteCommandClient::Delete()
{
	delete this;
}

IRemoteCommandConnection* CRemoteCommandClient::ConnectToServer(const class ServiceNetworkAddress& serverAddress)
{
	CryAutoLock<CryMutex> lock(m_accessMutex);

	// Do not connect twice to the same server
	for (TConnections::const_iterator it = m_pConnections.begin();
	     it != m_pConnections.end(); ++it)
	{
		if (ServiceNetworkAddress::CompareBaseAddress((*it)->GetRemoteAddress(), serverAddress))
		{
			LOG_VERBOSE(0, "Failed to connect to server '%s': already connected",
			            serverAddress.ToString().c_str());

			return NULL;
		}
	}

	// Open a network connection
	IServiceNetworkConnection* pNetConnection = gEnv->pServiceNetwork->Connect(serverAddress);
	if (NULL == pNetConnection)
	{
		LOG_VERBOSE(0, "Failed to connect to server '%s': server is not responding",
		            serverAddress.ToString().c_str());

		return NULL;
	}

	// Get current command ID (only commands after this one will be sent)
	const uint32 firstCommandId = m_commandId;

	// Create a wrapping class and add it to the connection list
	Connection* pConnection = new Connection(GetManager(), pNetConnection, firstCommandId);
	m_pConnections.push_back(pConnection);

	// Keep internal reference
	pConnection->AddRef();

	LOG_VERBOSE(0, "Connected to remote command server '%s', first command ID=%d",
	            serverAddress.ToString().c_str(),
	            firstCommandId);

	return pConnection;
}

bool CRemoteCommandClient::Schedule(const IRemoteCommand& command)
{
	// No connections
	if (m_pConnections.empty())
	{
		return false;
	}

	// Find ClassID for command
	uint32 classId = 0;
	if (!GetManager()->FindClassId(command.GetClass(), classId))
	{
		LOG_VERBOSE(0, "Class '%s' not recognized. Did you call RegisterClass() ?",
		            command.GetClass()->GetName());

		return false;
	}

	// Alloc new command ID and compile command data
	// TODO: consider moving the compilation to thread (this may be unsafe).
	const uint32 commandId = CryInterlockedIncrement((volatile int*) &m_commandId);
	Command* pCommand = Command::Compile(command, commandId, classId);

	// Register new command in all of the existing server connections
	if (NULL != pCommand)
	{
		CryAutoLock<CryMutex> lock(m_accessMutex);

		for (TConnections::const_iterator it = m_pConnections.begin();
		     it != m_pConnections.end(); ++it)
		{
			(*it)->AddToSendQueue(pCommand);
		}

		// We are done with our reference
		pCommand->Release();
	}

	// Signal the thread to process data
	m_threadEvent.Set();
	return true;
}

void CRemoteCommandClient::ThreadEntry()
{
	TConnections pUpdateList;
	while (!m_bCloseThread)
	{
		// copy to local list for updating
		{
			CryAutoLock<CryMutex> lock(m_accessMutex);
			pUpdateList = m_pConnections;
		}

		// update current connection list
		for (TConnections::const_iterator it = pUpdateList.begin();
		     it != pUpdateList.end(); ++it)
		{
			if (!(*it)->Update())
			{
				CryAutoLock<CryMutex> lock(m_accessMutex);
				m_pConnectionsToDelete.push_back(*it);
			}
		}

		// delete pending connections
		{
			CryAutoLock<CryMutex> lock(m_accessMutex);
			for (TConnections::iterator it = m_pConnectionsToDelete.begin();
			     it != m_pConnectionsToDelete.end(); ++it)
			{
				// delete the object
				(*it)->Release();
				(*it)->Close(true);

				// remove from connection list
				TConnections::iterator jt = std::find(m_pConnections.begin(), m_pConnections.end(), *it);
				if (jt != m_pConnections.end())
				{
					m_pConnections.erase(jt);
				}
			}

			// reset the array
			m_pConnectionsToDelete.clear();
		}

		// Limit the CPU usage
		const uint32 maxWaitTime = 100;
		m_threadEvent.Wait(maxWaitTime);
	}
}

void CRemoteCommandClient::SignalStopWork()
{
	m_bCloseThread = true;
}

//-----------------------------------------------------------------------------

// Do not remove (can mess up the uber file builds)
#undef LOG_VERBOSE

//-----------------------------------------------------------------------------
