// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptFile.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/IAbstractInterface.h>
#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Serialization/SerializationUtils.h>
#include <CrySchematyc2/Services/ILog.h>

#include "CVars.h"
#include "Script/ScriptFilePatcher.h"
#include "Script/ScriptSerializationUtils.h"
#include "Script/Elements/ScriptRoot.h"
#include "Serialization/SerializationContext.h"
#include "Serialization/ValidatorArchive.h"

namespace Schematyc2
{
	namespace DocUtils
	{
		// Ideally element type names should be reflected using the serialization system but right now that would break compatibility.
		static const struct { EScriptElementType elementType; string name; } ELEMENT_TYPES[] =
		{
			{ EScriptElementType::Include,                         "include"                           },
			{ EScriptElementType::Group,                           "group"                             },
			{ EScriptElementType::Enumeration,                     "enumeration"                       },
			{ EScriptElementType::Structure,                       "structure"                         },
			{ EScriptElementType::Signal,                          "signal"                            },
			{ EScriptElementType::AbstractInterface,               "abstract_interface"                },
			{ EScriptElementType::AbstractInterfaceFunction,       "abstract_interface_function"       },
			{ EScriptElementType::AbstractInterfaceTask,           "abstract_interface_task"           },
			{ EScriptElementType::Class,                           "schema"                            },
			{ EScriptElementType::ClassBase,                       "schema_base"                       },
			{ EScriptElementType::StateMachine,                    "state_machine"	                   },
			{ EScriptElementType::State,                           "state"                             },
			{ EScriptElementType::Variable,                        "variable"                          },
			{ EScriptElementType::Property,                        "property"                          },
			{ EScriptElementType::Container,                       "container"                         },
			{ EScriptElementType::Timer,                           "timer"                             },
			{ EScriptElementType::AbstractInterfaceImplementation, "abstract_interface_implementation" },
			{ EScriptElementType::ComponentInstance,               "component_instance"                },
			{ EScriptElementType::ActionInstance,                  "action_instance"                   },
			{ EScriptElementType::Graph,                           "graph"                             }
		};

		struct SDocGraphHeader
		{
			inline SDocGraphHeader()
				: type(EScriptGraphType::Unknown)
			{}

			inline SDocGraphHeader(const SGUID& _guid, const SGUID& _scopeGUID, const char* _szName, EScriptGraphType _type, const SGUID& _contextGUID)
				: guid(_guid)
				, scopeGUID(_scopeGUID)
				, name(_szName)
				, type(_type)
				, contextGUID(_contextGUID)
			{}

			void Serialize(Serialization::IArchive& arhcive)
			{
				arhcive(guid, "guid");
				arhcive(scopeGUID, "scope_guid");
				arhcive(name, "name");
				arhcive(type, "type");
				arhcive(contextGUID, "context_guid");
			}

			SGUID            guid;
			SGUID            scopeGUID;
			string           name;
			EScriptGraphType type;
			SGUID            contextGUID;
		};
	}

	static CDocPatcher g_docPatcher; // #SchematycTODO : Rename CScriptFilePatcher. Move to IFramework?

	//////////////////////////////////////////////////////////////////////////
	CScriptFile::CScriptFile(const char* szFileName, const SGUID& guid, EScriptFileFlags flags)
		: m_fileName(szFileName)
		, m_guid(guid)
		, m_flags(flags)
	{
		m_pRoot.reset(new CScriptRoot(*this));
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptFile::GetFileName() const
	{
		return m_fileName.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptFile::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::SetFlags(EScriptFileFlags flags)
	{
		m_flags = flags;
	}

	//////////////////////////////////////////////////////////////////////////
	EScriptFileFlags CScriptFile::GetFlags() const
	{
		return m_flags;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptInclude* CScriptFile::AddInclude(const SGUID& scopeGUID, const char* szFileName, const SGUID& refGUID)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptIncludePtr pInclude(new CScriptInclude(*this, guid, scopeGUID, szFileName, refGUID));
				m_elements.insert(Elements::value_type(guid, pInclude));
				AttachElement(*pInclude, scopeGUID);
				pInclude->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pInclude.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptInclude* CScriptFile::GetInclude(const SGUID& guid)
	{
		return GetElement<IScriptInclude>(EScriptElementType::Include, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptInclude* CScriptFile::GetInclude(const SGUID& guid) const
	{
		return GetElement<IScriptInclude>(EScriptElementType::Include, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitIncludes(const ScriptIncludeVisitor& visitor)
	{
		return VisitElements<IScriptInclude>(EScriptElementType::Include, visitor);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitIncludes(const ScriptIncludeVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptInclude>(EScriptElementType::Include, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitIncludes(const ScriptIncludeConstVisitor& visitor) const
	{
		return VisitElements<IScriptInclude>(EScriptElementType::Include, visitor);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitIncludes(const ScriptIncludeConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptInclude>(EScriptElementType::Include, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptGroup* CScriptFile::AddGroup(const SGUID& scopeGUID, const char* szName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptGroupPtr pGroup(new CScriptGroup(*this, guid, scopeGUID, szName));
				m_elements.insert(Elements::value_type(guid, pGroup));
				AttachElement(*pGroup, scopeGUID);
				pGroup->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pGroup.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptGroup* CScriptFile::GetGroup(const SGUID& guid)
	{
		return GetElement<IScriptGroup>(EScriptElementType::Group, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptGroup* CScriptFile::GetGroup(const SGUID& guid) const
	{
		return GetElement<IScriptGroup>(EScriptElementType::Group, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitGroups(const ScriptGroupVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptGroup>(EScriptElementType::Group, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitGroups(const ScriptGroupConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptGroup>(EScriptElementType::Group, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptEnumeration* CScriptFile::AddEnumeration(const SGUID& scopeGUID, const char* szName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptEnumerationPtr pEnumeration(new CScriptEnumeration(*this, guid, scopeGUID, szName));
				m_elements.insert(Elements::value_type(guid, pEnumeration));
				AttachElement(*pEnumeration, scopeGUID);
				pEnumeration->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pEnumeration.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptEnumeration* CScriptFile::GetEnumeration(const SGUID& guid)
	{
		return GetElement<IScriptEnumeration>(EScriptElementType::Enumeration, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptEnumeration* CScriptFile::GetEnumeration(const SGUID& guid) const
	{
		return GetElement<IScriptEnumeration>(EScriptElementType::Enumeration, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitEnumerations(const ScriptEnumerationVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptEnumeration>(EScriptElementType::Enumeration, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitEnumerations(const ScriptEnumerationConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptEnumeration>(EScriptElementType::Enumeration, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptStructure* CScriptFile::AddStructure(const SGUID& scopeGUID, const char* szName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptStructurePtr pStructure(new CScriptStructure(*this, guid, scopeGUID, szName));
				m_elements.insert(Elements::value_type(guid, pStructure));
				AttachElement(*pStructure, scopeGUID);
				pStructure->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pStructure.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptStructure* CScriptFile::GetStructure(const SGUID& guid)
	{
		return GetElement<IScriptStructure>(EScriptElementType::Structure, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptStructure* CScriptFile::GetStructure(const SGUID& guid) const
	{
		return GetElement<IScriptStructure>(EScriptElementType::Structure, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitStructures(const ScriptStructureVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptStructure>(EScriptElementType::Structure, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitStructures(const ScriptStructureConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptStructure>(EScriptElementType::Structure, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptSignal* CScriptFile::AddSignal(const SGUID& scopeGUID, const char* szName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptSignalPtr pSignal(new CScriptSignal(*this, guid, scopeGUID, szName));
				m_elements.insert(Elements::value_type(guid, pSignal));
				AttachElement(*pSignal, scopeGUID);
				pSignal->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pSignal.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptSignal* CScriptFile::GetSignal(const SGUID& guid)
	{
		return GetElement<IScriptSignal>(EScriptElementType::Signal, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptSignal* CScriptFile::GetSignal(const SGUID& guid) const
	{
		return GetElement<IScriptSignal>(EScriptElementType::Signal, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitSignals(const ScriptSignalVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptSignal>(EScriptElementType::Signal, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitSignals(const ScriptSignalConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptSignal>(EScriptElementType::Signal, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterface* CScriptFile::AddAbstractInterface(const SGUID& scopeGUID, const char* szName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptAbstractInterfacePtr pAbstractInterface(new CScriptAbstractInterface(*this, guid, scopeGUID, szName));
				m_elements.insert(Elements::value_type(guid, pAbstractInterface));
				AttachElement(*pAbstractInterface, scopeGUID);
				pAbstractInterface->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pAbstractInterface.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterface* CScriptFile::GetAbstractInterface(const SGUID& guid)
	{
		return GetElement<IScriptAbstractInterface>(EScriptElementType::AbstractInterface, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptAbstractInterface* CScriptFile::GetAbstractInterface(const SGUID& guid) const
	{
		return GetElement<IScriptAbstractInterface>(EScriptElementType::AbstractInterface, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitAbstractInterfaces(const ScriptAbstractInterfaceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptAbstractInterface>(EScriptElementType::AbstractInterface, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitAbstractInterfaces(const ScriptAbstractInterfaceConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptAbstractInterface>(EScriptElementType::AbstractInterface, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterfaceFunction* CScriptFile::AddAbstractInterfaceFunction(const SGUID& scopeGUID, const char* szName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptAbstractInterfaceFunctionPtr pAbstractInterfaceFunction(new CScriptAbstractInterfaceFunction(*this, guid, scopeGUID, szName));
				m_elements.insert(Elements::value_type(guid, pAbstractInterfaceFunction));
				AttachElement(*pAbstractInterfaceFunction, scopeGUID);
				pAbstractInterfaceFunction->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pAbstractInterfaceFunction.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterfaceFunction* CScriptFile::GetAbstractInterfaceFunction(const SGUID& guid)
	{
		return GetElement<IScriptAbstractInterfaceFunction>(EScriptElementType::AbstractInterfaceFunction, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptAbstractInterfaceFunction* CScriptFile::GetAbstractInterfaceFunction(const SGUID& guid) const
	{
		return GetElement<IScriptAbstractInterfaceFunction>(EScriptElementType::AbstractInterfaceFunction, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitAbstractInterfaceFunctions(const ScriptAbstractInterfaceFunctionVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptAbstractInterfaceFunction>(EScriptElementType::AbstractInterfaceFunction, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitAbstractInterfaceFunctions(const ScriptAbstractInterfaceFunctionConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptAbstractInterfaceFunction>(EScriptElementType::AbstractInterfaceFunction, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterfaceTask* CScriptFile::AddAbstractInterfaceTask(const SGUID& scopeGUID, const char* szName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptAbstractInterfaceTaskPtr pAbstractInterfaceTask(new CScriptAbstractInterfaceTask(*this, guid, scopeGUID, szName));
				m_elements.insert(Elements::value_type(guid, pAbstractInterfaceTask));
				AttachElement(*pAbstractInterfaceTask, scopeGUID);
				pAbstractInterfaceTask->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pAbstractInterfaceTask.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterfaceTask* CScriptFile::GetAbstractInterfaceTask(const SGUID& guid)
	{
		return GetElement<IScriptAbstractInterfaceTask>(EScriptElementType::AbstractInterfaceTask, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptAbstractInterfaceTask* CScriptFile::GetAbstractInterfaceTask(const SGUID& guid) const
	{
		return GetElement<IScriptAbstractInterfaceTask>(EScriptElementType::AbstractInterfaceTask, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitAbstractInterfaceTasks(const ScriptAbstractInterfaceTaskVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptAbstractInterfaceTask>(EScriptElementType::AbstractInterfaceTask, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitAbstractInterfaceTasks(const ScriptAbstractInterfaceTaskConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptAbstractInterfaceTask>(EScriptElementType::AbstractInterfaceTask, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptClass* CScriptFile::AddClass(const SGUID& scopeGUID, const char* szName, const SGUID& foundationGUID)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptClassPtr pClass(new CScriptClass(*this, guid, scopeGUID, szName, foundationGUID));
				m_elements.insert(Elements::value_type(guid, pClass));
				AttachElement(*pClass, scopeGUID);
				pClass->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pClass.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptClass* CScriptFile::GetClass(const SGUID& guid)
	{
		return GetElement<IScriptClass>(EScriptElementType::Class, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptClass* CScriptFile::GetClass(const SGUID& guid) const
	{
		return GetElement<IScriptClass>(EScriptElementType::Class, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitClasses(const ScriptClassVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptClass>(EScriptElementType::Class, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitClasses(const ScriptClassConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptClass>(EScriptElementType::Class, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptClassBase* CScriptFile::AddClassBase(const SGUID& scopeGUID, const SGUID& refGUID)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptClassBasePtr pClassBase(new CScriptClassBase(*this, guid, scopeGUID, refGUID));
				m_elements.insert(Elements::value_type(guid, pClassBase));
				AttachElement(*pClassBase, scopeGUID);
				pClassBase->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pClassBase.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptClassBase* CScriptFile::GetClassBase(const SGUID& guid)
	{
		return GetElement<IScriptClassBase>(EScriptElementType::ClassBase, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptClassBase* CScriptFile::GetClassBase(const SGUID& guid) const
	{
		return GetElement<IScriptClassBase>(EScriptElementType::ClassBase, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitClassBases(const ScriptClassBaseVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptClassBase>(EScriptElementType::ClassBase, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitClassBases(const ScriptClassBaseConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptClassBase>(EScriptElementType::ClassBase, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptStateMachine* CScriptFile::AddStateMachine(const SGUID& scopeGUID, const char* szName, EScriptStateMachineLifetime lifetime, const SGUID& contextGUID, const SGUID& partnerGUID)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptStateMachinePtr pStateMachine(new CScriptStateMachine(*this, guid, scopeGUID, szName, lifetime, contextGUID, partnerGUID));
				m_elements.insert(Elements::value_type(guid, pStateMachine));
				AttachElement(*pStateMachine, scopeGUID);
				pStateMachine->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pStateMachine.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptStateMachine* CScriptFile::GetStateMachine(const SGUID& guid)
	{
		return GetElement<IScriptStateMachine>(EScriptElementType::StateMachine, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptStateMachine* CScriptFile::GetStateMachine(const SGUID& guid) const
	{
		return GetElement<IScriptStateMachine>(EScriptElementType::StateMachine, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitStateMachines(const ScriptStateMachineVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptStateMachine>(EScriptElementType::StateMachine, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitStateMachines(const ScriptStateMachineConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptStateMachine>(EScriptElementType::StateMachine, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptState* CScriptFile::AddState(const SGUID& scopeGUID, const char* szName, const SGUID& partnerGUID)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptStatePtr pState(new CScriptState(*this, guid, scopeGUID, szName, partnerGUID));
				m_elements.insert(Elements::value_type(guid, pState));
				AttachElement(*pState, scopeGUID);
				pState->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pState.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptState* CScriptFile::GetState(const SGUID& guid)
	{
		return GetElement<IScriptState>(EScriptElementType::State, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptState* CScriptFile::GetState(const SGUID& guid) const
	{
		return GetElement<IScriptState>(EScriptElementType::State, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitStates(const ScriptStateVisitor& visitor)
	{
		return VisitElements<IScriptState>(EScriptElementType::State, visitor);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitStates(const ScriptStateVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptState>(EScriptElementType::State, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitStates(const ScriptStateConstVisitor& visitor) const
	{
		return VisitElements<IScriptState>(EScriptElementType::State, visitor);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitStates(const ScriptStateConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptState>(EScriptElementType::State, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptVariable* CScriptFile::AddVariable(const SGUID& scopeGUID, const char* szName, const CAggregateTypeId& typeId)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptVariablePtr pVariable(new CScriptVariable(*this, guid, scopeGUID, szName, typeId));
				m_elements.insert(Elements::value_type(guid, pVariable));
				AttachElement(*pVariable, scopeGUID);
				pVariable->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pVariable.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptVariable* CScriptFile::GetVariable(const SGUID& guid)
	{
		return GetElement<IScriptVariable>(EScriptElementType::Variable, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptVariable* CScriptFile::GetVariable(const SGUID& guid) const
	{
		return GetElement<IScriptVariable>(EScriptElementType::Variable, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitVariables(const ScriptVariableVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptVariable>(EScriptElementType::Variable, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitVariables(const ScriptVariableConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptVariable>(EScriptElementType::Variable, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptProperty* CScriptFile::AddProperty(const SGUID& scopeGUID, const char* szName, const SGUID& refGUID, const CAggregateTypeId& typeId)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptPropertyPtr pProperty(new CScriptProperty(*this, guid, scopeGUID, szName, refGUID, typeId));
				m_elements.insert(Elements::value_type(guid, pProperty));
				AttachElement(*pProperty, scopeGUID);
				pProperty->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pProperty.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptProperty* CScriptFile::GetProperty(const SGUID& guid)
	{
		return GetElement<IScriptProperty>(EScriptElementType::Property, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptProperty* CScriptFile::GetProperty(const SGUID& guid) const
	{
		return GetElement<IScriptProperty>(EScriptElementType::Property, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitProperties(const ScriptPropertyVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptProperty>(EScriptElementType::Property, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitProperties(const ScriptPropertyConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptProperty>(EScriptElementType::Property, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptContainer* CScriptFile::AddContainer(const SGUID& scopeGUID, const char* szName, const SGUID& typeGUID)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptContainerPtr pContainer(new CScriptContainer(*this, guid, scopeGUID, szName, typeGUID));
				m_elements.insert(Elements::value_type(guid, pContainer));
				AttachElement(*pContainer, scopeGUID);
				pContainer->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pContainer.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptContainer* CScriptFile::GetContainer(const SGUID& guid)
	{
		return GetElement<IScriptContainer>(EScriptElementType::Container, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptContainer* CScriptFile::GetContainer(const SGUID& guid) const
	{
		return GetElement<IScriptContainer>(EScriptElementType::Container, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitContainers(const ScriptContainerVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptContainer>(EScriptElementType::Container, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitContainers(const ScriptContainerConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptContainer>(EScriptElementType::Container, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptTimer* CScriptFile::AddTimer(const SGUID& scopeGUID, const char* szName)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptTimerPtr pTimer(new CScriptTimer(*this, guid, scopeGUID, szName));
				m_elements.insert(Elements::value_type(guid, pTimer));
				AttachElement(*pTimer, scopeGUID);
				pTimer->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pTimer.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptTimer* CScriptFile::GetTimer(const SGUID& guid)
	{
		return GetElement<IScriptTimer>(EScriptElementType::Timer, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptTimer* CScriptFile::GetTimer(const SGUID& guid) const
	{
		return GetElement<IScriptTimer>(EScriptElementType::Timer, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitTimers(const ScriptTimerVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptTimer>(EScriptElementType::Timer, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitTimers(const ScriptTimerConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptTimer>(EScriptElementType::Timer, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterfaceImplementation* CScriptFile::AddAbstractInterfaceImplementation(const SGUID& scopeGUID, EDomain domain, const SGUID& refGUID)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptAbstractInterfaceImplementationPtr pAbstractInterfaceImplementation(new CScriptAbstractInterfaceImplementation(*this, guid, scopeGUID, domain, refGUID));
				m_elements.insert(Elements::value_type(guid, pAbstractInterfaceImplementation));
				AttachElement(*pAbstractInterfaceImplementation, scopeGUID);
				pAbstractInterfaceImplementation->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pAbstractInterfaceImplementation.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptAbstractInterfaceImplementation* CScriptFile::GetAbstractInterfaceImplementation(const SGUID& guid)
	{
		return GetElement<IScriptAbstractInterfaceImplementation>(EScriptElementType::AbstractInterfaceImplementation, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptAbstractInterfaceImplementation* CScriptFile::GetAbstractInterfaceImplementation(const SGUID& guid) const
	{
		return GetElement<IScriptAbstractInterfaceImplementation>(EScriptElementType::AbstractInterfaceImplementation, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitAbstractInterfaceImplementations(const ScriptAbstractInterfaceImplementationVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptAbstractInterfaceImplementation>(EScriptElementType::AbstractInterfaceImplementation, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitAbstractInterfaceImplementations(const ScriptAbstractInterfaceImplementationConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptAbstractInterfaceImplementation>(EScriptElementType::AbstractInterfaceImplementation, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptComponentInstance* CScriptFile::AddComponentInstance(const SGUID& scopeGUID, const char* szName, const SGUID& componentGUID, EScriptComponentInstanceFlags flags)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptComponentInstancePtr pComponentInstance(new CScriptComponentInstance(*this, guid, scopeGUID, szName, componentGUID, flags));
				m_elements.insert(Elements::value_type(guid, pComponentInstance));
				AttachElement(*pComponentInstance, scopeGUID);
				pComponentInstance->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorDependencyModified));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pComponentInstance.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptComponentInstance* CScriptFile::GetComponentInstance(const SGUID& guid)
	{
		return GetElement<IScriptComponentInstance>(EScriptElementType::ComponentInstance, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptComponentInstance* CScriptFile::GetComponentInstance(const SGUID& guid) const
	{
		return GetElement<IScriptComponentInstance>(EScriptElementType::ComponentInstance, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitComponentInstances(const ScriptComponentInstanceVisitor& visitor)
	{
		return VisitElements<IScriptComponentInstance>(EScriptElementType::ComponentInstance, visitor);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitComponentInstances(const ScriptComponentInstanceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptComponentInstance>(EScriptElementType::ComponentInstance, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	EVisitStatus CScriptFile::VisitComponentInstances(const ScriptComponentInstanceConstVisitor& visitor) const
	{
		return VisitElements<IScriptComponentInstance>(EScriptElementType::ComponentInstance, visitor);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitComponentInstances(const ScriptComponentInstanceConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptComponentInstance>(EScriptElementType::ComponentInstance, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptActionInstance* CScriptFile::AddActionInstance(const SGUID& scopeGUID, const char* szName, const SGUID& actionGUID, const SGUID& componentInstanceGUID)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				CScriptActionInstancePtr pActionInstance(new CScriptActionInstance(*this, guid, scopeGUID, szName, actionGUID, componentInstanceGUID));
				m_elements.insert(Elements::value_type(guid, pActionInstance));
				AttachElement(*pActionInstance, scopeGUID);
				pActionInstance->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
				SetFlags(GetFlags() | EScriptFileFlags::Modified);
				return pActionInstance.get();
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptActionInstance* CScriptFile::GetActionInstance(const SGUID& guid)
	{
		return GetElement<IScriptActionInstance>(EScriptElementType::ActionInstance, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptActionInstance* CScriptFile::GetActionInstance(const SGUID& guid) const
	{
		return GetElement<IScriptActionInstance>(EScriptElementType::ActionInstance, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitActionInstances(const ScriptActionInstanceVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptActionInstance>(EScriptElementType::ActionInstance, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitActionInstances(const ScriptActionInstanceConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IScriptActionInstance>(EScriptElementType::ActionInstance, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	IDocGraph* CScriptFile::AddGraph(const SScriptGraphParams& params)
	{
		SCHEMATYC2_SYSTEM_ASSERT(gEnv->IsEditor());
		if(gEnv->IsEditor())
		{
			const SGUID guid = gEnv->pSchematyc2->CreateGUID();
			if(!GetElement(guid)) // #SchematycTODO : Can we remove/replace this check once we have a central guid registry?
			{
				IDocGraphPtr pGraph = DocSerializationUtils::g_docGraphFactory.CreateGraph(*this, guid, params.scopeGUID, params.szName, params.type, params.contextGUID);
				SCHEMATYC2_SYSTEM_ASSERT(pGraph);
				if(pGraph)
				{
					m_elements.insert(Elements::value_type(guid, pGraph));
					AttachElement(*pGraph, params.scopeGUID);
					pGraph->Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorAdd));
					SetFlags(GetFlags() | EScriptFileFlags::Modified);
					return pGraph.get();
				}
			}
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IDocGraph* CScriptFile::GetGraph(const SGUID& guid)
	{
		return GetElement<IDocGraph>(EScriptElementType::Graph, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	const IDocGraph* CScriptFile::GetGraph(const SGUID& guid) const
	{
		return GetElement<IDocGraph>(EScriptElementType::Graph, guid);
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitGraphs(const DocGraphVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IDocGraph>(EScriptElementType::Graph, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitGraphs(const DocGraphConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements<IDocGraph>(EScriptElementType::Graph, visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::RemoveElement(const SGUID& guid, bool clearScope)
	{
		Elements::iterator itElement = m_elements.find(guid);
		if(itElement != m_elements.end())
		{
			m_elements.erase(itElement);
			if(clearScope)
			{
				ClearScope(guid);
			}
			SetFlags(GetFlags() | EScriptFileFlags::Modified);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptElement* CScriptFile::GetElement(const SGUID& guid)
	{
		Elements::iterator itElement = m_elements.find(guid);
		return itElement != m_elements.end() ? itElement->second.get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptElement* CScriptFile::GetElement(const SGUID& guid) const
	{
		Elements::const_iterator itElement = m_elements.find(guid);
		return itElement != m_elements.end() ? itElement->second.get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptElement* CScriptFile::GetElement(const SGUID& guid, EScriptElementType elementType)
	{
		Elements::iterator itElement = m_elements.find(guid);
		return (itElement != m_elements.end()) && (itElement->second->GetElementType() == elementType) ? itElement->second.get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	const IScriptElement* CScriptFile::GetElement(const SGUID& guid, EScriptElementType elementType) const
	{
		Elements::const_iterator itElement = m_elements.find(guid);
		return (itElement != m_elements.end()) && (itElement->second->GetElementType() == elementType) ? itElement->second.get() : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitElements(const ScriptElementVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements(visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitElements(const ScriptElementConstVisitor& visitor, const SGUID& scopeGUID, bool bRecurseHierarchy) const
	{
		const IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		return pParentElement ? VisitElements(visitor, *pParentElement, bRecurseHierarchy) : EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CScriptFile::IsElementNameUnique(const SGUID& scopeGUID, const char* szName) const
	{
		// #SchematycTODO : Need to check against included documents too!!!
		SCHEMATYC2_SYSTEM_ASSERT(szName);
		if(szName)
		{
			for(const Elements::value_type& element : m_elements)
			{
				if((element.second->GetScopeGUID() == scopeGUID) && (stricmp(element.second->GetName(), szName) == 0))
				{
					return false;
				}
			}
		}
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef CScriptFile::CopyElementsToXml(const SGUID& guid, bool bRecurseHierarchy) const
	{
		XmlNodeRef xml = gEnv->pSystem->CreateXmlNode("schematyc", true);
		xml->setAttr("version", g_docPatcher.GetCurrentVersion());
		XmlNodeRef xmlElements = gEnv->pSystem->CreateXmlNode("elements", true);
		xml->addChild(xmlElements);

		if(guid) // #SchematycTODO : Should guid in fact be scopeGUID?
		{
			const IScriptElement* pElement = GetElement(guid);
			if(pElement)
			{
				CopyElementsToXmlRecursive(*pElement, xmlElements, bRecurseHierarchy);
			}
		}
		else if(bRecurseHierarchy)
		{
			for(const Elements::value_type& element : m_elements)
			{
				if(element.second->GetScopeGUID() == guid) // #SchematycTODO : Iterate through element's children rather then iterating through all elements and checking scope!!!
				{
					CopyElementsToXmlRecursive(*element.second, xmlElements, true);
				}
			}
		}
		
		return xml;
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::PasteElementsFromXml(const SGUID& scopeGUID, const XmlNodeRef& xml)
	{
		if(gEnv->IsEditor())
		{
			if(xml->isTag("schematyc"))
			{
				uint32 version = 0;
				xml->getAttr("version", version);
				const uint32 currentVersion = g_docPatcher.GetCurrentVersion();
				if(version == currentVersion)
				{
					CreateElementsFromXml(scopeGUID, xml, EElementOrigin::Clipboard);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::Load()
	{
		LOADING_TIME_PROFILE_SECTION_ARGS(m_fileName.c_str());

		if(!m_fileName.empty())
		{
			SCHEMATYC2_SYSTEM_COMMENT("Loading script file : %s [%s]", m_fileName.c_str(), (m_flags & EScriptFileFlags::OnDisk) != 0 ? "Disk" : "Pak");

			XmlNodeRef xml = gEnv->pSystem->LoadXmlFromFile(m_fileName.c_str());
			if(xml)
			{
				xml = g_docPatcher.PatchDoc(xml, m_fileName.c_str(), (m_flags & EScriptFileFlags::OnDisk) == 0);
				if(xml && xml->isTag("schematyc"))
				{
					const SFileVersion currentBuildVersion = gEnv->pSystem->GetBuildVersion();
					const SFileVersion productionVersion = gEnv->pSystem->GetProductVersion();
					const SFileVersion buildVersion(xml->getAttr("buildVersion"));
					if(!(currentBuildVersion == productionVersion) && (buildVersion > currentBuildVersion))
					{
						SCHEMATYC2_SYSTEM_WARNING("Script file was previously saved in a newer build");
					}

					m_guid.sysGUID = StringUtils::SysGUIDFromString(xml->getAttr("guid"));

					CreateElementsFromXml(SGUID(), xml, EElementOrigin::File);
				}
			}
			else
			{
				SCHEMATYC2_SYSTEM_CRITICAL_ERROR("Failed to parse script file, see log for details : %s", m_fileName.c_str());
			}

			SCHEMATYC2_SYSTEM_COMMENT("Loading complete");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::Save()
	{
		XmlNodeRef xml = GetISystem()->CreateXmlNode("schematyc", true);
		xml->setAttr("version", g_docPatcher.GetCurrentVersion());
		{
			char buildVersion[256] = "";
			gEnv->pSystem->GetBuildVersion().ToString(buildVersion);
			xml->setAttr("buildVersion", buildVersion);
		}
		{
			char printBuffer[StringUtils::s_guidStringBufferSize] = "";
			StringUtils::SysGUIDToString(m_guid.sysGUID, printBuffer);
			xml->setAttr("guid", printBuffer);
		}
		xml->addChild(SaveElements(EScriptElementType::Include, "includes"));
		xml->addChild(SaveElements(EScriptElementType::Group, "groups"));
		xml->addChild(SaveElements(EScriptElementType::Enumeration, "enumerations"));
		xml->addChild(SaveElements(EScriptElementType::Structure, "structures"));
		xml->addChild(SaveElements(EScriptElementType::Signal, "signals"));
		xml->addChild(SaveElements(EScriptElementType::AbstractInterface, "abstract_interfaces"));
		xml->addChild(SaveElements(EScriptElementType::AbstractInterfaceFunction, "abstract_interface_functions"));
		xml->addChild(SaveElements(EScriptElementType::AbstractInterfaceTask, "abstract_interface_tasks"));
		xml->addChild(SaveElements(EScriptElementType::Class, "schemas"));
		xml->addChild(SaveElements(EScriptElementType::ClassBase, "schema_bases"));
		xml->addChild(SaveElements(EScriptElementType::StateMachine, "state_machines"));
		xml->addChild(SaveElements(EScriptElementType::State, "states"));
		xml->addChild(SaveElements(EScriptElementType::Variable, "variables"));
		xml->addChild(SaveElements(EScriptElementType::Property, "properties"));
		xml->addChild(SaveElements(EScriptElementType::Container, "containers"));
		xml->addChild(SaveElements(EScriptElementType::Timer, "timers"));
		xml->addChild(SaveElements(EScriptElementType::AbstractInterfaceImplementation, "abstract_interface_implementations"));
		xml->addChild(SaveElements(EScriptElementType::ComponentInstance, "component_instances"));
		xml->addChild(SaveElements(EScriptElementType::ActionInstance, "action_instances"));
		xml->addChild(SaveGraphs());
		if(xml->saveToFile(m_fileName.c_str()))
		{
			SCHEMATYC2_SYSTEM_COMMENT("Saving Script File : %s", m_fileName.c_str());
			m_flags &= ~EScriptFileFlags::Modified;
		}
		else
		{
			SCHEMATYC2_SYSTEM_ERROR("Failed To Save Script File : %s", m_fileName.c_str());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION_ARGS(m_fileName.c_str());
		for(Elements::value_type& element : m_elements)
		{
			element.second->Refresh(params);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CScriptFile::GetClipboardInfo(const XmlNodeRef& xml, SScriptElementClipboardInfo& clipboardInfo) const
	{
		if(xml)
		{
			const char*              szElementTypeName = xml->getTag();
			const EScriptElementType elementType = GetElementType(szElementTypeName);
			if(elementType != EScriptElementType::None)
			{
				clipboardInfo.elementType = elementType;
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	CScriptFile::SNewElement::SNewElement(const IScriptElementPtr& _pElement, const XmlNodeRef& _xml)
		: pElement(_pElement)
		, xml(_xml)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::AttachElement(IScriptElement& element, const SGUID& scopeGUID)
	{
		if(scopeGUID)
		{
			IScriptElement* pParantElement = GetElement(scopeGUID);
			if(pParantElement)
			{
				pParantElement->AttachChild(element);
			}
			else
			{
				SCHEMATYC2_SYSTEM_WARNING("Failed to find parent element!");
			}
		}
		else
		{
			m_pRoot->AttachChild(element);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	template <typename TYPE> TYPE* CScriptFile::GetElement(EScriptElementType type, const SGUID& guid)
	{
		Elements::iterator itElement = m_elements.find(guid);
		return (itElement != m_elements.end()) && (itElement->second->GetElementType() == type) ? static_cast<TYPE*>(itElement->second.get()) : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	template <typename TYPE> const TYPE* CScriptFile::GetElement(EScriptElementType type, const SGUID& guid) const
	{
		Elements::const_iterator itElement = m_elements.find(guid);
		return (itElement != m_elements.end()) && (itElement->second->GetElementType() == type) ? static_cast<const TYPE*>(itElement->second.get()) : nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitElements(const ScriptElementVisitor& visitor, IScriptElement& parentElement, bool bRecurseHierarchy)
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(IScriptElement* pElement = parentElement.GetFirstChild(); pElement; pElement = pElement->GetNextSibling())
			{
				const EVisitStatus visitStatus = visitor(*pElement);
				if(visitStatus != EVisitStatus::Continue)
				{
					return visitStatus;
				}
				if(bRecurseHierarchy)
				{
					const EVisitStatus recurseStatus = VisitElements(visitor, *pElement, true);
					if(recurseStatus != EVisitStatus::End)
					{
						return recurseStatus;
					}
				}
			}
		}
		return EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	template <typename TYPE> EVisitStatus CScriptFile::VisitElements(EScriptElementType type, const TemplateUtils::CDelegate<EVisitStatus (TYPE&)>& visitor)
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(const Elements::value_type& element : m_elements)
			{
				IScriptElement* pElement = element.second.get();
				if(pElement->GetElementType() == type)
				{
					const EVisitStatus visitStatus = visitor(*static_cast<TYPE*>(pElement));
					if(visitStatus != EVisitStatus::Continue)
					{
						return visitStatus;
					}
				}
			}
		}
		return EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	template <typename TYPE> EVisitStatus CScriptFile::VisitElements(EScriptElementType type, const TemplateUtils::CDelegate<EVisitStatus (TYPE&)>& visitor, IScriptElement& parentElement, bool bRecurseHierarchy)
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(IScriptElement* pElement = parentElement.GetFirstChild(); pElement; pElement = pElement->GetNextSibling())
			{
				if(pElement->GetElementType() == type)
				{
					const EVisitStatus visitStatus = visitor(*static_cast<TYPE*>(pElement));
					if(visitStatus != EVisitStatus::Continue)
					{
						return visitStatus;
					}
				}
				if(bRecurseHierarchy)
				{
					const EVisitStatus recurseStatus = VisitElements<TYPE>(type, visitor, *pElement, true);
					if(recurseStatus != EVisitStatus::End)
					{
						return recurseStatus;
					}
				}
			}
		}
		return EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CScriptFile::VisitElements(const ScriptElementConstVisitor& visitor, const IScriptElement& parentElement, bool bRecurseHierarchy) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(const IScriptElement* pElement = parentElement.GetFirstChild(); pElement; pElement = pElement->GetNextSibling())
			{
				const EVisitStatus visitStatus = visitor(*pElement);
				if(visitStatus != EVisitStatus::Continue)
				{
					return visitStatus;
				}
				if(bRecurseHierarchy)
				{
					const EVisitStatus recurseStatus = VisitElements(visitor, *pElement, true);
					if(recurseStatus != EVisitStatus::End)
					{
						return recurseStatus;
					}
				}
			}
		}
		return EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	template <typename TYPE> EVisitStatus CScriptFile::VisitElements(EScriptElementType type, const TemplateUtils::CDelegate<EVisitStatus (const TYPE&)>& visitor) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(const Elements::value_type& element : m_elements)
			{
				const IScriptElement* pElement = element.second.get();
				if(pElement->GetElementType() == type)
				{
					const EVisitStatus visitStatus = visitor(*static_cast<const TYPE*>(pElement));
					if(visitStatus != EVisitStatus::Continue)
					{
						return visitStatus;
					}
				}
			}
		}
		return EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	template <typename TYPE> EVisitStatus CScriptFile::VisitElements(EScriptElementType type, const TemplateUtils::CDelegate<EVisitStatus (const TYPE&)>& visitor, const IScriptElement& parentElement, bool bRecurseHierarchy) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(visitor);
		if(visitor)
		{
			for(const IScriptElement* pElement = parentElement.GetFirstChild(); pElement; pElement = pElement->GetNextSibling())
			{
				if(pElement->GetElementType() == type)
				{
					const EVisitStatus visitStatus = visitor(*static_cast<const TYPE*>(pElement));
					if(visitStatus != EVisitStatus::Continue)
					{
						return visitStatus;
					}
				}
				if(bRecurseHierarchy)
				{
					const EVisitStatus recurseStatus = VisitElements<TYPE>(type, visitor, *pElement, true);
					if(recurseStatus != EVisitStatus::End)
					{
						return recurseStatus;
					}
				}
			}
		}
		return EVisitStatus::End;
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::CopyElementsToXmlRecursive(const IScriptElement& element, const XmlNodeRef& xmlElements, bool bRecurseHierarchy) const
	{
		XmlNodeRef xmlElement;
		if(element.GetElementType() == EScriptElementType::Graph)
		{
			IDocGraph&                graph = static_cast<IDocGraph&>(const_cast<IScriptElement&>(element));
			DocUtils::SDocGraphHeader graphHeader(graph.GetGUID(), graph.GetScopeGUID(), graph.GetName(), graph.GetType(), graph.GetContextGUID());
			
			xmlElement = gEnv->pSystem->CreateXmlNode("Element", true);
			XmlNodeRef xmlElementType = gEnv->pSystem->CreateXmlNode("elementType", true);
			xmlElementType->setAttr("value", "Graph");
			xmlElement->addChild(xmlElementType);

			xmlElement->addChild(Serialization::SaveXmlNode(graphHeader, "header"));
			xmlElement->addChild(Serialization::SaveXmlNode(graph, "detail"));
		}
		else
		{
			xmlElement = Serialization::SaveXmlNode(element, "Element");
		}
		if(xmlElement)
		{
			xmlElements->addChild(xmlElement);
			if(bRecurseHierarchy)
			{
				const SGUID& elementGUID = element.GetGUID();
				for(const Elements::value_type& element : m_elements)
				{
					if(element.second->GetScopeGUID() == elementGUID) // #SchematycTODO : Iterate through element's children rather then iterating through all elements and checking scope!!!
					{
						CopyElementsToXmlRecursive(*element.second, xmlElements, true);
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::CreateElementsFromXml(const SGUID& scopeGUID, const XmlNodeRef& xml, EElementOrigin origin)
	{
		IScriptElement* pParentElement = scopeGUID ? GetElement(scopeGUID) : m_pRoot.get();
		if(pParentElement)
		{
			// Load elements.
			DocSerializationUtils::CInputBlock inputBlock(*this);
			Serialization::LoadXmlNode(inputBlock, xml);

			DocSerializationUtils::CInputBlock::Elements& elements = inputBlock.GetElements();
			if(!elements.empty())
			{
				inputBlock.BuildElementHierarchy(*pParentElement);

				if(origin == EElementOrigin::Clipboard)
				{
					// Rename elements in root scope.
					for(DocSerializationUtils::CInputBlock::SElement& element : elements)
					{
						if(element.ptr->GetParent() == pParentElement)
						{
							stack_string elementName = element.ptr->GetName();
							if(!IsElementNameUnique(scopeGUID, elementName.c_str()))
							{
								elementName.append("_Copy");
								MakeElementNameUnique(scopeGUID, elementName);
								element.ptr->SetName(elementName.c_str());
							}
						}
					}
					// Re-map element GUIDs.
					CGUIDRemapper guidRemapper;
					for(DocSerializationUtils::CInputBlock::SElement& element : elements)
					{
						guidRemapper.Bind(element.ptr->GetGUID(), gEnv->pSchematyc2->CreateGUID());
					}
					for(DocSerializationUtils::CInputBlock::SElement& element : elements)
					{
						element.ptr->RemapGUIDs(guidRemapper);
					}
				}

				inputBlock.SortElementsByDependency();

				// Copy elements to file.
				for(DocSerializationUtils::CInputBlock::SElement& element : elements)
				{
					Serialization::LoadBlackBox(element, element.blackBox);
					m_elements.insert(Elements::value_type(element.ptr->GetGUID(), element.ptr));
				}
				for(DocSerializationUtils::CInputBlock::SElement& element : elements)
				{
					Serialization::LoadBlackBox(element, element.blackBox);
				}
				// Validate elements.
				CValidatorArchive     validatorArchive(EValidatorArchiveFlags::ForwardWarningsToLog | EValidatorArchiveFlags::ForwardErrorsToLog);
				CSerializationContext serializationContext(SSerializationContextParams(validatorArchive, this, ESerializationPass::Validate));
				for(DocSerializationUtils::CInputBlock::SElement& element : elements)
				{
					element.ptr->Serialize(validatorArchive);
				}

				if(origin == EElementOrigin::Clipboard)
				{
					// Refresh all elements and mark file as modified.
					Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorPaste));
					SetFlags(GetFlags() | EScriptFileFlags::Modified);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::GatherElementsToSave(EScriptElementType type, ElementsToSave& elementsToSave)
	{
		for(const Elements::value_type& element : m_elements)
		{
			const IScriptElementPtr& pElement = element.second;
			if(pElement->GetElementType() == type)
			{
				if ((CVars::sc_DiscardOnSave == 0) || ((pElement->GetElementFlags() & EScriptElementFlags::Discard) == 0))
				{
					elementsToSave[string(pElement->GetName())].insert( ElementsScopeGuidMap::value_type(pElement->GetScopeGUID(), pElement.get()) );
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef CScriptFile::SaveElements(EScriptElementType type, const char* szTag)
	{
		ElementsToSave elementsToSave;
		GatherElementsToSave(type, elementsToSave);

		XmlNodeRef  xmlElements = GetISystem()->CreateXmlNode(szTag, true);
		const char* szElementTypeName = GetElementTypeName(type);
		for(ElementsToSave::value_type& element : elementsToSave)
		{
			for (ElementsScopeGuidMap::value_type& elementSortedByScope : element.second)
			{
				xmlElements->addChild(Serialization::SaveXmlNode(*elementSortedByScope.second, szElementTypeName));
			}
		}
		return xmlElements;
	}

	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef CScriptFile::SaveGraphs()
	{
		ElementsToSave elementsToSave;
		GatherElementsToSave(EScriptElementType::Graph, elementsToSave);

		XmlNodeRef xmlGraphs = GetISystem()->CreateXmlNode("graphs", true);
		for(ElementsToSave::value_type& element : elementsToSave)
		{
			for (ElementsScopeGuidMap::value_type& elementSortedByScope : element.second)
			{
				XmlNodeRef xmlGraph = GetISystem()->CreateXmlNode("Element", true);
				xmlGraphs->addChild(xmlGraph);
				IDocGraph&                graph = *static_cast<IDocGraph*>(elementSortedByScope.second);
				DocUtils::SDocGraphHeader graphHeader(graph.GetGUID(), graph.GetScopeGUID(), graph.GetName(), graph.GetType(), graph.GetContextGUID());
				XmlNodeRef                xmlGraphHeader = Serialization::SaveXmlNode(graphHeader, "header");
				XmlNodeRef                xmlGraphDetail = Serialization::SaveXmlNode(graph, "detail");
				xmlGraph->addChild(xmlGraphHeader);
				xmlGraph->addChild(xmlGraphDetail);
			}
		}
		return xmlGraphs;
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::MakeElementNameUnique(const SGUID& scopeGUID, stack_string& name) const
	{
		if(!IsElementNameUnique(scopeGUID, name.c_str()))
		{
			const size_t length = name.length();
			char         stringBuffer[StringUtils::s_sizeTStringBufferSize] = "";
			size_t       postfix = 1;
			do
			{
				name.resize(length);
				name.append(StringUtils::SizeTToString(postfix, stringBuffer));
				++ postfix;
			} while(!IsElementNameUnique(scopeGUID, name.c_str()));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptFile::GetElementTypeName(EScriptElementType elementType) const
	{
		for(size_t elementTypeIdx = 0; elementTypeIdx < CRY_ARRAY_COUNT(DocUtils::ELEMENT_TYPES); ++ elementTypeIdx)
		{
			if(DocUtils::ELEMENT_TYPES[elementTypeIdx].elementType == elementType)
			{
				return DocUtils::ELEMENT_TYPES[elementTypeIdx].name.c_str();
			}
		}
		return "";
	}

	//////////////////////////////////////////////////////////////////////////
	EScriptElementType CScriptFile::GetElementType(const char* szElementTypeName) const
	{
		for(size_t elementTypeIdx = 0; elementTypeIdx < CRY_ARRAY_COUNT(DocUtils::ELEMENT_TYPES); ++ elementTypeIdx)
		{
			if(strcmp(DocUtils::ELEMENT_TYPES[elementTypeIdx].name.c_str(), szElementTypeName) == 0)
			{
				return DocUtils::ELEMENT_TYPES[elementTypeIdx].elementType;
			}
		}
		return EScriptElementType::None;
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptFile::ClearScope(const SGUID& scopeGUID)
	{
		GUIDVector childScopeGUIDs;
		childScopeGUIDs.reserve(256);
		for(Elements::iterator itNextElement = m_elements.begin(), itEndElement = m_elements.end(); itNextElement != itEndElement; )
		{
			Elements::iterator  itElement = itNextElement ++;
			const IScriptElementPtr& pElement = itElement->second;
			if(pElement->GetScopeGUID() == scopeGUID)
			{
				childScopeGUIDs.push_back(pElement->GetGUID());
				m_elements.erase(itElement);
			}
		}
		for(GUIDVector::const_iterator itChildScopeGUID = childScopeGUIDs.begin(), itEndChildScopeGUID = childScopeGUIDs.end(); itChildScopeGUID != itEndChildScopeGUID; ++ itChildScopeGUID)
		{
			ClearScope(*itChildScopeGUID);
		}
	}
}

// #SchematycTODO : Remove!!!
SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, EScriptGraphNodeType, "Schematyc Script Graph Node Type")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Unknown, "unknown", "Unknown")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Begin, "begin", "Begin")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::BeginConstructor, "begin_constructor", "Begin Constructor")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::BeginDestructor, "begin_destructor", "Begin Destructor")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::BeginSignalReceiver, "begin_signal_receiver", "Begin Signal Receiver")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Return, "return", "Return")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Sequence, "sequence", "Sequence")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Branch, "branch", "Branch")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Switch, "switch", "Switch")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::MakeEnumeration, "make_enumeration", "Make Enumeration")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ForLoop, "for_loop", "For Loop")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ProcessSignal, "process_signal", "Process Signal")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::SendSignal, "send_signal", "Send Signal")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::BroadcastSignal, "broadcast_signal", "Broadcast Signal")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Function, "call_function", "Function") 
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Condition, "call condition", "Condition")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::AbstractInterfaceFunction, "call_abstract_interface_function", "Abstract Interface Function")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::GetObject, "get_object", "Get Object")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Set, "set", "Set")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Get, "get", "Get")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ContainerAdd, "container_add", "Container Add")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ContainerRemoveByIndex, "container_remove_by_index", "Container Remove By Index")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ContainerRemoveByValue, "container_remove_by_value", "Container Remove By Value")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ContainerClear, "container_clear", "Container Clear")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ContainerSize, "container_size", "Container Size")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ContainerGet, "container_get", "Container Get")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ContainerSet, "container_set", "Container Set")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ContainerFindByValue, "container_find_by_value", "Container Find By Value")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::StartTimer, "start_timer", "Start Timer")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::StopTimer, "stop_timer", "Stop Timer")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::ResetTimer, "reset_timer", "Reset Timer")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::State, "state", "State")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::BeginState, "begin_state", "Begin State")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::EndState, "end_state", "End State")
	SERIALIZATION_ENUM(Schematyc2::EScriptGraphNodeType::Comment, "comment", "Comment")
SERIALIZATION_ENUM_END()
