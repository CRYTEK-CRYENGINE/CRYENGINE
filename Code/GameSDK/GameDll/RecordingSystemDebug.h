// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RECORDINGSYSTEMDEBUG_H__
#define __RECORDINGSYSTEMDEBUG_H__
#include "RecordingSystemDefines.h"

#ifdef RECSYS_DEBUG


class CRecordingSystemDebug : public IRecordingSystemListener
{
public:
	CRecordingSystemDebug(CRecordingSystem& system);
	virtual ~CRecordingSystemDebug();

	//IRecordingSystemListener
	virtual void OnPlaybackRequested( const SPlaybackRequest& info ) {}
	virtual void OnPlaybackStarted( const SPlaybackInfo& info ) {}
	virtual void OnPlaybackEnd( const SPlaybackInfo& info ) {}
	//~IRecordingSystemListener

	void PrintFirstPersonPacketData ( const SRecording_Packet& packet ) const;
	void PrintThirdPersonPacketData ( const SRecording_Packet& packet, float& frameTime ) const;
	void PrintFirstPersonPacketData ( uint8* data, size_t size, const char* const msg ) const;
	void PrintThirdPersonPacketData ( uint8* data, size_t size, const char* const msg ) const;
	void PrintFirstPersonPacketData ( CRecordingBuffer& buffer, const char* const msg ) const;
	void PrintThirdPersonPacketData ( CRecordingBuffer& buffer, const char* const msg ) const;

private:
	CRecordingSystem& m_system;
};


#endif //RECSYS_DEBUG
#endif // __RECORDINGSYSTEMDEBUG_H__