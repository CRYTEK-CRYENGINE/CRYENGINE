// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2012
// -------------------------------------------------------------------------
//  File name:   AnimUtils.h
//  Created:     9/11/2006 by Michael S.
//  Description: Animation utilities
//
////////////////////////////////////////////////////////////////////////////
struct ICharacterInstance;

namespace AnimUtils
{
void StartAnimation(ICharacterInstance* pCharacter, const char* pAnimName);
void SetAnimationTime(ICharacterInstance* pCharacter, float fNormalizedTime);
void StopAnimations(ICharacterInstance* pCharacter);
}

