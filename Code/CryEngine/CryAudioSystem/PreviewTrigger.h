// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "Entity.h"
	#include "Common.h"

namespace CryAudio
{
namespace Impl
{
struct ITriggerInfo;
} // namespace Impl

class CPreviewTrigger final : public Control
{
public:

	CPreviewTrigger(CPreviewTrigger const&) = delete;
	CPreviewTrigger(CPreviewTrigger&&) = delete;
	CPreviewTrigger& operator=(CPreviewTrigger const&) = delete;
	CPreviewTrigger& operator=(CPreviewTrigger&&) = delete;

	CPreviewTrigger()
		: Control(g_previewTriggerId, GlobalContextId, g_szPreviewTriggerName)
	{}

	~CPreviewTrigger();

	void Execute(Impl::ITriggerInfo const& triggerInfo);
	void Execute(XmlNodeRef const& node);
	void Clear();

private:

	void Execute();

	TriggerConnections m_connections;
};
}      // namespace CryAudio
#endif // CRY_AUDIO_USE_DEBUG_CODE