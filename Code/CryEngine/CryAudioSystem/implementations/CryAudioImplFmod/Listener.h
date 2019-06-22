// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <IListener.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CListener final : public IListener
{
public:

	CListener() = delete;
	CListener(CListener const&) = delete;
	CListener(CListener&&) = delete;
	CListener& operator=(CListener const&) = delete;
	CListener& operator=(CListener&&) = delete;

	explicit CListener(CTransformation const& transformation, int const id);
	virtual ~CListener() override = default;

	ILINE int                 GetId() const     { return m_id; }
	ILINE FMOD_3D_ATTRIBUTES& Get3DAttributes() { return m_attributes; }

	// CryAudio::Impl::IListener
	virtual void                   Update(float const deltaTime) override;
	virtual void                   SetName(char const* const szName) override;
	virtual void                   SetTransformation(CTransformation const& transformation) override;
	virtual CTransformation const& GetTransformation() const override { return m_transformation; }
	// ~CryAudio::Impl::IListener

	Vec3 const& GetPosition() const { return m_position; }
	Vec3 const& GetVelocity() const { return m_velocity; }

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

private:

	void SetVelocity();

	int                m_id;
	bool               m_isMovingOrDecaying;
	Vec3               m_velocity;
	Vec3               m_position;
	Vec3               m_previousPosition;
	CTransformation    m_transformation;
	FMOD_3D_ATTRIBUTES m_attributes;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	CryFixedStringT<MaxObjectNameLength> m_name;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
