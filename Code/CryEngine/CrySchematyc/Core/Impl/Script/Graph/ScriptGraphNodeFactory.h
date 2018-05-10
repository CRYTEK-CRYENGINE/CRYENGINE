// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/IScriptGraph.h>

namespace Schematyc
{
struct IScriptView;
struct IScriptGraphNodeCreationMenu;

struct IScriptGraphNodeCreator
{
	virtual ~IScriptGraphNodeCreator() {}

	virtual CryGUID             GetTypeGUID() const = 0;
	virtual IScriptGraphNodePtr CreateNode(const CryGUID& guid) = 0;
	virtual void                PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph) = 0;
};

DECLARE_SHARED_POINTERS(IScriptGraphNodeCreator)

class CScriptGraphNodeFactory
{
public:

	bool                     RegisterCreator(const IScriptGraphNodeCreatorPtr& pCreator);
	bool                     CanCreateNode(const CryGUID& typeGUID, const IScriptView& scriptView, const IScriptGraph& graph) const;
	IScriptGraphNodeCreator* GetCreator(const CryGUID& typeGUID);
	IScriptGraphNodePtr      CreateNode(const CryGUID& typeGUID, const CryGUID& guid = CryGUID());
	void                     PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph);

private:

	typedef std::unordered_map<CryGUID, IScriptGraphNodeCreatorPtr> Creators;

	Creators m_creators;
};
}
