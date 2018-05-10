// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Bridge_LibUtils.h"

#include "CrySchematyc2/LibUtils.h"

namespace Schematyc2 {

namespace LibUtils {

void FindReferences(SGUID refGuid, SGUID refGoBack, const char* szItemName, const TScriptFile* pFile, SReferencesLogInfo& result)
{
	result.header.Format("Searching references of %s in %s", szItemName, pFile->GetFileName());

	auto visitAbstractInterfaces = [refGuid, refGoBack, szItemName, &result](const IScriptAbstractInterfaceImplementation& abstractInterface)->EVisitStatus
	{
		std::stack<string> path;
		if (abstractInterface.GetRefGUID() == refGuid)
		{
			stack_string graphPath;
			graphPath.append("<font color = \"yellow\">");
			const IScriptElement *pParent = abstractInterface.GetParent();
			while (pParent)
			{
				path.push(pParent->GetName());
				pParent = pParent->GetParent();
			}

			while (!path.empty())
			{
				graphPath.append(path.top().c_str());
				graphPath.append("<font color = \"black\">");
				graphPath.append("//");
				graphPath.append("<font color = \"yellow\">");
				path.pop();
			}

			graphPath.append(abstractInterface.GetName());

			SLogMetaItemGUID gotoItem(abstractInterface.GetGUID());
			SLogMetaChildGUID gobackItem(refGoBack);

			stack_string pathLink;
			pathLink.Format("Reference of <font color = \"yellow\">%s <font color = \"lightgray\">found in %s", szItemName, graphPath.c_str());

			result.references.emplace_back(gotoItem, gobackItem, pathLink.c_str());
		}

		return EVisitStatus::Continue;
	};

	auto visitGraph = [refGuid, refGoBack, szItemName, &result](const IDocGraph& graph) -> EVisitStatus
	{
		auto visitNode = [refGuid, refGoBack, &graph, szItemName, &result](const IScriptGraphNode& node) -> EVisitStatus
		{
			bool foundInOutput(false);
			for (size_t i = 0, count = node.GetOutputCount(); i < count; ++i)
			{
				CAggregateTypeId typeId = node.GetOutputTypeId(i);
				if (typeId.AsScriptTypeGUID() == refGuid)
				{
					foundInOutput = true;
					break;
				}
			}

			std::stack<string> path;
			if (node.GetRefGUID() == refGuid || graph.GetContextGUID() == refGuid || foundInOutput)
			{
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

				stack_string pathLink;
				pathLink.Format("Reference of <font color = \"yellow\">%s <font color = \"lightgray\">found in %s", szItemName, graphPath.c_str());

				result.references.emplace_back(gotoItem, gobackItem, pathLink.c_str());
			}

			return EVisitStatus::Continue;
		};

		graph.VisitNodes(ScriptGraphNodeConstVisitor::FromLambdaFunction(visitNode));
		return EVisitStatus::Continue;
	};

	auto visitActions = [refGuid, refGoBack, szItemName, &result](const IScriptActionInstance& action)->EVisitStatus
	{
		std::stack<string> path;
		if (action.GetActionGUID() == refGuid)
		{
			stack_string graphPath;
			graphPath.append("<font color = \"yellow\">");
			const IScriptElement *parent = action.GetParent();
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

			graphPath.append(action.GetName());

			SLogMetaItemGUID gotoItem(action.GetGUID());
			SLogMetaChildGUID gobackItem(refGoBack);

			stack_string pathLink;
			pathLink.Format("Reference of <font color = \"yellow\">%s <font color = \"lightgray\">found in %s", szItemName, graphPath.c_str());

			result.references.emplace_back(gotoItem, gobackItem, pathLink.c_str());
		}

		return EVisitStatus::Continue;
	};

	auto visitComponentInstances = [refGuid, refGoBack, szItemName, &result](const IScriptComponentInstance& componentInstance) -> EVisitStatus
	{
		if (const IScriptGraphExtension* pGraph = static_cast<const IScriptGraphExtension*>(componentInstance.GetExtensions().QueryExtension(Graph)))
		{
			const IScriptElement& graph = pGraph->GetElement_New();

			auto visitNode = [refGuid, refGoBack, &graph, szItemName, &result, &componentInstance](const IScriptGraphNode& node) -> EVisitStatus
			{
				std::stack<string> path;
				if (node.GetRefGUID() == refGuid)
				{
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

					SLogMetaItemGUID gotoItem(componentInstance.GetGUID());
					SLogMetaChildGUID gobackItem(refGoBack);

					stack_string pathLink;
					pathLink.Format("Reference of <font color = \"yellow\">%s <font color = \"lightgray\">found in %s", szItemName, graphPath.c_str());

					result.references.emplace_back(gotoItem, gobackItem, pathLink.c_str());
				}

				return EVisitStatus::Continue;
			};

			pGraph->VisitNodes(ScriptGraphNodeConstVisitor::FromLambdaFunction(visitNode));
		}

		return EVisitStatus::Continue;
	};

	pFile->VisitGraphs(DocGraphConstVisitor::FromLambdaFunction(visitGraph), SGUID(), true);
	pFile->VisitComponentInstances(ScriptComponentInstanceConstVisitor::FromLambdaFunction(visitComponentInstances), SGUID(), true);
	pFile->VisitActionInstances(ScriptActionInstanceConstVisitor::FromLambdaFunction(visitActions), SGUID(), true);
	pFile->VisitAbstractInterfaceImplementations(ScriptAbstractInterfaceImplementationConstVisitor::FromLambdaFunction(visitAbstractInterfaces), SGUID(), true);

	result.footer.Format("References found: %d", result.GetReferenceCount());
}

void FindAndLogReferences(SGUID refGuid, SGUID refGoBack, const char* szItemName, const TScriptFile* pFile, bool searchInAllFiles)
{
	// Check if the refGuid is actually a action and assign the proper search guid
	if (const IScriptActionInstance* pAction = pFile->GetActionInstance(refGuid))
	{
		refGuid = pAction->GetActionGUID();
	}

	if (searchInAllFiles)
	{
		auto visitFiles = [refGuid, refGoBack, szItemName](const TScriptFile& file)->EVisitStatus
		{
			SReferencesLogInfo refInfo;
			FindReferences(refGuid, refGoBack, szItemName, &file, refInfo);
			if (!refInfo.IsEmpty())
			{
				PrintReferencesToSchematycConsole(refInfo);
			}
			return EVisitStatus::Continue;
		};

		GetSchematyc()->GetScriptRegistry().VisitFiles(TScriptFileConstVisitor::FromLambdaFunction(visitFiles));
	}
	else
	{
		SReferencesLogInfo refInfo;
		FindReferences(refGuid, refGoBack, szItemName, pFile, refInfo);
		PrintReferencesToSchematycConsole(refInfo);
	}
}

} // namespace LibUtils

} // namespace Schematyc
