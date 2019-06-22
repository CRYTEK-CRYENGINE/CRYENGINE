// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParameterInfo.h"
#include <ISwitchStateConnection.h>
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CParameterState final : public ISwitchStateConnection, public CPoolObject<CParameterState, stl::PSyncNone>
{
public:

	CParameterState() = delete;
	CParameterState(CParameterState const&) = delete;
	CParameterState(CParameterState&&) = delete;
	CParameterState& operator=(CParameterState const&) = delete;
	CParameterState& operator=(CParameterState&&) = delete;

	explicit CParameterState(CParameterInfo const& parameterInfo, float const value)
		: m_parameterInfo(parameterInfo)
		, m_value(value)
	{}

	virtual ~CParameterState() override;

	// ISwitchStateConnection
	virtual void Set(IObject* const pIObject) override;
	virtual void SetGlobally() override;
	// ~ISwitchStateConnection

private:

	CParameterInfo m_parameterInfo;
	float const    m_value;
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
