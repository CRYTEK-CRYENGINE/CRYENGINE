// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IAIBubblesSystem.h
//  Version:     v1.00
//  Created:     30/7/2009 by Ivo Frey
//  Compilers:   Visual Studio.NET
//  Description: Bubbles System interface
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __IAIBubblesSystem_h__
#define __IAIBubblesSystem_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#define MAX_BUBBLE_MESSAGE_LENGHT 512
#define MIN_CHARACTERS_INTERVAL   30

#include <CryEntitySystem/IEntity.h>

typedef uint32 TBubbleRequestOptionFlags;

struct SAIBubbleRequest
{
	enum ERequestType
	{
		eRT_ErrorMessage = 0,
		eRT_PrototypeDialog,
	};

	SAIBubbleRequest(const EntityId _entityID, const char* _message, TBubbleRequestOptionFlags _flags, float _duration = 0.0f, const ERequestType _requestType = eRT_ErrorMessage)
		: entityID(_entityID)
		, message(_message)
		, flags(_flags)
		, duration(_duration)
		, requestType(_requestType)
		, totalLinesFormattedMessage(0)
	{
		FormatMessageWithMultiline(MIN_CHARACTERS_INTERVAL);
	}

	SAIBubbleRequest(const SAIBubbleRequest& request)
		: entityID(request.entityID)
		, message(request.message)
		, flags(request.flags)
		, duration(request.duration)
		, requestType(request.requestType)
		, totalLinesFormattedMessage(request.totalLinesFormattedMessage)
	{}

	size_t GetFormattedMessage(stack_string& outMessage) const { outMessage = message; return totalLinesFormattedMessage; }
	void   GetMessage(stack_string& outMessage) const
	{
		outMessage = message;
		std::replace(outMessage.begin(), outMessage.end(), '\n', ' ');
	}

	const EntityId            GetEntityID() const        { return entityID; }
	TBubbleRequestOptionFlags GetFlags() const           { return flags; }
	float                     GetDuration() const        { return duration; }
	bool                      IsDurationInfinite() const { return duration == FLT_MAX; }
	ERequestType              GetRequestType() const     { return requestType; }

private:
	void FormatMessageWithMultiline(size_t minimumCharactersInterval)
	{
		size_t pos = message.find(' ', minimumCharactersInterval);
		size_t i = 1;
		while (pos != message.npos)
		{
			message.replace(pos, 1, "\n");
			pos = message.find(' ', minimumCharactersInterval * (++i));
		}
		totalLinesFormattedMessage = i;
	}

	EntityId                                   entityID;
	CryFixedStringT<MAX_BUBBLE_MESSAGE_LENGHT> message;
	TBubbleRequestOptionFlags                  flags;
	float duration;
	const ERequestType                         requestType;
	size_t totalLinesFormattedMessage;

};

enum EBubbleRequestOption
{
	eBNS_None                         = 0,
	eBNS_Log                          = BIT(0),
	eBNS_Balloon                      = BIT(1),
	eBNS_BlockingPopup                = BIT(2),
	eBNS_LogWarning                   = BIT(3),
	eBNS_BalloonNoFrame               = BIT(4),
	eBNS_BalloonOverrideDepthTestCVar = BIT(5),
	eBNS_BalloonUseDepthTest          = BIT(6),
};

struct SBubbleRequestId
{
	SBubbleRequestId() : id(0) {}
	explicit SBubbleRequestId(const uint32 _id) : id(_id) {}
	SBubbleRequestId& operator++() { ++id; return *this; }
	bool operator==(const SBubbleRequestId& other) const { return id == other.id; }
	operator uint32() const { return id; }

	static SBubbleRequestId Invalid() { return SBubbleRequestId(); }

private:
	uint32 id;
};

struct IAIBubblesSystem
{
	virtual ~IAIBubblesSystem() {};

	virtual void Init() = 0;
	/*
	   messageName is an identifier for the part of the code where the
	   QueueMessage is called to avoid queuing the same message
	   several time before the previous message is actually expired
	   inside the BubbleSystem
	 */
	virtual SBubbleRequestId QueueMessage(const char* messageName, const SAIBubbleRequest& request) = 0;
	virtual bool CancelMessageBubble(const EntityId entityId, const SBubbleRequestId& bubbleRequestId) = 0;
	virtual void Update() = 0;
	virtual void Reset() = 0;
	/*
		Name filter allows to show only messages with passing messageName.
		If filter string is empty - all message are shown. Otherwise,
		szMessageNameFilter is a string with space-delimited list of allowed names.
	*/
	virtual void SetNameFilter(const char* szMessageNameFilter) = 0;
};

#endif // __IAIBubblesSystem_h__
