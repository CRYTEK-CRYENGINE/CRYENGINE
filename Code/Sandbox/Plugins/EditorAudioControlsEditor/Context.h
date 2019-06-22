// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <PoolObject.h>

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryString/CryString.h>

namespace ACE
{
class CContext final : public CryAudio::CPoolObject<CContext, stl::PSyncNone>
{
public:

	CContext() = delete;
	CContext(CContext const&) = delete;
	CContext(CContext&&) = delete;
	CContext& operator=(CContext const&) = delete;
	CContext& operator=(CContext&&) = delete;

	explicit CContext(
		CryAudio::ContextId const id,
		string const& name,
		bool isActive,
		bool const isRegistered)
		: m_id(id)
		, m_name(name)
		, m_isActive(isActive)
		, m_isRegistered(isRegistered)
	{}

	~CContext() = default;

	CryAudio::ContextId GetId() const   { return m_id; }

	string const&       GetName() const { return m_name; }
	void                SetName(string const& name);

	bool                IsActive() const                       { return m_isActive; }
	void                SetActive(bool const isActive)         { m_isActive = isActive; }

	bool                IsRegistered() const                   { return m_isRegistered; }
	void                SetRegistered(bool const isRegistered) { m_isRegistered = isRegistered; }

private:

	CryAudio::ContextId m_id;
	string              m_name;
	bool                m_isActive;
	bool                m_isRegistered; // True if files within this context exist, false for unsaved new contexts.
};
} // namespace ACE
