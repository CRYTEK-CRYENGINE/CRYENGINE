// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "BaseEnv/BaseEnv_Prerequisites.h"
#include "BaseEnv/BaseEnv_AutoRegistrar.h"

#include <CrySerialization/Forward.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>
#include <CryMath/Cry_Math.h>
#include <CryMath/Random.h>

namespace SchematycBaseEnv
{
	enum class EComparisonMode : int
	{
		NotSet = 0,
		Equal,
		NotEqual,
		LessThan,
		LessThanOrEqual,
		GreaterThan,
		GreaterThanOrEqual,
	};

	enum class EOldComparisonMode : int
	{
		NotSet = 0,
		Equal,
		NotEqual,
		LessThan,
		LessThanOrEqual,
		GreaterThan,
		GreaterThanOrEqual
	};
};

SERIALIZATION_ENUM_BEGIN_NESTED(SchematycBaseEnv, EComparisonMode, "Comparison mode")
	SERIALIZATION_ENUM(SchematycBaseEnv::EComparisonMode::NotSet, "NotSet", "Not set (do not use)")
	SERIALIZATION_ENUM(SchematycBaseEnv::EComparisonMode::Equal, "Equal", "Equal")
	SERIALIZATION_ENUM(SchematycBaseEnv::EComparisonMode::NotEqual, "NotEqual", "Not equal")
	SERIALIZATION_ENUM(SchematycBaseEnv::EComparisonMode::LessThan, "LessThan", "Less than")
	SERIALIZATION_ENUM(SchematycBaseEnv::EComparisonMode::LessThanOrEqual, "LessThanOrEqual", "Less than or equal")
	SERIALIZATION_ENUM(SchematycBaseEnv::EComparisonMode::GreaterThan, "GreaterThan", "Greater than")
	SERIALIZATION_ENUM(SchematycBaseEnv::EComparisonMode::GreaterThanOrEqual, "GreaterThanOrEqual", "Greater than or equal")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(SchematycBaseEnv, EOldComparisonMode, "Comparison mode")
SERIALIZATION_ENUM(SchematycBaseEnv::EComparisonMode::NotSet, "NOT_SET", "Not set (do not use)")
	SERIALIZATION_ENUM(SchematycBaseEnv::EOldComparisonMode::Equal, "EQUAL", "Equal")
	SERIALIZATION_ENUM(SchematycBaseEnv::EOldComparisonMode::NotEqual, "NOT_EQUAL", "Not equal")
	SERIALIZATION_ENUM(SchematycBaseEnv::EOldComparisonMode::LessThan, "LESS_THAN", "Less than")
	SERIALIZATION_ENUM(SchematycBaseEnv::EOldComparisonMode::LessThanOrEqual, "LESS_THAN_OR_EQUAL", "Less than or equal")
	SERIALIZATION_ENUM(SchematycBaseEnv::EOldComparisonMode::GreaterThan, "GREATER_THAN", "Greater than")
	SERIALIZATION_ENUM(SchematycBaseEnv::EOldComparisonMode::GreaterThanOrEqual, "GREATER_THAN_OR_EQUAL", "Greater than or equal")
SERIALIZATION_ENUM_END()

namespace MathUtils
{
	inline Matrix33 CreateOrthonormalTM33(Vec3 at, Vec3 up = Vec3(0.0f, 0.0f, 1.0f))
	{
		CRY_ASSERT(IsEquivalent(at, at.GetNormalized()));
		CRY_ASSERT(IsEquivalent(up, up.GetNormalized()));
		const Vec3	right = at.Cross(up).GetNormalized();
		up = right.Cross(at).GetNormalized();
		return Matrix33::CreateFromVectors(right, at, up);
	}
}

namespace SchematycBaseEnv
{
	// This override of the standard enumeration serialize function is required in order to patch old scripts.
	// Once all scripts are up to date we can remove this function, the EComparisonMode::NotSet value and EOldComparisonMode enumeration.
	bool Serialize(Serialization::IArchive& archive, EComparisonMode& value, const char* szName, const char* szLabel)
	{
		const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<EComparisonMode>();
		if(archive.isOutput() || archive.isEdit() || archive.isInPlace())
		{
			return enumDescription.serialize(archive, reinterpret_cast<int&>(value), szName, szLabel);
		}
		else
		{
			EComparisonMode inputValue = EComparisonMode::NotSet;
			enumDescription.serialize(archive, reinterpret_cast<int&>(inputValue), szName, szLabel);
			if(inputValue == EComparisonMode::NotSet)
			{
				const Serialization::EnumDescription& oldEnumDescription = Serialization::getEnumDescription<EOldComparisonMode>();
				oldEnumDescription.serialize(archive, reinterpret_cast<int&>(inputValue), szName, szLabel);
			}
			if(inputValue != EComparisonMode::NotSet)
			{
				value = inputValue;
				return true;
			}
			return false;
		}
	}

	namespace Bool
	{
		bool Flip(bool value)
		{
			return !value;
		}

		bool Or(bool a, bool b)
		{
			return a || b;
		}

		bool And(bool a, bool b)
		{
			return a && b;
		}

		bool Xor(bool a, bool b)
		{
			return a != b;
		}

		bool Compare(bool a, bool b, EComparisonMode mode)
		{
			switch(mode)
			{
			case EComparisonMode::Equal:
				{
					return a == b;
				}
			case EComparisonMode::NotEqual:
				{
					return a != b;
				}
			case EComparisonMode::LessThan:
				{
					return a < b;
				}
			case EComparisonMode::LessThanOrEqual:
				{
					return a <= b;
				}
			case EComparisonMode::GreaterThan:
				{
					return a > b;
				}
			case EComparisonMode::GreaterThanOrEqual:
				{
					return a >= b;
				}
			default:
				{
					return false;
				}
			}
		}

		int32 ToInt32(bool value) 
		{
			return value ? 1 : 0;
		}

		uint32 ToUInt32(bool value) 
		{
			return value ? 1 : 0;
		}

		Schematyc2::CPoolString ToString(bool value)
		{
			char temp[Schematyc2::StringUtils::s_boolStringBufferSize] = "";
			Schematyc2::StringUtils::BoolToString(value, temp);
			return temp;
		}

		static void Register(Schematyc2::IEnvRegistry& envRegistry)
		{
			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Bool::Flip, "6a1d0e15-c4ad-4c87-ab38-3739020dd708");
				pFunction->SetNamespace("Types::Bool");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Flip boolean value");
				pFunction->BindInput(1, "Value", "Value", false);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Bool::Or, "ff6f2ed1-942b-4726-9f13-b9e711cd28df");
				pFunction->SetNamespace("Types::Bool");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Logical OR operation");
				pFunction->BindInput(1, "A", "A");
				pFunction->BindInput(2, "B", "B");
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Bool::And, "87d62e0c-d32f-4ea7-aa5c-22aeeb1c9cd5");
				pFunction->SetNamespace("Types::Bool");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Logical AND operation");
				pFunction->BindInput(1, "A", "A", false);
				pFunction->BindInput(2, "B", "B", false);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Bool::Xor, "14a312fc-0885-4ab7-8bef-f124588e4c45");
				pFunction->SetNamespace("Types::Bool");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Logical XOR operation");
				pFunction->BindInput(1, "A", "A", false);
				pFunction->BindInput(2, "B", "B", false);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Bool::Compare, "f4c59052-39c1-4a56-b614-39ca32b400af");
				pFunction->SetNamespace("Types::Bool");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Compare A and B");
				pFunction->BindInput(1, "A", "A", false);
				pFunction->BindInput(2, "B", "B", false);
				pFunction->BindInput(3, "Mode", "Comparison mode", EComparisonMode::Equal);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Bool::ToInt32, "F93DB38C-E4A1-4553-84D2-255A1E47AC8A");
				pFunction->SetNamespace("Types::Bool");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert boolean to int32");
				pFunction->BindInput(1, "Value", "Value", false);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Bool::ToUInt32, "FE42F543-AA16-47E8-A1EA-D4374C01B4A3");
				pFunction->SetNamespace("Types::Bool");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert boolean to uint32");
				pFunction->BindInput(1, "Value", "Value", false);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Bool::ToString, "9a6f082e-ba3c-4510-9248-8bf8235d5e03");
				pFunction->SetNamespace("Types::Bool");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert boolean to string");
				pFunction->BindInput(1, "Value", "Value", false);
				pFunction->BindOutput(0, "String", "String"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}
		}
	}

	namespace Int32
	{
		int32 Add(int32 a, int32 b)
		{
			return a + b;
		}

		int32 Subtract(int32 a, int32 b)
		{
			return a - b;
		}

		int32 Multiply(int32 a, int32 b)
		{
			return a * b;
		}

		int32 Divide(int32 a, int32 b)
		{
			return b != 0 ? a / b : 0;
		}

		int32 Modulus(int32 a, int32 b)
		{
			return b != 0 ? a % b : 0;
		}

		int32 Random(int32 min, int32 max)
		{
			return min < max ? cry_random(min, max) : min;
		}

		int32 Abs(int32 value)
		{
			return abs(value);
		}

		int32 Min(int32 a, int32 b)
		{
			return min(a, b);
		}

		int32 Max(int32 a, int32 b)
		{
			return max(a, b);
		}

		int32 ClampValue(int32 value, int32 minValue, int32 maxValue)
		{
			if( minValue >= maxValue ) 
			{
				std::swap(minValue, maxValue);
				SCHEMATYC2_GAME_WARNING("ClampValue: min range value is bigger than max: min=%d, max=%d", minValue, maxValue);
			}
			
			return clamp_tpl(value, minValue, maxValue);
		}

		bool InRange(int32 value, int32 minBound, int32 maxBound )
		{
			return inrange(value, minBound, maxBound) != 0; 
		}
		
		bool Compare(int32 a, int32 b, EComparisonMode mode)
		{
			switch(mode)
			{
			case EComparisonMode::Equal:
				{
					return a == b;
				}
			case EComparisonMode::NotEqual:
				{
					return a != b;
				}
			case EComparisonMode::LessThan:
				{
					return a < b;
				}
			case EComparisonMode::LessThanOrEqual:
				{
					return a <= b;
				}
			case EComparisonMode::GreaterThan:
				{
					return a > b;
				}
			case EComparisonMode::GreaterThanOrEqual:
				{
					return a >= b;
				}
			default:
				{
					return false;
				}
			}
		}

		float ToFloat(int32 value) 
		{
			return static_cast<float>(value);
		}

		uint32 ToUInt32(int32 value)
		{
			return static_cast<uint32>(value);
		}

		Schematyc2::CPoolString ToString(int32 value)
		{
			char temp[Schematyc2::StringUtils::s_int32StringBufferSize] = "";
			Schematyc2::StringUtils::Int32ToString(value, temp);
			return temp;
		}

		static void Register(Schematyc2::IEnvRegistry& envRegistry)
		{
			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::Add, "8aaf394a-5288-40df-b31e-47e0c9757a93");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Add A to B");
				pFunction->BindInput(1, "A", "A", int32(0));
				pFunction->BindInput(2, "B", "B", int32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::Subtract, "cf5e826b-f22d-4fde-8a0c-259ceea75416");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Subtract B from A");
				pFunction->BindInput(1, "A", "A", int32(0));
				pFunction->BindInput(2, "B", "B", int32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::Multiply, "d03e2bfe-79fc-4321-bf30-13be640443bc");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Multiply A by B");
				pFunction->BindInput(1, "A", "A", int32(0));
				pFunction->BindInput(2, "B", "B", int32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::Divide, "450af09f-e7ea-4c6b-8cda-a913e53f4460");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Divide A by B");
				pFunction->BindInput(1, "A", "A", int32(0));
				pFunction->BindInput(2, "B", "B", int32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::Modulus, "37a5e2ba-caba-431f-882a-3048a6b18904");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate remainder when A is divided by B");
				pFunction->BindInput(1, "A", "A", int32(0));
				pFunction->BindInput(2, "B", "B", int32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::Random, "67bc3c63-e6f2-42c2-85af-a30fc30d0b11");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Generate random number");
				pFunction->BindInput(1, "Min", "Minimum value", int32(0));
				pFunction->BindInput(2, "Max", "Maximum value", int32(100));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::Abs, "f2f63e0a-3adc-4659-9444-32546d73c550");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Returns the absolute value");
				pFunction->BindInput(1, "Value", "Value", int32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::Min, "eda046fd-34af-424b-b541-e694a3026acb");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Returns the minimum value");
				pFunction->BindInput(1, "A", "A", int32(0));
				pFunction->BindInput(2, "B", "B", int32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::Max, "bcbc7a5b-be1c-47ee-b4ca-8747f0023d3c");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Returns the maximum value");
				pFunction->BindInput(1, "A", "A", int32(0));
				pFunction->BindInput(2, "B", "B", int32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::ClampValue, "2c37a9ae-f1f8-4473-9c3c-67284d479219");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Clamp an int32 value");
				pFunction->BindInput(1, "Value", "Value", int32(0));
				pFunction->BindInput(2, "MinValue", "Min Value", int32(0));
				pFunction->BindInput(3, "MaxValue", "Max Value", int32(1));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::InRange, "1304d1d8-3cac-40f1-95a1-ff3ea8b0ad87");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Check if a value is in a given range");
				pFunction->BindInput(1, "Value", "Value", int32(0));
				pFunction->BindInput(2, "MinBound", "Min Bound", int32(0));
				pFunction->BindInput(3, "MaxBound", "Max Bound", int32(1));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}
			
			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::Compare, "938d2cb0-62b3-4612-af76-915bdda8ef80");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Compare A and B");
				pFunction->BindInput(1, "A", "A", int32(0));
				pFunction->BindInput(2, "B", "B", int32(0));
				pFunction->BindInput(3, "Mode", "Comparison mode", EComparisonMode::Equal);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::ToFloat, "dc514c68-89ea-4c14-8ce3-9b45f950409a");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert int32 to float");
				pFunction->BindInput(1, "Value", "Value", int32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::ToUInt32, "FE95372E-42BA-48C7-A051-7B91BDC85DE4");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert int32 to uint32");
				pFunction->BindInput(1, "Value", "Value", int32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Int32::ToString, "71b9bfa9-8578-42e3-8ad3-18ff4bb74cca");
				pFunction->SetNamespace("Types::Int32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert int32 to string");
				pFunction->BindInput(1, "Value", "Value", int32(0));
				pFunction->BindOutput(0, "String", "String"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}
		}
	}

	namespace UInt32 
	{
		uint32 Add(uint32 a, uint32 b)
		{
			return a + b;
		}

		uint32 Subtract(uint32 a, uint32 b)
		{
			return a - b;
		}

		uint32 Multiply(uint32 a, uint32 b)
		{
			return a * b;
		}

		uint32 Divide(uint32 a, uint32 b)
		{
			return b != 0 ? a / b : 0;
		}

		uint32 Modulus(uint32 a, uint32 b)
		{
			return b != 0 ? a % b : 0;
		}

		uint32 Random(uint32 min, uint32 max)
		{
			return min < max ? cry_random(min, max) : min;
		}

		uint32 Min(uint32 a, uint32 b)
		{
			return min(a, b);
		}

		uint32 Max(uint32 a, uint32 b)
		{
			return max(a, b);
		}

		bool Compare(uint32 a, uint32 b, EComparisonMode mode)
		{
			switch(mode)
			{
			case EComparisonMode::Equal:
				{
					return a == b;
				}
			case EComparisonMode::NotEqual:
				{
					return a != b;
				}
			case EComparisonMode::LessThan:
				{
					return a < b;
				}
			case EComparisonMode::LessThanOrEqual:
				{
					return a <= b;
				}
			case EComparisonMode::GreaterThan:
				{
					return a > b;
				}
			case EComparisonMode::GreaterThanOrEqual:
				{
					return a >= b;
				}
			default:
				{
					return false;
				}
			}
		}

		float ToFloat(uint32 value) 
		{
			return static_cast<float>(value);
		}

		int32 ToInt32(uint32 value) 
		{
			return static_cast<int32>(value);
		}

		Schematyc2::CPoolString ToString(uint32 value)
		{
			char temp[Schematyc2::StringUtils::s_uint32StringBufferSize] = "";
			Schematyc2::StringUtils::UInt32ToString(value, temp);
			return temp;
		}

		static void Register(Schematyc2::IEnvRegistry& envRegistry)
		{
			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::Add, "5dcc5f2e-573e-41e8-8b38-4aaf5a9c0854");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Add A to B");
				pFunction->BindInput(1, "A", "A", uint32(0));
				pFunction->BindInput(2, "B", "B", uint32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::Subtract, "bd78264c-f111-4760-b9bc-7a65fe2b722e");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Subtract B from A");
				pFunction->BindInput(1, "A", "A", uint32(0));
				pFunction->BindInput(2, "B", "B", uint32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::Multiply, "53a75fa7-e113-4f24-99c0-a88a54ea1bf7");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Multiply A by B");
				pFunction->BindInput(1, "A", "A", uint32(0));
				pFunction->BindInput(2, "B", "B", uint32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::Divide, "2258b2d2-387d-41f3-bf98-63f2458fc1d9");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Divide A by B");
				pFunction->BindInput(1, "A", "A", uint32(0));
				pFunction->BindInput(2, "B", "B", uint32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::Modulus, "770748ab-83c5-4da5-8bf8-c9006b75d8d7");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate remainder when A is divided by B");
				pFunction->BindInput(1, "A", "A", uint32(0));
				pFunction->BindInput(2, "B", "B", uint32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::Random, "d06c1c65-a8be-4473-aae0-23e7f8e1ff98");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Generate random number");
				pFunction->BindInput(1, "Min", "Minimum value", uint32(0));
				pFunction->BindInput(2, "Max", "Maximum value", uint32(100));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::Min, "fb6b23f1-83d0-43b6-983b-142de81d9cc9");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Returns the minimum value");
				pFunction->BindInput(1, "A", "A", uint32(0));
				pFunction->BindInput(2, "B", "B", uint32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::Max, "555d2b12-2c60-4efb-8de5-2158fcf691c7");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Returns the maximum value");
				pFunction->BindInput(1, "A", "A", uint32(0));
				pFunction->BindInput(2, "B", "B", uint32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::Compare, "e7c171a1-5608-42cc-a357-407140e27cbd");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Compare A and B");
				pFunction->BindInput(1, "A", "A", uint32(0));
				pFunction->BindInput(2, "B", "B", uint32(0));
				pFunction->BindInput(3, "Mode", "Comparison mode", EComparisonMode::Equal);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::ToFloat, "89fc18a2-0fde-4d07-ada6-7d95ef613132");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert uint32 to float");
				pFunction->BindInput(1, "Value", "Value", uint32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::ToInt32, "ee7dbbc9-d950-44a4-9fd4-0a3891414147");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert uint32 to int32");
				pFunction->BindInput(1, "Value", "Value", uint32(0));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(UInt32::ToString, "38a094d6-a8b2-4a87-baee-8d456faf3739");
				pFunction->SetNamespace("Types::UInt32");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert uint32 to string");
				pFunction->BindInput(1, "Value", "Value", uint32(0));
				pFunction->BindOutput(0, "String", "String"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}
		}
	}

	namespace Float
	{
		float Add(float a, float b)
		{
			return a + b;
		}

		float Subtract(float a, float b)
		{
			return a - b;
		}

		float Multiply(float a, float b)
		{
			return a * b;
		}

		float Divide(float a, float b)
		{
			return b != 0.0f ? a / b : 0.0f;
		}

		float Modulus(float a, float b)
		{
			return b != 0.0f ? fmodf(a, b) : 0.0f;
		}

		float Sin(float value)
		{
			return sin_tpl(DEG2RAD(value));
		}

		float ArcSin(float value)
		{
			return RAD2DEG(asin_tpl(value));
		}

		float Cos(float value)
		{
			return cos_tpl(DEG2RAD(value));
		}

		float ArcCos(float value)
		{
			return RAD2DEG(acos_tpl(value));
		}

		float Tan(float value)
		{
			return tan_tpl(DEG2RAD(value));
		}

		float ArcTan(float value)
		{
			return RAD2DEG(atan_tpl(value));
		}

		float Random(float min, float max)
		{
			return min < max ? cry_random(min, max) : min;
		}

		float Min(float a, float b)
		{
			return min(a, b);
		}

		float Max(float a, float b)
		{
			return max(a, b);
		}

		float Abs(float value)
		{
			return fabs_tpl(value);
		}

		float ClampValue(float value, float minValue, float maxValue)
		{
			if( minValue >= maxValue ) 
			{
				std::swap(minValue, maxValue);
				SCHEMATYC2_GAME_WARNING("ClampValue: min range value is bigger than max: min=%g, max=%g", minValue, maxValue);
			}
			
			return clamp_tpl(value, minValue, maxValue);
		}

		bool InRange(float value, float minBound, float maxBound )
		{
			return inrange(value, minBound, maxBound) != 0; 
		}

		bool Compare(float a, float b, EComparisonMode mode)
		{
			switch(mode)
			{
			case EComparisonMode::Equal:
				{
					return a == b;
				}
			case EComparisonMode::NotEqual:
				{
					return a != b;
				}
			case EComparisonMode::LessThan:
				{
					return a < b;
				}
			case EComparisonMode::LessThanOrEqual:
				{
					return a <= b;
				}
			case EComparisonMode::GreaterThan:
				{
					return a > b;
				}
			case EComparisonMode::GreaterThanOrEqual:
				{
					return a >= b;
				}
			default:
				{
					return false;
				}
			}
		}

		bool CompareWithPrecision(float a, float b, float epsilon)
		{
			return IsEquivalent(a, b, epsilon);
		}

		int32 ToInt32(float value) 
		{
			return static_cast<int32>(value);
		}

		uint32 ToUInt32(float value) 
		{
			return static_cast<uint32>(value);
		}

		Schematyc2::CPoolString ToString(float value)
		{
			char temp[Schematyc2::StringUtils::s_floatStringBufferSize] = "";
			Schematyc2::StringUtils::FloatToString(value, temp);
			return temp;
		}

		static void Register(Schematyc2::IEnvRegistry& envRegistry)
		{
			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Add, "d62aede8-ed2c-47d9-b7f3-6de5ed0ef51f");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Add A to B");
				pFunction->BindInput(1, "A", "A", 0.f);
				pFunction->BindInput(2, "B", "B", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Subtract, "3a7c8a1d-2938-40f8-b5ac-b6becf32baf2");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Subtract B from A");
				pFunction->BindInput(1, "A", "A", 0.f);
				pFunction->BindInput(2, "B", "B", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Multiply, "b04f428a-4408-4c3d-b73b-173a9738c857");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Multiply A by B");
				pFunction->BindInput(1, "A", "A", 0.f);
				pFunction->BindInput(2, "B", "B", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Divide, "fd26901b-dbbe-4f4d-b8c1-6547f55339ae");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Divide A by B");
				pFunction->BindInput(1, "A", "A", 0.f);
				pFunction->BindInput(2, "B", "B", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Modulus, "fc35a48f-7926-4417-b940-08162bc4a9a5");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate remainder when A is divided by B");
				pFunction->BindInput(1, "A", "A", 0.f);
				pFunction->BindInput(2, "B", "B", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Min, "1097ef40-e2c4-4c80-ab6f-d3cb998fed97");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Returns the minimum value");
				pFunction->BindInput(1, "A", "A", 0.0f);
				pFunction->BindInput(2, "B", "B", 0.0f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Max, "122be784-0a65-480a-be2b-2873389fbc6f");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Returns the maximum value");
				pFunction->BindInput(1, "A", "A", 0.0f);
				pFunction->BindInput(2, "B", "B", 0.0f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Sin, "d21a3090-e17f-42a4-a5e5-2f153adadd8b");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate sine");
				pFunction->BindInput(1, "Value", "Value (degrees)", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::ArcSin, "3898f5c4-fead-4976-87f7-11d902cff5ad");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate arc-sine");
				pFunction->BindInput(1, "Value", "Value", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Cos, "0a6e3000-9ccc-4a72-874d-6b0c2546eef4");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate cosine");
				pFunction->BindInput(1, "Value", "Value (degrees)", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::ArcCos, "3df97c26-17f0-45a7-b07d-dae4ddbd84bd");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate arc-cosine");
				pFunction->BindInput(1, "Value", "Value", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Tan, "0865c078-a019-472e-8d36-62a68d504edd");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate tangent");
				pFunction->BindInput(1, "Value", "Value (degrees)", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::ArcTan, "7ac8f74c-4ed9-4138-853a-5df14c452efc");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate arc-tangent");
				pFunction->BindInput(1, "Value", "Value", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Random, "a17b82b7-3b4a-440d-a4f1-082f3d9cf0f5");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Generate random number");
				pFunction->BindInput(1, "Min", "Minimum value", 0.f);
				pFunction->BindInput(2, "Max", "Maximum value", 100.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Abs, "08e66eec-9e40-4fe9-b4dd-0df8073c681d");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Returns the absolute value");
				pFunction->BindInput(1, "Value", "Value", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::ClampValue, "1669a3c2-7c2b-496c-82c9-9b9f2d94dff9");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Clamp an int32 value");
				pFunction->BindInput(1, "Value", "Value", float(0));
				pFunction->BindInput(2, "MinValue", "Min Value", float(0));
				pFunction->BindInput(3, "MaxValue", "Max Value", float(1));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::InRange, "0e767c2c-c52b-42d4-a406-d377001826bd");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Check if a value is in a given range");
				pFunction->BindInput(1, "Value", "Value", float(0));
				pFunction->BindInput(2, "MinBound", "Min Bound", float(0));
				pFunction->BindInput(3, "MaxBound", "Max Bound", float(1));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::Compare, "580eb35f-7032-4a87-8c62-eb140c4f9b3e");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Compare A and B");
				pFunction->BindInput(1, "A", "A", 0.f);
				pFunction->BindInput(2, "B", "B", 0.f);
				pFunction->BindInput(3, "Mode", "Comparison mode", EComparisonMode::Equal);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::CompareWithPrecision, "1B14BFD7-7C40-4676-A9DC-C9A2AF854B48");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Compare A and B for equality with the precision");
				pFunction->BindInput(1, "A", "A", 0.f);
				pFunction->BindInput(2, "B", "B", 0.f);
				pFunction->BindInput(3, "Precision", "Precision", FLT_EPSILON);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::ToInt32, "6e6356f3-7cf6-4746-9ef7-e8031c2a4117");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert float to int32");
				pFunction->BindInput(1, "Value", "Value", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::ToUInt32, "FAA240D7-7187-4A9F-8345-3D77A095EDE2");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert float to uint32");
				pFunction->BindInput(1, "Value", "Value", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Float::ToString, "171188d6-b66a-4d4b-8508-b6c003c9da78");
				pFunction->SetNamespace("Types::Float");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert float to string");
				pFunction->BindInput(1, "Value", "Value", 0.f);
				pFunction->BindOutput(0, "String", "String"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}
		}
	}

	namespace Vector2
	{
		Vec2 Create(float x, float y)
		{
			return Vec2(x,y);
		}

		void Expand(Vec2 value, float& x, float& y)
		{
			x = value.x;
			y = value.y;
		}

		float Length(Vec2 value)
		{
			return value.GetLength();
		}

		float Distance(const Vec2 &a, const Vec2& b)
		{
			return (a - b).GetLength();
		}

		float DistanceSq(const Vec2& a, const Vec2& b)
		{
			return (a - b).GetLengthSquared();
		}

		Vec2 Normalize(const Vec2& value)
		{
			return value.GetNormalized();
		}

		Vec2 Add(const Vec2& a, const Vec2& b)
		{
			return a + b;
		}

		Vec2 Subtract(const Vec2& a, const Vec2& b)
		{
			return a - b;
		}

		Vec2 Scale(const Vec2& a, float b)
		{
			return a * b;
		}

		float DotProduct(const Vec2& a, const Vec2& b)
		{
			return a.Dot(b);
		}

		float AngleBetween(const Vec2& a, const Vec2& b)
		{
			const float deg = RAD2DEG(atan2(a.y, a.x) - atan2(b.y, b.x));
			return (deg > 0.0f ? deg : 360.0f + deg);
		}

		Schematyc2::CPoolString ToString(const Vec2& value)
		{
			char temp[Schematyc2::StringUtils::s_vec2StringBufferSize] = "";
			Schematyc2::StringUtils::Vec2ToString(value, temp);
			return temp;
		}

		Vec3 ToVector3(const Vec2& value, float z)
		{
			return Vec3(value.x, value.y, z);
		}

		static void Register(Schematyc2::IEnvRegistry& envRegistry)
		{
			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::Create, "19f18792-2347-4385-bd90-433c903260aa");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Create 2D vector");
				pFunction->BindInput(1, "X", "X", 0.f);
				pFunction->BindInput(2, "Y", "Y", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::Expand, "e03eebae-2f92-4a1d-b5e3-81d5a69dcabf");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Expand 2D vector");
				pFunction->BindInput(0, "Value", "Value", Vec2(ZERO)); //pFunction->BindInput(0, "Result", "Result", Vec2(ZERO));
				pFunction->BindOutput(1, "X", "X");
				pFunction->BindOutput(2, "Y", "Y");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::Length, "796a20cd-dc27-47c5-99b3-292f16f46e46");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate length");
				pFunction->BindInput(1, "Value", "Value", Vec2(ZERO));
				pFunction->BindOutput(0, "Length", "Length"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::Distance, "6ad3e5fa-715d-4e4a-bd03-ffa5ba7d9d6d");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Get distance between two 2D vectors");
				pFunction->BindInput(1, "A", "A", Vec2(ZERO));
				pFunction->BindInput(2, "B", "B", Vec2(ZERO));
				pFunction->BindOutput(0, "Distance", "Distance"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::DistanceSq, "4f626698-3eae-4a73-81e2-eae835d22688");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Gets squared distance between two 2D vectors");
				pFunction->BindInput(1, "A", "A", Vec2(ZERO));
				pFunction->BindInput(2, "B", "B", Vec2(ZERO));
				pFunction->BindOutput(0, "Distance", "Distance"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::Normalize, "7aa8e16e-3318-427d-980e-2213c0fdBd13");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Normalize 2D vector");
				pFunction->BindInput(1, "Value", "Value", Vec2(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::Add, "d1b45d39-bef5-4f18-b7e1-7fad1ea78dd3");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Add A to B");
				pFunction->BindInput(1, "A", "A", Vec2(ZERO));
				pFunction->BindInput(2, "B", "B", Vec2(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::Subtract, "edc23583-23ee-4a1c-b24b-1fbd6a503b03");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Subtract B from A");
				pFunction->BindInput(1, "A", "A", Vec2(ZERO));
				pFunction->BindInput(2, "B", "B", Vec2(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::Scale, "db715617-aaf7-4f0a-a7d4-ecd0e6967c80");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Scale 2D vector");
				pFunction->BindInput(1, "A", "A", Vec2(ZERO));
				pFunction->BindInput(2, "B", "B", 1.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::DotProduct, "283b624b-c536-4448-8daf-92f22d0cd02d");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate dot product of two 2D vectors");
				pFunction->BindInput(1, "A", "A", Vec2(ZERO));
				pFunction->BindInput(2, "B", "B", Vec2(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::AngleBetween, "20d0ed4b-70ef-4210-8349-2966d7619126");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate angle between two 2D vectors");
				pFunction->BindInput(1, "A", "A", Vec2(ZERO));
				pFunction->BindInput(2, "B", "B", Vec2(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::ToString, "548ed62f-76e5-4b44-b247-751195b89605");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert 2D vector to string");
				pFunction->BindInput(1, "Value", "Value", Vec2(ZERO));
				pFunction->BindOutput(0, "String", "String"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector2::ToVector3, "e5853217-c60d-4c26-aa85-7252e70c4e00");
				pFunction->SetNamespace("Types::Vector2");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert 2D vector to 3D vector");
				pFunction->BindInput(1, "Value", "Value", Vec2(ZERO));
				pFunction->BindInput(2, "Z", "Z", 0.0f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}
		}
	}

	namespace Vector3
	{
		Vec3 Create(float x, float y, float z)
		{
			return Vec3(x, y, z);
		}

		void Expand(const Vec3& value, float& x, float& y, float& z)
		{
			x = value.x;
			y = value.y;
			z = value.z;
		}

		float Length(Vec3 value)
		{
			return value.GetLength();
		}

		float Distance(const Vec3 &a, const Vec3& b)
		{
			return (a - b).GetLength();
		}

		float DistanceSq(const Vec3& a, const Vec3& b)
		{
			return (a - b).GetLengthSquared();
		}

		Vec3 Normalize(const Vec3& value)
		{
			return value.GetNormalized();
		}

		Vec3 Add(const Vec3& a, const Vec3& b)
		{
			return a + b;
		}

		Vec3 Subtract(const Vec3& a, const Vec3& b)
		{
			return a - b;
		}

		Vec3 Scale(const Vec3& a, float b)
		{
			return a * b;
		}

		float DotProduct(const Vec3& a, const Vec3& b)
		{
			return a.Dot(b);
		}

		Vec3 CrossProduct(const Vec3& a, const Vec3& b)
		{
			return a.Cross(b);
		}

		Schematyc2::CPoolString ToString(const Vec3& value)
		{
			char temp[Schematyc2::StringUtils::s_vec3StringBufferSize] = "";
			Schematyc2::StringUtils::Vec3ToString(value, temp);
			return temp;
		}

		Vec2 ToVector2(const Vec3& value) 
		{
			return Vec2(value.x, value.y);
		}

		bool IsZero(const Vec3& a)
		{
			return a.IsZero();
		}

		float DistanceToSegment(const Vec3& p, const Vec3& lineSegStart, const Vec3& lineSegEnd)
		{
			float t;
			return Distance::Point_Lineseg(p, Lineseg(lineSegStart, lineSegEnd), t);
		}

		Vec3 RotateVectorByAngleAroundAxis(const Vec3& axis, const float angle, const Vec3& vector)
		{
			return vector.GetRotated(axis, DEG2RAD(angle));
		}

		static void Register(Schematyc2::IEnvRegistry& envRegistry)
		{
			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::Create, "3496016c-8092-4a63-9198-9adce657fd1a");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Create 3D vector");
				pFunction->BindInput(1, "X", "X", 0.f);
				pFunction->BindInput(2, "Y", "Y", 0.f);
				pFunction->BindInput(3, "Z", "Z", 0.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::Expand, "e0170ced-db1d-4438-ba58-02fa2b713f59");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Expand 3D vector");
				pFunction->BindInput(0, "Value", "Value", Vec3(ZERO)); //pFunction->BindInput(0, "Result", "Result", Vec3(ZERO));
				pFunction->BindOutput(1, "X", "X");
				pFunction->BindOutput(2, "Y", "Y");
				pFunction->BindOutput(3, "Z", "Z");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::Length, "e8020fdb-a227-4431-b888-816ba10b6bcf");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate length");
				pFunction->BindInput(1, "Value", "Value", Vec3(ZERO));
				pFunction->BindOutput(0, "Length", "Length"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::Distance, "6138150A-CCD7-40D8-A696-0100516441D8");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Get distance between two 3D vectors");
				pFunction->BindInput(1, "A", "A", Vec3(ZERO));
				pFunction->BindInput(2, "B", "B", Vec3(ZERO));
				pFunction->BindOutput(0, "Distance", "Distance"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::DistanceSq, "E2A07540-D821-4C11-A1C2-D40C1D179469");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Gets squared distance between two 3D vectors");
				pFunction->BindInput(1, "A", "A", Vec3(ZERO));
				pFunction->BindInput(2, "B", "B", Vec3(ZERO));
				pFunction->BindOutput(0, "Distance", "Distance"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::Normalize, "d953506a-6710-472d-9017-5a969f936b19");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Normalize 3D vector");
				pFunction->BindInput(1, "Value", "Value", Vec3(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::Add, "e50a9ee9-99dd-4011-82f7-05c67e013abb");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Add A to B");
				pFunction->BindInput(1, "A", "A", Vec3(ZERO));
				pFunction->BindInput(2, "B", "B", Vec3(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::Subtract, "dbafe377-fce6-430f-92e5-38b579e9833b");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Subtract B from A");
				pFunction->BindInput(1, "A", "A", Vec3(ZERO));
				pFunction->BindInput(2, "B", "B", Vec3(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::Scale, "03d7b941-57f8-4731-b518-781ea3e1dd16");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Scale 3D vector");
				pFunction->BindInput(1, "A", "A", Vec3(ZERO));
				pFunction->BindInput(2, "B", "B", 1.f);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::DotProduct, "b2806327-f747-4dbb-905c-fadc027dbe2c");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate dot product of two 3D vectors");
				pFunction->BindInput(1, "A", "A", Vec3(ZERO));
				pFunction->BindInput(2, "B", "B", Vec3(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::CrossProduct, "17b6dcf4-6cc9-4a04-a5a9-876857245411");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Calculate cross product of two 3D vectors");
				pFunction->BindInput(1, "A", "A", Vec3(ZERO));
				pFunction->BindInput(2, "B", "B", Vec3(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::ToString, "4564e7fe-adcb-4545-8009-4acc713e305f");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert 3D vector to string");
				pFunction->BindInput(1, "Value", "Value", Vec3(ZERO));
				pFunction->BindOutput(0, "String", "String"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::ToVector2, "ecf32b36-d142-4f46-bbf6-6797cb4d574f");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert 3D vector to 2D vector");
				pFunction->SetDescription("Convert vector3 to vector2");
				pFunction->BindInput(1, "Value", "Value", Vec3(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::IsZero, "0c903001-4bfe-4f23-8308-00311268cba9");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Compare 3D vector to Zero");
				pFunction->BindInput(1, "Value", "Value", Vec3(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::DistanceToSegment, "786290a7-3b01-421c-8ed5-8a5fdc72493e");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Distance between a 3D vector and a segment of two 3D vectors");
				pFunction->BindInput(1, "P", "P", Vec3(ZERO));
				pFunction->BindInput(2, "Start", "Start", Vec3(ZERO));
				pFunction->BindInput(3, "End", "End", Vec3(ZERO));
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(Vector3::RotateVectorByAngleAroundAxis, "c8d46945-8d00-4f2f-9619-356c7af2c06d");
				pFunction->SetNamespace("Types::Vector3");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Return a vector obtained by rotating an initial vector counter clockwise by a given angle in relation to a given axis");
				pFunction->BindInput(1, "Axis", "Axis in relation to which the vector will be rotated", Vec3(ZERO));
				pFunction->BindInput(2, "Angle", "Angle of rotation - counter clockwise, in degrees", 0.0f);
				pFunction->BindInput(3, "V", "Initial vector that we want to rotate", Vec3(ZERO));

				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

		}
	}

	namespace QRotation
	{
		Schematyc2::QRotation FromVector3(const Vec3& forward, Vec3 up)
		{
			return Schematyc2::QRotation(Quat(MathUtils::CreateOrthonormalTM33(forward.normalized(), up.normalized())));
		}

		void ToVector3(Schematyc2::QRotation rot, Vec3& right, Vec3& forward, Vec3& up)
		{
			right   = rot.GetColumn0();
			forward = rot.GetColumn1();
			up      = rot.GetColumn2();
		}

		void TransformVector3(const Schematyc2::QRotation& rot, const Vec3& vector, Vec3& result)
		{
			result = rot * vector;
		}

		Schematyc2::CPoolString ToString(Schematyc2::QRotation value)
		{
			char temp[Schematyc2::StringUtils::s_quatStringBufferSize] = "";
			Schematyc2::StringUtils::QuatToString(value, temp);
			return temp;
		}

		static void Register(Schematyc2::IEnvRegistry& envRegistry)
		{
			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(QRotation::FromVector3, "0261e9ac-0b2e-4e71-b04d-9027a9cbd3c8");
				pFunction->SetNamespace("Types::QRotation");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Create quaternion rotation from vectors");
				pFunction->BindInput(1, "Forward", "Forward direction vector", Vec3Constants<float>::fVec3_OneY);
				pFunction->BindInput(2, "Up", "Up direction vector ", Vec3Constants<float>::fVec3_OneZ);
				pFunction->BindOutput(0, "Rotation", "Quaternion rotation"); //pFunction->BindOutput(0, "Result", "Quaternion rotation");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(QRotation::ToVector3, "e53f4ebd-db4a-4218-8f97-6480e6308965");
				pFunction->SetNamespace("Types::QRotation");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert quaternion rotation to right, forward and up 3D direction vectors");
				pFunction->BindInput(0, "Rotation", "Quaternion rotation", Schematyc2::QRotation());
				pFunction->BindOutput(1, "Right", "Right direction vector");
				pFunction->BindOutput(2, "Forward", "Forward direction vector");
				pFunction->BindOutput(3, "Up", "Up direction vector");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(QRotation::TransformVector3, "1abea881-67ed-43b5-8e18-2ff79623e692");
				pFunction->SetNamespace("Types::QRotation");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Transform 3d vector by quaternion rotation");
				pFunction->BindInput(0, "Rotation", "Quaternion rotation", Schematyc2::QRotation());
				pFunction->BindInput(1, "Vector", "Vector");
				pFunction->BindOutput(2, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(QRotation::ToString, "F2CAE291-9373-482C-9554-36D9FF49BE08");
				pFunction->SetNamespace("Types::QRotation");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert quaternion rotation to string");
				pFunction->BindInput(1, "Value", "Value", Schematyc2::QRotation());
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}
		}
	}

	namespace String
	{
		Schematyc2::CPoolString Append(const char* a, const char* b)
		{
			stack_string temp = a;
			temp.append(b);
			return temp.c_str();
		}

		bool Compare(const char* a, const char* b, EComparisonMode mode)
		{
			const int delta = strcmp(a, b);
			switch(mode)
			{
			case EComparisonMode::Equal:
				{
					return delta == 0;
				}
			case EComparisonMode::NotEqual:
				{
					return delta != 0;
				}
			case EComparisonMode::LessThan:
				{
					return delta < 0;
				}
			case EComparisonMode::LessThanOrEqual:
				{
					return delta <= 0;
				}
			case EComparisonMode::GreaterThan:
				{
					return delta > 0;
				}
			case EComparisonMode::GreaterThanOrEqual:
				{
					return delta >= 0;
				}
			default:
				{
					return false;
				}
			}
		}

		int32 ToInt32(const char* szValue)
		{
			return atoi(szValue);
		}

		uint32 ToUInt32(const char* szValue)
		{
			return strtoul(szValue, nullptr, 10);
		}

		static void Register(Schematyc2::IEnvRegistry& envRegistry)
		{
			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(String::Append, "f7984665-576c-44cb-8bbb-7401365faa7a");
				pFunction->SetNamespace("Types::String");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Combine A and B");
				pFunction->BindInput(1, "A", "A");
				pFunction->BindInput(2, "B", "B");
				pFunction->BindOutput(0, "String", "String"); //pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(String::Compare, "fd768ad6-4cf0-48f3-bf77-dbbbfdb670ef");
				pFunction->SetNamespace("Types::String");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Compare A and B");
				pFunction->BindInput(1, "A", "A");
				pFunction->BindInput(2, "B", "B");
				pFunction->BindInput(3, "Mode", "Comparison mode", EComparisonMode::Equal);
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(String::ToInt32, "8467C419-5829-408C-8A4D-C08F841ED303");
				pFunction->SetNamespace("Types::String");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert string to int32");
				pFunction->BindInput(1, "Value", "Value", "");
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}

			{
				Schematyc2::IGlobalFunctionPtr pFunction = SCHEMATYC2_MAKE_GLOBAL_FUNCTION_SHARED(String::ToUInt32, "158D991D-E5C0-4BB3-BE14-04252052520D");
				pFunction->SetNamespace("Types::String");
				pFunction->SetAuthor("Crytek");
				pFunction->SetDescription("Convert string to uint32");
				pFunction->BindInput(1, "Value", "Value", "");
				pFunction->BindOutput(0, "Result", "Result");
				envRegistry.RegisterGlobalFunction(pFunction);
			}
		}
	}

	void RegisterUtilFunctions()
	{
		Schematyc2::IEnvRegistry& envRegistry = gEnv->pSchematyc2->GetEnvRegistry();

		Bool::Register(envRegistry);
		Int32::Register(envRegistry);
		UInt32::Register(envRegistry);
		Float::Register(envRegistry);
		Vector2::Register(envRegistry);
		Vector3::Register(envRegistry);
		QRotation::Register(envRegistry);
		String::Register(envRegistry);
	}
}

SCHEMATYC2_GAME_ENV_AUTO_REGISTER(&SchematycBaseEnv::RegisterUtilFunctions)
