// Copyright 2001-2015 Crytek GmbH. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"

#include <functional>

struct IEditor;
struct ISystem;

class QWidget;
class QString;

namespace EditorCommon
{

//! Preferably be called after IEditor is fully initialized
void EDITOR_COMMON_API                        SetIEditor(IEditor* editor);
void EDITOR_COMMON_API                        Initialize();
void EDITOR_COMMON_API                        Deinitialize();
}

