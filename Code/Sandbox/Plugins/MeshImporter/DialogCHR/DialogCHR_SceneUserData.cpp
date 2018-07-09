// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "DialogCHR_SceneUserData.h"

#include <CryString/CryStringUtils.h>

#include <unordered_map>

namespace
{

struct Hash
{
	uint32 operator()(const string& str) const
	{
		return CryStringUtils::CalculateHashLowerCase(str.c_str());
	}
};

bool StripProxySuffix(string& str)
{
	static const string suffix("phys");
	const string::size_type pos = str.rfind(suffix);
	const string::size_type prev = pos - 1;
	if (pos != string::npos && pos > 0 && (str[prev] == ' ' || str[prev] == '_'))
	{
		str = str.substr(0, prev);
		return true;
	}
	return false;
}

} // unnamed namespace

void CSceneCHR::AutoAssignProxies(const FbxTool::CScene& scene)
{
	// Try to find proxy nodes based on 3dsMax and Maya naming conventions.
	// Max: Proxy ends with " phys" (note the space)
	// Maya: Proxy ends with "_phys"

	typedef std::pair<const FbxTool::SNode*, bool> Value;

	std::unordered_map<string, Value, Hash> map(scene.GetNodeCount());

	for (int i = 0, N = scene.GetNodeCount(); i < N; ++i)
	{
		const FbxTool::SNode* const pNode = scene.GetNodeByIndex(i);
		string key = pNode->szName;
		const bool bIsProxy = StripProxySuffix(key);

		auto res = map.insert(std::make_pair(key, std::make_pair(pNode, bIsProxy)));
		if (!res.second)
		{
			const Value& val = res.first->second;
			if(val.second == bIsProxy)
			{
				// Both nodes are proxies, or none of them is. In either case, we cannot match.
				continue;  
			}
			const FbxTool::SNode* const pOther = val.first;
			if (bIsProxy)
			{
				SetPhysicsProxyNode(pOther, pNode);
			}
			else
			{
				SetPhysicsProxyNode(pNode, pOther);
			}
			map.erase(res.first);
		}
	}
}

