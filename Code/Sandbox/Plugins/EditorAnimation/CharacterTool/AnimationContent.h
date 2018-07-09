// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>
#include "AnimEvent.h"
#include "Shared/AnimSettings.h"
#include "BlendSpace.h"

struct IDefaultSkeleton;
struct IAnimationSet;

namespace CharacterTool {

struct AnimationContent
{
	enum Type
	{
		ANIMATION,
		BLEND_SPACE,
		COMBINED_BLEND_SPACE,
		AIMPOSE,
		LOOKPOSE,
		ANM
	};

	enum EImportState
	{
		NOT_SET,
		NEW_ANIMATION,
		WAITING_FOR_CHRPARAMS_RELOAD,
		IMPORTED,
		COMPILED_BUT_NO_ANIMSETTINGS
	};

	Type                   type;

	EImportState           importState;
	int                    size;
	bool                   loadedInEngine;
	bool                   loadedAsAdditive;
	bool                   delayApplyUntilStart;
	int                    animationId;
	SAnimSettings          settings;
	BlendSpace             blendSpace;
	CombinedBlendSpace     combinedBlendSpace;
	string                 newAnimationSkeleton;
	std::vector<AnimEvent> events;

	AnimationContent();

	void ApplyToCharacter(bool* triggerPreview, ICharacterInstance* characterInstance, const char* animationPath, bool animationStarting);
	void ApplyAfterStart(ICharacterInstance* characterInstance, const char* animationPath);
	void UpdateBlendSpaceMotionParameters(IAnimationSet* animationSet, IDefaultSkeleton* defaultSkeleton);

	void Reset();
	void Serialize(Serialization::IArchive& ar);

	bool HasAudioEvents() const;
};

}

