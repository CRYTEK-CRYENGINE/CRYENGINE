// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CommunicationChannelManager_h__
#define __CommunicationChannelManager_h__

#pragma once

#include "Communication.h"
#include "CommunicationChannel.h"

class CommunicationChannelManager
{
public:
	bool                               LoadChannel(const XmlNodeRef& channelNode, const CommChannelID& parentID);

	void                               Clear();
	void                               Reset();
	void                               Update(float updateTime);

	CommChannelID                      GetChannelID(const char* name) const;
	const SCommunicationChannelParams& GetChannelParams(const CommChannelID& channelID) const;
	CommunicationChannel::Ptr          GetChannel(const CommChannelID& channelID, EntityId sourceId) const;
	CommunicationChannel::Ptr          GetChannel(const CommChannelID& channelID, EntityId sourceId);

private:
	typedef std::map<CommChannelID, SCommunicationChannelParams> ChannelParams;
	ChannelParams m_params;

	typedef std::map<CommChannelID, CommunicationChannel::Ptr> Channels;
	Channels m_globalChannels;

	typedef std::map<int, Channels> GroupChannels;
	GroupChannels m_groupChannels;

	typedef std::map<EntityId, Channels> PersonalChannels;
	PersonalChannels m_personalChannels;
};

#endif //__CommunicationChannelManager_h__
