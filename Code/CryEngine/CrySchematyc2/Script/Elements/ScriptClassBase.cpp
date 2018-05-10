// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptClassBase.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>
#include <CrySchematyc2/Serialization/SerializationUtils.h>

namespace Schematyc2
{
	typedef std::unordered_map<SGUID, IScriptProperty*>       PropertyMap;
	typedef std::unordered_map<SGUID, const IScriptProperty*> BasePropertyMap;
	typedef std::vector<const IScriptGroup*>                  Groups;

	namespace ScriptClassBaseUtils
	{
		struct SPropertyCollector
		{
			SPropertyCollector(PropertyMap& _properties, IScriptFile& _file, const SGUID& _scopeGUID)
				: properties(_properties)
				, file(_file)
				, scopeGUID(_scopeGUID)
			{
				file.VisitElements(ScriptElementVisitor::FromMemberFunction<SPropertyCollector, &SPropertyCollector::VisitElement>(*this), scopeGUID, false);
			}
			
			EVisitStatus VisitElement(IScriptElement& element)
			{
				switch(element.GetElementType())
				{
				case EScriptElementType::Group:
					{
						file.VisitElements(ScriptElementVisitor::FromMemberFunction<SPropertyCollector, &SPropertyCollector::VisitElement>(*this), element.GetGUID(), false);
						break;
					}
				case EScriptElementType::Property:
					{
						IScriptProperty& property = static_cast<IScriptProperty&>(element);
						properties.insert(PropertyMap::value_type(property.GetRefGUID(), &property));
						break;
					}
				}
				return EVisitStatus::Continue;
			}

			PropertyMap& properties;
			IScriptFile& file;
			SGUID        scopeGUID;
		};

		struct SBasePropertyCollector
		{
			SBasePropertyCollector(BasePropertyMap& _properties, const IScriptFile& _file, const SGUID& _scopeGUID)
				: properties(_properties)
				, file(_file)
				, scopeGUID(_scopeGUID)
			{
				file.VisitElements(ScriptElementConstVisitor::FromMemberFunction<SBasePropertyCollector, &SBasePropertyCollector::VisitElement>(*this), scopeGUID, false);
			}

			EVisitStatus VisitElement(const IScriptElement& element)
			{
				switch(element.GetElementType())
				{
				case EScriptElementType::Group:
					{
						file.VisitElements(ScriptElementConstVisitor::FromMemberFunction<SBasePropertyCollector, &SBasePropertyCollector::VisitElement>(*this), element.GetGUID(), false);
						break;
					}
				case EScriptElementType::Property:
					{
						const IScriptProperty& property = static_cast<const IScriptProperty&>(element);
						properties.insert(BasePropertyMap::value_type(property.GetGUID(), &property));
						break;
					}
				}
				return EVisitStatus::Continue;
			}

			BasePropertyMap&   properties;
			const IScriptFile& file;
			SGUID              scopeGUID;
		};

		inline const IScriptGroup* FindGroup(IScriptFile& file, const SGUID& scopeGUID, const char* szName)
		{
			SCHEMATYC2_SYSTEM_ASSERT(szName);
			const IScriptGroup* pResult = nullptr;
			auto visitGroup = [szName, &pResult] (const IScriptGroup& group) -> EVisitStatus
			{
				if(strcmp(szName, group.GetName()) == 0)
				{
					pResult = &group;
					return EVisitStatus::End;
				}
				return EVisitStatus::Continue;
			};
			file.VisitGroups(ScriptGroupConstVisitor::FromLambdaFunction(visitGroup), scopeGUID, false);
			return pResult;
		}
	}

	CScriptClassBase::CScriptClassBase(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const SGUID& refGUID)
		: CScriptElementBase(EScriptElementType::ClassBase, file)
		, m_guid(guid)
		, m_scopeGUID(scopeGUID)
		, m_refGUID(refGUID)
	{}

	EAccessor CScriptClassBase::GetAccessor() const
	{
		return EAccessor::Private;
	}

	SGUID CScriptClassBase::GetGUID() const
	{
		return m_guid;
	}

	SGUID CScriptClassBase::GetScopeGUID() const
	{
		return m_scopeGUID;
	}

	bool CScriptClassBase::SetName(const char* szName)
	{
		return false;
	}

	const char* CScriptClassBase::GetName() const
	{
		return m_name.c_str();
	}

	void CScriptClassBase::EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(enumerator);
		if(enumerator)
		{
			enumerator(m_refGUID);
		}
	}

	void CScriptClassBase::Refresh(const SScriptRefreshParams& params)
	{
		ScriptIncludeRecursionUtils::GetClassResult baseClass = ScriptIncludeRecursionUtils::GetClass(CScriptElementBase::GetFile(), m_refGUID);
		if(baseClass.second)
		{
			m_name = baseClass.second->GetName();
			if((params.reason == EScriptRefreshReason::EditorAdd) || (params.reason == EScriptRefreshReason::EditorFixUp) || (params.reason == EScriptRefreshReason::EditorPaste) || (params.reason == EScriptRefreshReason::EditorDependencyModified))
			{
				RefreshProperties(*baseClass.first, *baseClass.second);
			}
		}
	}

	void CScriptClassBase::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CScriptElementBase::Serialize(archive);

		SerializationContext::SetValidatorLink(archive, SValidatorLink(m_guid)); // #SchematycTODO : Can we set this from CScriptElementBase?
		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
		case ESerializationPass::Save:
			{
				Serialization::SContext fileContext(archive, static_cast<IScriptFile*>(&CScriptElementBase::GetFile())); // #SchematycTODO : Do we still need this?
				Serialization::SContext context(archive, static_cast<IScriptClassBase*>(this)); // #SchematycTODO : Do we still need this?

				archive(m_guid, "guid");
				archive(m_scopeGUID, "scopeGUID");
				archive(m_refGUID, "refGUID");
				archive(m_userDocumentation, "userDocumentation", "Documentation");
				break;
			}
		case ESerializationPass::Validate:
			{
				Validate(archive);
				break;
			}
		}
	}

	void CScriptClassBase::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		m_guid      = guidRemapper.Remap(m_guid);
		m_scopeGUID = guidRemapper.Remap(m_scopeGUID);
		m_refGUID   = guidRemapper.Remap(m_refGUID);
	}

	SGUID CScriptClassBase::GetRefGUID() const
	{
		return m_refGUID;
	}

	void CScriptClassBase::RefreshProperties(const IScriptFile& baseClassFile, const IScriptClass& baseClass)
	{
		// Retrieve derived properties and properties from base class.
		IScriptFile& file = CScriptElementBase::GetFile();
		PropertyMap  properties;
		ScriptClassBaseUtils::SPropertyCollector(properties, file, m_guid);
		BasePropertyMap baseProperties;
		ScriptClassBaseUtils::SBasePropertyCollector(baseProperties, baseClassFile, m_refGUID);
		// Remove all derived properties that are either finalized of no longer part of base class.
		for(PropertyMap::value_type& property : properties)
		{
			BasePropertyMap::iterator itBaseProperty = baseProperties.find(property.first);
			if((itBaseProperty == baseProperties.end()) || (itBaseProperty->second->GetOverridePolicy() == EOverridePolicy::Finalize))
			{
				file.RemoveElement(property.second->GetGUID(), true);
			}
		}
		// Create/update derived properties. 
		for(const BasePropertyMap::value_type& baseProperty : baseProperties)
		{
			if(baseProperty.second->GetOverridePolicy() != EOverridePolicy::Finalize)
			{
				PropertyMap::iterator itProperty = properties.find(baseProperty.first);
				if(itProperty != properties.end())
				{
					// Update derived property name.
					itProperty->second->SetName(baseProperty.second->GetName());
				}
				else
				{
					// Retrieve group hierarchy from base class.
					Groups baseGroups;
					SGUID  baseScopeGUID = baseProperty.second->GetScopeGUID();
					while(baseScopeGUID != m_refGUID)
					{
						const IScriptGroup* pBaseGroup = baseClassFile.GetGroup(baseScopeGUID);
						SCHEMATYC2_SYSTEM_ASSERT(pBaseGroup);
						if(pBaseGroup)
						{
							baseGroups.push_back(pBaseGroup);
							baseScopeGUID = pBaseGroup->GetScopeGUID();
						}
						else
						{
							break;
						}
					}
					std::reverse(baseGroups.begin(), baseGroups.end());
					// Clone group hierarchy.
					SGUID	scopeGUID = m_guid;
					for(const IScriptGroup* pBaseGroup : baseGroups)
					{
						const char*         szGroupName = pBaseGroup->GetName();
						const IScriptGroup* pGroup = ScriptClassBaseUtils::FindGroup(file, scopeGUID, szGroupName);
						if(!pGroup)
						{
							pGroup = file.AddGroup(scopeGUID, szGroupName);
						}
						SCHEMATYC2_SYSTEM_ASSERT(pGroup);
						if(pGroup)
						{
							scopeGUID = pGroup->GetGUID();
						}
					}
					// Create new derived property. 
					file.AddProperty(scopeGUID, baseProperty.second->GetName(), baseProperty.first, baseProperty.second->GetTypeId());
				}
			}
		}
	}

	void CScriptClassBase::Validate(Serialization::IArchive& archive)
	{
		if(m_refGUID)
		{
			const IScriptClass* pBaseClass = ScriptIncludeRecursionUtils::GetClass(CScriptElementBase::GetFile(), m_refGUID).second;
			if(!pBaseClass)
			{
				archive.error(*this, "Unable to retrieve base class!");
			}
		}
	}
}
