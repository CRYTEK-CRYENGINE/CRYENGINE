// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <fmod_common.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CParameterInfo final
{
public:

	CParameterInfo() = delete;
	CParameterInfo& operator=(CParameterInfo const&) = delete;

	explicit CParameterInfo(FMOD_STUDIO_PARAMETER_ID const& id, bool const isGlobal, char const* const szName)
		: m_id(id)
		, m_isGlobal(isGlobal)
		, m_name(szName)
	{}

	explicit CParameterInfo(char const* const szName)
		: m_isGlobal(false)
		, m_name(szName)
	{
		ZeroStruct(m_id);
	}

	bool                            operator==(CParameterInfo const& rhs) const { return (m_id.data1 == rhs.m_id.data1) && (m_id.data2 == rhs.m_id.data2); }
	bool                            operator<(CParameterInfo const& rhs) const  { return (m_id.data1 < rhs.m_id.data1); }

	FMOD_STUDIO_PARAMETER_ID const& GetId() const                               { return m_id; }
	void                            SetId(FMOD_STUDIO_PARAMETER_ID const& id)   { m_id = id; }

	char const*                     GetName() const                             { return m_name.c_str(); }

	bool                            IsGlobal() const                            { return m_isGlobal; }
	bool                            HasValidId() const                          { return (m_id.data1 != 0) || (m_id.data2 != 0); }

private:

	FMOD_STUDIO_PARAMETER_ID                    m_id;
	bool const                                  m_isGlobal;
	CryFixedStringT<MaxControlNameLength> const m_name;
};

using Parameters = std::map<CParameterInfo, float>;
}      // namespace Fmod
}      // namespace Impl
}      // namespace CryAudio
