// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Use domain context for queries and name qualification.

#include "StdAfx.h"
#include "BrowserUtils.h"

#include <CrySchematyc2/AggregateTypeId.h>
#include <CrySchematyc2/IDomainContext.h>
#include <CrySchematyc2/IEnvTypeDesc.h>

#include "NameDlg.h"
#include "PluginUtils.h"
#include "QuickSearchDlg.h"

namespace Schematyc2
{
	namespace BrowserUtils
	{
		struct SInclude
		{
			inline SInclude(const char* _szFileName, const SGUID& _guid)
				: fileName(_szFileName)
				, guid(_guid)
			{}

			string fileName;
			SGUID  guid;
		};

		class CIncludeQuickSearchOptions : public Schematyc2::IQuickSearchOptions
		{
		public:

			inline CIncludeQuickSearchOptions(const TScriptFile& scriptFile)
				: m_scriptFile(scriptFile)
			{
				m_exclusions.reserve(64);
				m_exclusions.push_back(m_scriptFile.GetGUID());
				CExclusionCollector(m_scriptFile, m_exclusions);

				m_includes.reserve(64);
				GetSchematyc()->GetScriptRegistry().VisitFiles(TScriptFileConstVisitor::FromMemberFunction<CIncludeQuickSearchOptions, &CIncludeQuickSearchOptions::VisitScriptFile>(*this));

				auto sortIncludes = [] (const SInclude& lhs, const SInclude& rhs)
				{
					return lhs.guid < rhs.guid;
				};
				std::sort(m_includes.begin(), m_includes.end(), sortIncludes);
			}

			// IQuickSearchOptions

			virtual const char* GetToken() const override
			{
				return "/";
			}

			virtual size_t GetCount() const override
			{
				return m_includes.size();
			}

			virtual const char* GetLabel(size_t optionIdx) const override
			{
				return optionIdx < m_includes.size() ? m_includes[optionIdx].fileName.c_str() : "";
			}

			virtual const char* GetDescription(size_t optionIdx) const override
			{
				return nullptr;
			}

			virtual const char* GetWikiLink(size_t optionIdx) const override
			{
				return nullptr;
			}

			// ~IQuickSearchOptions

			inline const SInclude* GetInclude(size_t includeIdx) const
			{
				return includeIdx < m_includes.size() ? &m_includes[includeIdx] : nullptr;
			}

		private:

			typedef std::vector<SGUID> Exclusions;

			class CExclusionCollector
			{
			public:

				inline CExclusionCollector(const TScriptFile& scriptFile, Exclusions& exclusions)
					: m_exclusions(exclusions)
				{
					ScriptIncludeRecursionUtils::VisitIncludes(scriptFile, ScriptIncludeRecursionUtils::IncludeVisitor::FromMemberFunction<CExclusionCollector, &CExclusionCollector::VisitInclude>(*this));
				}

				inline void VisitInclude(const TScriptFile& scriptFile, const IScriptInclude& include)
				{
					m_exclusions.push_back(include.GetRefGUID());
				}

			private:

				Exclusions& m_exclusions;
			};

			typedef std::vector<SInclude> Includes;

			EVisitStatus VisitScriptFile(const TScriptFile& scriptFile)
			{
				if((scriptFile.GetFlags() & EScriptFileFlags::OnDisk) != 0)
				{
					const SGUID guid = scriptFile.GetGUID();
					if(std::find(m_exclusions.begin(), m_exclusions.end(), guid) == m_exclusions.end())
					{
						Exclusions reverseExclusions;
						CExclusionCollector(scriptFile, reverseExclusions);
						if(std::find(reverseExclusions.begin(), reverseExclusions.end(), guid) == reverseExclusions.end())
						{
							m_includes.push_back(SInclude(scriptFile.GetFileName(), guid));
						}
					}
				}
				return EVisitStatus::Continue;
			}

			const TScriptFile& m_scriptFile;
			Exclusions                       m_exclusions;
			Includes                         m_includes;
		};

		struct SClassBase
		{
			inline SClassBase() {}

			inline SClassBase(const char* _szName, const char* _szDescription, SGUID& _baseGUID, SGUID& _foundationGUID)
				: name(_szName)
				, description(_szDescription)
				, baseGUID(_baseGUID)
				, foundationGUID(_foundationGUID)
			{}

			string name;
			string description;
			SGUID  baseGUID;
			SGUID  foundationGUID;
		};

		typedef std::vector<SClassBase> ClassBases;

		class CClassBaseQuickSearchOptions : public Schematyc2::IQuickSearchOptions
		{
		public:

			inline CClassBaseQuickSearchOptions(const TScriptFile& scriptFile)
			{
				m_classBases.reserve(32);
				GetSchematyc()->GetEnvRegistry().VisitFoundations(EnvFoundationVisitor::FromMemberFunction<CClassBaseQuickSearchOptions, &CClassBaseQuickSearchOptions::VisitEnvFoundation>(*this));
				ScriptIncludeRecursionUtils::VisitClasses(scriptFile, ScriptIncludeRecursionUtils::ClassVisitor::FromMemberFunction<CClassBaseQuickSearchOptions, &CClassBaseQuickSearchOptions::VisitClass>(*this), SGUID(), EVisitFlags::RecurseHierarchy);
			}

			// IQuickSearchOptions

			virtual const char* GetToken() const override
			{
				return "::";
			}

			virtual size_t GetCount() const override
			{
				return m_classBases.size();
			}

			virtual const char* GetLabel(size_t optionIdx) const override
			{
				return optionIdx < m_classBases.size() ? m_classBases[optionIdx].name.c_str() : "";
			}

			virtual const char* GetDescription(size_t optionIdx) const override
			{
				return optionIdx < m_classBases.size() ? m_classBases[optionIdx].description.c_str() : "";
			}

			virtual const char* GetWikiLink(size_t optionIdx) const override
			{
				return nullptr;
			}

			// ~IQuickSearchOptions

			inline const SClassBase* GetClassBase(size_t classBaseIdx) const
			{
				return classBaseIdx < m_classBases.size() ? &m_classBases[classBaseIdx] : nullptr;
			}

		private:

			EVisitStatus VisitEnvFoundation(const IFoundationConstPtr& pFoundation)
			{
				m_classBases.push_back(SClassBase(pFoundation->GetName(), /*pFoundation->GetDescription()*/"", SGUID(), pFoundation->GetGUID()));
				return EVisitStatus::Continue;
			}

			void VisitClass(const TScriptFile& classFile, const IScriptClass& _class)
			{
				if(!DocUtils::Count(classFile, _class.GetGUID(), EScriptElementType::ClassBase, EVisitFlags::None))
				{
					stack_string fullName;
					DocUtils::GetFullElementName(classFile, _class.GetGUID(), fullName);
					fullName.insert(0, "Class::");
					m_classBases.push_back(SClassBase(fullName.c_str(), _class.GetDescription(), _class.GetGUID(), SGUID()));
				}
			}

			ClassBases m_classBases;
		};

		struct SVariableType
		{
			inline SVariableType() {}

			inline SVariableType(const char* _szName, const CAggregateTypeId& _typeId)
				: name(_szName)
				, typeId(_typeId)
			{}

			string           name;
			CAggregateTypeId typeId;
		};

		class CVariableTypeQuickSearchOptions : public Schematyc2::IQuickSearchOptions
		{
		public:

			inline CVariableTypeQuickSearchOptions(const TScriptFile& scriptFile)
			{
				GetSchematyc()->GetEnvRegistry().VisitTypeDescs(EnvTypeDescVisitor::FromMemberFunction<CVariableTypeQuickSearchOptions, &CVariableTypeQuickSearchOptions::VisitEnvTypeDesc>(*this));
				ScriptIncludeRecursionUtils::VisitEnumerations(scriptFile, ScriptIncludeRecursionUtils::EnumerationVisitor::FromMemberFunction<CVariableTypeQuickSearchOptions, &CVariableTypeQuickSearchOptions::VisitScriptEnumeration>(*this), SGUID(), true);
				ScriptIncludeRecursionUtils::VisitStructures(scriptFile, ScriptIncludeRecursionUtils::StructureVisitor::FromMemberFunction<CVariableTypeQuickSearchOptions, &CVariableTypeQuickSearchOptions::VisitScriptStructure>(*this), SGUID(), true);
			}

			// IQuickSearchOptions

			virtual const char* GetToken() const override
			{
				return "::";
			}

			virtual size_t GetCount() const override
			{
				return m_variableTypes.size();
			}

			virtual const char* GetLabel(size_t optionIdx) const override
			{
				return optionIdx < m_variableTypes.size() ? m_variableTypes[optionIdx].name.c_str() : "";
			}

			virtual const char* GetDescription(size_t optionIdx) const override
			{
				return nullptr;
			}

			virtual const char* GetWikiLink(size_t optionIdx) const override
			{
				return nullptr;
			}

			// ~IQuickSearchOptions

			inline const SVariableType* GetVariableType(size_t variableTypeIdx) const
			{
				return variableTypeIdx < m_variableTypes.size() ? &m_variableTypes[variableTypeIdx] : nullptr;
			}

		private:

			typedef std::vector<SVariableType> VariableTypes;

			EVisitStatus VisitEnvTypeDesc(const IEnvTypeDesc& typeDesc)
			{
				m_variableTypes.push_back(SVariableType(typeDesc.GetName(), typeDesc.GetTypeInfo().GetTypeId()));
				return EVisitStatus::Continue;
			}

			void VisitScriptEnumeration(const TScriptFile& enumerationFile, const IScriptEnumeration& enumeration)
			{
				stack_string fullName;
				DocUtils::GetFullElementName(enumerationFile, enumeration.GetGUID(), fullName);
				m_variableTypes.push_back(SVariableType(fullName.c_str(), enumeration.GetTypeId()));
			}

			void VisitScriptStructure(const TScriptFile& structureFile, const IScriptStructure& structure)
			{
				stack_string fullName;
				DocUtils::GetFullElementName(structureFile, structure.GetGUID(), fullName);
				m_variableTypes.push_back(SVariableType(fullName.c_str(), structure.GetTypeId()));
			}

			VariableTypes m_variableTypes;
		};

		struct SComponent
		{
			inline SComponent() {}

			inline SComponent(const SGUID& _guid, const char* _szName, const char* _szFullName, const char* szDescription, const char* szWikiLink, bool _bFixedName)
				: guid(_guid)
				, name(_szName)
				, fullName(_szFullName)
				, description(szDescription)
				, wikiLink(szWikiLink)
				, bFixedName(_bFixedName)
			{}

			SGUID  guid;
			string name;
			string fullName;
			string description;
			string wikiLink;
			bool   bFixedName;
		};

		class CComponentQuickSearchOptions : public Schematyc2::IQuickSearchOptions
		{
		public:

			inline CComponentQuickSearchOptions(const TScriptFile& scriptFile, const SGUID& scopeGUID)
			{
				const IScriptElement* pScopeElement = scriptFile.GetElement(scopeGUID);
				if(pScopeElement)
				{
					SGUID attachmentTypeGUID;
					switch(pScopeElement->GetElementType())
					{
					case EScriptElementType::ComponentInstance:
						{
							IComponentFactoryConstPtr pEnvComponentFactory = GetSchematyc()->GetEnvRegistry().GetComponentFactory(static_cast<const IScriptComponentInstance*>(pScopeElement)->GetComponentGUID());
							if(pEnvComponentFactory)
							{
								attachmentTypeGUID = pEnvComponentFactory->GetAttachmentType(EComponentSocket::Child);
							}
							break;
						}
					}
					
					TDomainContextPtr pDomainContext = GetSchematyc()->CreateDomainContext(TDomainContextScope(&scriptFile, scopeGUID));
					auto visitEnvComponentFactory = [this, attachmentTypeGUID, &pDomainContext] (const IComponentFactoryConstPtr& pEnvComponentFactory) -> EVisitStatus
					{
						if(!attachmentTypeGUID || (pEnvComponentFactory->GetAttachmentType(EComponentSocket::Parent) == attachmentTypeGUID))
						{
							stack_string fullName;
							pDomainContext->QualifyName(*pEnvComponentFactory, fullName);
							m_components.push_back(SComponent(pEnvComponentFactory->GetComponentGUID(), pEnvComponentFactory->GetName(), fullName.c_str(), pEnvComponentFactory->GetDescription(), pEnvComponentFactory->GetWikiLink(), (pEnvComponentFactory->GetFlags() & EComponentFlags::Singleton) != 0));
						}
						return EVisitStatus::Continue;
					};
					pDomainContext->VisitEnvComponentFactories(EnvComponentFactoryVisitor::FromLambdaFunction(visitEnvComponentFactory));

					auto componentSortPredicate = [] (const SComponent& lhs, const SComponent& rhs)
					{
						return lhs.fullName < rhs.fullName;
					};
					std::sort(m_components.begin(), m_components.end(), componentSortPredicate);
				}
			}

			// IQuickSearchOptions

			virtual const char* GetToken() const override
			{
				return "::";
			}

			virtual size_t GetCount() const override
			{
				return m_components.size();
			}

			virtual const char* GetLabel(size_t optionIdx) const override
			{
				return optionIdx < m_components.size() ? m_components[optionIdx].fullName.c_str() : "";
			}

			virtual const char* GetDescription(size_t optionIdx) const override
			{
				return optionIdx < m_components.size() ? m_components[optionIdx].description.c_str() : "";
			}

			virtual const char* GetWikiLink(size_t optionIdx) const override
			{
				return optionIdx < m_components.size() ? m_components[optionIdx].wikiLink.c_str() : "";
			}

			// ~IQuickSearchOptions

			inline const SComponent* GetComponent(size_t componentIdx) const
			{
				return componentIdx < m_components.size() ? &m_components[componentIdx] : nullptr;
			}

		private:

			typedef std::vector<SComponent> Components;

			Components m_components;
		};

		struct SAction
		{
			inline SAction() {}

			inline SAction(const SGUID& _guid, const SGUID& _componentInstanceGUID, const char* _szName, const char* _szFullName, const char* _szDescription, const char* _szWikiLink, bool _bFixedName)
				: guid(_guid)
				, componentInstanceGUID(_componentInstanceGUID)
				, name(_szName)
				, fullName(_szFullName)
				, description(_szDescription)
				, wikiLink(_szWikiLink)
				, bFixedName(_bFixedName)
			{}

			SGUID  guid;
			SGUID  componentInstanceGUID;
			string name;
			string fullName;
			string description;
			string wikiLink;
			bool   bFixedName;
		};

		class CActionQuickSearchOptions : public Schematyc2::IQuickSearchOptions
		{
		public:

			inline CActionQuickSearchOptions(const TScriptFile& scriptFile, const SGUID& scopeGUID)
				: m_scriptFile(scriptFile)
				, m_scopeGUID(scopeGUID)
			{
				GetSchematyc()->GetEnvRegistry().VisitActionFactories(EnvActionFactoryVisitor::FromMemberFunction<CActionQuickSearchOptions, &CActionQuickSearchOptions::VisitActionFactory>(*this));

				auto actionSortPredicate = [] (const SAction& lhs, const SAction& rhs)
				{
					return lhs.fullName < rhs.fullName;
				};
				std::sort(m_actions.begin(), m_actions.end(), actionSortPredicate);
			}

			// IQuickSearchOptions

			virtual const char* GetToken() const override
			{
				return "::";
			}

			virtual size_t GetCount() const override
			{
				return m_actions.size();
			}

			virtual const char* GetLabel(size_t optionIdx) const override
			{
				return optionIdx < m_actions.size() ? m_actions[optionIdx].fullName.c_str() : "";
			}

			virtual const char* GetDescription(size_t optionIdx) const override
			{
				return optionIdx < m_actions.size() ? m_actions[optionIdx].description.c_str() : "";
			}

			virtual const char* GetWikiLink(size_t optionIdx) const override
			{
				return optionIdx < m_actions.size() ? m_actions[optionIdx].wikiLink.c_str() : "";
			}

			// ~IQuickSearchOptions

			inline const SAction* GetAction(size_t actionIdx) const
			{
				return actionIdx < m_actions.size() ? &m_actions[actionIdx] : nullptr;
			}

		private:

			typedef std::vector<SAction> Actions;

			EVisitStatus VisitActionFactory(const IActionFactoryConstPtr& pActionFactory)
			{
				const SGUID& componentGUID = pActionFactory->GetComponentGUID();
				if(componentGUID)
				{
					const IScriptClass* pScriptClass = DocUtils::FindOwnerClass(m_scriptFile, m_scopeGUID);
					if(pScriptClass)
					{
						TScriptComponentInstanceConstVector componentInstances;
						DocUtils::CollectComponentInstances(m_scriptFile, pScriptClass->GetGUID(), true, componentInstances);
						for(TScriptComponentInstanceConstVector::const_iterator iComponentInstance = componentInstances.begin(), iEndComponentInstance = componentInstances.end(); iComponentInstance != iEndComponentInstance; ++ iComponentInstance)
						{
							const IScriptComponentInstance& componentInstance = *(*iComponentInstance);
							if(componentInstance.GetComponentGUID() == componentGUID)
							{
								const char*  szName = pActionFactory->GetName();
								stack_string fullName;
								DocUtils::GetFullElementName(m_scriptFile, componentInstance, fullName, EScriptElementType::Class);
								fullName.append("::");
								fullName.append(szName);
								m_actions.push_back(SAction(pActionFactory->GetActionGUID(), componentInstance.GetGUID(), szName, fullName.c_str(), pActionFactory->GetDescription(), pActionFactory->GetWikiLink(), (pActionFactory->GetFlags() & EActionFlags::Singleton) != 0));
							}
						}
					}
				}
				else
				{
					const char* szNamespace = pActionFactory->GetNamespace();
					if(DocUtils::IsElementAvailableInScope(m_scriptFile, m_scopeGUID, szNamespace))
					{
						const char*  szName = pActionFactory->GetName();
						stack_string fullName;
						EnvRegistryUtils::GetFullName(szName, szNamespace, componentGUID, fullName);
						m_actions.push_back(SAction(pActionFactory->GetActionGUID(), componentGUID, szName, fullName.c_str(), pActionFactory->GetDescription(), pActionFactory->GetWikiLink(), (pActionFactory->GetFlags() & EActionFlags::Singleton) != 0));
					}
				}
				return EVisitStatus::Continue;
			}

			const TScriptFile& m_scriptFile;
			const SGUID&                     m_scopeGUID;
			Actions                          m_actions;
		};

		TScriptFile* CreateScriptFile(CWnd* pWnd, CPoint point, const char* szPath)
		{
			SET_LOCAL_RESOURCE_SCOPE

			auto nameValidator = [] (HWND hWnd, const char* szName, bool displayErrorMessages)
			{
				return PluginUtils::ValidateFilePath(hWnd, szName, displayErrorMessages);
			};
			CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::Lowercase, nameValidator);
			if(nameDlg.DoModal() == IDOK)
			{
				stack_string filePath = szPath;
				filePath.append("/");
				filePath.append(nameDlg.GetResult());
				TScriptFile* pScriptFile = GetSchematyc()->GetScriptRegistry().CreateFile(filePath.c_str());
				if(pScriptFile)
				{
					pScriptFile->Save();
				}
				return pScriptFile;
			}
			return nullptr;
		}

		IScriptInclude* AddInclude(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			CIncludeQuickSearchOptions quickSearchOptions(scriptFile);
			CQuickSearchDlg            quickSearchDlg(pWnd, point, "Include", nullptr, nullptr, quickSearchOptions);
			if(quickSearchDlg.DoModal() == IDOK)
			{
				const SInclude* pInclude = quickSearchOptions.GetInclude(quickSearchDlg.GetResult());
				SCHEMATYC2_SYSTEM_ASSERT(pInclude);
				if(pInclude)
				{
					return scriptFile.AddInclude(scopeGUID, pInclude->fileName.c_str(), pInclude->guid);
				}
			}
			return nullptr;
		}

		IScriptGroup* AddGroup(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
			{
				return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
			};
			CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
			if(nameDlg.DoModal() == IDOK)
			{
				return scriptFile.AddGroup(scopeGUID, nameDlg.GetResult());
			}
			return nullptr;
		}

		IScriptEnumeration* AddEnumeration(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
			{
				return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
			};
			CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
			if(nameDlg.DoModal() == IDOK)
			{
				return scriptFile.AddEnumeration(scopeGUID, nameDlg.GetResult());
			}
			return nullptr;
		}

		IScriptStructure* AddStructure(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
			{
				return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
			};
			CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
			if(nameDlg.DoModal() == IDOK)
			{
				return scriptFile.AddStructure(scopeGUID, nameDlg.GetResult());
			}
			return nullptr;
		}

		IScriptSignal* AddSignal(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
			{
				return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
			};
			CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
			if(nameDlg.DoModal() == IDOK)
			{
				return scriptFile.AddSignal(scopeGUID, nameDlg.GetResult());
			}
			return nullptr;
		}

		IScriptAbstractInterface* AddAbstractInterface(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
			{
				return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
			};
			CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
			if(nameDlg.DoModal() == IDOK)
			{
				return scriptFile.AddAbstractInterface(scopeGUID, nameDlg.GetResult());
			}
			return nullptr;
		}

		IScriptAbstractInterfaceFunction* AddAbstractInterfaceFunction(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
			{
				return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
			};
			CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
			if(nameDlg.DoModal() == IDOK)
			{
				return scriptFile.AddAbstractInterfaceFunction(scopeGUID, nameDlg.GetResult());
			}
			return nullptr;
		}

		IScriptAbstractInterfaceTask* AddAbstractInterfaceTask(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
			{
				return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
			};
			CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
			if(nameDlg.DoModal() == IDOK)
			{
				return scriptFile.AddAbstractInterfaceTask(scopeGUID, nameDlg.GetResult());
			}
			return nullptr;
		}

		IScriptClass* AddClass(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			CClassBaseQuickSearchOptions quickSearchOptions(scriptFile);
			CQuickSearchDlg              quickSearchDlg(pWnd, point, "Base", nullptr, nullptr, quickSearchOptions);
			if(quickSearchDlg.DoModal() == IDOK)
			{
				auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool bDisplayErrorMessages)
				{
					return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, bDisplayErrorMessages);
				};
				CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
				if(nameDlg.DoModal() == IDOK)
				{
					const SClassBase* pClassBase = quickSearchOptions.GetClassBase(quickSearchDlg.GetResult());
					SCHEMATYC2_SYSTEM_ASSERT(pClassBase);
					if(pClassBase)
					{
						IScriptClass* pClass = scriptFile.AddClass(scopeGUID, nameDlg.GetResult(), pClassBase->foundationGUID);
						SCHEMATYC2_SYSTEM_ASSERT(pClass);
						if(pClass && pClassBase->baseGUID)
						{
							scriptFile.AddClassBase(pClass->GetGUID(), pClassBase->baseGUID);
						}
						return pClass;
					}
				}
			}
			return nullptr;
		}

		IScriptStateMachine* AddStateMachine(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID, EStateMachineLifetime lifeTime)
		{
			SET_LOCAL_RESOURCE_SCOPE

			auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
			{
				return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
			};
			CNameDlg nameDlg(pWnd, point, "Name", lifeTime == EStateMachineLifetime::Persistent ? "StateMachine" : "", ENameDlgTextCase::MixedCase, nameValidator);
			if(nameDlg.DoModal() == IDOK)
			{
				return scriptFile.AddStateMachine(scopeGUID, nameDlg.GetResult(), lifeTime, SGUID(), SGUID());
			}
			return nullptr;
		}

		IScriptVariable* AddVariable(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			CVariableTypeQuickSearchOptions quickSearchOptions(scriptFile);
			CQuickSearchDlg                 quickSearchDlg(pWnd, point, "Type", nullptr, nullptr, quickSearchOptions);
			if(quickSearchDlg.DoModal() == IDOK)
			{
				auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
				{
					return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
				};
				CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
				if(nameDlg.DoModal() == IDOK)
				{
					const SVariableType* pVariableType = quickSearchOptions.GetVariableType(quickSearchDlg.GetResult());
					SCHEMATYC2_SYSTEM_ASSERT(pVariableType);
					if(pVariableType)
					{
						return scriptFile.AddVariable(scopeGUID, nameDlg.GetResult(), pVariableType->typeId);
					}
				}
			}
			return nullptr;
		}

		IScriptProperty* AddProperty(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			CVariableTypeQuickSearchOptions quickSearchOptions(scriptFile);
			CQuickSearchDlg                 quickSearchDlg(pWnd, point, "Type", nullptr, nullptr, quickSearchOptions);
			if(quickSearchDlg.DoModal() == IDOK)
			{
				auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
				{
					return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
				};
				CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
				if(nameDlg.DoModal() == IDOK)
				{
					const SVariableType* pVariableType = quickSearchOptions.GetVariableType(quickSearchDlg.GetResult());
					SCHEMATYC2_SYSTEM_ASSERT(pVariableType);
					if(pVariableType)
					{
						return scriptFile.AddProperty(scopeGUID, nameDlg.GetResult(), SGUID(), pVariableType->typeId);
					}
				}
			}
			return nullptr;
		}

		IScriptTimer* AddTimer(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
			{
				return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
			};
			CNameDlg nameDlg(pWnd, point, "Name", "", ENameDlgTextCase::MixedCase, nameValidator);
			if(nameDlg.DoModal() == IDOK)
			{
				return scriptFile.AddTimer(scopeGUID, nameDlg.GetResult());
			}
			return nullptr;
		}

		IScriptComponentInstance* AddComponentInstance(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			CComponentQuickSearchOptions quickSearchOptions(scriptFile, scopeGUID);
			CQuickSearchDlg              quickSearchDlg(pWnd, point, "Component", nullptr, nullptr, quickSearchOptions);
			if(quickSearchDlg.DoModal() == IDOK)
			{
				const SComponent* pComponent = quickSearchOptions.GetComponent(quickSearchDlg.GetResult());
				SCHEMATYC2_SYSTEM_ASSERT(pComponent);
				if(pComponent)
				{
					if(pComponent->bFixedName)
					{
						return scriptFile.AddComponentInstance(scopeGUID, pComponent->name.c_str(), pComponent->guid, EScriptComponentInstanceFlags::None);
					}
					else
					{
						auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
						{
							return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
						};
						CNameDlg nameDlg(pWnd, point, "Name", pComponent->name.c_str(), ENameDlgTextCase::MixedCase, nameValidator);
						if(nameDlg.DoModal() == IDOK)
						{
							return scriptFile.AddComponentInstance(scopeGUID, nameDlg.GetResult(), pComponent->guid, EScriptComponentInstanceFlags::None);
						}
					}
				}
			}
			return nullptr;
		}

		IScriptActionInstance* AddActionInstance(CWnd* pWnd, CPoint point, TScriptFile& scriptFile, const SGUID& scopeGUID)
		{
			SET_LOCAL_RESOURCE_SCOPE

			CActionQuickSearchOptions quickSearchOptions(scriptFile, scopeGUID);
			CQuickSearchDlg           quickSearchDlg(pWnd, point, "Action", nullptr, nullptr, quickSearchOptions);
			if(quickSearchDlg.DoModal() == IDOK)
			{
				const SAction* pAction = quickSearchOptions.GetAction(quickSearchDlg.GetResult());
				SCHEMATYC2_SYSTEM_ASSERT(pAction);
				if(pAction)
				{
					if(pAction->bFixedName)
					{
						return scriptFile.AddActionInstance(scopeGUID, pAction->name.c_str(), pAction->guid, pAction->componentInstanceGUID);
					}
					else
					{
						auto nameValidator = [&scriptFile, &scopeGUID] (HWND hWnd, const char* szName, bool displayErrorMessages)
						{
							return PluginUtils::ValidateScriptElementName(hWnd, scriptFile, scopeGUID, szName, displayErrorMessages);
						};
						CNameDlg nameDlg(pWnd, point, "Name", pAction->name.c_str(), ENameDlgTextCase::MixedCase, nameValidator);
						if(nameDlg.DoModal() == IDOK)
						{
							return scriptFile.AddActionInstance(scopeGUID, nameDlg.GetResult(), pAction->guid, pAction->componentInstanceGUID);
						}
					}
				}
			}
			return nullptr;
		}
	}
}
