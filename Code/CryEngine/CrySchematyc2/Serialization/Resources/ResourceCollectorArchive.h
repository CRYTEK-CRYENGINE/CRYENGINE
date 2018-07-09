// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Serialization/Resources/IResourceCollectorArchive.h>

namespace GameSerialization
{
	struct SGameResource;
}

namespace Schematyc2
{
	class CResourceCollectorArchive : public IResourceCollectorArchive
	{
	public:

		CResourceCollectorArchive(IGameResourceListPtr pResourceList);

	protected:

		using IResourceCollectorArchive::operator ();

		// Serialization::IArchive
		virtual bool operator () (bool& value, const char* szName = "", const char* szLabel = nullptr) override;
		virtual bool operator () (int8& value, const char* szName = "", const char* szLabel = nullptr) override;
		virtual bool operator () (uint8& value, const char* szName = "", const char* szLabel = nullptr) override;
		virtual bool operator () (int32& value, const char* szName = "", const char* szLabel = nullptr) override;
		virtual bool operator () (uint32& value, const char* szName = "", const char* szLabel = nullptr) override;
		virtual bool operator () (int64& value, const char* szName = "", const char* szLabel = nullptr) override;
		virtual bool operator () (uint64& value, const char* szName = "", const char* szLabel = nullptr) override;
		virtual bool operator () (float& value, const char* szName = "", const char* szLabel = nullptr) override;
		virtual bool operator () (Serialization::IString& value, const char* szName = "", const char* szLabel = nullptr) override;
		virtual bool operator () (const Serialization::SStruct& value, const char* szName = "", const char* szLabel = nullptr) override;
		virtual bool operator () (Serialization::IContainer& value, const char* szName = "", const char* szLabel = nullptr) override;

		virtual void validatorMessage(bool bError, const void* handle, const Serialization::TypeID& type, const char* szMessage) override;
		// ~Serialization::IArchive

		void ExtractResource(const GameSerialization::SGameResource* pResource);
		void ExtractResource(const char* szResourcePath);

	private:
		IGameResourceListPtr m_pResourceList;
	};
}

