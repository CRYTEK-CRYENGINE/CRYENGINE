// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Utilities for FbxTool.
#pragma once

#include "StdAfx.h"
#include "FbxUtil.h"
#include <algorithm>

namespace FbxTool
{
static const char* const kDefaultTextureName = "<anonymous>";

const char* const* GetSupportedFileExtensions(TIndex& numExtensions)
{
	static const char* ppExtensions[] =
	{
		"fbx",
		"dxf",
		"dae",
		"obj",
		"3ds"
	};
	numExtensions = (TIndex)(sizeof(ppExtensions) / sizeof(*ppExtensions));
	return ppExtensions;
}

string GetNodePath(FbxNode* pNode, const char* szLastName)
{
	static const char kSeparator = '/';
	const char szSeparator[] = { kSeparator, 0 };
	const char szRootName[] = "<root>";

	string result;
	if (pNode != nullptr && !pNode->GetParent())
	{
		result = szRootName;
	}
	while (pNode != nullptr && pNode->GetParent() != nullptr)
	{
		string nodeDescription;
		const char* const szName = pNode->GetName();
		if (szName && szName[0])
		{
			nodeDescription = szName;
		}
		else
		{
			int nodeIndex = -1;
			const FbxNode* const pParent = pNode->GetParent();
			if (pParent)
			{
				const int numChildren = pParent->GetChildCount();
				for (int i = 0; i < numChildren; ++i)
				{
					if (pParent->GetChild(i) == pNode)
					{
						nodeIndex = i;
						break;
					}
				}
			}
			if (nodeIndex >= 0)
			{
				char szDescription[100];
				std::sprintf(szDescription, "<node at index %d>", nodeIndex);
				nodeDescription = szDescription;
			}
			else
			{
				nodeDescription = "<anonymous node>";
			}
		}
		if (result.empty())
		{
			result.swap(nodeDescription);
		}
		else
		{
			result = nodeDescription + szSeparator + result;
		}
		pNode = pNode->GetParent();
	}
	if (result.empty())
	{
		result = "<null>";
	}
	if (szLastName && szLastName[0])
	{
		result = result + szSeparator + szLastName;
	}
	return result;
}
}

