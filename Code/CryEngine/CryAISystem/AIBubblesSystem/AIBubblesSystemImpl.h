// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Description: Central class to create and display speech bubbles over the
// AI agents to notify messages to the designers

#include "AIBubblesSystem.h"

#ifdef CRYAISYSTEM_DEBUG

class CAIBubblesSystem : public IAIBubblesSystem
{
public:
	// IAIBubblesSystem
	virtual void Init() override;
	virtual void Reset() override;
	virtual SBubbleRequestId QueueMessage(const char* messageName, const SAIBubbleRequest& request) override;
	virtual bool CancelMessageBubble(const EntityId entityId, const SBubbleRequestId& bubbleRequestId) override;
	virtual void Update() override;
	virtual void SetNameFilter(const char* szMessageNameFilter) override;
	// ~IAIBubblesSystem

private:

	// SAIBubbleRequestContainer needs only to be known internally in the 
	// BubblesNotifier
	struct SAIBubbleRequestContainer
	{
		SAIBubbleRequestContainer(const SBubbleRequestId& _requestId, const uint32 _messageNameId, const SAIBubbleRequest& _request)
			:messageNameId(_messageNameId)
			,requestId(_requestId)
			,expiringTime(.0f)
			,request(_request)
			,startExpiringTime(true)
			,neverExpireFromTime(false)
		{
		}

		const SAIBubbleRequest& GetRequest() 
		{ 
			if(startExpiringTime)
			{
				neverExpireFromTime = request.IsDurationInfinite();
				if (!neverExpireFromTime)
				{
					UpdateDuration(request.GetDuration());
				}
				startExpiringTime = false;
			}
			return request; 
		}
		uint32 GetMessageNameId() const { return messageNameId; }
		const SBubbleRequestId& GetRequestId() const { return requestId; }

		bool IsOld (const CTimeValue currentTimestamp) const
		{
			return !neverExpireFromTime && (currentTimestamp > expiringTime);
		}

	private:
		void UpdateDuration(const float duration)
		{
			expiringTime = GetAISystem()->GetFrameStartTime() + (duration ? CTimeValue(duration) : CTimeValue(gAIEnv.CVars.legacyBubblesSystem.BubblesSystemDecayTime));
		}

		uint32           messageNameId;
		SBubbleRequestId requestId;
		CTimeValue       expiringTime;
		bool             startExpiringTime;
		bool             neverExpireFromTime;
		SAIBubbleRequest request;
	};

	struct SRequestByMessageNameFinder
	{
		SRequestByMessageNameFinder(uint32 _messageNameId) : messageNameId(_messageNameId) {}
		bool operator()(const SAIBubbleRequestContainer& container) const {return container.GetMessageNameId() == messageNameId;}
		uint32 messageNameId;
	};

	struct SRequestByRequestIdFinder
	{
		SRequestByRequestIdFinder(const SBubbleRequestId& _requestId) : requestId(_requestId) {}
		bool operator()(const SAIBubbleRequestContainer& container) const {return container.GetRequestId() == requestId;}
		SBubbleRequestId requestId;
	};

	class CBubbleRender;

	bool DisplaySpeechBubble(SAIBubbleRequestContainer& requestContainer, CBubbleRender& bubbleRender) const;

	// Information logging/popping up
	void LogMessage(const char* const message, const TBubbleRequestOptionFlags flags) const;
	void PopupBlockingAlert(const char* const message, const TBubbleRequestOptionFlags flags) const;

	bool ShouldSuppressMessageVisibility(const SAIBubbleRequest::ERequestType requestType = SAIBubbleRequest::eRT_ErrorMessage) const;
	bool ShouldFilterOutMessage(const uint32 messsageNameUniqueId) const;

	typedef std::list<SAIBubbleRequestContainer> RequestsList;
	typedef std::unordered_map<EntityId, RequestsList, stl::hash_uint32> EntityRequestsMap;
	EntityRequestsMap	m_entityRequestsMap;

	typedef std::unordered_set<uint32> MessageNameFilterSet;
	MessageNameFilterSet m_messageNameFilterSet;

	SBubbleRequestId m_requestIdCounter;
};
#endif // CRYAISYSTEM_DEBUG
