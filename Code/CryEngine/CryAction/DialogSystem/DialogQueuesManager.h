// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __DIALOGQUEUESMANAGER_H__
#define __DIALOGQUEUESMANAGER_H__

class CDialogSession;

#include <CryCore/StlUtils.h>
#include <CryNetwork/ISerialize.h>
#include <CryNetwork/ISerializeHelper.h>

#ifndef _RELEASE
	#define DEBUGINFO_DIALOGBUFFER
#endif

// this works like a 'buffered channel' feature:.
// - Dialogs can be assigned to a buffer.
// - When a dialog that is being played is assigned to a buffer, any other dialog that tries to play through that buffer is hold in a waiting list, until all previous ones finish
class CDialogQueuesManager
{
public:
	typedef int TDialogId;

	CDialogQueuesManager();
	void      Reset();
	TDialogId Play(uint32 queueID, const string& name);
	bool      IsDialogWaiting(uint32 queueID, TDialogId dialogId);
	void      NotifyDialogDone(uint32 queueID, TDialogId dialogId);
	uint32    BufferEnumToId(uint32 queueEnum); // queue enum means the values stored in the .xml definition file
	bool      IsBufferFree(uint32 queueID);
	void      Serialize(TSerialize ser);
	void      Update();

	static const uint32 NO_QUEUE = (uint32) - 1;  //use this special queueID to mark dialogs outside of the queuing system (will always be played)

private:

	TDialogId CreateNewDialogId()                  { CryInterlockedIncrement(&m_uniqueDialogID); return m_uniqueDialogID; }
	bool      IsQueueIDValid(uint32 queueID) const { return queueID < m_numBuffers; }  //Will return false for "no_queue" dialogs, which causes the dialog not to be queued, but to be started directly

	typedef std::vector<uint32>  TBuffer;
	typedef std::vector<TBuffer> TBuffersList;

	TBuffersList m_buffersList;
	uint32       m_numBuffers;     // == m_buffersList.size().
	int          m_uniqueDialogID; // dialog ids come from increasing this

#ifdef DEBUGINFO_DIALOGBUFFER
	typedef std::vector<string>         TBufferNames;
	typedef std::map<TDialogId, string> TDialogNames;
	TBufferNames m_bufferNames;
	TDialogNames m_dialogNames;
#endif
};

#endif
