// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "ResourceSelector.h"
#include "ResourceFilePath.h"

#include <../CryAction/ICryMannequin.h>

namespace Serialization
{
//Do not add any more methods here, this is not a scalable way to call upon resource selectors. Use MakeResourceSelector() instead.
template<class T>
ResourceSelector<T> MakeResourceSelector(T& s, const char* selectorType)
{
	return ResourceSelector<T>(s, selectorType);
}

// animation resources
template<class T> ResourceSelector<T> AnimationAlias(T& s)                   { return ResourceSelector<T>(s, "AnimationAlias"); } // "name" from animation set
template<class T> ResourceSelector<T> AnimationPath(T& s)                    { return ResourceSelector<T>(s, "Animation"); }
template<class T> ResourceSelector<T> AnimationOrBlendSpacePath(T& s)        { return ResourceSelector<T>(s, "AnimationOrBSpace"); }
inline ResourceSelectorWithId         AnimationPathWithId(string& s, int id) { return ResourceSelectorWithId(s, "AnimationOrBSpace", id); }
template<class T> ResourceSelector<T> CharacterPath(T& s)                    { return ResourceSelector<T>(s, "Character"); }
template<class T> ResourceSelector<T> CharacterPhysicsPath(T& s)             { return ResourceSelector<T>(s, "CharacterPhysics"); }
template<class T> ResourceSelector<T> CharacterRigPath(T& s)                 { return ResourceSelector<T>(s, "CharacterRig"); }
template<class T> ResourceSelector<T> SkeletonPath(T& s)                     { return ResourceSelector<T>(s, "Skeleton"); }
template<class T> ResourceSelector<T> SkeletonOrCgaPath(T& s)                { return ResourceSelector<T>(s, "SkeletonOrCga"); } //Used by the character tool
template<class T> ResourceSelector<T> SkeletonParamsPath(T& s)               { return ResourceSelector<T>(s, "SkeletonParams"); } // CHRParams
template<class T> ResourceSelector<T> JointName(T& s)                        { return ResourceSelector<T>(s, "Joint"); }
template<class T> ResourceSelector<T> AttachmentName(T& s)                   { return ResourceSelector<T>(s, "Attachment"); }
template<class T> ResourceSelector<T> SkinnedMeshFilename(T& s)              { return ResourceSelector<T>(s, "SkinnedMesh"); }

// miscellaneous resources
template<class T> ResourceSelector<T> DialogName(T& s)                    { return ResourceSelector<T>(s, "Dialog"); }
template<class T> ResourceSelector<T> ForceFeedbackIdName(T& s)           { return ResourceSelector<T>(s, "ForceFeedbackId"); }
template<class T> ResourceSelector<T> ModelFilename(T& s)                 { return ResourceSelector<T>(s, "Model"); }
template<class T> ResourceSelector<T> StaticModelFilename(T& s)           { return ResourceSelector<T>(s, "Mesh"); }
template<class T> ResourceSelector<T> GeomCachePicker(T& s)               { return ResourceSelector<T>(s, "GeometryCache"); }
template<class T> ResourceSelector<T> ParticlePicker(T& s)                { return ResourceSelector<T>(s, "Particle"); }
template<class T> ResourceSelector<T> ParticlePickerLegacy(T& s)          { return ResourceSelector<T>(s, "ParticleLegacy"); }
template<class T> ResourceSelector<T> TextureFilename(T& s)               { return ResourceSelector<T>(s, "Texture"); }
template<class T> ResourceSelector<T> GeneralFilename(T& s)               { return ResourceSelector<T>(s, "AnyFile"); }
template<class T> ResourceSelector<T> SoundFilename(T& s)                 { return ResourceSelector<T>(s, "Sound"); }
template<class T> ResourceSelector<T> SmartObjectClasses(T& s)            { return ResourceSelector<T>(s, "SmartObjectClasses"); }
template<class T> ResourceSelector<T> MissionPicker(T& s)                 { return ResourceSelector<T>(s, "Missions"); }
template<class T> ResourceSelector<T> MaterialPicker(T& s)                { return ResourceSelector<T>(s, "Material"); }
template<class T> ResourceSelector<T> LevelLayerPicker(T& s)              { return ResourceSelector<T>(s, "LevelLayer"); }
template<class T> ResourceSelector<T> SequenceEventPicker(T& s)           { return ResourceSelector<T>(s, "SequenceEvent"); }
template<class T> ResourceSelector<T> EntityEventPicker(T& s)             { return ResourceSelector<T>(s, "EntityEvent"); }
template<class T> ResourceSelector<T> SequenceCameraPicker(T& s)          { return ResourceSelector<T>(s, "SequenceCamera"); }
template<class T> ResourceSelector<T> CharacterAnimationPicker(T& s)      { return ResourceSelector<T>(s, "CharacterAnimation"); }
template<class T> ResourceSelector<T> TrackCharacterAnimationPicker(T& s) { return ResourceSelector<T>(s, "TrackCharacterAnimation"); }
template<class T> ResourceSelector<T> ActionMapName(T& s)                 { return ResourceSelector<T>(s, "ActionMapName"); }
template<class T> ResourceSelector<T> ActionMapActionName(T& s)           { return ResourceSelector<T>(s, "ActionMapActionName"); }
template<class T> ResourceSelector<T> SurfaceTypeName(T& s)               { return ResourceSelector<T>(s, "SurfaceTypeName"); }
template<class T> ResourceSelector<T> EntityClassName(T& s)               { return ResourceSelector<T>(s, "EntityClassName"); }
// WORKAROUND: Can't register the same ResourceSelector (EntityClassName) for both Schematyc versions.
// Remove this in favor of EntityClassName when we finally have just one Schematyc.
template<class T> ResourceSelector<T> EntityClass(T& s)                  { return ResourceSelector<T>(s, "EntityClass"); }
// ~WORKAROUND

inline Serialization::ResourceFilePath ObjectIconPath(string& value)                    { return Serialization::ResourceFilePath(value, "Bitmap (bmp)|*.bmp"); }
inline Serialization::ResourceFilePath MannequinAnimationDatabasePath(string& value)    { return Serialization::ResourceFilePath(value, "Animation Database (adb)|*.adb"); }
inline Serialization::ResourceFilePath MannequinControllerDefinitionPath(string& value) { return Serialization::ResourceFilePath(value, "Controller Definition (xml)|*.xml"); }

struct SMannequinControllerDefResourceParams : public ICustomResourceParams
{
	const SControllerDef* pControllerDef = nullptr;
};

template<class T> ResourceSelector<T> MannequinScopeContextName(T& s, std::shared_ptr<SMannequinControllerDefResourceParams> pParams) { return ResourceSelector<T>(s, "MannequinScopeContextName", pParams); }
template<class T> ResourceSelector<T> MannequinFragmentName(T& s, std::shared_ptr<SMannequinControllerDefResourceParams> pParams)     { return ResourceSelector<T>(s, "MannequinFragmentName", pParams); }

//! Decorators namespace is obsolete now, SHOULD NOT BE USED.
namespace Decorators
{
template<class T> ResourceSelector<T> AnimationName(T& s)  { return ResourceSelector<T>(s, "Animation"); }
using Serialization::AttachmentName;
template<class T> ResourceSelector<T> ObjectFilename(T& s) { return ResourceSelector<T>(s, "Model"); }
using Serialization::JointName;
using Serialization::ForceFeedbackIdName;
}
}
