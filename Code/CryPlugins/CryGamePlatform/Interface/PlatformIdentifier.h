// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CryExtension/CryGUID.h>
#include <tuple>

namespace Cry
{
	namespace GamePlatform
	{
		// Forward declarations
		template <typename Traits>
		class Identifier;

		//! Simple opaque class used to store a service-specific identifier.
		template <typename Traits>
		class Identifier
		{
			using ValueType = typename Traits::ValueType;
			using ServiceType = typename Traits::ServiceType;
		public:
			Identifier(const ServiceType& svcId, const ValueType& accountId)
				: m_svcId(svcId)
				, m_value(accountId)
			{
			}

			Identifier() = default;

			Identifier(const Identifier&) = default;
			Identifier(Identifier&&) = default;

			Identifier& operator=(const Identifier&) = default;
			Identifier& operator=(Identifier&&) = default;

			const ServiceType& Service() const { return m_svcId; }

			bool operator==(const Identifier& other) const { return std::tie(m_svcId, m_value) == std::tie(other.m_svcId, other.m_value); }
			bool operator!=(const Identifier& other) const { return std::tie(m_svcId, m_value) != std::tie(other.m_svcId, other.m_value); }
			bool operator<(const Identifier& other) const { return std::tie(m_svcId, m_value) < std::tie(other.m_svcId, other.m_value); }

			bool GetAsUint64(uint64& out) const
			{
				const bool success = Traits::AsUint64(m_value, out);
				if (!success)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_COMMENT, "[GamePlatform] %s: Unable to convert '%s'.", __func__, ToDebugString());
				}

				return success;
			}

			template<class StrType>
			bool GetAsString(StrType& out) const
			{
				const bool success = Traits::AsString(m_value, out);
				if (!success)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_COMMENT, "[GamePlatform] %s: Unable to convert '%s'.", __func__, ToDebugString());
				}

				return success;
			}

			void Serialize(Serialization::IArchive& ar)
			{
				ar(m_svcId, "service");

				return Traits::Serialize(m_value, ar);
			}

			const char* ToDebugString() const
			{
				return Traits::ToDebugString(m_svcId, m_value);
			}

		private:
			ServiceType m_svcId = CryGUID::Null();
			ValueType m_value = Traits::Null();
		};
	}
}
