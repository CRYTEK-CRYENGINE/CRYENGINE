// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"
#include "CrySchematyc2/Serialization/Resources/IResourceCollectorArchive.h"

#include <CrySerialization/Decorators/ResourceFilePath.h>
#include <CrySerialization/Decorators/Resources.h>

namespace GameSerialization
{
	// Note: Do not use this directly (use the types below instead)
	struct SGameResource
	{
	private:
		friend struct SGameResourceFile;
		friend struct SGameResourceFileWithType;
		template <typename TString> friend struct SGameResourceSelector;

	private:
		SGameResource(const char* _szValue, const char* _szType)
			: szValue(_szValue)
			, szType(_szType)
		{

		}
		SGameResource() = delete;

	public:

		void Serialize(Serialization::IArchive& archive) {}; // Do nothing

		const char* szValue;
		const char* szType;
	};

	//////////////////////////////////////////////////////////////////////////

	struct SGameResourceFile
	{
		SGameResourceFile(string& path, const char* szFilter = "All files|*.*", const char* szStartFolder = "", int flags = 0)
			: filePath(path, szFilter, szStartFolder, flags)
		{

		}

		SGameResource ToGameResource() const
		{
			return SGameResource(filePath.path->c_str(), "Any");
		}

		Serialization::ResourceFilePath filePath;
	};

	inline bool Serialize(Serialization::IArchive& archive, SGameResourceFile& value, const char* szName, const char* szLabel)
	{
		if (archive.caps(Schematyc2::IResourceCollectorArchive::ArchiveCaps))
			return archive(value.ToGameResource(), szName, szLabel);
		else
			return archive(value.filePath, szName, szLabel);
	}

	//////////////////////////////////////////////////////////////////////////

	struct SGameResourceFileWithType
	{
		SGameResourceFileWithType(string& path, const char* szResourceType, const char* szFilter = "All files|*.*", const char* szStartFolder = "", int flags = 0)
			: filePath(path, szFilter, szStartFolder, flags)
			, szResourceType(szResourceType)
		{

		}

		SGameResource ToGameResource() const
		{
			return SGameResource(filePath.path->c_str(), szResourceType);
		}

		Serialization::ResourceFilePath filePath;
		const char* szResourceType;
	};

	inline bool Serialize(Serialization::IArchive& archive, SGameResourceFileWithType& value, const char* szName, const char* szLabel)
	{
		if (archive.caps(Schematyc2::IResourceCollectorArchive::ArchiveCaps))
			return archive(value.ToGameResource(), szName, szLabel);
		else
			return archive(value.filePath, szName, szLabel);
	}

	//////////////////////////////////////////////////////////////////////////

	template<typename TString>
	struct SGameResourceSelector
	{
		SGameResourceSelector(TString& value, const char* szResourceType)
			: selector(value, szResourceType)
		{

		}

		SGameResourceSelector(Serialization::ResourceSelector<TString>& src)
			: selector(src)
		{

		}

		SGameResource ToGameResource() const
		{
			return SGameResource(selector.GetValue(), selector.resourceType);
		}

		Serialization::ResourceSelector<TString> selector;
	};

	template<typename TString>
	bool Serialize(Serialization::IArchive& archive, SGameResourceSelector<TString>& value, const char* szName, const char* szLabel)
	{
		if (archive.caps(Schematyc2::IResourceCollectorArchive::ArchiveCaps))
			return archive(value.ToGameResource(), szName, szLabel);
		else
			return archive(value.selector, szName, szLabel);
	}
}
