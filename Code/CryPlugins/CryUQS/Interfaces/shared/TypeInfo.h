// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Shared
	{

		//===================================================================================
		//
		// CTypeInfo
		//
		//===================================================================================

		class CTypeInfo
		{
			template <class T>
			friend struct SDataTypeHelper;	// calls CTypeInfo::Get<T>()

		public:
			const char*                  name() const;
			bool                         operator==(const CTypeInfo& rhs) const;
			bool                         operator!=(const CTypeInfo& rhs) const;
			bool                         operator<(const CTypeInfo& rhs) const;
#if UQS_SCHEMATYC_SUPPORT
			const Schematyc::CTypeName&  GetSchematycTypeName() const;
#endif

		private:

#if UQS_SCHEMATYC_SUPPORT
			explicit                     CTypeInfo(const yasli::TypeID& yasliTypeID, const Schematyc::CTypeName& schematycTypeName);
#else
			explicit                     CTypeInfo(const yasli::TypeID& yasliTypeID);
#endif

			template <class T>
			static const CTypeInfo&      Get();

		private:
			const yasli::TypeID          m_yasliTypeID;
#if UQS_SCHEMATYC_SUPPORT
			const Schematyc::CTypeName&  m_schematycTypeName;
#endif
		};

#if UQS_SCHEMATYC_SUPPORT
		inline CTypeInfo::CTypeInfo(const yasli::TypeID& yasliTypeID, const Schematyc::CTypeName& schematycTypeName)
			: m_yasliTypeID(yasliTypeID)
			, m_schematycTypeName(schematycTypeName)
		{}
#else
		inline CTypeInfo::CTypeInfo(const yasli::TypeID& yasliTypeID)
			: m_yasliTypeID(yasliTypeID)
		{}
#endif

		inline const char* CTypeInfo::name() const
		{
			return m_yasliTypeID.name();
		}

		inline bool CTypeInfo::operator==(const CTypeInfo& rhs) const
		{
			return m_yasliTypeID == rhs.m_yasliTypeID;
		}

		inline bool CTypeInfo::operator!=(const CTypeInfo& rhs) const
		{
			return !operator==(rhs);
		}

		inline bool CTypeInfo::operator<(const CTypeInfo& rhs) const
		{
			return m_yasliTypeID < rhs.m_yasliTypeID;
		}

#if UQS_SCHEMATYC_SUPPORT
		inline const Schematyc::CTypeName& CTypeInfo::GetSchematycTypeName() const
		{
			return m_schematycTypeName;
		}
#endif

		template <class T>
		const CTypeInfo& CTypeInfo::Get()
		{
#if UQS_SCHEMATYC_SUPPORT
			static const CTypeInfo type(yasli::TypeID::get<T>(), Schematyc::GetTypeName<T>());
#else
			static const CTypeInfo type(yasli::TypeID::get<T>());
#endif

			return type;
		}

	}
}
