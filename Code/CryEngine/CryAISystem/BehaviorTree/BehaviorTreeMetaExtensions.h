// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

namespace BehaviorTree
{
class MetaExtensionFactory : public IMetaExtensionFactory
{
public:

	MetaExtensionFactory();

	// IMetaExtensionFactory
	virtual void RegisterMetaExtensionCreator(MetaExtensionCreator metaExtensionCreator) override;
	virtual void CreateMetaExtensions(MetaExtensionTable& metaExtensionTable) override;
	// ~IExtensionFactory

private:

	typedef DynArray<MetaExtensionCreator> MetaExtensionCreatorArray;

	MetaExtensionCreatorArray m_metaExtensionCreators;
};
}
