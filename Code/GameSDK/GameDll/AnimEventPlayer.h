// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/IAnimEventPlayer.h>
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/CryCreateClassInstance.h>

// Automatically found by the character tool due to implementing IAnimEventPlayerGame and usign CRYREGISTER_CLASS
class AnimEventPlayer_CRYENGINE_SDK : public IAnimEventPlayerGame
{
public:
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(IAnimEventPlayer)
		CRYINTERFACE_ADD(IAnimEventPlayerGame)
	CRYINTERFACE_END()
	CRYGENERATE_CLASS_GUID(AnimEventPlayer_CRYENGINE_SDK, "AnimEventPlayer_CRYENGINE_SDK", "7cb24140-2ca9-e311-058e-7ce6b77865e4"_cry_guid)

	virtual ~AnimEventPlayer_CRYENGINE_SDK() {}

	const SCustomAnimEventType* GetCustomType(int customTypeIndex) const override;
	int GetCustomTypeCount() const override;
	bool Play(ICharacterInstance* character, const AnimEventInstance& event) override;
};
