// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

namespace BehaviorTree
{
class XmlLoader
{
public:
	XmlNodeRef LoadBehaviorTreeXmlFile(const char* path, const char* name)
	{
		stack_string file;
		file.Format("%s%s.xml", path, name);
		const bool fileExists = gEnv->pCryPak->IsFileExist(file, ICryPak::eFileLocation_Any);
		if (fileExists)
		{
			return GetISystem()->LoadXmlFromFile(file);
		}

		return XmlNodeRef();
	}

	// isLoadingFromEditor = true -> Tree will be loaded even if the result of the operation is LoadFailure. This is used in the Behavior Tree Editor, since it's convinient to still load the tree with errors so we can fix them without having to manually edit the XML.
	// isLoadingFromEditor = false ->  Tree will only be loaded if the result of the operation is LoadSuccess. This is used when loading behavior trees in Runtime. It has to be strict to prevent running an invalid tree.
	INodePtr CreateBehaviorTreeRootNodeFromBehaviorTreeXml(const XmlNodeRef& behaviorTreeXmlNode, const LoadContext& context, const bool isLoadingFromEditor) const
	{
		XmlNodeRef rootXmlNode = behaviorTreeXmlNode->findChild("Root");
		IF_UNLIKELY (!rootXmlNode)
		{
			gEnv->pLog->LogError("Failed to load behavior tree '%s'. The 'Root' node is missing.", context.treeName);
			return INodePtr();
		}

		return CreateBehaviorTreeNodeFromXml(rootXmlNode, context, isLoadingFromEditor);
	}

	INodePtr CreateBehaviorTreeNodeFromXml(const XmlNodeRef& xmlNode, const LoadContext& context, const bool isLoadingFromEditor) const
	{
		if (xmlNode->getChildCount() != 1)
		{
			gEnv->pLog->LogWarning("Failed to load behavior tree '%s'. The node '%s' must contain exactly one child node.", context.treeName, xmlNode->getTag());
			return INodePtr();
		}

		return context.nodeFactory.CreateNodeFromXml(xmlNode->getChild(0), context, isLoadingFromEditor);
	}

};
}
