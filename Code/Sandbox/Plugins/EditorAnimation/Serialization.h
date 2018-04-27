// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../EditorCommon/Serialization.h"

#include <CrySerialization/Forward.h>

struct SkeletonAlias;
bool Serialize(Serialization::IArchive& ar, SkeletonAlias& value, const char* name, const char* label);

#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Decorators/ResourceFilePath.h>
#include <CrySerialization/Decorators/ResourceFolderPath.h>
#include <CrySerialization/Decorators/JointName.h>
#include <CrySerialization/Decorators/TagList.h>
#include <Serialization/Decorators/INavigationProvider.h>
#include <CrySerialization/Decorators/LocalFrame.h>

#include "Serialization/Qt.h"

using Serialization::AttachmentName;
using Serialization::ResourceFilePath;
using Serialization::MaterialPicker;
using Serialization::SkeletonOrCgaPath;
using Serialization::SkeletonParamsPath;
using Serialization::AnimationAlias;
using Serialization::AnimationPath;
using Serialization::AnimationOrBlendSpacePath;
using Serialization::AnimationPathWithId;
using Serialization::CharacterPath;
using Serialization::CharacterPhysicsPath;
using Serialization::CharacterRigPath;
using Serialization::ResourceFolderPath;
using Serialization::JointName;
using Serialization::BitFlags;
using Serialization::LocalToJoint;
using Serialization::ToggleButton;
using Serialization::SerializeToMemory;
using Serialization::SerializeToMemory;
using Serialization::SerializeFromMemory;
using Serialization::SerializeFromMemory;

