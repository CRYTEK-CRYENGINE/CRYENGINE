// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>

#include <CrySerialization/Forward.h>

struct SkeletonAlias;

bool Serialize(Serialization::IArchive& ar, SkeletonAlias& value, const char* name, const char* label);

#include <CrySerialization/STL.h>
#include <CrySerialization/Decorators/Range.h>
#include <CrySerialization/Decorators/OutputFilePath.h>
using Serialization::OutputFilePath;
#include <CrySerialization/Decorators/ResourceFilePath.h>
using Serialization::ResourceFilePath;
#include <CrySerialization/Decorators/JointName.h>
using Serialization::JointName;

#include <CrySerialization/IArchive.h>
#include <CrySerialization/SmartPtr.h>
#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/Enum.h>
using Serialization::IArchive;

