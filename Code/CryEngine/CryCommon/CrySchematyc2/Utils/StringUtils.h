// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Remove s_sizeTStringBufferSize and size_t functions, we should be using uint32 and uint64 instead.
// #SchematycTODO : Remove Vec2 and Vec3 functions?.
// #SchematycTODO : Remove to string and stack_string functions?
// #SchematycTODO : Create append functions for each type?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CryString/CryStringUtils.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ArrayView.h>

#include "CrySchematyc2/BasicTypes.h"
#include "CrySchematyc2/Deprecated/IStringPool.h"

namespace Schematyc2
{
	namespace StringUtils
	{
		static const int32 s_invalidPos = -1;

		static const size_t s_boolStringBufferSize   = 2;
		static const size_t s_int8StringBufferSize   = 8;
		static const size_t s_uint8StringBufferSize  = 8;
		static const size_t s_int16StringBufferSize  = 16;
		static const size_t s_uint16StringBufferSize = 16;
		static const size_t s_int32StringBufferSize  = 16;
		static const size_t s_uint32StringBufferSize = 16;
		static const size_t s_int64StringBufferSize  = 32;
		static const size_t s_uint64StringBufferSize = 32;
		static const size_t s_sizeTStringBufferSize  = 32;
		static const size_t s_floatStringBufferSize  = 50;
		static const size_t s_vec2StringBufferSize   = 102;
		static const size_t s_vec3StringBufferSize   = 154;
		static const size_t s_ang3StringBufferSize   = 154;
		static const size_t s_quatStringBufferSize	 = 208;

		// Convert hexadecimal character to unsigned char.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr unsigned char HexCharToUnisgnedChar(char x)
		{
			return x >= '0' && x <= '9' ? x - '0' :
				x >= 'a' && x <= 'f' ? x - 'a' + 10 :
				x >= 'A' && x <= 'F' ? x - 'A' + 10 : 0;
		}

		// Convert hexadecimal character to unsigned short.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr unsigned short HexCharToUnisgnedShort(char x)
		{
			return x >= '0' && x <= '9' ? x - '0' :
				x >= 'a' && x <= 'f' ? x - 'a' + 10 :
				x >= 'A' && x <= 'F' ? x - 'A' + 10 : 0;
		}

		// Convert hexadecimal character to unsigned long.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr unsigned long HexCharToUnisgnedLong(char x)
		{
			return x >= '0' && x <= '9' ? x - '0' :
				x >= 'a' && x <= 'f' ? x - 'a' + 10 :
				x >= 'A' && x <= 'F' ? x - 'A' + 10 : 0;
		}

		// Convert hexadecimal string to unsigned char.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr unsigned char HexStringToUnsignedChar(const char* szInput)
		{
			return (HexCharToUnisgnedChar(szInput[0]) << 4) +
				HexCharToUnisgnedChar(szInput[1]);
		}

		// Convert hexadecimal string to unsigned short.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr unsigned short HexStringToUnsignedShort(const char* szInput)
		{
			return (HexCharToUnisgnedShort(szInput[0]) << 12) +
				(HexCharToUnisgnedShort(szInput[1]) << 8) +
				(HexCharToUnisgnedShort(szInput[2]) << 4) +
				HexCharToUnisgnedShort(szInput[3]);
		}

		// Convert hexadecimal string to unsigned long.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		constexpr unsigned long HexStringToUnsignedLong(const char* szInput)
		{
			return (HexCharToUnisgnedLong(szInput[0]) << 28) +
				(HexCharToUnisgnedLong(szInput[1]) << 24) +
				(HexCharToUnisgnedLong(szInput[2]) << 20) +
				(HexCharToUnisgnedLong(szInput[3]) << 16) +
				(HexCharToUnisgnedLong(szInput[4]) << 12) +
				(HexCharToUnisgnedLong(szInput[5]) << 8) +
				(HexCharToUnisgnedLong(szInput[6]) << 4) +
				HexCharToUnisgnedLong(szInput[7]);
		}

		// Copy string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline int32 Copy(const char* szInput, const CharArrayView& output, int32 inputLength = s_invalidPos)
		{
			CRY_ASSERT(szInput);
			if(szInput)
			{
				const int32	outputSize = static_cast<int32>(output.size());
				CRY_ASSERT(outputSize > 0);
				if(inputLength < 0)
				{
					inputLength = strlen(szInput);
				}
				if(outputSize > inputLength)
				{
					memcpy(output.begin(), szInput, inputLength);
					output[inputLength] = '\0';
					return inputLength;
				}
			}
			return 0;
		}

		// Append string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline int32 Append(const char* szInput, const CharArrayView& output, int32 outputPos = s_invalidPos, int32 inputLength = s_invalidPos)
		{
			CRY_ASSERT(szInput);
			if(szInput)
			{
				if(outputPos == s_invalidPos)
				{
					outputPos = strlen(output.begin());
				}
				if(inputLength == s_invalidPos)
				{
					inputLength = strlen(szInput);
				}
				const int32	outputSize = static_cast<int32>(output.size());
				CRY_ASSERT(outputSize > outputPos);
				if(outputSize > (outputPos + inputLength))
				{
					memcpy(output.begin() + outputPos, szInput, inputLength);
					output[outputPos + inputLength] = '\0';
					return inputLength;
				}
			}
			return 0;
		}

		// Trim characters from right hand size of string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline int32 TrimRight(const CharArrayView& output, const char* szChars, int32 outputPos = s_invalidPos)
		{
			int32	trimLength = 0;
			CRY_ASSERT(szChars);
			if(szChars)
			{
				if(outputPos == s_invalidPos)
				{
					outputPos = strlen(output.begin());
				}
				while(outputPos > 0)
				{
					-- outputPos;
					bool	bTrim = false;
					for(const char* szCompare = szChars; *szCompare; ++ szCompare)
					{
						if(*szCompare == output[outputPos])
						{
							bTrim = true;
							break;
						}
					}
					if(bTrim)
					{
						output[outputPos] = '\0';
						++ trimLength;
					}
					else
					{
						break;
					}
				}
			}
			return trimLength;
		}
			
		// Write bool to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* BoolToString(bool input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_boolStringBufferSize);
			if(outputSize >= s_boolStringBufferSize)
			{
				output[0]	= input ? '1' : '0';
				output[1]	= '\0';
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write bool to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* BoolToString(bool input, stack_string& output)
		{
			output = input ? "1" : "0";
			return output.c_str();
		}

		// Write bool to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* BoolToString(bool input, string& output)
		{
			output = input ? "1" : "0";
			return output.c_str();
		}

		// Read bool from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline bool BoolFromString(const char* szInput, bool defaultOutput = false)
		{
			bool	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				output = atoi(szInput) != 0;
			}
			return output;
		}

		// Write int8 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int8ToString(int8 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_int8StringBufferSize);
			if(outputSize >= s_int8StringBufferSize)
			{
				itoa(input, output.begin(), 10);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write int8 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int8ToString(int8 input, stack_string& output)
		{
			char	temp[s_int8StringBufferSize] = "";
			itoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Write int8 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int8ToString(int8 input, string& output)
		{
			char	temp[s_int8StringBufferSize] = "";
			itoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Read int8 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline int8 Int8FromString(const char* szInput, int8 defaultOutput = 0)
		{
			int8	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				output = static_cast<int8>(clamp_tpl<int32>(atoi(szInput), -std::numeric_limits<int8>::max(), std::numeric_limits<int8>::max()));
			}
			return output;
		}

		// Write uint8 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt8ToString(uint8 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_uint8StringBufferSize);
			if(outputSize >= s_uint8StringBufferSize)
			{
				ltoa(input, output.begin(), 10);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write uint8 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt8ToString(uint8 input, stack_string& output)
		{
			char	temp[s_uint8StringBufferSize] = "";
			ltoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Write uint8 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt8ToString(uint8 input, string& output)
		{
			char	temp[s_uint8StringBufferSize] = "";
			ltoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Read uint8 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline uint8 UInt8FromString(const char* szInput, uint8 defaultOutput = 0)
		{
			uint8	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				output = std::min<uint8>(static_cast<uint8>(atol(szInput)), std::numeric_limits<uint8>::max());
			}
			return output;
		}

		// Write int16 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int16ToString(int16 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_int16StringBufferSize);
			if(outputSize >= s_int16StringBufferSize)
			{
				itoa(input, output.begin(), 10);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write int16 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int16ToString(int16 input, stack_string& output)
		{
			char	temp[s_int16StringBufferSize] = "";
			itoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Write int16 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int16ToString(int16 input, string& output)
		{
			char	temp[s_int16StringBufferSize] = "";
			itoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Read int16 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline int16 Int16FromString(const char* szInput, int16 defaultOutput = 0)
		{
			int16	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				output = static_cast<int16>(clamp_tpl<int32>(atoi(szInput), -std::numeric_limits<int16>::max(), std::numeric_limits<int16>::max()));
			}
			return output;
		}

		// Write uint16 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt16ToString(uint16 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_uint16StringBufferSize);
			if(outputSize >= s_uint16StringBufferSize)
			{
				ltoa(input, output.begin(), 10);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write uint16 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt16ToString(uint16 input, stack_string& output)
		{
			char	temp[s_uint16StringBufferSize] = "";
			ltoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Write uint16 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt16ToString(uint16 input, string& output)
		{
			char	temp[s_uint16StringBufferSize] = "";
			ltoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Read uint16 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline uint16 UInt16FromString(const char* szInput, uint16 defaultOutput = 0)
		{
			uint16	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				output = std::min<uint16>(static_cast<uint16>(atol(szInput)), std::numeric_limits<uint16>::max());
			}
			return output;
		}

		// Write int32 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int32ToString(int32 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_int32StringBufferSize);
			if(outputSize >= s_int32StringBufferSize)
			{
				itoa(input, output.begin(), 10);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write int32 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int32ToString(int32 input, stack_string& output)
		{
			char	temp[s_int32StringBufferSize] = "";
			itoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Write int32 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int32ToString(int32 input, string& output)
		{
			char	temp[s_int32StringBufferSize] = "";
			itoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Read int32 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline int32 Int32FromString(const char* szInput, int32 defaultOutput = 0)
		{
			int32	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				output = atoi(szInput);
			}
			return output;
		}

		// Write uint32 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt32ToString(uint32 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_uint32StringBufferSize);
			if(outputSize >= s_uint32StringBufferSize)
			{
				ltoa(input, output.begin(), 10);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write uint32 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt32ToString(uint32 input, stack_string& output)
		{
			char	temp[s_uint32StringBufferSize] = "";
			ltoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Write uint32 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt32ToString(uint32 input, string& output)
		{
			char	temp[s_uint32StringBufferSize] = "";
			ltoa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Read uint32 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline uint32 UInt32FromString(const char* szInput, uint32 defaultOutput = 0)
		{
			uint32	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				output = atol(szInput);
			}
			return output;
		}

		// Write int64 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int64ToString(int64 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_int64StringBufferSize);
			if(outputSize >= s_int64StringBufferSize)
			{
				SCHEMATYC2_I64TOA(input, output.begin(), 10);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write int64 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int64ToString(int64 input, stack_string& output)
		{
			char	temp[s_int64StringBufferSize] = "";
			SCHEMATYC2_I64TOA(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Write int64 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Int64ToString(int64 input, string& output)
		{
			char	temp[s_int64StringBufferSize] = "";
			SCHEMATYC2_I64TOA(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Read int64 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline int64 Int64FromString(const char* szInput, int64 defaultOutput = 0)
		{
			int64	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
#ifdef _MSC_VER
				output = _strtoi64(szInput, 0, 10);
#else
				output = strtoll(szInput, 0, 10);
#endif
			}
			return output;
		}

		// Write uint64 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt64ToString(uint64 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_uint64StringBufferSize);
			if(outputSize >= s_uint64StringBufferSize)
			{
				_ui64toa(input, output.begin(), 10);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write uint64 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt64ToString(uint64 input, stack_string& output)
		{
			char	temp[s_uint64StringBufferSize] = "";
			_ui64toa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Write uint64 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* UInt64ToString(uint64 input, string& output)
		{
			char	temp[s_uint64StringBufferSize] = "";
			_ui64toa(input, temp, 10);
			output = temp;
			return output.c_str();
		}

		// Read uint64 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline uint64 UInt64FromString(const char* szInput, uint64 defaultOutput = 0)
		{
			uint64	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
#ifdef _MSC_VER
				output = _strtoui64(szInput, 0, 10);
#else
				output = strtoull(szInput, 0, 10);
#endif
			}
			return output;
		}

		// Write size_t to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* SizeTToString(size_t input, const CharArrayView& output)
		{
			return UInt64ToString(static_cast<uint64>(input), output);
		}

		// Write size_t to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* SizeTToString(size_t input, stack_string& output)
		{
			return UInt64ToString(static_cast<uint64>(input), output);
		}

		// Write size_t to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* SizeTToString(size_t input, string& output)
		{
			return UInt64ToString(static_cast<uint64>(input), output);
		}

		// Read size_t from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline size_t SizeTFromString(const char* szInput, size_t defaultOutput = 0)
		{
			return static_cast<size_t>(UInt64FromString(szInput, defaultOutput));
		}

		// Write float to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* FloatToString(float input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_floatStringBufferSize);
			if(outputSize >= s_floatStringBufferSize)
			{
				cry_sprintf(output.begin(), outputSize, "%.8f", input);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write float to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* FloatToString(float input, stack_string& output)
		{
			char temp[s_floatStringBufferSize] = "";
			cry_sprintf(temp, "%.8f", input);
			output = temp;
			return output.c_str();
		}

		// Write float to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* FloatToString(float input, string& output)
		{
			char temp[s_floatStringBufferSize] = "";
			cry_sprintf(temp, "%.8f", input);
			output = temp;
			return output.c_str();
		}

		// Read float from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline float FloatFromString(const char* szInput, float defaultOutput = 0.0f)
		{
			float	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				const int	itemCount = sscanf(szInput, "%f", &output);
				CRY_ASSERT(itemCount == 1);
			}
			return output;
		}

		// Write Vec2 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Vec2ToString(Vec2 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_vec2StringBufferSize);
			if(outputSize >= s_vec2StringBufferSize)
			{
				cry_sprintf(output.begin(), outputSize, "%.8f, %.8f", input.x, input.y);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write Vec2 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Vec2ToString(Vec2& input, stack_string& output)
		{
			char temp[s_vec2StringBufferSize] = "";
			cry_sprintf(temp, "%.8f, %.8f", input.x, input.y);
			output = temp;
			return output.c_str();
		}

		// Write Vec2 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Vec2ToString(Vec2& input, string& output)
		{
			char temp[s_vec2StringBufferSize] = "";
			cry_sprintf(temp, "%.8f, %.8f", input.x, input.y);
			output = temp;
			return output.c_str();
		}

		// Read Vec2 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline Vec2 Vec2FromString(const char* szInput, const Vec2& defaultOutput = ZERO)
		{
			Vec2	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				const int	itemCount = sscanf(szInput, "%f, %f", &output.x, &output.y);
				CRY_ASSERT(itemCount == 2);
			}
			return output;
		}

		// Write Vec3 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Vec3ToString(Vec3 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_vec3StringBufferSize);
			if(outputSize >= s_vec3StringBufferSize)
			{
				cry_sprintf(output.begin(), outputSize, "%.8f, %.8f, %.8f", input.x, input.y, input.z);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write Vec3 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Vec3ToString(Vec3& input, stack_string& output)
		{
			char temp[s_vec3StringBufferSize] = "";
			cry_sprintf(temp, "%.8f, %.8f, %.8f", input.x, input.y, input.z);
			output = temp;
			return output.c_str();
		}

		// Write Vec3 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Vec3ToString(Vec3& input, string& output)
		{
			char temp[s_vec3StringBufferSize] = "";
			cry_sprintf(temp, "%.8f, %.8f, %.8f", input.x, input.y, input.z);
			output = temp;
			return output.c_str();
		}

		// Read Vec3 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline Vec3 Vec3FromString(const char* szInput, const Vec3& defaultOutput)
		{
			Vec3	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				const int	itemCount = sscanf(szInput, "%f, %f, %f", &output.x, &output.y, &output.z);
				CRY_ASSERT(itemCount == 3);
			}
			return output;
		}

		// Write Ang3 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Ang3ToString(Ang3 input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_ang3StringBufferSize);
			if(outputSize >= s_ang3StringBufferSize)
			{
				cry_sprintf(output.begin(), outputSize, "%.8f, %.8f, %.8f", input.x, input.y, input.z);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write Ang3 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Ang3ToString(Ang3& input, stack_string& output)
		{
			char	temp[s_ang3StringBufferSize];
			cry_sprintf(temp, "%.8f, %.8f, %.8f", input.x, input.y, input.z);
			output = temp;
			return output.c_str();
		}

		// Write Ang3 to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* Ang3ToString(Ang3& input, string& output)
		{
			char	temp[s_ang3StringBufferSize] = "";
			cry_sprintf(temp, "%.8f, %.8f, %.8f", input.x, input.y, input.z);
			output = temp;
			return output.c_str();
		}

		// Read Ang3 from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline Ang3 Ang3FromString(const char* szInput, const Ang3& defaultOutput)
		{
			Ang3	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				const int	itemCount = sscanf(szInput, "%f, %f, %f", &output.x, &output.y, &output.z);
				CRY_ASSERT(itemCount == 3);
			}
			return output;
		}

		// Write Quat to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* QuatToString(Quat input, const CharArrayView& output)
		{
			const int32	outputSize = static_cast<int32>(output.size());
			CRY_ASSERT(outputSize >= s_quatStringBufferSize);
			if(outputSize >= s_quatStringBufferSize)
			{
				cry_sprintf(output.begin(), outputSize, "%.8f, %.8f, %.8f, %.8f", input.v.x, input.v.y, input.v.z, input.w);
			}
			else if(outputSize > 0)
			{
				output[0] = '\0';
			}
			return output.begin();
		}

		// Write Quat to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* QuatToString(Quat& input, stack_string& output)
		{
			char	temp[s_quatStringBufferSize];
			cry_sprintf(temp, "%.8f, %.8f, %.8f, %.8f", input.v.x, input.v.y, input.v.z, input.w);
			output = temp;
			return output.c_str();
		}

		// Write Quat to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* QuatToString(Quat& input, string& output)
		{
			char	temp[s_quatStringBufferSize] = "";
			cry_sprintf(temp, "%.8f, %.8f, %.8f, %.8f", input.v.x, input.v.y, input.v.z, input.w);
			output = temp;
			return output.c_str();
		}

		// Read Quat from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline Quat QuatFromString(const char* szInput, const Quat& defaultOutput)
		{
			Quat	output = defaultOutput;
			CRY_ASSERT(szInput);
			if(szInput)
			{
				const int	itemCount = sscanf(szInput, "%f, %f, %f, %f", &output.v.x, &output.v.y, &output.v.z, &output.w);
				CRY_ASSERT(itemCount == 4);
			}
			return output;
		}

		// Filter string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline bool FilterString(string input, string filter)
		{
			// Convert inputs to lower case.
			input.MakeLower();
			filter.MakeLower();
			// Tokenize filter and ensure input contains all words in filter.
			string::size_type	pos = 0;
			string::size_type	filterEnd = -1;
			do
			{
				const string::size_type	filterStart = filterEnd + 1;
				filterEnd	= filter.find(' ', filterStart);
				pos				= input.find(filter.substr(filterStart, filterEnd - filterStart), pos);
				if(pos == string::npos)
				{
					return false;			
				}
				pos	+= filterEnd - filterStart;
			} while(filterEnd != string::npos);
			return true;
		}

		// Separate type name and namespace.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline void SeparateTypeNameAndNamespace(const char* szDeclaration, string& typeName, string& output)
		{
			CRY_ASSERT(szDeclaration);
			if(szDeclaration)
			{
				const size_t	length = strlen(szDeclaration);
				size_t				pos = length - 1;
				size_t				templateDepth = 0;
				do
				{
					if(szDeclaration[pos] == '>')
					{
						++ templateDepth;
					}
					else if(szDeclaration[pos] == '<')
					{
						-- templateDepth;
					}
					else if((templateDepth == 0) && (szDeclaration[pos] == ':') && (szDeclaration[pos - 1] == ':'))
					{
						break;
					}
					-- pos;
				} while(pos > 1);
				if(pos > 1)
				{
					typeName	= string(szDeclaration + pos + 1, length - pos);
					output		= string(szDeclaration, pos - 1);
				}
				else
				{
					typeName	= szDeclaration;
					output		= "";
				}
			}
		}

		// Make project relative file name.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline void MakeProjectRelativeFileName(const char* szFileName, const char* szProjectDir, string& output)
		{
			CRY_ASSERT(szFileName && szProjectDir);
			if(szFileName && szProjectDir)
			{
				output = szFileName;
				const string::size_type	projectDirPos = output.find(szProjectDir);
				if(projectDirPos != string::npos)
				{
					output.erase(0, projectDirPos);
					output.TrimLeft("\\/");
				}
			}
		}
	}
}
