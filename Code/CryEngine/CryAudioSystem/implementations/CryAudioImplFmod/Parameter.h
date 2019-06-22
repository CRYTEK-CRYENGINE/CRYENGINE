// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParameterInfo.h"
#include <IParameterConnection.h>
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CParameter final : public IParameterConnection, public CPoolObject<CParameter, stl::PSyncNone>
{
public:

	CParameter() = delete;
	CParameter(CParameter const&) = delete;
	CParameter(CParameter&&) = delete;
	CParameter& operator=(CParameter const&) = delete;
	CParameter& operator=(CParameter&&) = delete;

	explicit CParameter(
		CParameterInfo const& parameterInfo,
		float const multiplier,
		float const shift)
		: m_parameterInfo(parameterInfo)
		, m_multiplier(multiplier)
		, m_shift(shift)
	{}

	virtual ~CParameter() override;

	// IParameterConnection
	virtual void Set(IObject* const pIObject, float const value) override;
	virtual void SetGlobally(float const value) override;
	// ~IParameterConnection

private:

	CParameterInfo m_parameterInfo;
	float const    m_multiplier;
	float const    m_shift;
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
