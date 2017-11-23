// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Schematyc_Prerequisites.h"
#include "CrySchematyc2/Script/Schematyc_IScriptGraph.h"

#include "Bridge_IScriptFile.h"

namespace Schematyc2 {

namespace LibUtils {

inline void FindAndLogReferences(SGUID refGuid, SGUID refGoBack, const char* szItemName, const Bridge::IScriptFile* pFile)
{
	SCHEMATYC2_COMMENT(Schematyc2::LOG_STREAM_DEFAULT, "Searching references of %s", szItemName);
	int refFound = 0;

	auto visitGraph = [refGuid, refGoBack, szItemName, &refFound](const IDocGraph& graph) -> EVisitStatus
	{
		auto visitNode = [refGuid, refGoBack, &graph, szItemName, &refFound](const IScriptGraphNode& node) -> EVisitStatus
		{
			bool bFoundInOutput(false);
			for (size_t i = 0, count = node.GetOutputCount(); i < count; ++i)
			{
				CAggregateTypeId typeId = node.GetOutputTypeId(i);
				if (typeId.AsScriptTypeGUID() == refGuid)
				{
					bFoundInOutput = true;
					break;
				}
			}

			std::stack<string> path;
			if (node.GetRefGUID() == refGuid || graph.GetContextGUID() == refGuid || bFoundInOutput)
			{
				++refFound;

				stack_string graphPath;
				graphPath.append("<font color = \"yellow\">");
				const IScriptElement *parent = graph.GetParent();
				while (parent)
				{
					path.push(parent->GetName());
					parent = parent->GetParent();
				}

				while (!path.empty())
				{
					graphPath.append(path.top().c_str());
					graphPath.append("<font color = \"black\">");
					graphPath.append("//");
					graphPath.append("<font color = \"yellow\">");
					path.pop();
				}

				graphPath.append(graph.GetName());

				SLogMetaItemGUID gotoItem(graph.GetType() == EScriptGraphType::Transition ? graph.GetScopeGUID() : graph.GetGUID());
				SLogMetaChildGUID gobackItem(refGoBack);

				SCHEMATYC2_METAINFO_COMMENT(Schematyc2::LOG_STREAM_DEFAULT, CLogMessageMetaInfo(ECryLinkCommand::Goto, gotoItem, gobackItem), "Reference of <font color = \"yellow\">%s <font color = \"lightgray\">found in %s", szItemName, graphPath.c_str());
			}

			return EVisitStatus::Continue;
		};

		graph.VisitNodes(ScriptGraphNodeConstVisitor::FromLambdaFunction(visitNode));
		return EVisitStatus::Continue;
	};

	pFile->VisitGraphs(DocGraphConstVisitor::FromLambdaFunction(visitGraph), SGUID(), true);
	SCHEMATYC2_COMMENT(Schematyc2::LOG_STREAM_DEFAULT, "References found: %d", refFound);
}

}

}