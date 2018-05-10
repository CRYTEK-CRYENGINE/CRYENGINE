// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Serialization/IValidatorArchive.h>

namespace Schematyc2
{
	class CValidatorArchive : public IValidatorArchive
	{
	public:

		CValidatorArchive(const SValidatorArchiveParams& params);

		// IValidatorArchive

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

		using IValidatorArchive::operator ();

		virtual uint32 GetWarningCount() const override;
		virtual uint32 GetErrorCount() const override;

		// ~IValidatorArchive

	protected:

		// Serialization::IArchive
		virtual void validatorMessage(bool bError, const void* handle, const Serialization::TypeID& type, const char* szMessage) override;
		// ~Serialization::IArchive

	private:

		EValidatorArchiveFlags m_flags;
		uint32                 m_warningCount;
		uint32                 m_errorCount;
	};
}
