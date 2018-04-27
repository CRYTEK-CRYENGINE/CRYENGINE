// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CharacterTool
{

struct SAnimationReference
{
	SAnimationReference()
		: pathCRC(0)
		, animationName()
		, bIsRegularCaf(false)
	{}

	SAnimationReference(unsigned int pathCRC, const char* animationName, bool bIsCaf)
		: pathCRC(0)
	{
		reset(pathCRC, animationName, bIsCaf);
	}

	SAnimationReference(const SAnimationReference& rhs)
		: pathCRC(0)
	{
		reset(rhs.pathCRC, rhs.animationName.c_str(), rhs.bIsRegularCaf);
	}

	SAnimationReference& operator=(const SAnimationReference& rhs)
	{
		reset(rhs.pathCRC, rhs.animationName.c_str(), rhs.bIsRegularCaf);
		return *this;
	}

	~SAnimationReference()
	{
		reset(0, nullptr, false);
	}

	void         reset(unsigned int crc, const char* animationName, bool bIsRegularCaf);

	unsigned int PathCRC() const       { return pathCRC; }

	const char*  AnimationName() const { return animationName.c_str(); }

private:

	unsigned int pathCRC;
	string       animationName;
	bool         bIsRegularCaf;
};

}

