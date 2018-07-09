// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include <CrySerialization/Forward.h>

class QString;
class QByteArray;
class QColor;

//Wrappers for convenience to use Qt classes directly with serialization
bool EDITOR_COMMON_API Serialize(Serialization::IArchive& ar, QString& value, const char* name, const char* label);
bool EDITOR_COMMON_API Serialize(Serialization::IArchive& ar, QByteArray& byteArray, const char* name, const char* label);
bool EDITOR_COMMON_API Serialize(Serialization::IArchive& ar, QColor& color, const char* name, const char* label);
