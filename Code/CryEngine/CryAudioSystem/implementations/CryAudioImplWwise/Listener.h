// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IListener.h>
#include <AK/SoundEngine/Common/AkTypes.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CListener final : public IListener
{
public:

	CListener() = delete;
	CListener(CListener const&) = delete;
	CListener(CListener&&) = delete;
	CListener& operator=(CListener const&) = delete;
	CListener& operator=(CListener&&) = delete;

	explicit CListener(CTransformation const& transformation, AkGameObjectID const id);

	virtual ~CListener() override = default;

	// CryAudio::Impl::IListener
	virtual void                   Update(float const deltaTime) override;
	virtual void                   SetName(char const* const szName) override;
	virtual void                   SetTransformation(CTransformation const& transformation) override;
	virtual CTransformation const& GetTransformation() const override { return m_transformation; }
	// ~CryAudio::Impl::IListener

	AkGameObjectID GetId() const       { return m_id; }
	bool           HasMoved() const    { return m_hasMoved; }
	Vec3 const&    GetPosition() const { return m_position; }
	Vec3 const&    GetVelocity() const { return m_velocity; }

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

private:

	AkGameObjectID const m_id;
	bool                 m_hasMoved;
	bool                 m_isMovingOrDecaying;
	Vec3                 m_velocity;
	Vec3                 m_position;
	Vec3                 m_previousPosition;
	CTransformation      m_transformation;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	CryFixedStringT<MaxObjectNameLength> m_name;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
};
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
