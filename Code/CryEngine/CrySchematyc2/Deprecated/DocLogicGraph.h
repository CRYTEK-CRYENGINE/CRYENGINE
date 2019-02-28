// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

#include "Deprecated/DocGraphBase.h"

namespace Schematyc2
{
	class CDocLogicGraph : public CDocGraphBase
	{
	public:

		CDocLogicGraph(IScriptFile& file, const SGUID& guid = SGUID(), const SGUID& scopeGUID = SGUID(), const char* szName = nullptr, EScriptGraphType type = EScriptGraphType::Unknown, const SGUID& contextGUID = SGUID());

		// IScriptElement
		virtual EAccessor GetAccessor() const override;
		virtual void EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		// ~IScriptElement

		// IDocGraph
		virtual void RefreshAvailableNodes(const CAggregateTypeId& inputTypeId) override;
		// ~IDocGraph

	protected:

		// CDocGraphBase
		virtual IScriptGraphNodePtr CreateNode(const SGUID& guid, EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos) override;
		// ~CDocGraphBase

	private:

		struct SInfoSerializer
		{
			SInfoSerializer(CDocLogicGraph& _graph);

			void Serialize(Serialization::IArchive& archive);

			CDocLogicGraph& graph;
		};

		bool RefreshInputsAndOutputs();
		void Validate(Serialization::IArchive& archive);
		void VisitEnvGlobalFunctions(const IDomainContext& domainContext);
		void VisitEnvAbstractInterfaces(const IDomainContext& domainContext);
		void VisitEnvComponentMemberFunctions(const IDomainContext& domainContext);
		void VisitEnvActionMemberFunctions(const IDomainContext& domainContext);
		void VisitVariables(const IDomainContext& domainContext);
		void VisitProperties(const IDomainContext& domainContext);
		void VisitGraphs(const IDomainContext& domainContext);

		EAccessor m_accessor;
	};
}
