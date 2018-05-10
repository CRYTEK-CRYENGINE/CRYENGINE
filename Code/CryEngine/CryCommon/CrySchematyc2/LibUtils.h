// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/ILib.h"
#include "CrySchematyc2/Script/IScriptGraph.h"
#include "CrySchematyc2/Script/IScriptRegistry.h"

namespace Schematyc2
{
	namespace NodeUtils
	{
		inline bool CanGoToNodeType(Schematyc2::EScriptGraphNodeType nodeType)
		{
			return nodeType == Schematyc2::EScriptGraphNodeType::Function || nodeType == Schematyc2::EScriptGraphNodeType::Condition || nodeType == Schematyc2::EScriptGraphNodeType::Get || nodeType == Schematyc2::EScriptGraphNodeType::Set ||
				nodeType == Schematyc2::EScriptGraphNodeType::SendSignal || nodeType == Schematyc2::EScriptGraphNodeType::ProcessSignal || nodeType == Schematyc2::EScriptGraphNodeType::BroadcastSignal;
		}
	}

	namespace LibUtils
	{
		struct SReferencesLogInfo
		{
			struct SRefInfo
			{
				SLogMetaItemGUID gotoItem;
				SLogMetaChildGUID gobackItem;
				string pathLink;

				SRefInfo(const SLogMetaItemGUID& gotoItem, const SLogMetaChildGUID gobackItem, const char* szPath)
					: gotoItem(gotoItem)
					, gobackItem(gobackItem)
					, pathLink(szPath)
				{}
			};

			bool   IsEmpty()           const { return references.empty(); }
			size_t GetReferenceCount() const { return references.size(); }

			string                header;
			string                footer;
			std::vector<SRefInfo> references;
		};

		inline void PrintReferencesToSchematycConsole(const SReferencesLogInfo& references)
		{
			SCHEMATYC2_COMMENT(Schematyc2::LOG_STREAM_DEFAULT, references.header.c_str());
			for (const SReferencesLogInfo::SRefInfo& ref : references.references)
			{
				SCHEMATYC2_METAINFO_COMMENT(Schematyc2::LOG_STREAM_DEFAULT, CLogMessageMetaInfo(ECryLinkCommand::Goto, ref.gotoItem, ref.gobackItem), ref.pathLink.c_str());
			}
			SCHEMATYC2_COMMENT(Schematyc2::LOG_STREAM_DEFAULT, references.footer.c_str());
		}

		inline void FindReferences(SGUID refGuid, SGUID refGoBack, const char* szItemName, const Schematyc2::IScriptFile* pFile, SReferencesLogInfo& result)
		{
			result.header.Format("Searching references of %s in %s", szItemName, pFile->GetFileName());

			auto visitAbstractInterfaces = [refGuid, refGoBack, szItemName, &result](const IScriptAbstractInterfaceImplementation& abstractInterface)->EVisitStatus
			{
				std::stack<string> path;
				if (abstractInterface.GetRefGUID() == refGuid)
				{
					stack_string graphPath;
					graphPath.append("<font color = \"yellow\">");
					const IScriptElement *parent = abstractInterface.GetParent();
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

		inline void FindAndLogReferences(SGUID refGuid, SGUID refGoBack, const char* szItemName, const Schematyc2::IScriptFile* pFile, bool searchInAllFiles = false)
		{
			// Check if the refGuid is actually a action and assign the proper search guid
			if (const IScriptActionInstance* pAction = pFile->GetActionInstance(refGuid))
			{
				refGuid = pAction->GetActionGUID();
			}

			if (searchInAllFiles)
			{
				auto visitFiles = [refGuid, refGoBack, szItemName](const IScriptFile& file)->EVisitStatus
				{
					SReferencesLogInfo refInfo;
					FindReferences(refGuid, refGoBack, szItemName, &file, refInfo);
					if (!refInfo.IsEmpty())
					{
						PrintReferencesToSchematycConsole(refInfo);
					}
					return EVisitStatus::Continue;
				};

				gEnv->pSchematyc2->GetScriptRegistry().VisitFiles(ScriptFileConstVisitor::FromLambdaFunction(visitFiles));
			}
			else
			{
				SReferencesLogInfo refInfo;
				FindReferences(refGuid, refGoBack, szItemName, pFile, refInfo);
				PrintReferencesToSchematycConsole(refInfo);
			}
		}

		inline SGUID FindStateMachineByName(const ILibClass& libClass, const char* szName)
		{
			CRY_ASSERT(szName);
			if(szName)
			{
				for(size_t stateMachineIdx = 0, stateMachineCount = libClass.GetStateMachineCount(); stateMachineIdx < stateMachineCount; ++ stateMachineIdx)
				{
					const ILibStateMachine&	stateMachine = *libClass.GetStateMachine(stateMachineIdx);
					if(strcmp(stateMachine.GetName(), szName) == 0)
					{
						return stateMachine.GetGUID();
					}
				}
			}
			return SGUID();
		}

		inline size_t FindStateMachineByGUID(const ILibClass& libClass, const SGUID& guid)
		{
			for(size_t stateMachineIdx = 0, stateMachineCount = libClass.GetStateMachineCount(); stateMachineIdx < stateMachineCount; ++ stateMachineIdx)
			{
				if(libClass.GetStateMachine(stateMachineIdx)->GetGUID() == guid)
				{
					return stateMachineIdx;
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindStateByGUID(const ILibClass& libClass, const SGUID& guid)
		{
			for(size_t stateIdx = 0, stateCount = libClass.GetStateCount(); stateIdx < stateCount; ++ stateIdx)
			{
				if(libClass.GetState(stateIdx)->GetGUID() == guid)
				{
					return stateIdx;
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindVariableByGUID(const ILibClass& libClass, const SGUID& guid)
		{
			for(size_t variableIdx = 0, variableCount = libClass.GetVariableCount(); variableIdx < variableCount; ++ variableIdx)
			{
				if(libClass.GetVariable(variableIdx)->GetGUID() == guid)
				{
					return variableIdx;
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindContainerByGUID(const ILibClass& libClass, const SGUID& guid)
		{
			for(size_t containerIdx = 0, containerCount = libClass.GetContainerCount(); containerIdx < containerCount; ++ containerIdx)
			{
				if(libClass.GetContainer(containerIdx)->GetGUID() == guid)
				{
					return containerIdx;
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindTimerByGUID(const ILibClass& libClass, const SGUID& guid)
		{
			for(size_t timerIdx = 0, timerCount = libClass.GetTimerCount(); timerIdx < timerCount; ++ timerIdx)
			{
				if(libClass.GetTimer(timerIdx)->GetGUID() == guid)
				{
					return timerIdx;
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindAbstractInterfaceImplementation(const ILibClass& libClass, const SGUID& guid)
		{
			for(size_t abstractInterfaceImplementationIdx = 0, abstractInterfaceImplementationCount = libClass.GetAbstractInterfaceImplementationCount(); abstractInterfaceImplementationIdx < abstractInterfaceImplementationCount; ++ abstractInterfaceImplementationIdx)
			{
				if(libClass.GetAbstractInterfaceImplementation(abstractInterfaceImplementationIdx)->GetInterfaceGUID() == guid)
				{
					return abstractInterfaceImplementationIdx;
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindAbstractInterfaceFunction(const ILibAbstractInterfaceImplementation& abstractInterfaceImplementation, const SGUID& guid)
		{
			for(size_t functionIdx = 0, functionCount = abstractInterfaceImplementation.GetFunctionCount(); functionIdx < functionCount; ++ functionIdx)
			{
				if(abstractInterfaceImplementation.GetFunctionGUID(functionIdx) == guid)
				{
					return functionIdx;
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindComponentInstanceByGUID(const ILibClass& libClass, const SGUID& guid)
		{
			for(size_t componentInstanceIdx = 0, componentInstanceCount = libClass.GetComponentInstanceCount(); componentInstanceIdx < componentInstanceCount; ++ componentInstanceIdx)
			{
				if(libClass.GetComponentInstance(componentInstanceIdx)->GetGUID() == guid)
				{
					return componentInstanceIdx;
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindComponentInstanceByComponentGUID(const ILibClass& libClass, const SGUID& componentGUID)
		{
			for(size_t componentInstanceIdx = 0, componentInstanceCount = libClass.GetComponentInstanceCount(); componentInstanceIdx < componentInstanceCount; ++ componentInstanceIdx)
			{
				if(libClass.GetComponentInstance(componentInstanceIdx)->GetComponentGUID() == componentGUID)
				{
					return componentInstanceIdx;
				}
			}
			return INVALID_INDEX;
		}

		inline size_t FindActionInstanceByGUID(const ILibClass& libClass, const SGUID& guid)
		{
			for(size_t actionInstanceIdx = 0, actionInstanceCount = libClass.GetActionInstanceCount(); actionInstanceIdx < actionInstanceCount; ++ actionInstanceIdx)
			{
				if(libClass.GetActionInstance(actionInstanceIdx)->GetGUID() == guid)
				{
					return actionInstanceIdx;
				}
			}
			return INVALID_INDEX;
		}
	}
}
