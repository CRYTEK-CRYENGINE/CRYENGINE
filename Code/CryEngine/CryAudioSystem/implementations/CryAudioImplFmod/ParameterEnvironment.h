// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParameterInfo.h"
#include <IEnvironmentConnection.h>
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CParameterEnvironment final : public IEnvironmentConnection, public CPoolObject<CParameterEnvironment, stl::PSyncNone>
{
public:

	CParameterEnvironment() = delete;
	CParameterEnvironment(CParameterEnvironment const&) = delete;
	CParameterEnvironment(CParameterEnvironment&&) = delete;
	CParameterEnvironment& operator=(CParameterEnvironment const&) = delete;
	CParameterEnvironment& operator=(CParameterEnvironment&&) = delete;

	explicit CParameterEnvironment(
		CParameterInfo const& parameterInfo,
		float const multiplier,
		float const shift)
		: m_parameterInfo(parameterInfo)
		, m_multiplier(multiplier)
		, m_shift(shift)
	{}

	virtual ~CParameterEnvironment() override;

	// IEnvironmentConnection
	virtual void Set(IObject* const pIObject, float const amount) override;
	// ~IEnvironmentConnection

private:

	CParameterInfo m_parameterInfo;
	float const    m_multiplier;
	float const    m_shift;
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
