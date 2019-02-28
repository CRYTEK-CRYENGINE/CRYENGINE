// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <vector>

struct ICharacterInstance;
#include <CrySerialization/Forward.h>

namespace CharacterTool
{

using std::vector;
struct SCharacterRigDummyComponent
{
	string name;

	void   Serialize(Serialization::IArchive& ar);
};

struct SCharacterRigContent
{
	vector<SCharacterRigDummyComponent> m_components;

	void Serialize(Serialization::IArchive& ar);
	void ApplyToCharacter(ICharacterInstance* instance);

	void Reset() { *this = SCharacterRigContent(); }
};

}

