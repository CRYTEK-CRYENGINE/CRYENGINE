// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/BaseTypes.h>  // Fixed-size integer types.
#include <CryMath/Cry_Math.h>

struct ICharacterInstance;

class QViewport;

extern const int32 kInvalidJointId;

//! PickJoint picks joint under mouse cursor.
//! \param x, y Screen coordinates of mouse cursor.
//! \return Returns ID of joint under mouse cursor, or -1, if there is no such joint.
int32 PickJoint(const ICharacterInstance& charIns, QViewport& viewPort, float x, float y);

//! For each joint, draw a wireframe box and axes system.
void DrawJoints(const ICharacterInstance& charIns);

void DrawHighlightedJoints(const ICharacterInstance& charIns, std::vector<float>& heat, const Vec3& eye);
