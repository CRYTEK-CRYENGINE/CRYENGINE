// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _H_LIVECREATEHOST_H_
#define _H_LIVECREATEHOST_H_

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <CryLiveCreate/ILiveCreateHost.h>
#include <CryNetwork/IServiceNetwork.h>
#include <CryLiveCreate/ILiveCreateCommon.h>
#include <CryThreading/IThreadManager.h>
#include <CryNetwork/CrySocks.h>

struct IServiceNetworkListener;
struct IServiceNetworkConnection;
struct IServiceNetworkMessage;
struct IRemoteCommandServer;

#ifndef NO_LIVECREATE

namespace LiveCreate
{

//-----------------------------------------------------------------------------

struct SSelectedObject
{
	Vec3     m_position;
	uint8    m_flags;
	string   m_name;
	Matrix34 m_matrix;
	AABB     m_box;

	ILINE SSelectedObject()
		: m_flags(0)
		, m_position(0, 0, 0)
	{
	}

	template<class T>
	friend T& operator<<(T& stream, SSelectedObject& obj)
	{
		stream << obj.m_flags;
		stream << obj.m_position;

		if (obj.m_flags & 1)
		{
			stream << obj.m_name;
		}

		if (obj.m_flags & 2)
		{
			stream << obj.m_matrix;
			stream << obj.m_box.min;
			stream << obj.m_box.max;
		}

		return stream;
	}
};

//-----------------------------------------------------------------------------
/// LiveCreate "are you there" service with basic file transfer functionality
class CPlatformService : public IThread
{
private:
	class CHost*  m_pHost;
	CRYSOCKET     m_socket;
	uint16        m_port;
	volatile bool m_bQuiet;

public:
	CPlatformService(class CHost* pHost, const uint16 listeningPort);
	virtual ~CPlatformService();

	virtual void ThreadEntry();

	void         SignalStopWork() { m_bQuiet = true; }

	bool         Bind();
	bool         Process(IDataReadStream& reader, IDataWriteStream& writer);
};

//-----------------------------------------------------------------------------
/// LiveCreate file transfer and utility service
class CUtilityService : public IThread
{
private:
	class CHost* m_pHost;

	// TCP/IP listener for file-based traffic
	IServiceNetworkListener* m_pListener;
	volatile bool            m_bQuit;

	// File transfer (a single connection)
	class FileTransferInProgress
	{
	private:
		IServiceNetworkConnection* m_pConnection;
		FILE*                      m_destFile;
		string                     m_destFilePath;
		uint32                     m_fileSize;
		uint32                     m_fileDataLeft;
		CTimeValue                 m_startTime;

	public:
		inline const string& GetFileName() const
		{
			return m_destFilePath;
		}

	public:
		FileTransferInProgress(IServiceNetworkConnection* pConnection, const string& destFilePath, FILE* destFile, const uint32 fileSize);
		~FileTransferInProgress();

		// update (returns true if finished)
		bool Update();
	};

	typedef std::vector<FileTransferInProgress*> TFileTransferList;
	TFileTransferList m_fileTransfers;

public:
	CUtilityService(class CHost* pHost, const uint16 listeningPort);
	virtual ~CUtilityService();

	virtual void ThreadEntry();
	void         SignalStopWork() { m_bQuiet = true; }
};

//-----------------------------------------------------------------------------
/// LiveCreate host interface
class CHost : public IHost
	            , public IRemoteCommandListenerSync
	            , public IRemoteCommandListenerAsync
{
public:
	CHost();
	virtual ~CHost();

	// IHost interface
	virtual bool Initialize(const uint16 listeningPort = kDefaultHostListenPort,
	                        const uint16 discoveryServiceListeningPort = kDefaultDiscoverySerivceListenPost,
	                        const uint16 fileTransferServiceListeningPort = kDefaultFileTransferServicePort);
	virtual void Close();
	virtual void SuppressCommandExecution();
	virtual void ResumeCommandExecution();
	virtual void ExecuteCommands();
	virtual void DrawOverlay();

	// IRemoteCommandListener interface implementation
	virtual bool OnRawMessageSync(const ServiceNetworkAddress& remoteAddress, struct IDataReadStream& msg, struct IDataWriteStream& response);
	virtual bool OnRawMessageAsync(const ServiceNetworkAddress& remoteAddress, struct IDataReadStream& msg, struct IDataWriteStream& response);

private:
	IRemoteCommandServer* m_pCommandServer;

	// Entity CRC
	typedef std::map<EntityId, uint> TEntityCRCMap;
	TEntityCRCMap m_entityCrc;

	// Sync&Refresh
	bool m_updateTimeOfDay;
	bool m_bCommandsSuppressed;

	// Selection data
	typedef std::vector<SSelectedObject> TSelectionObjects;
	TSelectionObjects m_selection;

	// General host platform service (UDP listening socket)
	CPlatformService* m_pPlatformService;

	// General file-transfer and utility service (CopyFrom/CopyTo)
	CUtilityService* m_pUtilityService;

	// IDs of executed console commands - (prevents from executing the same command twice in case it was resent)
	typedef std::set<CryGUID> TLastCommandIDs;
	TLastCommandIDs m_lastConsoleCommandsExecuted;

public:
	// Entity CRC management
	bool GetEntityCRC(EntityId id, uint32& outCrc) const;
	void SetEntityCRC(EntityId id, const uint32 outCrc);
	void RemoveEntityCRC(EntityId id);

	// FullSync level data parsing
	bool SyncFullLevelState(const char* szFileName);

	// Selection display
	void ClearSelectionData();
	void SetSelectionData(const std::vector<SSelectedObject>& selectedObjects);
	void DrawSelection();

	void RequestTimeOfDayUpdate();

	void FillHostInfo(CHostInfoPacket& outHostInfo) const;
};

//-----------------------------------------------------------------------------

}

#endif

#endif
