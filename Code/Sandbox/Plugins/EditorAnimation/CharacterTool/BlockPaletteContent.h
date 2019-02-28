// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include "Serialization.h"

struct BlockPaletteItem
{
	string name;
	ColorB color;
	union
	{
		long long userId;
		void*     userPointer;
	};
	std::vector<char> userPayload;

	BlockPaletteItem()
		: color(255, 255, 255, 255)
		, userId(0)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(name, "name", "Name");
		ar(color, "color", "Color");
		ar(userId, "userId", "UserID");
	}
};
typedef std::vector<BlockPaletteItem> BlockPaletteItems;

struct BlockPaletteContent
{
	BlockPaletteItems items;

	int               lineHeight;
	int               padding;

	BlockPaletteContent()
		: lineHeight(24)
		, padding(2)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(lineHeight, "lineHeight", "Line Height");
		ar(padding, "padding", "Padding");
		ar(items, "items", "Items");
	}
};

