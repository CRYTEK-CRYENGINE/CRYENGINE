// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IListener.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
struct IListener;
} // namespace Impl

class CListener final : public CryAudio::IListener
{
public:

	CListener() = delete;
	CListener(CListener const&) = delete;
	CListener(CListener&&) = delete;
	CListener& operator=(CListener const&) = delete;
	CListener& operator=(CListener&&) = delete;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	explicit CListener(Impl::IListener* const pImplData, ListenerId const id, bool const isUserCreated, char const* const szName)
		: m_pImplData(pImplData)
		, m_id(id)
		, m_isUserCreated(isUserCreated)
		, m_name(szName)
	{}

	explicit CListener(ListenerId const id, bool const isUserCreated, char const* const szName)
		: m_pImplData(nullptr)
		, m_id(id)
		, m_isUserCreated(isUserCreated)
		, m_name(szName)
	{}
#else
	explicit CListener(Impl::IListener* const pImplData, ListenerId const id, bool const isUserCreated)
		: m_pImplData(pImplData)
		, m_id(id)
		, m_isUserCreated(isUserCreated)
	{}

	explicit CListener(ListenerId const id, bool const isUserCreated)
		: m_pImplData(nullptr)
		, m_id(id)
		, m_isUserCreated(isUserCreated)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	// CryAudio::IListener
	virtual ListenerId GetId() const override { return m_id; }
	virtual void       SetTransformation(CTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void       SetName(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	// ~CryAudio::IListener

	Impl::IListener*       GetImplData() const                            { return m_pImplData; }
	void                   SetImplData(Impl::IListener* const pIListener) { m_pImplData = pIListener; }

	bool                   IsUserCreated() const                          { return m_isUserCreated; }
	void                   Update(float const deltaTime);
	void                   HandleSetTransformation(CTransformation const& transformation);
	CTransformation const& GetTransformation() const;

private:

	Impl::IListener* m_pImplData;
	ListenerId       m_id;
	bool             m_isUserCreated;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
public:

	void                   HandleSetName(char const* const szName);
	char const*            GetName() const                { return m_name.c_str(); }
	CTransformation const& GetDebugTransformation() const { return m_transformation; }

private:

	CTransformation                      m_transformation;
	CryFixedStringT<MaxObjectNameLength> m_name;
#endif // CRY_AUDIO_USE_DEBUG_CODE
};
} // namespace CryAudio
