// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

#include "Deprecated/DocGraphBase.h"

namespace Schematyc2
{
	class CDocTransitionGraph : public CDocGraphBase
	{
	public:

		CDocTransitionGraph(IScriptFile& file, const SGUID& guid = SGUID(), const SGUID& scopeGUID = SGUID(), const char* szName = nullptr, EScriptGraphType type = EScriptGraphType::Unknown, const SGUID& contextGUID = SGUID());

		// IScriptElement
		virtual EAccessor GetAccessor() const override;
		// ~IScriptElement

		// IDocGraph
		virtual void RefreshAvailableNodes(const CAggregateTypeId& inputTypeId) override;
		// ~IDocGraph

	protected:

		// CDocGraphBase
		virtual IScriptGraphNodePtr CreateNode(const SGUID& guid, EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos) override;
		// ~CDocGraphBase

	private:

		void VisitEnvGlobalFunctions(const IDomainContext& domainContext);
		void VisitEnvComponentMemberFunctions(const IDomainContext& domainContext);
		void VisitEnvActionMemberFunctions(const IDomainContext& domainContext);
		void VisitVariables(const IDomainContext& domainContext);
		void VisitProperties(const IDomainContext& domainContext);
		void VisitGraphs(const IDomainContext& domainContext);
	};
}
