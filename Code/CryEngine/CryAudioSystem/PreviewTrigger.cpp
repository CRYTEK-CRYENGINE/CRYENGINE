// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "PreviewTrigger.h"
	#include "TriggerUtils.h"
	#include "Object.h"
	#include "Common/IImpl.h"
	#include "Common/IObject.h"
	#include "Common/ITriggerConnection.h"
	#include "Common/ITriggerInfo.h"
	#include "Common/Logger.h"

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CPreviewTrigger::~CPreviewTrigger()
{
	CRY_ASSERT_MESSAGE(m_connections.empty(), "There are still connections during %s", __FUNCTION__);
}

//////////////////////////////////////////////////////////////////////////
void CPreviewTrigger::Execute(Impl::ITriggerInfo const& triggerInfo)
{
	Clear();

	Impl::ITriggerConnection* const pConnection = g_pIImpl->ConstructTriggerConnection(&triggerInfo);

	if (pConnection != nullptr)
	{
		m_connections.push_back(pConnection);
		Execute();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewTrigger::Execute(XmlNodeRef const& node)
{
	Clear();

	int const numConnections = node->getChildCount();

	for (int i = 0; i < numConnections; ++i)
	{
		XmlNodeRef const triggerImplNode(node->getChild(i));

		if (triggerImplNode.isValid())
		{
			float radius = 0.0f;
			Impl::ITriggerConnection* const pConnection = g_pIImpl->ConstructTriggerConnection(triggerImplNode, radius);

			if (pConnection != nullptr)
			{
				m_connections.push_back(pConnection);
			}
		}
	}

	if (!m_connections.empty())
	{
		Execute();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewTrigger::Execute()
{
	Impl::IObject* const pIObject = g_previewObject.GetImplData();

	uint16 numPlayingInstances = 0;
	uint16 numPendingInstances = 0;

	for (auto const pConnection : m_connections)
	{
		ETriggerResult const result = pConnection->Execute(pIObject, g_triggerInstanceIdCounter);

		if ((result == ETriggerResult::Playing) || (result == ETriggerResult::Virtual) || (result == ETriggerResult::Pending))
		{
			if (result != ETriggerResult::Pending)
			{
				++numPlayingInstances;
			}
			else
			{
				++numPendingInstances;
			}
		}
		else if (result != ETriggerResult::DoNotTrack)
		{
			Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" failed on object "%s" during %s)", GetName(), g_previewObject.GetName(), __FUNCTION__);
		}
	}

	if ((numPlayingInstances > 0) || (numPendingInstances > 0))
	{
		g_previewObject.ConstructTriggerInstance(m_id, INVALID_ENTITYID, numPlayingInstances, numPendingInstances, ERequestFlags::None, nullptr, nullptr, nullptr, 0.0f);
	}
	else
	{
		// All of the events have either finished before we got here or never started, immediately inform the user that the trigger has finished.
		SendFinishedTriggerInstanceRequest(m_id, INVALID_ENTITYID);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewTrigger::Clear()
{
	CRY_ASSERT_MESSAGE(g_pIImpl != nullptr, "g_pIImpl mustn't be nullptr during %s", __FUNCTION__);

	for (auto const pConnection : m_connections)
	{
		g_pIImpl->DestructTriggerConnection(pConnection);
	}

	m_connections.clear();
}
}      // namespace CryAudio
#endif // CRY_AUDIO_USE_DEBUG_CODE