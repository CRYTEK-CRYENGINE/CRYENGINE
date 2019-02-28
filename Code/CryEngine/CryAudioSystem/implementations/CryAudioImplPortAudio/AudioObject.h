// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLEntities.h"
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
class CEvent;

class CObject final : public IObject, public CPoolObject<CObject, stl::PSyncNone>
{
public:

	CObject() = default;
	virtual ~CObject() override = default;

	CObject(CObject const&) = delete;
	CObject(CObject&&) = delete;
	CObject& operator=(CObject const&) = delete;
	CObject& operator=(CObject&&) = delete;

	// CryAudio::Impl::IObject
	virtual ERequestStatus Update() override;
	virtual ERequestStatus Set3DAttributes(SObject3DAttributes const& attributes) override;
	virtual ERequestStatus SetEnvironment(IEnvironment const* const pIEnvironment, float const amount) override;
	virtual ERequestStatus SetParameter(IParameter const* const pIParameter, float const value) override;
	virtual ERequestStatus SetSwitchState(ISwitchState const* const pISwitchState) override;
	virtual ERequestStatus SetObstructionOcclusion(float const obstruction, float const occlusion) override;
	virtual ERequestStatus ExecuteTrigger(ITrigger const* const pITrigger, IEvent* const pIEvent) override;
	virtual ERequestStatus StopAllTriggers() override;
	virtual ERequestStatus PlayFile(IStandaloneFile* const pIStandaloneFile) override;
	virtual ERequestStatus StopFile(IStandaloneFile* const pIStandaloneFile) override;
	virtual ERequestStatus SetName(char const* const szName) override;
	// ~CryAudio::Impl::IObject

	void StopEvent(uint32 const pathId);
	void RegisterEvent(CEvent* const pEvent);
	void UnregisterEvent(CEvent* const pEvent);

private:

	std::vector<CEvent*> m_activeEvents;
};
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio
