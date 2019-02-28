// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ResourceCollectorArchive.h"

#include <CrySchematyc2/Serialization/Resources/GameResources.h>

#include <CrySystem/File/ICryPak.h>
#include <CrySerialization/TypeID.h>

#include <CryEntitySystem/IEntitySystem.h>

namespace Schematyc2
{
	CResourceCollectorArchive::CResourceCollectorArchive(IGameResourceListPtr pResourceList)
		: m_pResourceList(pResourceList)
	{

	}

	bool CResourceCollectorArchive::operator()(bool& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		return true;
	}

	bool CResourceCollectorArchive::operator()(int8& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		return true;
	}

	bool CResourceCollectorArchive::operator()(uint8& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		return true;
	}

	bool CResourceCollectorArchive::operator()(int32& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		return true;
	}

	bool CResourceCollectorArchive::operator()(uint32& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		return true;
	}

	bool CResourceCollectorArchive::operator()(int64& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		return true;
	}

	bool CResourceCollectorArchive::operator()(uint64& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		return true;
	}

	bool CResourceCollectorArchive::operator()(float& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		return true;
	}

	bool CResourceCollectorArchive::operator()(Serialization::IString& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		return true;
	}

	bool CResourceCollectorArchive::operator()(const Serialization::SStruct& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		if (value.type() == Serialization::TypeID::get<GameSerialization::SGameResource>())
		{
			ExtractResource(static_cast<const GameSerialization::SGameResource*>(value.pointer()));
		}
		else
		{
			value(*this);
		}
		return true;
	}

	bool CResourceCollectorArchive::operator()(Serialization::IContainer& value, const char* szName /*= ""*/, const char* szLabel /*= nullptr*/)
	{
		if (value.size() > 0)
		{
			do
			{
				value(*this, szName, szLabel);
			} while (value.next());
		}
		return true;
	}

	void CResourceCollectorArchive::validatorMessage(bool bError, const void* handle, const Serialization::TypeID& type, const char* szMessage)
	{

	}

	void CResourceCollectorArchive::ExtractResource(const GameSerialization::SGameResource* pResource)
	{
		stack_string resourcePath = pResource->szValue;
		if (resourcePath.empty())
			return;

		CryFixedStringT<64> resourceType = pResource->szType;

		if ((resourceType.compareNoCase("Model") == 0) || (resourceType.compareNoCase("Character") == 0))
		{
			CryFixedStringT<16> extension = PathUtil::GetExt(resourcePath.c_str());

			if ((extension.compareNoCase("cdf") == 0) || (extension.compareNoCase("chr") == 0) || extension.compareNoCase("skin") == 0)
			{
				m_pResourceList->AddResource(resourcePath.c_str(), IGameResourceList::EType::Character);
			}
			else if (extension.compareNoCase("cgf") == 0)
			{
				m_pResourceList->AddResource(resourcePath.c_str(), IGameResourceList::EType::StatObject);
			}
		}
		else if (resourceType.compareNoCase("Texture") == 0)
		{
			m_pResourceList->AddResource(resourcePath.c_str(), IGameResourceList::EType::Texture);
		}
		else if (resourceType.compareNoCase("Material") == 0)
		{
			m_pResourceList->AddResource(resourcePath.c_str(), IGameResourceList::EType::Material);
		}
		else if (resourceType.compareNoCase("Particle") == 0)
		{
			m_pResourceList->AddResource(resourcePath.c_str(), IGameResourceList::EType::ParticleFX);
		}
		else if (resourceType.compareNoCase("SpawnableEntityClassName") == 0)
		{
			const IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(resourcePath.c_str());
			if (pClass)
			{
				if ((pClass->GetFlags() & ECLF_ENTITY_ARCHETYPE) != 0)
					m_pResourceList->AddResource(resourcePath.c_str(), IGameResourceList::EType::EntityArchetype);
				else
					m_pResourceList->AddResource(resourcePath.c_str(), IGameResourceList::EType::EntityClass);
			}
		}
		else if (resourceType.compareNoCase("BehaviorTree") == 0)
		{
			m_pResourceList->AddResource(resourcePath.c_str(), IGameResourceList::EType::BehaviorTree);
		}
		else if (resourceType.compareNoCase("LensFlare") == 0)
		{
			m_pResourceList->AddResource(resourcePath.c_str(), IGameResourceList::EType::LensFlare);
		}
		else if (resourceType.compareNoCase("MannequinControllerDefinition") == 0)
		{
			m_pResourceList->AddResource(resourcePath.c_str(), IGameResourceList::EType::MannequinControllerDefinition);
		}
		else
		{
			ExtractResource(resourcePath.c_str());
		}
	}

	void CResourceCollectorArchive::ExtractResource(const char* szResourcePath)
	{
		CryFixedStringT<16> extension = PathUtil::GetExt(szResourcePath);

		if ((extension.compareNoCase("cdf") == 0) || (extension.compareNoCase("chr") == 0))
		{
			m_pResourceList->AddResource(szResourcePath, IGameResourceList::EType::Character);
		}
		else if (!extension.compareNoCase("cgf"))
		{
			m_pResourceList->AddResource(szResourcePath, IGameResourceList::EType::StatObject);
		}
		else if ((extension.compareNoCase("dds") == 0) || (extension.compareNoCase("tif") == 0))
		{
			m_pResourceList->AddResource(szResourcePath, IGameResourceList::EType::Texture);
		}
		else if (extension.compareNoCase("mtl") == 0)
		{
			m_pResourceList->AddResource(szResourcePath, IGameResourceList::EType::Material);
		}
		else if (extension.compareNoCase("pfx") == 0)
		{
			m_pResourceList->AddResource(szResourcePath, IGameResourceList::EType::ParticleFX);
		}
		else if (extension.compareNoCase("adb") == 0)
		{
			m_pResourceList->AddResource(szResourcePath, IGameResourceList::EType::MannequinADB);
		}
	}
}
