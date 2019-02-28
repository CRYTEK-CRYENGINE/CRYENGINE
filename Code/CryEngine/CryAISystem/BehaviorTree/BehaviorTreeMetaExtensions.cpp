// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "BehaviorTreeMetaExtensions.h"

#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

namespace BehaviorTree
{

MetaExtensionFactory::MetaExtensionFactory()
	: m_metaExtensionCreators()
{
}

void MetaExtensionFactory::RegisterMetaExtensionCreator(MetaExtensionCreator metaExtensionCreator)
{
	m_metaExtensionCreators.push_back(metaExtensionCreator);
}

void MetaExtensionFactory::CreateMetaExtensions(MetaExtensionTable& metaExtensionTable)
{
	for (MetaExtensionCreator& metaExtensionCreator : m_metaExtensionCreators)
	{
		metaExtensionTable.AddMetaExtension(metaExtensionCreator());
	}
}

}
