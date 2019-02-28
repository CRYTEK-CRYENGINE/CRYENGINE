// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIBubblesSystemImpl.h"
#include "../DebugDrawContext.h"

bool AIQueueBubbleMessage(const char* messageName, const EntityId entityID,
                          const char* message, uint32 flags, float duration /*= 0*/, SAIBubbleRequest::ERequestType requestType /* = eRT_ErrorMessage */)
{
#ifdef CRYAISYSTEM_DEBUG
	if (IAIBubblesSystem* pAIBubblesSystem = gAIEnv.pBubblesSystem)
	{
		SAIBubbleRequest request(entityID, message, flags, duration, requestType);
		pAIBubblesSystem->QueueMessage(messageName, request);
		return true;
	}
#endif
	return false;
}

#ifdef CRYAISYSTEM_DEBUG

void CAIBubblesSystem::Init()
{
	SetNameFilter(gAIEnv.CVars.BubblesSystemNameFilter);
}

void CAIBubblesSystem::Reset()
{
	stl::free_container(m_entityRequestsMap);
}

static uint32 ComputeMessageNameId(const char* szMessageName)
{
	return CCrc32::ComputeLowercase(szMessageName);
}

SBubbleRequestId CAIBubblesSystem::QueueMessage(const char* messageName, const SAIBubbleRequest& request)
{
	const TBubbleRequestOptionFlags requestFlags = request.GetFlags();
	const EntityId entityID = request.GetEntityID();
	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);
	if (!pEntity)
	{
		LogMessage("The AIBubblesSystem received a message for a non existing entity."
		           " The message will be discarded.", requestFlags);
		return SBubbleRequestId::Invalid();
	}

	RequestsList& requestsList = m_entityRequestsMap[request.GetEntityID()];
	const uint32 messageNameId = ComputeMessageNameId(messageName);
	RequestsList::iterator requestContainerIt = std::find_if(requestsList.begin(), requestsList.end(), SRequestByMessageNameFinder(messageNameId));

	if (request.GetRequestType() == SAIBubbleRequest::eRT_PrototypeDialog && requestContainerIt != requestsList.end())
	{
		requestsList.erase(requestContainerIt);
		requestContainerIt = requestsList.end();
	}

	if (requestContainerIt == requestsList.end())
	{
		const SBubbleRequestId requestId = ++m_requestIdCounter;

		if (request.GetRequestType() == SAIBubbleRequest::eRT_PrototypeDialog)
			requestsList.push_back(SAIBubbleRequestContainer(requestId, messageNameId, request));
		else
			requestsList.push_front(SAIBubbleRequestContainer(requestId, messageNameId, request));

		// Log the message and if it's needed show the blocking popup
		stack_string sTmp;
		stack_string sMessage;
		request.GetMessage(sTmp);
		sMessage.Format("%s - Message: %s", pEntity->GetName(), sTmp.c_str());

		LogMessage(sMessage.c_str(), requestFlags);

		PopupBlockingAlert(sMessage.c_str(), requestFlags);

		return requestId;
	}

	return SBubbleRequestId::Invalid();
}

bool CAIBubblesSystem::CancelMessageBubble(const EntityId entityId, const SBubbleRequestId& bubbleRequestId)
{
	auto iter = m_entityRequestsMap.find(entityId);
	if (iter != m_entityRequestsMap.end())
	{
		RequestsList& requestsList = iter->second;
		RequestsList::iterator requestContainerIt = std::find_if(requestsList.begin(), requestsList.end(), SRequestByRequestIdFinder(bubbleRequestId));
		if (requestContainerIt != requestsList.end())
		{
			requestsList.erase(requestContainerIt);
			return true;
		}
	}
	return false;
}

class CAIBubblesSystem::CBubbleRender
{
public:
	explicit CBubbleRender(CDebugDrawContext& drawContext, EntityId entityId)
		: m_drawContext(drawContext)
		, m_entityId(entityId)
		, m_bubbleOnscreenPos(ZERO)
		, m_currentTextSize(1.0f)
		, m_bInitialized(false)
		, m_bOnScreen(false)
	{}

	bool Prepare()
	{
		if (m_entityId == INVALID_ENTITYID)
		{
			return false;
		}

		if (!m_bInitialized)
		{
			const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId);
			if (!pEntity)
			{
				m_entityId = INVALID_ENTITYID;
				return false;
			}

			m_bInitialized = true;

			Vec3 entityPos = pEntity->GetWorldPos();
			AABB aabb;
			pEntity->GetLocalBounds(aabb);
			entityPos.z = entityPos.z + aabb.max.z + 0.2f;

			float x, y, z;

			if (m_drawContext->ProjectToScreen(entityPos.x, entityPos.y, entityPos.z, &x, &y, &z))
			{
				if ((z >= 0.0f) && (z <= 1.0f))
				{
					m_bOnScreen = true;

					x *= m_drawContext->GetWidth() * 0.01f;
					y *= m_drawContext->GetHeight() * 0.01f;

					m_bubbleOnscreenPos = Vec3(x, y, z);

					const float distance = gEnv->pSystem->GetViewCamera().GetPosition().GetDistance(entityPos);
					m_currentTextSize = gAIEnv.CVars.BubblesSystemFontSize / distance;
				}
			}
		}

		return true;
	}

	bool IsOnScreen() const
	{
		return m_bOnScreen;
	}

	void DrawBubble(const char* const message, const size_t numberOfLines, const EBubbleRequestOption flags)
	{
		const float lineHeight = 11.5f * m_currentTextSize;
		const float messageHeight = lineHeight * (numberOfLines - 1);
		const float messageShift = messageHeight + lineHeight;
		const Vec3 drawingPosition = m_bubbleOnscreenPos + Vec3(0.0f, messageHeight, 0.0f);

		const bool bFramed = (flags & eBNS_BalloonNoFrame) == 0;
		bool bUseDepthTest;
		if (flags & eBNS_BalloonOverrideDepthTestCVar)
		{
			bUseDepthTest = (flags & eBNS_BalloonUseDepthTest) != 0;
		}
		else
		{
			bUseDepthTest = ((gAIEnv.CVars.BubblesSystemUseDepthTest) != 0);
		}

		DrawBubbleMessage(message, drawingPosition, bFramed, bUseDepthTest);

		m_bubbleOnscreenPos = m_bubbleOnscreenPos + Vec3(0.0f, -messageShift, 0.0f);
	}
private:

	void DrawBubbleMessage(const char* const szMessage, const Vec3& drawingPosition, const bool bFramed, const bool bDepthTest)
	{
		SDrawTextInfo ti;
		ti.scale = Vec2(m_currentTextSize);
		ti.flags = eDrawText_IgnoreOverscan | eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_Center
		           | (bDepthTest ? eDrawText_DepthTest : 0)
		           | (bFramed ? eDrawText_Framed : 0);
		ti.color[0] = 0.0f;
		ti.color[1] = 0.0f;
		ti.color[2] = 0.0f;
		ti.color[3] = 1.0f;

		m_drawContext->DrawLabel(drawingPosition, ti, szMessage);
	}

private:
	CDebugDrawContext& m_drawContext;
	EntityId           m_entityId;
	Vec3               m_bubbleOnscreenPos;
	float              m_currentTextSize;
	bool               m_bInitialized;
	bool               m_bOnScreen;
};

void CAIBubblesSystem::Update()
{
	if (!m_entityRequestsMap.empty())
	{
		CDebugDrawContext debugDrawContext;
		const CTimeValue currentTimestamp = gEnv->pTimer->GetFrameStartTime();
		EntityRequestsMap::iterator it = m_entityRequestsMap.begin();
		EntityRequestsMap::iterator end = m_entityRequestsMap.end();
		for (; it != end; ++it)
		{
			RequestsList& requestsList = it->second;
			if (!requestsList.empty())
			{
				const EntityId entityId = it->first;
				CBubbleRender bubbleRender(debugDrawContext, entityId);

				auto reqIt = requestsList.begin();
				auto reqItEnd = requestsList.end();

				while (reqIt != reqItEnd)
				{
					SAIBubbleRequestContainer& requestContainer = *reqIt;
					bool result = DisplaySpeechBubble(requestContainer, bubbleRender);
					if (requestContainer.IsOld(currentTimestamp) || !result)
					{
						reqIt = requestsList.erase(reqIt);
					}
					else
					{
						++reqIt;
					}
				}
			}
		}
	}
}

bool CAIBubblesSystem::DisplaySpeechBubble(SAIBubbleRequestContainer& requestContainer, CBubbleRender& bubbleRender) const
{
	/* This function returns false if the entity connected to the message stop
	   to exist. In this case the corresponding message will be removed from the queue */

	const SAIBubbleRequest& request = requestContainer.GetRequest();

	if (ShouldSuppressMessageVisibility(request.GetRequestType()))
	{
		// We only want to suppress the visibility of the message
		// but we don't want to remove it from the queue
		return true;
	}

	if (ShouldFilterOutMessage(requestContainer.GetMessageNameId()))
	{
		return true;
	}

	const TBubbleRequestOptionFlags requestFlags = request.GetFlags();

	if (requestFlags & eBNS_Balloon && gAIEnv.CVars.BubblesSystemAlertnessFilter & eBNS_Balloon)
	{
		if (!bubbleRender.Prepare())
		{
			return false;
		}

		if (bubbleRender.IsOnScreen())
		{
			stack_string sFormattedMessage;
			const size_t numberOfLines = request.GetFormattedMessage(sFormattedMessage);

			bubbleRender.DrawBubble(sFormattedMessage.c_str(), numberOfLines, (EBubbleRequestOption)requestFlags);
		}
	}

	return true;
}

void CAIBubblesSystem::LogMessage(const char* const message, const TBubbleRequestOptionFlags flags) const
{
	if (gAIEnv.CVars.BubblesSystemAlertnessFilter & eBNS_Log)
	{
		if (flags & eBNS_LogWarning)
		{
			AIWarning("%s", message);
		}
		else if (flags & eBNS_Log)
		{
			gEnv->pLog->Log("%s", message);
		}
	}
}

void CAIBubblesSystem::PopupBlockingAlert(const char* const message, const TBubbleRequestOptionFlags flags) const
{
	if (ShouldSuppressMessageVisibility())
	{
		return;
	}

	if (flags & eBNS_BlockingPopup && gAIEnv.CVars.BubblesSystemAlertnessFilter & eBNS_BlockingPopup)
	{
		CryMessageBox(message, "AIBubbleSystemMessageBox");
	}
}

bool CAIBubblesSystem::ShouldSuppressMessageVisibility(const SAIBubbleRequest::ERequestType requestType) const
{
	if (gAIEnv.CVars.EnableBubblesSystem != 1)
	{
		return true;
	}

	switch (requestType)
	{
	case SAIBubbleRequest::eRT_ErrorMessage:
		return gAIEnv.CVars.DebugDraw < 1;
	case SAIBubbleRequest::eRT_PrototypeDialog:
		return gAIEnv.CVars.BubblesSystemAllowPrototypeDialogBubbles != 1;
	default:
		assert(0);
		return gAIEnv.CVars.DebugDraw > 0;
	}
}

bool CAIBubblesSystem::ShouldFilterOutMessage(const uint32 messsageNameUniqueId) const
{
	if (m_messageNameFilterSet.empty())
	{
		return false;
	}
	return m_messageNameFilterSet.find(messsageNameUniqueId) == m_messageNameFilterSet.cend();
}

void CAIBubblesSystem::SetNameFilter(const char* szMessageNameFilter)
{
	m_messageNameFilterSet.clear();
	if (!szMessageNameFilter || !szMessageNameFilter[0])
	{
		return;
	}

	stack_string messageNameFilter = szMessageNameFilter;
	const size_t length = messageNameFilter.length();
	int pos = 0;
	do
	{
		stack_string token = messageNameFilter.Tokenize(" ", pos);
		m_messageNameFilterSet.insert(ComputeMessageNameId(token.c_str()));
	}
	while (pos < length);
}

#endif // CRYAISYSTEM_DEBUG
