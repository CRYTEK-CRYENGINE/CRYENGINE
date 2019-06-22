// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Texture.h"

namespace TextureHelpers
{
//////////////////////////////////////////////////////////////////////////
EEfResTextures FindTexSlot(const char* texSemantic);
bool           VerifyTexSuffix(EEfResTextures texSlot, const char* texPath);
bool           VerifyTexSuffix(EEfResTextures texSlot, const string& texPath);
const char*    LookupTexSuffix(EEfResTextures texSlot);
int8           LookupTexPriority(EEfResTextures texSlot);
CTexture*      LookupTexDefault(EEfResTextures texSlot);
CTexture*      LookupTexNeutral(EEfResTextures texSlot);
bool           IsSlotAvailable(EEfResTextures texSlot);
EShaderStage   GetShaderStagesForTexSlot(EEfResTextures texSlot);
}
