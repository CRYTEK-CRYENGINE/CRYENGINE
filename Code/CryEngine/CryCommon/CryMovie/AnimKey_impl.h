// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   AnimKey_impl.h
//  Created:     02/4/2014 by Axel.
//
////////////////////////////////////////////////////////////////////////////

#include "AnimKey.h"
#include <CrySerialization/IntrusiveFactory.h>

SERIALIZATION_ENUM_BEGIN_NESTED(SMusicKey, EMusicKeyType, "Music Key Type")
SERIALIZATION_ENUM(SMusicKey::EMusicKeyType::eMusicKeyType_SetMood, "setMood", "Set Mood")
SERIALIZATION_ENUM(SMusicKey::EMusicKeyType::eMusicKeyType_VolumeRamp, "volumeRamp", "Volume Rampe")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(SCommentKey, ETextAlign, "Text Alignment")
SERIALIZATION_ENUM(SCommentKey::ETextAlign::eTA_Left, "left", "Left")
SERIALIZATION_ENUM(SCommentKey::ETextAlign::eTA_Center, "center", "Center")
SERIALIZATION_ENUM(SCommentKey::ETextAlign::eTA_Right, "right", "Right")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(SScreenFaderKey, EFadeType, "Fade Type")
SERIALIZATION_ENUM(SScreenFaderKey::EFadeType::eFT_FadeIn, "fadeIn", "Fade In")
SERIALIZATION_ENUM(SScreenFaderKey::EFadeType::eFT_FadeOut, "fadeOut", "Fade Out")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(SScreenFaderKey, EFadeChangeType, "Fade Change Type")
SERIALIZATION_ENUM(SScreenFaderKey::EFadeChangeType::eFCT_Linear, "linear", "Linear")
SERIALIZATION_ENUM(SScreenFaderKey::EFadeChangeType::eFCT_Square, "square", "Square")
SERIALIZATION_ENUM(SScreenFaderKey::EFadeChangeType::eFCT_CubicSquare, "cubicSquare", "Cubic Square")
SERIALIZATION_ENUM(SScreenFaderKey::EFadeChangeType::eFCT_SquareRoot, "squareRoot", "Square Root")
SERIALIZATION_ENUM(SScreenFaderKey::EFadeChangeType::eFCT_Sin, "sin", "Sinus")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(SCaptureFormatInfo, ECaptureFileFormat, "Capture Format")
SERIALIZATION_ENUM(SCaptureFormatInfo::ECaptureFileFormat::eCaptureFormat_TGA, "tag", "TGA")
SERIALIZATION_ENUM(SCaptureFormatInfo::ECaptureFileFormat::eCaptureFormat_JPEG, "jpg", "JPEG")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(SCaptureFormatInfo, ECaptureBuffer, "Capture Buffer")
SERIALIZATION_ENUM(SCaptureFormatInfo::ECaptureBuffer::eCaptureBuffer_Color, "color", "Color")
SERIALIZATION_ENUM_END()

SERIALIZATION_ANIM_KEY(STrackKey);
SERIALIZATION_ANIM_KEY(STrackDurationKey);
SERIALIZATION_ANIM_KEY(S2DBezierKey);
SERIALIZATION_ANIM_KEY(SEventKey);
SERIALIZATION_ANIM_KEY(STrackEventKey);
SERIALIZATION_ANIM_KEY(SCameraKey);
SERIALIZATION_ANIM_KEY(SFaceSequenceKey);
SERIALIZATION_ANIM_KEY(SSequenceKey);
SERIALIZATION_ANIM_KEY(SAudioTriggerKey);
SERIALIZATION_ANIM_KEY(SAudioFileKey);
SERIALIZATION_ANIM_KEY(SAudioSwitchKey);
SERIALIZATION_ANIM_KEY(SDynamicResponseSignalKey);
SERIALIZATION_ANIM_KEY(STimeRangeKey);
SERIALIZATION_ANIM_KEY(SCharacterKey);
SERIALIZATION_ANIM_KEY(SMannequinKey);
SERIALIZATION_ANIM_KEY(SExprKey);
SERIALIZATION_ANIM_KEY(SConsoleKey);
SERIALIZATION_ANIM_KEY(SLookAtKey);
SERIALIZATION_ANIM_KEY(SDiscreteFloatKey);
SERIALIZATION_ANIM_KEY(SCaptureKey);
SERIALIZATION_ANIM_KEY(SBoolKey);
SERIALIZATION_ANIM_KEY(SCommentKey);
SERIALIZATION_ANIM_KEY(SScreenFaderKey);
