// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/GraphNodes/ScriptGraphNodeFactory.h"

#include "Script/GraphNodes/ScriptGraphGetNode.h" // #SchematycTODO : Obviously this factory shouldn't know anything about the nodes it creates!!!

namespace Schematyc2
{
	bool CScriptGraphNodeFactory::RegisterCreator(const IScriptGraphNodeCreatorPtr& pCreator)
	{
		if(!pCreator)
		{
			return false;
		}
		const SGUID& typeGUID = pCreator->GetTypeGUID();
		if(GetCreator(typeGUID))
		{
			return false;
		}
		m_creators.insert(Creators::value_type(typeGUID, pCreator));
		return true;
	}

	IScriptGraphNodeCreator* CScriptGraphNodeFactory::GetCreator(const SGUID& typeGUID)
	{
		Creators::iterator itCreator = m_creators.find(typeGUID);
		return itCreator != m_creators.end() ? itCreator->second.get() : nullptr;
	}

	IScriptGraphNodePtr CScriptGraphNodeFactory::CreateNode(const SGUID& typeGUID)
	{
		IScriptGraphNodeCreator* pCreator = GetCreator(typeGUID);
		return pCreator ? pCreator->CreateNode() : IScriptGraphNodePtr();
	}

	void CScriptGraphNodeFactory::PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IDomainContext& domainContext, const IScriptGraphExtension& graph)
	{
		for(Creators::value_type& creator : m_creators)
		{
			creator.second->PopulateNodeCreationMenu(nodeCreationMenu, domainContext, graph);
		}
	}
}
