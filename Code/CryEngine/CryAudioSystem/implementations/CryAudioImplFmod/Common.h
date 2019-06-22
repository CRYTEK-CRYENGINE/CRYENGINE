// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <fmod_common.h>

#define CRY_AUDIO_IMPL_FMOD_INFO_STRING "Fmod Studio - "

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	#define CRY_AUDIO_IMPL_FMOD_ASSERT_OK                    CRY_ASSERT(fmodResult == FMOD_OK)
	#define CRY_AUDIO_IMPL_FMOD_ASSERT_OK_OR_INVALID_HANDLE  CRY_ASSERT(fmodResult == FMOD_OK || fmodResult == FMOD_ERR_INVALID_HANDLE)
	#define CRY_AUDIO_IMPL_FMOD_ASSERT_OK_OR_NOT_LOADED      CRY_ASSERT(fmodResult == FMOD_OK || fmodResult == FMOD_ERR_STUDIO_NOT_LOADED)
	#define CRY_AUDIO_IMPL_FMOD_ASSERT_OK_OR_EVENT_NOT_FOUND CRY_ASSERT(fmodResult == FMOD_OK || fmodResult == FMOD_ERR_EVENT_NOTFOUND)
#else
	#define CRY_AUDIO_IMPL_FMOD_ASSERT_OK                    (void)fmodResult
	#define CRY_AUDIO_IMPL_FMOD_ASSERT_OK_OR_INVALID_HANDLE  (void)fmodResult
	#define CRY_AUDIO_IMPL_FMOD_ASSERT_OK_OR_NOT_LOADED      (void)fmodResult
	#define CRY_AUDIO_IMPL_FMOD_ASSERT_OK_OR_EVENT_NOT_FOUND (void)fmodResult
#endif // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

#define CRY_AUDIO_IMPL_FMOD_INVALID_INDEX (-1)

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CImpl;
class CObject;
class CEventInstance;
class CReturn;
class CListener;
class CEvent;
class CParameterInfo;

extern FMOD::System* g_pCoreSystem;
extern FMOD::Studio::System* g_pStudioSystem;

extern CImpl* g_pImpl;
extern uint32 g_numObjectsWithDoppler;

extern bool g_masterBusPaused;

constexpr char const* g_szOcclusionParameterName = "occlusion";
constexpr char const* g_szAbsoluteVelocityParameterName = "absolute_velocity";

extern CParameterInfo g_occlusionParameterInfo;
extern CParameterInfo g_absoluteVelocityParameterInfo;

using Objects = std::vector<CObject*>;
using EventInstances = std::vector<CEventInstance*>;
using Listeners = std::vector<CListener*>;

using Returns = std::map<CReturn const*, float>;
using SnapshotEventInstances = std::map<uint32, FMOD::Studio::EventInstance*>;

using ParameterIdToIndex = std::map<uint32, int>;
using EventToParameterIndexes = std::map<CEvent const* const, ParameterIdToIndex>;

extern Objects g_constructedObjects;
extern EventToParameterIndexes g_eventToParameterIndexes;
extern SnapshotEventInstances g_activeSnapshots;

///////////////////////////////////////////////////////////////////////////
inline void Fill3DAttributeTransformation(CTransformation const& transformation, FMOD_3D_ATTRIBUTES& attributes)
{
	attributes.forward.x = transformation.GetForward().x;
	attributes.forward.z = transformation.GetForward().y;
	attributes.forward.y = transformation.GetForward().z;

	attributes.position.x = transformation.GetPosition().x;
	attributes.position.z = transformation.GetPosition().y;
	attributes.position.y = transformation.GetPosition().z;

	attributes.up.x = transformation.GetUp().x;
	attributes.up.z = transformation.GetUp().y;
	attributes.up.y = transformation.GetUp().z;

	attributes.velocity.x = 0.0f;
	attributes.velocity.z = 0.0f;
	attributes.velocity.y = 0.0f;
}

///////////////////////////////////////////////////////////////////////////
inline void Fill3DAttributeVelocity(Vec3 const& velocity, FMOD_3D_ATTRIBUTES& attributes)
{
	attributes.velocity.x = velocity.x;
	attributes.velocity.z = velocity.y;
	attributes.velocity.y = velocity.z;
}

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
class CVca;

using ActiveSnapshots = std::vector<CryFixedStringT<MaxControlNameLength>>;
extern ActiveSnapshots g_activeSnapshotNames;

using VcaValues = std::map<CryFixedStringT<MaxControlNameLength>, float>;
extern VcaValues g_vcaValues;

enum class EDebugListFilter : EnumFlagsType
{
	None           = 0,
	EventInstances = BIT(6),  // a
	Snapshots      = BIT(7),  // b
	Vcas           = BIT(8),  // c
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EDebugListFilter);

constexpr EDebugListFilter g_debugListMask =
	EDebugListFilter::EventInstances |
	EDebugListFilter::Snapshots |
	EDebugListFilter::Vcas;
#endif // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}      // namespace Fmod
}      // namespace Impl
}      // namespace CryAudio
