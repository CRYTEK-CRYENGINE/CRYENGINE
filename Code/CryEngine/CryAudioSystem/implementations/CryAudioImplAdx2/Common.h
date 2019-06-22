// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioSystem.h>

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
class CImpl;
class CObject;
class CCueInstance;

extern CImpl* g_pImpl;

using Objects = std::vector<CObject*>;
extern Objects g_constructedObjects;

using AcbHandles = std::map<uint32, CriAtomExAcbHn>;
extern AcbHandles g_acbHandles;

extern CriAtomExPlayerConfig g_playerConfig;
extern CriAtomEx3dSourceConfig g_3dSourceConfig;

extern uint32 g_numObjectsWithDoppler;

constexpr CriChar8 const* g_szAbsoluteVelocityAisacName = "absolute_velocity";
constexpr CriChar8 const* g_szOcclusionAisacName = "occlusion";

extern CriAtomExAisacControlId g_absoluteVelocityAisacId;
extern CriAtomExAisacControlId g_occlusionAisacId;

using CueInstances = std::vector<CCueInstance*>;

struct S3DAttributes final
{
	S3DAttributes() = default;

	CriAtomExVector pos;
	CriAtomExVector fwd;
	CriAtomExVector up;
	CriAtomExVector vel;
};

//////////////////////////////////////////////////////////////////////////
inline void Fill3DAttributeTransformation(CTransformation const& transformation, S3DAttributes& attributes)
{
	attributes.pos.x = static_cast<CriFloat32>(transformation.GetPosition().x);
	attributes.pos.y = static_cast<CriFloat32>(transformation.GetPosition().z);
	attributes.pos.z = static_cast<CriFloat32>(transformation.GetPosition().y);

	attributes.fwd.x = static_cast<CriFloat32>(transformation.GetForward().x);
	attributes.fwd.y = static_cast<CriFloat32>(transformation.GetForward().z);
	attributes.fwd.z = static_cast<CriFloat32>(transformation.GetForward().y);

	attributes.up.x = static_cast<CriFloat32>(transformation.GetUp().x);
	attributes.up.y = static_cast<CriFloat32>(transformation.GetUp().z);
	attributes.up.z = static_cast<CriFloat32>(transformation.GetUp().y);

	attributes.vel.x = 0.0f;
	attributes.vel.y = 0.0f;
	attributes.vel.z = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
inline void Fill3DAttributeVelocity(Vec3 const& velocity, S3DAttributes& attributes)
{
	attributes.vel.x = static_cast<CriFloat32>(velocity.x);
	attributes.vel.y = static_cast<CriFloat32>(velocity.z);
	attributes.vel.z = static_cast<CriFloat32>(velocity.y);
}

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
using GameVariableValues = std::map<CryFixedStringT<MaxControlNameLength>, float>;
extern GameVariableValues g_gameVariableValues;

using CategoryValues = std::map<CryFixedStringT<MaxControlNameLength>, float>;
extern CategoryValues g_categoryValues;

enum class EDebugListFilter : EnumFlagsType
{
	None          = 0,
	CueInstances  = BIT(6), // a
	GameVariables = BIT(7), // b
	Categories    = BIT(8), // c
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EDebugListFilter);

constexpr EDebugListFilter g_debugListMask =
	EDebugListFilter::CueInstances |
	EDebugListFilter::GameVariables |
	EDebugListFilter::Categories;
#endif // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}      // namespace Adx2
}      // namespace Impl
}      // namespace CryAudio
