// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IListener.h>
#include <PoolObject.h>

#include "Common.h"

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
class CListener final : public IListener
{
public:

	CListener() = delete;
	CListener(CListener const&) = delete;
	CListener(CListener&&) = delete;
	CListener& operator=(CListener const&) = delete;
	CListener& operator=(CListener&&) = delete;

	explicit CListener(CTransformation const& transformation, CriAtomEx3dListenerHn const pHandle);
	virtual ~CListener() override = default;

	// CryAudio::Impl::IListener
	virtual void                   Update(float const deltaTime) override;
	virtual void                   SetName(char const* const szName) override;
	virtual void                   SetTransformation(CTransformation const& transformation) override;
	virtual CTransformation const& GetTransformation() const override { return m_transformation; }
	// ~CryAudio::Impl::IListener

	CriAtomEx3dListenerHn GetHandle() const   { return m_pHandle; }
	Vec3 const&           GetPosition() const { return m_position; }
	Vec3 const&           GetVelocity() const { return m_velocity; }

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

private:

	void SetVelocity();

	CriAtomEx3dListenerHn const m_pHandle;
	bool                        m_isMovingOrDecaying;
	Vec3                        m_velocity;
	Vec3                        m_position;
	Vec3                        m_previousPosition;
	CTransformation             m_transformation;
	S3DAttributes               m_3dAttributes;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	CryFixedStringT<MaxObjectNameLength> m_name;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
};
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
