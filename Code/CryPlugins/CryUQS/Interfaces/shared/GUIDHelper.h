// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <algorithm>
#include <cctype>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Shared
	{
		namespace Internal
		{

			//===================================================================================
			//
			// CGUIDHelper
			//
			// - intermediate helper class to extend CryGUID's functionality until Timur submits his changes
			//
			//===================================================================================

			// example GUID as string: "dbbb8931-6ab6-42a7-b67e-3e871f7d9bdd"
			//                          u32      u16  u16  2*u8 6*u8
			//                          012345678901234567890123456789012345
			//                                    1         2         3

			class CGUIDHelper
			{
			public:

				static bool     TryParseFromString(CryGUID& out, const char* szGuidAsString);  // returns false and leaves 'out' untouched if given string is no valid GUID representation
				static void     ToString(const CryGUID& guid, Shared::IUqsString& out);

			private:

				static bool     IsValidStringRepresentation(const char* szGuidAsStringToCheck);
				static bool     ValidateHexCharsInRange(const char* szBegin, const char* szEnd);  // szEnd is exclusive (just like in any STL container)
			};

			inline bool CGUIDHelper::TryParseFromString(CryGUID& out, const char* szGuidAsString)
			{
				if (IsValidStringRepresentation(szGuidAsString))
				{
					out = CryGUID::FromString(szGuidAsString);
					return true;
				}
				else
				{
					return false;
				}
			}

			inline void CGUIDHelper::ToString(const CryGUID& guid, Shared::IUqsString& out)
			{
				out.Set(guid.ToString().c_str());
			}

			inline bool CGUIDHelper::IsValidStringRepresentation(const char* szGuidAsStringToCheck)
			{
				// example GUID as string: "dbbb8931-6ab6-42a7-b67e-3e871f7d9bdd"
				//                          u32      u16  u16  2*u8 6*u8
				//                          012345678901234567890123456789012345
				//                                    1         2         3

				const size_t len = strlen(szGuidAsStringToCheck);
				if (szGuidAsStringToCheck[0] == '{' && len == CRY_ARRAY_COUNT("{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}") - 1)
					szGuidAsStringToCheck += 1;
				else if (len != CRY_ARRAY_COUNT("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX") - 1)
					return false;

				if (szGuidAsStringToCheck[8] != '-')
					return false;

				if (szGuidAsStringToCheck[13] != '-')
					return false;

				if (szGuidAsStringToCheck[18] != '-')
					return false;

				if (szGuidAsStringToCheck[23] != '-')
					return false;

				if (!ValidateHexCharsInRange(szGuidAsStringToCheck + 0, szGuidAsStringToCheck + 8))
					return false;

				if (!ValidateHexCharsInRange(szGuidAsStringToCheck + 9, szGuidAsStringToCheck + 13))
					return false;

				if (!ValidateHexCharsInRange(szGuidAsStringToCheck + 14, szGuidAsStringToCheck + 18))
					return false;

				if (!ValidateHexCharsInRange(szGuidAsStringToCheck + 19, szGuidAsStringToCheck + 23))
					return false;

				if (!ValidateHexCharsInRange(szGuidAsStringToCheck + 24, szGuidAsStringToCheck + 36))
					return false;

				return true;
			}

			inline bool CGUIDHelper::ValidateHexCharsInRange(const char* szBegin, const char* szEnd)
			{
				return std::all_of(szBegin, szEnd, ::isxdigit);
			}
		}
	}
}
