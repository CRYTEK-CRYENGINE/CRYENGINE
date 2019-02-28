// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <vector>

struct ICharacterInstance;
#include <CrySerialization/Forward.h>

namespace CharacterTool
{
using std::vector;

struct SPhysicsComponent
{
	bool   enabled;
	string name;

	void   Serialize(Serialization::IArchive& ar);
};

struct SCharacterPhysicsContent
{
	vector<SPhysicsComponent> m_components;

	void Serialize(Serialization::IArchive& ar);

	void ApplyToCharacter(ICharacterInstance* instance);

	void Reset() { *this = SCharacterPhysicsContent(); }
};

}

