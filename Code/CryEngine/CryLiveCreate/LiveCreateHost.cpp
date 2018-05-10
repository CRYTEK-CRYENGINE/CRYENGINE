// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "LiveCreateHost.h"

#ifndef NO_LIVECREATE
	#include <CryNetwork/IServiceNetwork.h>
	#include <CryNetwork/IRemoteCommand.h>
	#include "LiveCreateManager.h"
	#include "LiveCreateNativeFile.h"
	#include "LiveCreateCommands.h"
	#include <CrySystem/ICmdLine.h>
	#include <CryCore/Platform/IPlatformOS.h>
	#include <CryNetwork/CrySocks.h>
	#include "PlatformHandler_GamePC.h"

namespace LiveCreate
{

//-----------------------------------------------------------------------------

CPlatformService::CPlatformService(CHost* pHost, const uint16 listeningPort /*= kDefaultDiscoverySerivceListenPost*/)
	: m_socket(0)
	, m_pHost(pHost)
	, m_port(listeningPort)
	, m_bQuiet(false)
{
	Bind();

	if (!gEnv->pThreadManager->SpawnThread(this, "LiveCreatePlatformService"))
	{
		CryFatalError("Error spawning \"LiveCreatePlatformService\" thread.");
	}
}

CPlatformService::~CPlatformService()
{
	if (m_socket > 0)
	{
		CrySock::shutdown(m_socket, 2);
		CrySock::closesocket(m_socket);
		m_socket = 0;
	}
}

bool CPlatformService::Bind()
{
	// close current socket
	if (m_socket > 0)
	{
		CrySock::shutdown(m_socket, 2);
		CrySock::closesocket(m_socket);
		m_socket = 0;
	}

	// create new listening socket
	CRYSOCKET s = CrySock::socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
	{
		gEnv->pLog->LogWarning("LiveCreate discovery service NOT STARTED: socket error %d", s);
	}
	else
	{
		// bind socket to specified port
		CRYSOCKADDR_IN servaddr;
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons(m_port);
		const int ret = CrySock::bind(s, (struct CRYSOCKADDR*)&servaddr, sizeof(servaddr));
		if (ret < 0)
		{
			gEnv->pLog->LogWarning("LiveCreate discovery service NOT STARTED: bind error %d", ret);
		}
		else
		{
			gEnv->pLog->Log("LiveCreate discovery service started at port %d", m_port);
			m_socket = s;
		}

		// set socket timeout
		CrySock::SetRecvTimeout(s, 1, 0);
	}

	return m_socket != 0;
}

const char* GetPlatformName()
{
	#if CRY_PLATFORM_ORBIS
	return "PS4";
	#elif CRY_PLATFORM_DURANGO
	return "XBOXONE";
	#elif defined(_WINDOWS_)
	return "PC";
	#else
	return "UNKNOWN";
	#endif
}

bool CPlatformService::Process(IDataReadStream& reader, IDataWriteStream& writer)
{
	const string cmd(reader.ReadString());

	if (cmd == "Cmd")
	{
		const string consoleCmd(reader.ReadString());
		gEnv->pConsole->ExecuteString(consoleCmd.c_str(), false, true);
		return true;
	}
	else if (cmd == "SuppressCommands")
	{
		m_pHost->SuppressCommandExecution();
		return true;
	}
	else if (cmd == "GetInfo")
	{
		CHostInfoPacket info;
		m_pHost->FillHostInfo(info);

		writer.WriteString("Info");
		info.Serialize(writer);
		return true;
	}

	return false;
}

void CPlatformService::ThreadEntry()
{
	while (!m_bQuiet)
	{
		CRYSOCKADDR_IN addr;
		CRYSOCKLEN addrLen = sizeof(addr);
		char buffer[1000];

		// get data from connection
		int length = CrySock::recvfrom(m_socket, buffer, sizeof(buffer), 0, (CRYSOCKADDR*)&addr, &addrLen);
		if (length < 0)
		{
			const CrySock::eCrySockError errCode = CrySock::TranslateSocketError(length);
			if (errCode == CrySock::eCSE_EWOULDBLOCK || errCode == CrySock::eCSE_ETIMEDOUT)
			{
				// just a timeout
				CrySleep(500);
				continue;
			}

			gEnv->pLog->LogWarning("LiveCreate discovery service socket error: %d", length);

			// try to restart
			if (!Bind())
			{
				gEnv->pLog->LogWarning("LiveCreate: Failed to rebind platform service socket. Closing service.");
				break;
			}

			continue;
		}

		// process data
		TAutoDelete<IDataReadStream> reader(gEnv->pServiceNetwork->CreateMessageReader(buffer, length));
		if (reader)
		{
			TAutoDelete<IDataWriteStream> writer(gEnv->pServiceNetwork->CreateMessageWriter());
			if (writer)
			{
				if (Process(reader, writer))
				{
					// send the response data - size of the data is limited
					char responseBuffer[1024];
					CRY_ASSERT(writer->GetSize() < sizeof(responseBuffer));
					if (writer->GetSize() < sizeof(responseBuffer))
					{
						writer->CopyToBuffer(responseBuffer);

						// send the message back
						int ret = CrySock::sendto(m_socket, responseBuffer, writer->GetSize(), 0, (CRYSOCKADDR*)&addr, addrLen);
						if (ret < 0)
						{
							gEnv->pLog->LogWarning("LiveCreate: sendto() error: %d", ret);
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------

CUtilityService::FileTransferInProgress::FileTransferInProgress(IServiceNetworkConnection* pConnection, const string& destFilePath, FILE* destFile, const uint32 fileSize)
	: m_pConnection(pConnection)
	, m_destFilePath(destFilePath)
	, m_destFile(destFile)
	, m_fileSize(fileSize)
	, m_fileDataLeft(fileSize)
	, m_startTime(gEnv->pTimer->GetAsyncTime())
{
	if (!gEnv->pThreadManager->SpawnThread(this, "LiveCreateUtilityService"))
	{
		CryFatalError("Error spawning \"LiveCreateUtilityService\" thread.");
	}
}

CUtilityService::FileTransferInProgress::~FileTransferInProgress()
{
	// close output file
	gEnv->pCryPak->FClose(m_destFile);

	// delete file if not transfered completely
	if (m_fileDataLeft > 0)
	{
		gEnv->pCryPak->RemoveFile(m_destFilePath.c_str());
	}

	// close connection
	if (NULL != m_pConnection)
	{
		m_pConnection->Close();
		m_pConnection->Release();
		m_pConnection = NULL;
	}
}

bool CUtilityService::FileTransferInProgress::Update()
{
	// connection is dead :(
	if (!m_pConnection->IsAlive())
	{
		gEnv->pLog->LogWarning("FileTransfer for '%s': connection interrupted (data left: %d)", m_destFilePath.c_str(), m_fileDataLeft);
		return true;
	}

	// handle new data
	IServiceNetworkMessage* pMessage = m_pConnection->ReceiveMsg();
	while (NULL != pMessage)
	{
		const uint8* pData = (const uint8*)pMessage->GetPointer();
		const uint32 dataSize = pMessage->GetSize();
		if (dataSize > m_fileDataLeft)
		{
			gEnv->pLog->LogWarning("FileTransfer for '%s': data overflow (%d>%d)", m_destFilePath.c_str(), dataSize, m_fileDataLeft);
			pMessage->Release();
			return true;
		}

		// write data to file
		const uint32 writtenSize = gEnv->pCryPak->FWrite(pData, 1, dataSize, m_destFile);
		if (writtenSize != dataSize)
		{
			gEnv->pLog->LogWarning("FileTransfer for '%s': write error (%d requested, %d written)", m_destFilePath.c_str(), dataSize, writtenSize);
			pMessage->Release();
			return true;
		}

		pMessage->Release();
		pMessage = m_pConnection->ReceiveMsg();
		m_fileDataLeft -= dataSize;
	}

	// finished
	if (m_fileDataLeft == 0)
	{
		const float time = gEnv->pTimer->GetAsyncTime().GetDifferenceInSeconds(m_startTime);
		gEnv->pLog->LogAlways("FileTransfer for '%s' completed in %1.2fs (%1.2f MB/s)",
		                      m_destFilePath.c_str(), time, ((float)m_fileSize / time) / (1024.0f * 1024.0f));
	}

	// close when completed
	return m_fileDataLeft == 0;
}

CUtilityService::CUtilityService(class CHost* pHost, const uint16 listeningPort)
	: m_pHost(pHost)
	, m_bQuit(false)
{
	m_pListener = gEnv->pServiceNetwork->CreateListener(listeningPort);

	if (!gEnv->pThreadManager->SpawnThread(this, "LiveCreateUtilityService"))
	{
		CryFatalError("Error spawning \"LiveCreateUtilityService\" thread.");
	}
}

CUtilityService::~CUtilityService()
{
	// close all files
	for (TFileTransferList::const_iterator it = m_fileTransfers.begin();
	     it != m_fileTransfers.end(); ++it)
	{
		delete (*it);
	}

	m_fileTransfers.clear();
}

void CUtilityService::ThreadEntry()
{
	typedef std::vector<IServiceNetworkConnection*> TConnections;
	TConnections createdConnections;

	while (!m_bQuit)
	{
		// any new connections ?
		IServiceNetworkConnection* pNewConnection = m_pListener->Accept();
		while (NULL != pNewConnection)
		{
			createdConnections.push_back(pNewConnection);
			pNewConnection = m_pListener->Accept();
		}

		// wait for initialization packet
		for (TConnections::iterator it = createdConnections.begin();
		     it != createdConnections.end(); )
		{
			IServiceNetworkConnection* pConnection = (*it);

			if (!pConnection->IsAlive())
			{
				pConnection->Close();
				pConnection->Release();
				it = createdConnections.erase(it);
			}
			else
			{
				IServiceNetworkMessage* pMessages = pConnection->ReceiveMsg();
				if (NULL != pMessages)
				{
					TAutoDelete<IDataReadStream> reader(pMessages->CreateReader());
					TAutoDelete<IDataWriteStream> writer(gEnv->pServiceNetwork->CreateMessageWriter());

					const string destPath = reader->ReadString();
					const uint32 id = reader->ReadUint32();
					const uint32 fileSize = reader->ReadUint32();

					gEnv->pLog->LogAlways("FileTransfer ID%d request received for file '%s', size %d.",
					                      id, destPath.c_str(), fileSize);

					// do we already have a file transfer request for this file ? if so - delete it
					for (TFileTransferList::iterator jt = m_fileTransfers.begin();
					     jt != m_fileTransfers.end(); ++jt)
					{
						FileTransferInProgress* pCurrentTransfer = (*jt);
						if (pCurrentTransfer->GetFileName() == destPath)
						{
							gEnv->pLog->LogAlways("FileTransfer for file '%s' already pending: canceling it.",
							                      destPath.c_str());

							delete pCurrentTransfer;
							m_fileTransfers.erase(jt);
							break;
						}
					}

					// create the access for output file
					FILE* pDestFile = gEnv->pCryPak->FOpen(destPath.c_str(), "wb");
					if (NULL == pDestFile)
					{
						gEnv->pLog->LogAlways("FileTransfer ID%d, file '%s' error: cannot open file to writing.",
						                      id, destPath.c_str());

						// response
						writer->WriteUint8(0);
					}
					else
					{
						FileTransferInProgress* pFileTransfer = new FileTransferInProgress(pConnection, destPath, pDestFile, fileSize);
						m_fileTransfers.push_back(pFileTransfer);

						// response
						writer->WriteUint8(1);
					}

					// send response
					IServiceNetworkMessage* pMessage = writer->BuildMessage();
					pConnection->SendMsg(pMessage);
					SAFE_RELEASE(pMessage);

					// if not file was opened and no file transfer was started we can close the connection
					if (NULL == pDestFile)
					{
						pConnection->FlushAndClose();
					}

					// connection was transfered to file transfer list or destroyed
					it = createdConnections.erase(it);
				}
				else
				{
					// no message yet
					++it;
				}
			}
		}

		// update file transfers
		for (TFileTransferList::iterator it = m_fileTransfers.begin();
		     it != m_fileTransfers.end(); )
		{
			FileTransferInProgress* pFileTransfer = (*it);

			if (pFileTransfer->Update())
			{
				// done
				it = m_fileTransfers.erase(it);
				delete pFileTransfer;
			}
			else
			{
				++it;
			}
		}
	}
}

//-----------------------------------------------------------------------------

CHost::CHost()
	: m_pCommandServer(NULL)
	, m_updateTimeOfDay(false)
	, m_bCommandsSuppressed(false)
	, m_pPlatformService(NULL)
	, m_pUtilityService(NULL)
{
}

CHost::~CHost()
{
	// make sure we close the host interface
	CHost::Close();
}

bool CHost::Initialize(const uint16 listeningPort /* = kDefaultHostListenPort*/,
                       const uint16 discoveryServiceListeningPort /*= kDefaultDiscoverySerivceListenPost*/,
                       const uint16 fileTransferServiceListeningPort /*= kDefaultFileTransferServicePort*/)
{
	// Create the command manager
	m_pCommandServer = gEnv->pRemoteCommandManager->CreateServer(listeningPort);
	if (NULL == m_pCommandServer)
	{
		return false;
	}

	// Register host as listener to raw communication on the command stream
	m_pCommandServer->RegisterAsyncMessageListener(this);
	m_pCommandServer->RegisterSyncMessageListener(this);

	// Create discovery service thread
	if (discoveryServiceListeningPort != 0)
	{
		m_pPlatformService = new CPlatformService(this, discoveryServiceListeningPort);
	}

	// Create file transfer service thread
	if (fileTransferServiceListeningPort != 0)
	{
		m_pUtilityService = new CUtilityService(this, fileTransferServiceListeningPort);
	}

	return true;
}

void CHost::Close()
{
	// Force close the command server
	if (NULL != m_pCommandServer)
	{
		m_pCommandServer->UnregisterAsyncMessageListener(this);
		m_pCommandServer->UnregisterSyncMessageListener(this);
		m_pCommandServer->Delete();
		m_pCommandServer = NULL;
	}

	// Stop discovery service
	if (NULL != m_pPlatformService)
	{
		m_pPlatformService->SingnalStopWork();
		gEnv->pThreadManager->JoinThread(m_pPlatformService);
		delete m_pPlatformService;
		m_pPlatformService = NULL;
	}

	// Stop file transfer service (will delete all in-flight files)
	if (NULL != m_pUtilityService)
	{
		m_pUtilityService->SignalStopWork();
		gEnv->pThreadManager->JoinThread(m_pUtilityService);
		delete m_pUtilityService;
		m_pUtilityService = NULL;
	}
}

void CHost::SuppressCommandExecution()
{
	// pass directly to remove command server
	if (NULL != m_pCommandServer && !m_bCommandsSuppressed)
	{
		m_pCommandServer->SuppressCommands();
		m_bCommandsSuppressed = true;

		// broadcast "Busy" message
		{
			IDataWriteStream* pStream = gEnv->pServiceNetwork->CreateMessageWriter();
			if (NULL != pStream)
			{
				pStream->WriteString("Busy");

				IServiceNetworkMessage* pMessage = pStream->BuildMessage();
				if (NULL != pMessage)
				{
					m_pCommandServer->Broadcast(pMessage);
					pMessage->Release();
				}

				pStream->Delete();
			}
		}
	}
}

void CHost::ResumeCommandExecution()
{
	// pass directly to remove command server
	if (NULL != m_pCommandServer && m_bCommandsSuppressed)
	{
		m_pCommandServer->ResumeCommands();
		m_bCommandsSuppressed = false;

		// broadcast "Ready" message
		{
			IDataWriteStream* pStream = gEnv->pServiceNetwork->CreateMessageWriter();
			if (NULL != pStream)
			{
				pStream->WriteString("Ready");

				IServiceNetworkMessage* pMessage = pStream->BuildMessage();
				if (NULL != pMessage)
				{
					m_pCommandServer->Broadcast(pMessage);
					pMessage->Release();
				}

				pStream->Delete();
			}
		}
	}
}

void CHost::DrawOverlay()
{
	DrawSelection();
}

void CHost::ExecuteCommands()
{
	// execute RPC commands
	if (NULL != m_pCommandServer)
	{
		m_pCommandServer->FlushCommandQueue();
	}

	if (m_updateTimeOfDay)
	{
		m_updateTimeOfDay = false;
		gEnv->p3DEngine->GetTimeOfDay()->Update(true, true);
	}
}

bool CHost::OnRawMessageSync(const ServiceNetworkAddress& remoteAddress, IDataReadStream& msg, IDataWriteStream& response)
{
	const string cmd = msg.ReadString();

	// External console command (usually "map", "unload", and "quit")
	if (cmd == "ConsoleCommand")
	{
		// read unique console command ID
		const uint64 lo = msg.ReadUint64();
		const uint64 hi = msg.ReadUint64();
		const CryGUID id = CryGUID::Construct(hi, lo);

		// read command string
		const string cmdString = msg.ReadString();

		if (cmdString == "SuppressCommands")
		{
			SuppressCommandExecution();
		}
		else if (cmdString == "ResumeCommands")
		{
			ResumeCommandExecution();
		}
		else
		{
			// was already executed ?
			TLastCommandIDs::const_iterator it = m_lastConsoleCommandsExecuted.find(id);
			if (it != m_lastConsoleCommandsExecuted.end())
			{
				gEnv->pLog->Log("LiveCreate: command '%s' already executed. Skipping.", cmdString.c_str());
			}
			else
			{
				m_lastConsoleCommandsExecuted.insert(id);
				gEnv->pConsole->ExecuteString(cmdString.c_str(), false, true);
			}
		}

		// write general OK response
		response.WriteString("CmdResponse");
		response.WriteUint64(lo);
		response.WriteUint64(hi);
		response.WriteString("OK");

		return true;
	}

	// not processed
	return false;
}

bool CHost::OnRawMessageAsync(const ServiceNetworkAddress& remoteAddress, IDataReadStream& msg, IDataWriteStream& response)
{
	const string cmd = msg.ReadString();

	// Ready flag
	if (cmd == "GetReadyFlag")
	{
		if (m_bCommandsSuppressed)
		{
			response.WriteString("Busy");
		}
		else
		{
			response.WriteString("Ready");
		}

		return true;
	}

	// not processed
	return false;
}

bool CHost::GetEntityCRC(EntityId id, uint32& outCrc) const
{
	TEntityCRCMap::const_iterator it = m_entityCrc.find(id);
	if (it != m_entityCrc.end())
	{
		outCrc = it->second;
		return true;
	}
	else
	{
		return false;
	}
}

void CHost::SetEntityCRC(EntityId id, const uint32 outCrc)
{
	m_entityCrc[id] = outCrc;
}

void CHost::RemoveEntityCRC(EntityId id)
{
	TEntityCRCMap::iterator it = m_entityCrc.find(id);
	if (it != m_entityCrc.end())
	{
		m_entityCrc.erase(it);
	}
}

bool CHost::SyncFullLevelState(const char* szFileName)
{
	string fullPath = szFileName;
	IDataReadStream* pStream = CLiveCreateFileReader::CreateReader(fullPath);
	if (NULL == pStream)
	{
		gEnv->pLog->LogAlways("LiveCreate full sync failed: failed to open file '%s'", (const char*)fullPath);
		return false;
	}

	// get the data header
	const uint32 magic = pStream->ReadUint32();
	if (magic != 'LCFS')
	{
		gEnv->pLog->LogAlways("LiveCreate full sync failed: invalid file '%s' header (0x%X)", (const char*)fullPath, magic);
		return false;
	}

	// Flush rendering thread :( another length stuff that is unfortunately needed here because it crashes without it
	gEnv->pRenderer->FlushRTCommands(true, true, true);

	// sync entity system (layers & entities)
	gEnv->pEntitySystem->LoadInternalState(*pStream);

	// sync objects system (brushes, etc)
	{
		const uint32 kMaxLayers = 1024;

		// get mask for visible layers
		uint8 layerMask[kMaxLayers / 8];
		memset(layerMask, 0, sizeof(layerMask));
		gEnv->pEntitySystem->GetVisibleLayerIDs(layerMask, kMaxLayers);

		// layer mapping
		std::vector<string> layerNames;
		std::vector<uint16> layerIds;
		*pStream << layerNames;
		*pStream << layerIds;

		// load layer names
		uint16 layerIdTable[kMaxLayers];
		memset(layerIdTable, 0, sizeof(layerIdTable));
		for (uint32 i = 0; i < layerNames.size(); ++i)
		{
			const uint16 dataId = layerIds[i];
			if (dataId < kMaxLayers)
			{
				const int layerId = gEnv->pEntitySystem->GetLayerId(layerNames[i].c_str());
				if (layerId != -1)
				{
					layerIdTable[dataId] = (uint16)layerId;
				}
			}
		}

		gEnv->p3DEngine->LoadInternalState(*pStream, layerMask, layerIdTable);
	}

	// parse the CVars
	gEnv->pConsole->LoadInternalState(*pStream);

	// parse the TOD dataa
	gEnv->p3DEngine->GetTimeOfDay()->LoadInternalState(*pStream);

	// done
	return true;
}

void CHost::RequestTimeOfDayUpdate()
{
	m_updateTimeOfDay = true;
}

void CHost::FillHostInfo(CHostInfoPacket& outHostInfo) const
{
	outHostInfo.platformName = GetPlatformName();
	outHostInfo.hostName = gEnv->pSystem->GetPlatformOS()->GetHostName();
	outHostInfo.gameFolder = gEnv->pCryPak->GetGameFolder();
	outHostInfo.rootFolder = gEnv->pSystem->GetRootFolder();
	outHostInfo.bAllowsLiveCreate = true;
	outHostInfo.bHasLiveCreateConnection = m_pCommandServer ? m_pCommandServer->HasConnectedClients() : false;
	outHostInfo.screenWidth = gEnv->pRenderer ? gEnv->pRenderer->GetWidth() : 0;
	outHostInfo.screenHeight = gEnv->pRenderer ? gEnv->pRenderer->GetHeight() : 0;

	if (gEnv->pGameFramework)
	{
		ILevelSystem* pSystem = gEnv->pGameFramework->GetILevelSystem();
		if (NULL != pSystem)
		{
			ILevel* pLevel = pSystem->GetCurrentLevel();
			if (NULL != pLevel && (NULL != pLevel->GetLevelInfo()))
			{
				outHostInfo.currentLevel = pLevel->GetLevelInfo()->GetName();
			}
		}
	}

	#if CRY_PLATFORM_WINAPI
	// extract valid executable path from module name
	char szExecutablePath[MAX_PATH];
	cry_strcpy(szExecutablePath, gEnv->pSystem->GetICmdLine()->GetArg(0)->GetValue());
	for (size_t i = 0; i < MAX_PATH; ++i)
	{
		if (szExecutablePath[i] == '/')
		{
			szExecutablePath[i] = '\\';
		}
	}
	outHostInfo.buildExecutable = szExecutablePath;

	// extract valid build path from executable name
	char* ch = strrchr(szExecutablePath, '\\');
	if (ch != NULL)
	{
		ch[1] = 0;
	}
	outHostInfo.buildDirectory = szExecutablePath;
	#endif
}

void CHost::ClearSelectionData()
{
	m_selection.clear();
}

void CHost::SetSelectionData(const std::vector<SSelectedObject>& selectedObjects)
{
	m_selection = selectedObjects;
}

void CHost::DrawSelection()
{
	for (TSelectionObjects::const_iterator it = m_selection.begin();
	     it != m_selection.end(); ++it)
	{
		const SSelectedObject& obj = (*it);

		IRenderAuxGeom* pAux = gEnv->pRenderer->GetIRenderAuxGeom();

		// bounding box
		const ColorB lineColor(255, 255, 128, 255);
		if (!obj.m_box.IsEmpty())
		{
			pAux->DrawAABB(obj.m_box, obj.m_matrix, false, lineColor, eBBD_Faceted);
		}

		// pivot
		const float s = 0.1f;
		pAux->DrawLine(obj.m_position - Vec3(s, 0, 0), lineColor,
		               obj.m_position + Vec3(s, 0, 0), lineColor, 2.0f);
		pAux->DrawLine(obj.m_position - Vec3(0, s, 0), lineColor,
		               obj.m_position + Vec3(0, s, 0), lineColor, 2.0f);
		pAux->DrawLine(obj.m_position - Vec3(0, 0, s), lineColor,
		               obj.m_position + Vec3(0, 0, s), lineColor, 2.0f);

		// name
		if (!obj.m_name.empty())
		{
			IRenderAuxText::DrawText(obj.m_position, Vec2(2), NULL, eDrawText_Center | eDrawText_FixedSize | eDrawText_800x600, obj.m_name.c_str());
		}
	}
}

}

#endif
