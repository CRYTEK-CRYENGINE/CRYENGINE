// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <IObject.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
class CCue;
class CCueInstance;
class CListener;

enum class EObjectFlags : EnumFlagsType
{
	None                    = 0,
	MovingOrDecaying        = BIT(0),
	TrackAbsoluteVelocity   = BIT(1),
	TrackVelocityForDoppler = BIT(2),
	UpdateVirtualStates     = BIT(3),
	IsVirtual               = BIT(4),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EObjectFlags);

class CObject final : public IObject, public CPoolObject<CObject, stl::PSyncNone>
{
public:

	CObject() = delete;
	CObject(CObject const&) = delete;
	CObject(CObject&&) = delete;
	CObject& operator=(CObject const&) = delete;
	CObject& operator=(CObject&&) = delete;

	explicit CObject(CTransformation const& transformation, CListener* const pListener);
	virtual ~CObject() override;

	// CryAudio::Impl::IObject
	virtual void                   Update(float const deltaTime) override;
	virtual void                   SetTransformation(CTransformation const& transformation) override;
	virtual CTransformation const& GetTransformation() const override                            { return m_transformation; }
	virtual void                   SetOcclusion(IListener* const pIListener, float const occlusion, uint8 const numRemainingListeners) override;
	virtual void                   SetOcclusionType(EOcclusionType const occlusionType) override {}
	virtual void                   StopAllTriggers() override;
	virtual ERequestStatus         SetName(char const* const szName) override;
	virtual void                   AddListener(IListener* const pIListener) override;
	virtual void                   RemoveListener(IListener* const pIListener) override;
	virtual void                   ToggleFunctionality(EObjectFunctionality const type, bool const enable) override {}

	// Below data is only used when CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE is defined!
	virtual void DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY, char const* const szTextFilter) override;
	// ~CryAudio::Impl::IObject

	CriAtomExPlayerHn   GetPlayer() const       { return m_pPlayer; }
	CueInstances const& GetCueInstances() const { return m_cueInstances; }

	void                AddCueInstance(CCueInstance* const pCueInstance);
	void                StopCue(uint32 const cueId);
	void                PauseCue(uint32 const cueId);
	void                ResumeCue(uint32 const cueId);
	void                MutePlayer(CriBool const shouldPause);
	void                PausePlayer(CriBool const shouldPause);
	void                UpdateVelocityTracking();

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	CListener*  GetListener() const { return m_pListener; }
	char const* GetName() const     { return m_name.c_str(); }
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

private:

	void UpdateVirtualState(CCueInstance* const pCueInstance);
	void UpdateVelocities(float const deltaTime);

	CListener*          m_pListener;
	EObjectFlags        m_flags;
	float               m_occlusion;
	float               m_previousAbsoluteVelocity;
	CTransformation     m_transformation;
	Vec3                m_position;
	Vec3                m_previousPosition;
	Vec3                m_velocity;
	S3DAttributes       m_3dAttributes;
	CriAtomEx3dSourceHn m_p3dSource;
	CriAtomExPlayerHn   m_pPlayer;
	CueInstances        m_cueInstances;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	float m_absoluteVelocity;
	float m_absoluteVelocityNormalized;
	CryFixedStringT<MaxObjectNameLength> m_name;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
};
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
