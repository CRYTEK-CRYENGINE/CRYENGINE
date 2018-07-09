// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/CryVariant.h>
#include <CryMemory/CrySizer.h>
#include <CryNetwork/SerializeFwd.h>
#include <CryNetwork/ISerialize.h>

#include <CryEntitySystem/IEntity.h>

#define _UICONFIG(x) x

struct IFlowGraphModuleManager;
struct IFlowGraphDebugger;
struct IGameTokenSystem;

typedef uint8  TFlowPortId;
typedef uint16 TFlowNodeId;
typedef uint16 TFlowNodeTypeId;
typedef uint32 TFlowGraphId;
typedef int    TFlowSystemContainerId;

static const TFlowNodeId InvalidFlowNodeId = ~TFlowNodeId(0);
static const TFlowPortId InvalidFlowPortId = ~TFlowPortId(0);
static const TFlowNodeTypeId InvalidFlowNodeTypeId = 0; //!< Must be 0 - FlowSystem.cpp relies on it.
static const TFlowGraphId InvalidFlowGraphId = ~TFlowGraphId(0);
static const TFlowSystemContainerId InvalidContainerId = ~TFlowSystemContainerId(0);

//! This is a special type which means "no input data".
struct SFlowSystemVoid
{
	void Serialize(TSerialize ser)               {}
	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

inline bool operator==(const SFlowSystemVoid& a, const SFlowSystemVoid& b)
{
	return true;
}

//! By adding types to this list, we can extend the flow system to handle new data types.
//! Important: If types need to be added, add them at the end, otherwise it breaks serialization.
//! \note CFlowData::ConfigureInputPort must be updated simultaneously.
//! \see CFlowData::ConfigureInputPort.
typedef CryVariant<
	SFlowSystemVoid,
	int,
	float,
	EntityId,
	Vec3,
	string,
	bool
	> TFlowInputDataVariant;

enum EFlowDataTypes
{
	eFDT_Any = -1,
	eFDT_Void = cry_variant::get_index<SFlowSystemVoid, TFlowInputDataVariant>::value,
	eFDT_Int = cry_variant::get_index<int, TFlowInputDataVariant>::value,
	eFDT_Float = cry_variant::get_index<float, TFlowInputDataVariant>::value,
	eFDT_EntityId = cry_variant::get_index<EntityId, TFlowInputDataVariant>::value,
	eFDT_Vec3 = cry_variant::get_index<Vec3, TFlowInputDataVariant>::value,
	eFDT_String = cry_variant::get_index<string, TFlowInputDataVariant>::value,
	eFDT_Bool = cry_variant::get_index<bool, TFlowInputDataVariant>::value,
};

inline EFlowDataTypes FlowNameToType(const char *typeName)
{
	EFlowDataTypes flowDataType = eFDT_Any;
	if (0 == strcmp(typeName, "Void"))
		flowDataType = eFDT_Void;
	else if (0 == strcmp(typeName, "Int"))
		flowDataType = eFDT_Int;
	else if (0 == strcmp(typeName, "Float"))
		flowDataType = eFDT_Float;
	else if (0 == strcmp(typeName, "EntityId"))
		flowDataType = eFDT_EntityId;
	else if (0 == strcmp(typeName, "Vec3"))
		flowDataType = eFDT_Vec3;
	else if (0 == strcmp(typeName, "String"))
		flowDataType = eFDT_String;
	else if (0 == strcmp(typeName, "Bool"))
		flowDataType = eFDT_Bool;

	return flowDataType;
}

inline const char* FlowTypeToName(EFlowDataTypes flowDataType)
{
	switch (flowDataType)
	{
	case eFDT_Any:
		return "Any";
	case eFDT_Void:
		return "Void";
	case eFDT_Int:
		return "Int";
	case eFDT_Float:
		return "Float";
	case eFDT_EntityId:
		return "EntityId";
	case eFDT_Vec3:
		return "Vec3";
	case eFDT_String:
		return "String";
	case eFDT_Bool:
		return "Bool";
	}
	return "";
}

inline const char* FlowTypeToHumanName(EFlowDataTypes flowDataType)
{
	const char* szTypeName = FlowTypeToName(flowDataType);
	if (*szTypeName == '\0')
		return "Unrecognized Flow Data type";
	else
		return szTypeName;
}

//! \cond INTERNAL
//! Default conversion uses C++ rules.
template<class From, class To>
struct SFlowSystemConversion
{
	static ILINE bool ConvertValue(const From& from, To& to)
	{
		to = static_cast<To>(from);
		return true;
	}
};

namespace cry_variant
{
	template<class To, size_t I = 0>
	ILINE bool ConvertVariant(const TFlowInputDataVariant& from, To& to)
	{
		if (from.index() == I)
		{
			return SFlowSystemConversion<typename stl::variant_alternative<I, TFlowInputDataVariant>::type, To>::ConvertValue(stl::get<I>(from), to);
		}
		else
		{
			return ConvertVariant<To, I + 1>(from, to);
		}
	}

	template<size_t I = 0>
	ILINE bool ConvertVariant(const TFlowInputDataVariant& from, Vec3& to)
	{
		if (stl::holds_alternative<Vec3>(from))
		{
			to = stl::get<Vec3>(from);
		}
		else
		{
			float temp;
			if (from.index() == I)
			{
				if (!SFlowSystemConversion<typename stl::variant_alternative<I, TFlowInputDataVariant>::type, float>::ConvertValue(stl::get<I>(from), temp))
					return false;
			}
			else
			{
				return ConvertVariant<float, I + 1>(from, temp);
			}
			to.x = to.y = to.z = temp;
		}
		return true;
	}

	template<size_t I = 0>
	ILINE bool ConvertVariant(const TFlowInputDataVariant& from, TFlowInputDataVariant& to)
	{
		if (from.index() == I)
		{
			typename stl::variant_alternative<I, TFlowInputDataVariant>::type temp;
			if (!ConvertVariant(from, temp))
				return false;
			to = temp;
			return true;
		}
		else
		{
			return ConvertVariant<TFlowInputDataVariant, I + 1>(from, to);
		}
	}

#define FLOWSYSTEM_CONVERTVARIANT_SPECIALIZATION(T) \
template<> \
ILINE bool ConvertVariant<T, stl::variant_size<TFlowInputDataVariant>::value>(const TFlowInputDataVariant&, T&) \
{ \
	CRY_ASSERT_MESSAGE(false, "Invalid variant index."); \
	return false; \
}
	FLOWSYSTEM_CONVERTVARIANT_SPECIALIZATION(SFlowSystemVoid);
	FLOWSYSTEM_CONVERTVARIANT_SPECIALIZATION(int);
	FLOWSYSTEM_CONVERTVARIANT_SPECIALIZATION(float);
	FLOWSYSTEM_CONVERTVARIANT_SPECIALIZATION(EntityId);
	FLOWSYSTEM_CONVERTVARIANT_SPECIALIZATION(Vec3);
	FLOWSYSTEM_CONVERTVARIANT_SPECIALIZATION(string);
	FLOWSYSTEM_CONVERTVARIANT_SPECIALIZATION(bool);
	FLOWSYSTEM_CONVERTVARIANT_SPECIALIZATION(TFlowInputDataVariant);
#undef FLOWSYSTEM_CONVERTVARIANT_SPECIALIZATION

	template<class From, size_t I = 0>
	ILINE bool ConvertToVariant(const From& from, TFlowInputDataVariant& to)
	{
		if (to.index() == I)
		{
			typedef typename stl::variant_alternative<I, TFlowInputDataVariant>::type TypeInVariant;
			if (std::is_same<TypeInVariant, From>::value)
			{
				to = from;
			}
			else
			{
				TypeInVariant temp;
				if (!SFlowSystemConversion<From, TypeInVariant>::ConvertValue(from, temp))
					return false;
				to = temp;
			}
			return true;
		}
		else
		{
			return ConvertToVariant<From, I + 1>(from, to);
		}
	}
#define FLOWSYSTEM_CONVERTTOVARIANT_SPECIALIZATION(T) \
template<> \
ILINE bool ConvertToVariant<T, stl::variant_size<TFlowInputDataVariant>::value>(const T&, TFlowInputDataVariant&) \
{ \
	CRY_ASSERT_MESSAGE(false, "Invalid variant index."); \
	return false; \
}
	FLOWSYSTEM_CONVERTTOVARIANT_SPECIALIZATION(SFlowSystemVoid);
	FLOWSYSTEM_CONVERTTOVARIANT_SPECIALIZATION(int);
	FLOWSYSTEM_CONVERTTOVARIANT_SPECIALIZATION(float);
	FLOWSYSTEM_CONVERTTOVARIANT_SPECIALIZATION(EntityId);
	FLOWSYSTEM_CONVERTTOVARIANT_SPECIALIZATION(Vec3);
	FLOWSYSTEM_CONVERTTOVARIANT_SPECIALIZATION(string);
	FLOWSYSTEM_CONVERTTOVARIANT_SPECIALIZATION(bool);
	FLOWSYSTEM_CONVERTTOVARIANT_SPECIALIZATION(TFlowInputDataVariant);
#undef FLOWSYSTEM_CONVERTTOVARIANT_SPECIALIZATION
}
template<class To>
struct SFlowSystemConversion<TFlowInputDataVariant, To>
{
	static ILINE bool ConvertValue(const TFlowInputDataVariant& from, To& to)
	{
		return cry_variant::ConvertVariant(from, to);
	}
};

//! Special cases to avoid ambiguity.
template<>
struct SFlowSystemConversion<TFlowInputDataVariant, bool>
{
	static ILINE bool ConvertValue(const TFlowInputDataVariant& from, bool& to)
	{
		return cry_variant::ConvertVariant(from, to);
	}
};
template<>
struct SFlowSystemConversion<TFlowInputDataVariant, Vec3>
{
	static ILINE bool ConvertValue(const TFlowInputDataVariant& from, Vec3& to)
	{
		return cry_variant::ConvertVariant(from, to);
	}
};
template<>
struct SFlowSystemConversion<TFlowInputDataVariant, TFlowInputDataVariant>
{
	static ILINE bool ConvertValue(const TFlowInputDataVariant& from, TFlowInputDataVariant& to)
	{
		return cry_variant::ConvertVariant(from, to);
	}
};

#define FLOWSYSTEM_NO_CONVERSION(T)                                                   \
  template<> struct SFlowSystemConversion<T, T> {                                     \
    static ILINE bool ConvertValue(const T &from, T & to) { to = from; return true; } \
  }
FLOWSYSTEM_NO_CONVERSION(SFlowSystemVoid);
FLOWSYSTEM_NO_CONVERSION(int);
FLOWSYSTEM_NO_CONVERSION(float);
FLOWSYSTEM_NO_CONVERSION(EntityId);
FLOWSYSTEM_NO_CONVERSION(Vec3);
FLOWSYSTEM_NO_CONVERSION(string);
FLOWSYSTEM_NO_CONVERSION(bool);
#undef FLOWSYSTEM_NO_CONVERSION

//! Specialization for converting to bool to avoid compiler warnings.
template<class From>
struct SFlowSystemConversion<From, bool>
{
	static ILINE bool ConvertValue(const From& from, bool& to)
	{
		to = (from != 0);
		return true;
	}
};
//! "Void" conversion needs some help.
//! Converting from void to anything yields default value of type.
template<class To>
struct SFlowSystemConversion<SFlowSystemVoid, To>
{
	static ILINE bool ConvertValue(SFlowSystemVoid, To& to)
	{
		to = To();
		return true;
	}
};
template<>
struct SFlowSystemConversion<SFlowSystemVoid, bool>
{
	static ILINE bool ConvertValue(const SFlowSystemVoid& from, bool& to)
	{
		to = false;
		return true;
	}
};
template<>
struct SFlowSystemConversion<SFlowSystemVoid, Vec3>
{
	static ILINE bool ConvertValue(const SFlowSystemVoid& from, Vec3& to)
	{
		to = Vec3(0, 0, 0);
		return true;
	}
};
template<>
struct SFlowSystemConversion<Vec3, SFlowSystemVoid>
{
	static ILINE bool ConvertValue(const Vec3& from, SFlowSystemVoid&)
	{
		return true;
	}
};

//! Converting to void is trivial - just lose the value info.
template<class From>
struct SFlowSystemConversion<From, SFlowSystemVoid>
{
	static ILINE bool ConvertValue(const From& from, SFlowSystemVoid&)
	{
		return true;
	}
};

#define FLOWSYSTEM_STRING_CONVERSION(type, fmt)                   \
  template<>                                                      \
  struct SFlowSystemConversion<type, string>                      \
  {                                                               \
    static ILINE bool ConvertValue(const type &from, string & to) \
    {                                                             \
      to.Format(fmt, from);                                       \
      return true;                                                \
    }                                                             \
  };                                                              \
  template<>                                                      \
  struct SFlowSystemConversion<string, type>                      \
  {                                                               \
    static ILINE bool ConvertValue(const string &from, type & to) \
    {                                                             \
      return 1 == sscanf(from.c_str(), fmt, &to);                 \
    }                                                             \
  };

FLOWSYSTEM_STRING_CONVERSION(int, "%d");
FLOWSYSTEM_STRING_CONVERSION(float, "%g");
FLOWSYSTEM_STRING_CONVERSION(EntityId, "%u");

#undef FLOWSYSTEM_STRING_CONVERSION

template<>
struct SFlowSystemConversion<bool, string>
{
	static ILINE bool ConvertValue(const bool& from, string& to)
	{
		to.Format("%s", from ? "true" : "false");
		return true;
	}
};

template<>
struct SFlowSystemConversion<string, bool>
{
	//! Leaves 'to' unchanged in case of error.
	static ILINE bool ConvertValue(const string& from, bool& to)
	{
		int to_i;
		if (1 == sscanf(from.c_str(), "%d", &to_i))
		{
			if (to_i == 0 || to_i == 1)
			{
				to = (to_i == 1);
				return true;
			}
		}
		else
		{
			if (0 == stricmp(from.c_str(), "true"))
			{
				to = true;
				return true;
			}
			if (0 == stricmp(from.c_str(), "false"))
			{
				to = false;
				return true;
			}
		}
		return false;
	}
};

template<>
struct SFlowSystemConversion<Vec3, string>
{
	static ILINE bool ConvertValue(const Vec3& from, string& to)
	{
		to.Format("%g,%g,%g", from.x, from.y, from.z);
		return true;
	}
};

template<>
struct SFlowSystemConversion<string, Vec3>
{
	static ILINE bool ConvertValue(const string& from, Vec3& to)
	{
		return 3 == sscanf(from.c_str(), "%g,%g,%g", &to.x, &to.y, &to.z);
	}
};

//! Vec3 conversions.
template<class To>
struct SFlowSystemConversion<Vec3, To>
{
	static ILINE bool ConvertValue(const Vec3& from, To& to)
	{
		return SFlowSystemConversion<float, To>::ConvertValue(from.x, to);
	}
};
template<class From>
struct SFlowSystemConversion<From, Vec3>
{
	static ILINE bool ConvertValue(const From& from, Vec3& to)
	{
		float temp;
		if (!SFlowSystemConversion<From, float>::ConvertValue(from, temp))
			return false;
		to.x = to.y = to.z = temp;
		return true;
	}
};
template<>
struct SFlowSystemConversion<Vec3, bool>
{
	static ILINE bool ConvertValue(const Vec3& from, bool& to)
	{
		to = from.GetLengthSquared() > 0;
		return true;
	}
};

//! Conversions to variant
template<class From>
struct SFlowSystemConversion<From, TFlowInputDataVariant>
{
	static ILINE bool ConvertValue(const From& from, TFlowInputDataVariant& to)
	{
		return cry_variant::ConvertToVariant(from, to);
	}
};
template<>
struct SFlowSystemConversion<SFlowSystemVoid, TFlowInputDataVariant>
{
	static ILINE bool ConvertValue(const SFlowSystemVoid& from, TFlowInputDataVariant& to)
	{
		return cry_variant::ConvertToVariant(from, to);
	}
};
template<>
struct SFlowSystemConversion<Vec3, TFlowInputDataVariant>
{
	static ILINE bool ConvertValue(const Vec3& from, TFlowInputDataVariant& to)
	{
		return cry_variant::ConvertToVariant(from, to);
	}
};

template<class T>
struct DefaultInitialized
{
	T operator()() const { return T(); }
};

template<>
struct DefaultInitialized<Vec3>
{
	Vec3 operator()() const { return Vec3(ZERO); }
};

struct DefaultInitializedForTag
{
	bool operator()(EFlowDataTypes type, TFlowInputDataVariant& v) const
	{
		return Initialize(type, v);
	}

private:
	template<size_t I = 0>
	bool Initialize(EFlowDataTypes type, TFlowInputDataVariant& var) const
	{
		if (type == I)
		{
			DefaultInitialized<typename stl::variant_alternative<I, TFlowInputDataVariant>::type> create;
			var = create();
			return true;
		}
		else
		{
			return Initialize<I + 1>(type, var);
		}
	}
};
template<>
inline bool DefaultInitializedForTag::Initialize<stl::variant_size<TFlowInputDataVariant>::value>(EFlowDataTypes type, TFlowInputDataVariant& var) const
{
	if (type == EFlowDataTypes::eFDT_Any)
	{
		return true;
	}

	CRY_ASSERT_MESSAGE(var.index() == stl::variant_npos, "Invalid variant index.");
	return false;
}
//! \endcond

class TFlowInputData
{
	class IsSameType
	{
		class StrictlyEqual
		{
		public:
			bool operator()(const TFlowInputDataVariant& a, const TFlowInputDataVariant& b)
			{
				return a.index() == b.index();
			}
		};

	public:
		bool operator()(const TFlowInputData& a, const TFlowInputData& b) const
		{
			return stl::visit(StrictlyEqual(), a.m_variant, b.m_variant);
		}
	};

	template<typename Ref>
	class IsSameTypeExpl
	{
		class StrictlyEqual
		{
		public:
			bool operator()(const TFlowInputDataVariant& variant) const
			{
				return stl::holds_alternative<Ref>(variant);
			}
		};

	public:
		bool operator()(const TFlowInputData& a) const
		{
			return stl::visit(StrictlyEqual(), a.m_variant);
		}
	};

	class IsEqual
	{
		class StrictlyEqual
		{
		public:
			template<typename T, typename U>
			bool operator()(const T&, const U&) const
			{
				return false; // cannot compare different types
			}

			template<typename T>
			bool operator()(const T& lhs, const T& rhs) const
			{
				return lhs == rhs;
			}
		};

	public:
		bool operator()(const TFlowInputData& a, const TFlowInputData& b) const
		{
			return stl::visit(StrictlyEqual(), a.m_variant, b.m_variant);
		}
	};

	template<typename To>
	class ConvertType_Get
	{
	public:
		ConvertType_Get(To& to_) : to(to_) {}

		template<typename From>
		bool operator()(const From& from) const
		{
			return SFlowSystemConversion<From, To>::ConvertValue(from, to);
		}

		To& to;
	};

	template<typename From>
	class ConvertType_Set
	{
	public:
		ConvertType_Set(const From& from_) : from(from_) {}

		template<typename To>
		bool operator()(To& to) const
		{
			return SFlowSystemConversion<From, To>::ConvertValue(from, to);
		}

		const From& from;
	};

	class ConvertType_SetFlexi
	{
	public:
		ConvertType_SetFlexi(TFlowInputData& to_) : to(to_) {}

		template<typename From>
		bool operator()(const From& from) const
		{
			return stl::visit(ConvertType_Set<From>(from), to.m_variant);
		}

		TFlowInputData& to;
	};

	class WriteType
	{
	public:
		WriteType(TSerialize ser, bool userFlag) : m_ser(ser), m_userFlag(userFlag) {}

		void operator()(TFlowInputDataVariant& var)
		{
			int index = var.index();
			m_ser.Value("tag", index);
			m_ser.Value("ud", m_userFlag);
			SerializeVariant(var);
		}

	private:
		template<size_t I = 0>
		void SerializeVariant(TFlowInputDataVariant& var)
		{
			if (var.index() == I)
			{
				m_ser.Value("v", stl::get<I>(var));
			}
			else
			{
				SerializeVariant<I + 1>(var);
			}
		}

		TSerialize m_ser;
		bool       m_userFlag;
	};

	class LoadType
	{
	public:
		LoadType(TSerialize ser) : m_ser(ser) {}

		void operator()(TFlowInputDataVariant& var)
		{
			int index;
			m_ser.Value("tag", index);
			if (index >= 0)
				SerializeVariant(var);
		}

	private:
		template<size_t I = 0>
		void SerializeVariant(TFlowInputDataVariant& var)
		{
			if (var.index() == I)
			{
				typename stl::variant_alternative<I, TFlowInputDataVariant>::type value;
				m_ser.Value("v", value);
				var.emplace<I>(value);
			}
			else
			{
				SerializeVariant<I + 1>(var);
			}
		}

		TSerialize m_ser;
	};

	class MemStatistics
	{
	public:
		MemStatistics(ICrySizer* pSizer) : m_pSizer(pSizer) {}

		void operator()(const TFlowInputDataVariant& var)
		{
			AddVariant(var);
		}

	private:
		template<size_t I = 0>
		void AddVariant(const TFlowInputDataVariant& var)
		{
			if (var.index() == I)
			{
				m_pSizer->AddObject(stl::get<I>(var));
			}
			else
			{
				AddVariant<I + 1>(var);
			}
		}

		ICrySizer* m_pSizer;
	};

public:
	TFlowInputData()
		: m_variant()
		, m_userFlag(false)
		, m_locked(false)
	{}

	TFlowInputData(const TFlowInputData& rhs)
		: m_variant(rhs.m_variant)
		, m_userFlag(rhs.m_userFlag)
		, m_locked(rhs.m_locked)
	{}

	template<typename T>
	explicit TFlowInputData(const T& v, bool locked = false)
		: m_variant(v)
		, m_userFlag(false)
		, m_locked(locked)
	{}

	template<typename T>
	static TFlowInputData CreateDefaultInitialized(bool locked = false)
	{
		DefaultInitialized<T> create;
		return TFlowInputData(create(), locked);
	}

	static TFlowInputData CreateDefaultInitializedForTag(int tag, bool locked = false)
	{
		TFlowInputDataVariant v;
		DefaultInitializedForTag create;
		create(static_cast<EFlowDataTypes>(tag), v);
		return TFlowInputData(v, locked);
	}

	TFlowInputData& operator=(const TFlowInputData& rhs)
	{
		if (this != &rhs)
		{
			m_variant = rhs.m_variant;
			m_userFlag = rhs.m_userFlag;
			m_locked = rhs.m_locked;
		}
		return *this;
	}

	bool operator==(const TFlowInputData& rhs) const
	{
		IsEqual isEqual;
		return isEqual(*this, rhs);
	}

	bool operator!=(const TFlowInputData& rhs) const
	{
		IsEqual isEqual;
		return !isEqual(*this, rhs);
	}

	bool SetDefaultForTag(int tag)
	{
		DefaultInitializedForTag set;
		return set(static_cast<EFlowDataTypes>(tag), m_variant);
	}

	template<typename T>
	bool Set(const T& value)
	{
		IsSameTypeExpl<T> isSameType;
		if (IsLocked() && !isSameType(*this))
		{
			return false;
		}
		else
		{
			m_variant = value;
			return true;
		}
	}

	template<typename T>
	bool SetValueWithConversion(const T& value)
	{
		IsSameTypeExpl<T> isSameType;
		if (IsLocked() && !isSameType(*this))
		{
			return stl::visit(ConvertType_Set<T>(value), m_variant);
		}
		else
		{
			m_variant = value;
			return true;
		}
	}

	bool SetValueWithConversion(const TFlowInputData& value)
	{
		IsSameType isSameType;
		if (IsLocked() && !isSameType(*this, value))
		{
			return stl::visit(ConvertType_SetFlexi(*this), value.m_variant);
		}
		else
		{
			m_variant = value.m_variant;
			return true;
		}
	}

	//! Checks if the current value matches the given string or if it would require a conversion due to incompatible with the datatype
	// eg. setting a Bool with '1' is valid, setting it with '12' is not (so this will return true). For both cases the FlowData will be set to true
	bool CheckIfForcedConversionOfCurrentValueWithString(const string& valueStr)
	{
		string convertedValueStr;
		/*return !*/ GetValueWithConversion(convertedValueStr); // Note: ideally that return value should be used
		// but the internal conversions are currently accepting some conversions that we really want a warning for
		// (eg float to int truncates and is accepted)

		// value was transformed, check if it is accepted for the type
		if (convertedValueStr.compare(valueStr) != 0)
		{
			switch (GetType())
			{
			case eFDT_Bool:
				{
					// ignore "0" and "1" for bools: conversion will ret "false" and "true" but this accepted as valid
					if (valueStr.compare("0") == 0 || valueStr.compare("1") == 0)
					{
						return false;
					}

					// "True" and "False" are valid with any casing
					if (valueStr.compareNoCase("true") == 0 || valueStr.compareNoCase("false") == 0)
					{
						return false;
					}
				}
				break;
			case eFDT_Float:
				{
					// ignore trailing zeros. eg input of '-1.0' will be '-1'
					size_t lastNotZero = valueStr.find_last_not_of('0');
					if (lastNotZero != string::npos)
					{
						size_t dotPos = valueStr.rfind('.');
						if (dotPos != string::npos)
						{
							if (dotPos == lastNotZero)
							{
								lastNotZero--;
							}
							if (strncmp(convertedValueStr, valueStr.c_str(), lastNotZero + 1) == 0)
							{
								return false;
							}
						}
					}
				}
				// intentional fallthrough : float also checks for int leading zeros
			case eFDT_Int:
				{
					// ignore leading zeros. eg input of '001' will be '1'
					size_t firstNotZero = valueStr.find_first_not_of('0');
					if (firstNotZero != string::npos)
					{
						if (strncmp(convertedValueStr, valueStr.c_str() + firstNotZero, valueStr.size() - firstNotZero) == 0)
						{
							return false;
						}
					}
				}
				break;
			}

			return true;
		}

		return false;
	}

	template<typename T> T*       GetPtr()       { return stl::get_if<T>(&m_variant); }
	template<typename T> const T* GetPtr() const { return stl::get_if<const T>(&m_variant); }

	template<typename T>
	bool GetValueWithConversion(T& value) const
	{
		IsSameTypeExpl<T> isSameType;
		if (isSameType(*this))
		{
			value = *GetPtr<T>();
			return true;
		}
		else
		{
			return stl::visit(ConvertType_Get<T>(value), m_variant);
		}
	}

	EFlowDataTypes GetType() const         { return static_cast<EFlowDataTypes>(m_variant.index()); }

	bool           IsUserFlagSet() const   { return m_userFlag; }
	void           SetUserFlag(bool value) { m_userFlag = value; }
	void           ClearUserFlag()         { m_userFlag = false; }

	bool           IsLocked() const        { return m_locked; }
	void           SetLocked()             { m_locked = true; }
	void           SetUnlocked()           { m_locked = false; }

	void           Serialize(TSerialize ser)
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Configurable variant serialization");

		if (ser.IsWriting())
		{
			WriteType visitor(ser, IsUserFlagSet());
			stl::visit(visitor, m_variant);

			bool locked = m_locked;
			ser.Value("locked", locked);
		}
		else
		{
			int tag = -2; // should be safe
			ser.Value("tag", tag);

			SetDefaultForTag(tag);

			bool ud = m_userFlag;
			ser.Value("ud", ud);
			m_userFlag = ud;

			LoadType visitor(ser);
			stl::visit(visitor, m_variant);

			bool locked;
			ser.Value("locked", locked);
			m_locked = locked;
		}
	}

	template<class Visitor>
	void Visit(Visitor& visitor)
	{
		stl::visit(visitor, m_variant);
	}

	template<class Visitor>
	void Visit(Visitor& visitor) const
	{
		stl::visit(visitor, m_variant);
	}

	TFlowInputDataVariant&       GetVariant() { return m_variant; }
	const TFlowInputDataVariant& GetVariant() const { return m_variant; }

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		MemStatistics visitor(pSizer);
		stl::visit(visitor, m_variant);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		GetMemoryStatistics(pSizer);
	}

private:
	friend struct SEqualFlowInputData;

	TFlowInputDataVariant m_variant;

	bool                  m_userFlag : 1;
	bool                  m_locked   : 1;
};

template<>
ILINE void TFlowInputData::LoadType::SerializeVariant<stl::variant_size<TFlowInputDataVariant>::value>(TFlowInputDataVariant& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}
template<>
ILINE void TFlowInputData::WriteType::SerializeVariant<stl::variant_size<TFlowInputDataVariant>::value>(TFlowInputDataVariant& var)
{
}
template<>
ILINE void TFlowInputData::MemStatistics::AddVariant<stl::variant_size<TFlowInputDataVariant>::value>(const TFlowInputDataVariant&)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}

struct SFlowAddress
{
	SFlowAddress(TFlowNodeId node, TFlowPortId port, bool isOutput)
	{
		this->node = node;
		this->port = port;
		this->isOutput = isOutput;
	}
	SFlowAddress()
	{
		node = InvalidFlowNodeId;
		port = InvalidFlowPortId;
		isOutput = true;
	}
	bool operator==(const SFlowAddress& n) const { return node == n.node && port == n.port && isOutput == n.isOutput; }
	bool operator!=(const SFlowAddress& n) const { return !(*this == n); }

	TFlowNodeId node;
	TFlowPortId port;
	bool        isOutput;
};

//! Flags used by the flow system.
enum EFlowNodeFlags
{
	EFLN_TARGET_ENTITY        = 0x0001, //!< CORE FLAG: This node targets an entity, entity id must be provided.
	EFLN_HIDE_UI              = 0x0002, //!< CORE FLAG: This node cannot be selected by user for placement in flow graph UI.
	EFLN_DYNAMIC_OUTPUT       = 0x0004, //!< CORE FLAG: This node is setup for dynamic output port growth in runtime.
	EFLN_UNREMOVEABLE         = 0x0008, //!< CORE FLAG: This node cannot be deleted by the user.
	EFLN_CORE_MASK            = 0x000F, //!< CORE_MASK.

	EFLN_APPROVED             = 0x0010, //!< CATEGORY:  This node is approved for designers.
	EFLN_ADVANCED             = 0x0020, //!< CATEGORY:  This node is slightly advanced and approved.
	EFLN_DEBUG                = 0x0040, //!< CATEGORY:  This node is for debug purpose only.
	EFLN_OBSOLETE             = 0x0200, //!< CATEGORY:  This node is obsolete and is not available in the editor.
	EFLN_CATEGORY_MASK        = 0x0FF0, //!< CATEGORY_MASK.

	EFLN_USAGE_MASK           = 0xF000,  //!< USAGE_MASK.

	EFLN_AISEQUENCE_ACTION    = 0x010000,
	EFLN_AISEQUENCE_SUPPORTED = 0x020000,
	EFLN_AISEQUENCE_END       = 0x040000,
	EFLN_AIACTION_START       = 0x080000,
	EFLN_AIACTION_END         = 0x100000,
	EFLN_TYPE_MASK            = 0xFF0000,
};

//////////////////////////////////////////////////////////////////////////
enum EFlowSpecialEntityId
{
	EFLOWNODE_ENTITY_ID_GRAPH1 = -1,
	EFLOWNODE_ENTITY_ID_GRAPH2 = -2,
};

struct SInputPortConfig
{
	//! Name of this port.
	const char* name;

	//! Human readable name of this port (default: same as name).
	const char* humanName;

	//! Human readable description of this port (help).
	const char* description;

	//! UIConfig: enums for the variable
	//! Example:
	//! "enum_string:a,b,c"
	//! "enum_string:something=a,somethingelse=b,whatever=c"
	//! "enum_int:something=0,somethingelse=10,whatever=20"
	//! "enum_float:something=1.0,somethingelse=2.0"
	//! "enum_global:GlobalEnumName"
	const char* sUIConfig;

	//! Default data.
	TFlowInputData defaultData;

	void           GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct SOutputPortConfig
{
	//! Name of this port.
	const char* name;

	//! Human readable name of this port (default: same as name).
	const char* humanName;

	//! Human readable description of this port (help).
	const char* description;

	//! Type of our output (or -1 for "dynamic").
	int  type;

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

template<class T>
ILINE SOutputPortConfig OutputPortConfig(const char* name, const char* description = NULL, const char* humanName = NULL)
{
	SOutputPortConfig result = { name, humanName, description, cry_variant::get_index<T, TFlowInputDataVariant>::value };
	return result;
}

ILINE SOutputPortConfig OutputPortConfig_AnyType(const char* name, const char* description = NULL, const char* humanName = NULL)
{
	SOutputPortConfig result = { name, humanName, description, eFDT_Any };
	return result;
}

ILINE SOutputPortConfig OutputPortConfig_Void(const char* name, const char* description = NULL, const char* humanName = NULL)
{
	SOutputPortConfig result = { name, humanName, description, eFDT_Void };
	return result;
}

template<class T>
ILINE SInputPortConfig InputPortConfig(const char* name, const char* description = NULL, const char* humanName = NULL, const char* sUIConfig = NULL)
{
	SInputPortConfig result = { name, humanName, description, sUIConfig, TFlowInputData::CreateDefaultInitialized<T>(true) };
	return result;
}

template<class T, class ValueT>
ILINE SInputPortConfig InputPortConfig(const char* name, const ValueT& value, const char* description = NULL, const char* humanName = NULL, const char* sUIConfig = NULL)
{
	SInputPortConfig result = { name, humanName, description, sUIConfig, TFlowInputData(T(value), true) };
	return result;
}

ILINE SInputPortConfig InputPortConfig_AnyType(const char* name, const char* description = NULL, const char* humanName = NULL, const char* sUIConfig = NULL)
{
	SInputPortConfig result = { name, humanName, description, sUIConfig, TFlowInputData(0, false) };
	return result;
}

ILINE SInputPortConfig InputPortConfig_Void(const char* name, const char* description = NULL, const char* humanName = NULL, const char* sUIConfig = NULL)
{
	SInputPortConfig result = { name, humanName, description, sUIConfig, TFlowInputData(SFlowSystemVoid(), false) };
	return result;
}

ILINE const char* CopyStr(const char* str)
{
	char* var = new char[strlen(str) + 1];
	memcpy(var, str, strlen(str) + 1);
	return var;
}

struct SFlowNodeConfig
{
	SFlowNodeConfig() : pInputPorts(0), pOutputPorts(0), nFlags(EFLN_DEBUG), sDescription(0), sUIClassName(0) {}

	const SInputPortConfig*  pInputPorts;
	const SOutputPortConfig* pOutputPorts;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Setup
	void SetDescription(const char* description)
	{
		sDescription = CopyStr(description);
	}

	// Input Config
	void SetInputSize(int size)
	{
		pInputPorts = new SInputPortConfig[size + 1];
		memset((void*)pInputPorts, 0, sizeof(SInputPortConfig) * (size + 1));
	}
	void AddInputPort(int slot, const SInputPortConfig& cfg)
	{
		memcpy((void*)(&pInputPorts[slot]), (void*)&cfg, sizeof(SInputPortConfig));
	}
	void AddStringInputPort(int slot, const char* name, const char* description)
	{
		AddInputPort(slot, InputPortConfig<string>(CopyStr(name), CopyStr(description)));
	}
	void AddVoidInputPort(int slot, const char* name, const char* description)
	{
		AddInputPort(slot, InputPortConfig_Void(CopyStr(name), CopyStr(description)));
	}
	void AddIntInputPort(int slot, const char* name, const char* description)
	{
		AddInputPort(slot, InputPortConfig<int>(CopyStr(name), CopyStr(description)));
	}
	void AddBoolInputPort(int slot, const char* name, const char* description)
	{
		AddInputPort(slot, InputPortConfig<bool>(CopyStr(name), CopyStr(description)));
	}

	// Output Config
	void SetOutputSize(int size)
	{
		pOutputPorts = new SOutputPortConfig[size + 1];
		memset((void*)pOutputPorts, 0, sizeof(SOutputPortConfig) * (size + 1));
	}
	void AddOutputPort(int slot, const SOutputPortConfig& cfg)
	{
		memcpy((void*)(&pOutputPorts[slot]), (void*)&cfg, sizeof(SOutputPortConfig));
	}
	void AddStringOutputPort(int slot, const char* name, const char* description)
	{
		AddOutputPort(slot, OutputPortConfig<string>(CopyStr(name), CopyStr(description)));
	}
	void AddVoidOutputPort(int slot, const char* name, const char* description)
	{
		AddOutputPort(slot, OutputPortConfig_Void(CopyStr(name), CopyStr(description)));
	}
	void AddIntOutputPort(int slot, const char* name, const char* description)
	{
		AddOutputPort(slot, OutputPortConfig<int>(CopyStr(name), CopyStr(description)));
	}
	void AddBoolOutputPort(int slot, const char* name, const char* description)
	{
		AddOutputPort(slot, OutputPortConfig<bool>(CopyStr(name), CopyStr(description)));
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! Node configuration flags
	uint32      nFlags;
	const char* sDescription;
	const char* sUIClassName;

	ILINE void  SetCategory(uint32 flags)
	{
		nFlags = (nFlags & ~EFLN_CATEGORY_MASK) | flags;
	}

	ILINE uint32 GetCategory() const
	{
		return /* static_cast<EFlowNodeFlags> */ (nFlags & EFLN_CATEGORY_MASK);
	}

	ILINE uint32 GetCoreFlags() const
	{
		return nFlags & EFLN_CORE_MASK;
	}

	ILINE uint32 GetUsageFlags() const
	{
		return nFlags & EFLN_USAGE_MASK;
	}

	ILINE uint32 GetTypeFlags() const
	{
		return nFlags & EFLN_TYPE_MASK;
	}

};

struct IAIAction;
struct ICustomAction;
struct IFlowGraph;

struct IFlowNode;
TYPEDEF_AUTOPTR(IFlowNode);
typedef IFlowNode_AutoPtr IFlowNodePtr;

struct IFlowNode : public _i_reference_target_t
{
	struct SActivationInfo
	{
		typedef void (*ActivateOutputCallback)(SActivationInfo *actInfo,int nOutputPort, const TFlowInputData& value);

		SActivationInfo(IFlowGraph* pGraph = 0, TFlowNodeId myID = 0, void* pUserData = 0, TFlowInputData* pInputPorts = 0)
		{
			this->pGraph = pGraph;
			this->myID = myID;
			this->m_pUserData = pUserData;
			this->pInputPorts = pInputPorts;
			this->pEntity = 0;
			this->connectPort = InvalidFlowPortId;
		}
		IFlowGraph*     pGraph;
		TFlowNodeId     myID;
		IEntity*        pEntity;
		TFlowPortId     connectPort;
		TFlowInputData* pInputPorts;
		void*           m_pUserData;
		
		//! Optional override for output activation callback
		ActivateOutputCallback activateOutputCallback = nullptr;
		

		//! Mono-specific Helper.
		TFlowInputData* GetInputPort(int idx) { return &pInputPorts[idx]; }
	};

	enum EFlowEvent
	{
		eFE_Update,
		eFE_Activate,               //!< Sent if one or more input ports have been activated.
		eFE_FinalActivate,          //!< Must be eFE_Activate+1 (same as activate, but at the end of FlowSystem:Update).
		eFE_PrecacheResources,      //!< Called after flow graph loading to precache resources inside flow nodes.
		eFE_Initialize,             //!< Sent once after level has been loaded. it is NOT called on Serialization!.
		eFE_FinalInitialize,        //!< Must be eFE_Initialize+1.
		eFE_SetEntityId,            //!< This event is send to set the entity of the FlowNode. Might also be sent in conjunction (pre) other events (like eFE_Initialize).
		eFE_Suspend,
		eFE_Resume,
		eFE_EditorInputPortDataSet,
		eFE_ConnectInputPort,
		eFE_DisconnectInputPort,
		eFE_ConnectOutputPort,
		eFE_DisconnectOutputPort,
		eFE_DontDoAnythingWithThisPlease
	};

	IFlowNode() = default;

	// <interfuscator:shuffle>
	virtual ~IFlowNode(){}

	//! Provides base copy and move semantic without copying ref count in _i_reference_target_t
	IFlowNode(IFlowNode const&) { }

	//! Provides base copy and move semantic without copying ref count in _i_reference_target_t
	IFlowNode& operator= (IFlowNode const&) { return *this; }

	//! notification to be overridden in C# flow node
	virtual void OnDelete() const {}
	
	//! override to kick off a notification for C# flow node.
	//! to be removed when we get rid of C# flow node completely
	virtual void Release() const override
	{
		if (--m_nRefCounter == 0)
		{
			OnDelete();
			delete this;
		}
		else if (m_nRefCounter < 0)
		{
			assert(0);
			CryFatalError("Deleting Reference Counted Object Twice");
		}
	}

	virtual IFlowNodePtr Clone(SActivationInfo*) = 0;

	virtual void         GetConfiguration(SFlowNodeConfig&) = 0;
	virtual bool         SerializeXML(SActivationInfo*, const XmlNodeRef& root, bool reading) = 0;
	virtual void         Serialize(SActivationInfo*, TSerialize ser) = 0;
	virtual void         PostSerialize(SActivationInfo*) = 0;
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo*) = 0;

	virtual void         GetMemoryUsage(ICrySizer* s) const = 0;

	//! Used to return the global UI enum name to use for the given input port, if the port defines a UI enum of type 'enum_global_def'.
	//! \param[in] portId The input port Id for which to determine the global enum name.
	//! \param[in] pNodeEntity Current entity attached to the node.
	//! \param[in] szName The common name defined with the port (enum_global_def:commonName).
	//! \param[out] outGlobalEnum The global enum name to use for this port.
	//! \returns true if a global enum name was determined and should be used. Otherwise the common name is used.
	virtual bool GetPortGlobalEnum(uint32 portId, IEntity* pNodeEntity, const char* szName, string& outGlobalEnum) const { return false; }

	// </interfuscator:shuffle>
};

//! \cond INTERNAL
//! Wraps IFlowNode for specific data.
struct IFlowNodeData
{
	// <interfuscator:shuffle>
	virtual ~IFlowNodeData(){}

	//! Gets a pointer to the node.
	virtual IFlowNode* GetNode() = 0;

	//! Gets the ID of the node type.
	virtual TFlowNodeTypeId GetNodeTypeId() = 0;

	//! Gets the node's name.
	virtual const char* GetNodeName() = 0;
	virtual void        GetConfiguration(SFlowNodeConfig&) = 0;

	//! Gets the amount of ports in the flownode
	virtual int      GetNumInputPorts() const = 0;
	virtual int      GetNumOutputPorts() const = 0;

	virtual EntityId GetCurrentForwardingEntity() const = 0;

	//! Access internal array of the node input data.
	virtual TFlowInputData* GetInputData() const = 0;
	// </interfuscator:shuffle>
};
//! \endcond

struct IFlowGraph;
TYPEDEF_AUTOPTR(IFlowGraph);
typedef IFlowGraph_AutoPtr IFlowGraphPtr;

struct IFlowGraphHook;
TYPEDEF_AUTOPTR(IFlowGraphHook);
typedef IFlowGraphHook_AutoPtr IFlowGraphHookPtr;

struct IFlowGraphHook
{
	enum EActivation
	{
		eFGH_Stop,
		eFGH_Pass,
		eFGH_Debugger_Input,
		eFGH_Debugger_Output,
	};
	// <interfuscator:shuffle>
	virtual ~IFlowGraphHook(){}
	virtual void         AddRef() = 0;
	virtual void         Release() = 0;
	virtual IFlowNodePtr CreateNode(IFlowNode::SActivationInfo*, TFlowNodeTypeId typeId) = 0;

	//! Returns false to disallow this creation!
	virtual bool CreatedNode(TFlowNodeId id, const char* name, TFlowNodeTypeId typeId, IFlowNodePtr pNode) = 0;

	//! This gets called if CreatedNode was called, and then cancelled later
	virtual void                        CancelCreatedNode(TFlowNodeId id, const char* name, TFlowNodeTypeId typeId, IFlowNodePtr pNode) = 0;

	virtual IFlowGraphHook::EActivation PerformActivation(IFlowGraphPtr pFlowgraph, TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const TFlowInputData* value) = 0;
	// </interfuscator:shuffle>
	void                                GetMemoryUsage(ICrySizer* pSizer) const {}
};
//! Structure that permits to iterate through the node of the flowsystem.
struct IFlowNodeIterator
{
	// <interfuscator:shuffle>
	virtual ~IFlowNodeIterator(){}
	virtual void           AddRef() = 0;
	virtual void           Release() = 0;
	virtual IFlowNodeData* Next(TFlowNodeId& id) = 0;
	// </interfuscator:shuffle>
};

//! \cond INTERNAL
//! Structure that permits to iterate through the edge of the flowsystem.
struct IFlowEdgeIterator
{
	//! Structure describing flowsystem edges.
	struct Edge
	{
		TFlowNodeId fromNodeId; //!< ID of the starting node.
		TFlowPortId fromPortId; //!< ID of the starting port.
		TFlowNodeId toNodeId;   //!< ID of the arrival node.
		TFlowPortId toPortId;   //!< ID of the arrival port.
	};

	// <interfuscator:shuffle>
	virtual ~IFlowEdgeIterator(){}
	virtual void AddRef() = 0;
	virtual void Release() = 0;
	virtual bool Next(Edge& edge) = 0;
	// </interfuscator:shuffle>
};
//! \endcond

TYPEDEF_AUTOPTR(IFlowNodeIterator);
typedef IFlowNodeIterator_AutoPtr IFlowNodeIteratorPtr;
TYPEDEF_AUTOPTR(IFlowEdgeIterator);
typedef IFlowEdgeIterator_AutoPtr IFlowEdgeIteratorPtr;

struct SFlowNodeActivationListener
{
	// <interfuscator:shuffle>
	virtual ~SFlowNodeActivationListener(){}
	virtual bool OnFlowNodeActivation(IFlowGraphPtr pFlowgraph, TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const char* value) = 0;
	// </interfuscator:shuffle>
};

//! \cond INTERNAL
namespace NFlowSystemUtils
{

//! This class helps define IFlowGraph by declaring typed virtual functions corresponding to TFlowSystemDataTypes.
template<typename T>
struct Wrapper;

template<>
struct Wrapper<SFlowSystemVoid>
{
	explicit Wrapper(const SFlowSystemVoid& v) : value(v) {}
	const SFlowSystemVoid& value;
};
template<>
struct Wrapper<int>
{
	explicit Wrapper(const int& v) : value(v) {}
	const int& value;
};
template<>
struct Wrapper<float>
{
	explicit Wrapper(const float& v) : value(v) {}
	const float& value;
};
template<>
struct Wrapper<EntityId>
{
	explicit Wrapper(const EntityId& v) : value(v) {}
	const EntityId& value;
};
template<>
struct Wrapper<Vec3>
{
	explicit Wrapper(const Vec3& v) : value(v) {}
	const Vec3& value;
};
template<>
struct Wrapper<string>
{
	explicit Wrapper(const string& v) : value(v) {}
	const string& value;
};
template<>
struct Wrapper<bool>
{
	explicit Wrapper(const bool& v) : value(v) {}
	const bool& value;
};
//! \endcond

struct IFlowSystemTyped
{
	virtual ~IFlowSystemTyped() {}

	virtual void DoActivatePort(const SFlowAddress, const Wrapper<SFlowSystemVoid>& value) = 0;
	virtual void DoActivatePort(const SFlowAddress, const Wrapper<int>& value) = 0;
	virtual void DoActivatePort(const SFlowAddress, const Wrapper<float>& value) = 0;
	virtual void DoActivatePort(const SFlowAddress, const Wrapper<EntityId>& value) = 0;
	virtual void DoActivatePort(const SFlowAddress, const Wrapper<Vec3>& value) = 0;
	virtual void DoActivatePort(const SFlowAddress, const Wrapper<string>& value) = 0;
	virtual void DoActivatePort(const SFlowAddress, const Wrapper<bool>& value) = 0;
};
}

//! Refers to NFlowSystemUtils::IFlowSystemTyped to see extra per-flow system type functions that are added to this interface.
struct IFlowGraph : public NFlowSystemUtils::IFlowSystemTyped
{
	enum EFlowGraphType
	{
		eFGT_Default = 0,
		eFGT_AIAction,
		eFGT_UIAction,
		eFGT_Module,
		eFGT_CustomAction,
		eFGT_MaterialFx
	};
	// <interfuscator:shuffle>
	virtual void          AddRef() = 0;
	virtual void          Release() = 0;
	virtual IFlowGraphPtr Clone() = 0;

	//! Clears flow graph, deletes all nodes and edges.
	virtual void Clear() = 0;

	virtual void RegisterHook(IFlowGraphHookPtr) = 0;
	virtual void UnregisterHook(IFlowGraphHookPtr) = 0;

	virtual IFlowNodeIteratorPtr CreateNodeIterator() = 0;
	virtual IFlowEdgeIteratorPtr CreateEdgeIterator() = 0;

	//! Assigns id of the default entity associated with this flow graph.
	virtual void SetGraphEntity(EntityId id, int nIndex = 0) = 0;

	//! Retrieves id of the default entity associated with this flow graph.
	virtual EntityId GetGraphEntity(int nIndex) const = 0;

	//! Enables/disables the flowgraph - used from editor.
	virtual void SetEnabled(bool bEnable) = 0;

	//! Checks if flowgraph is currently enabled.
	virtual bool IsEnabled() const = 0;

	//! Activates/deactivates the flowgraph - used from FlowGraphProxy during Runtime.
	virtual void SetActive(bool bActive) = 0;

	//! Checks if flowgraph is currently active.
	virtual bool IsActive() const = 0;

	virtual void UnregisterFromFlowSystem() = 0;


	virtual void SetType(IFlowGraph::EFlowGraphType type) = 0;
	virtual IFlowGraph::EFlowGraphType GetType() const = 0;

	//! Get a human readable name for debug purposes
	virtual const char* GetDebugName() const = 0;
	//! Set a human readable name for debug purposes
	virtual void SetDebugName(const char* sName) = 0;

	//! Primary game interface.

	virtual void Update() = 0;
	virtual bool SerializeXML(const XmlNodeRef& root, bool reading) = 0;
	virtual void Serialize(TSerialize ser) = 0;
	virtual void PostSerialize() = 0;

	//! Similar to Update, but sends an eFE_Initialize instead of an eFE_Update
	virtual void InitializeValues() = 0;

	//! Send eFE_PrecacheResources event to all flow graph nodes, so they can cache thierinternally used assets.
	virtual void PrecacheResources() = 0;

	virtual void EnsureSortedEdges() = 0;

	//! Node manipulation.
	//! ##@{.
	virtual SFlowAddress ResolveAddress(const char* addr, bool isOutput) = 0;
	virtual TFlowNodeId  ResolveNode(const char* name) = 0;
	virtual TFlowNodeId  CreateNode(TFlowNodeTypeId typeId, const char* name, void* pUserData = 0) = 0;

	//!< Deprecated.
	virtual TFlowNodeId     CreateNode(const char* typeName, const char* name, void* pUserData = 0) = 0;

	virtual IFlowNodeData*  GetNodeData(TFlowNodeId id) = 0;
	virtual bool            SetNodeName(TFlowNodeId id, const char* sName) = 0;
	virtual const char*     GetNodeName(TFlowNodeId id) = 0;
	virtual TFlowNodeTypeId GetNodeTypeId(TFlowNodeId id) = 0;
	virtual const char*     GetNodeTypeName(TFlowNodeId id) = 0;
	virtual void            RemoveNode(const char* name) = 0;
	virtual void            RemoveNode(TFlowNodeId id) = 0;
	virtual void            SetUserData(TFlowNodeId id, const XmlNodeRef& data) = 0;
	virtual XmlNodeRef      GetUserData(TFlowNodeId id) = 0;
	virtual bool            LinkNodes(SFlowAddress from, SFlowAddress to) = 0;
	virtual void            UnlinkNodes(SFlowAddress from, SFlowAddress to) = 0;
	//! ##@}.

	//! Registers hypergraph/editor listeners for visual flowgraph debugging.
	virtual void RegisterFlowNodeActivationListener(SFlowNodeActivationListener* listener) = 0;

	//! Removes hypergraph/editor listeners for visual flowgraph debugging.
	virtual void RemoveFlowNodeActivationListener(SFlowNodeActivationListener* listener) = 0;

	virtual bool NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const char* value) = 0;

	virtual void     SetEntityId(TFlowNodeId, EntityId) = 0;
	virtual EntityId GetEntityId(TFlowNodeId) = 0;

	virtual IFlowGraphPtr GetClonedFlowGraph() const = 0;

	//! Gets node configuration.
	//! \note Low level functions.
	virtual void GetNodeConfiguration(TFlowNodeId id, SFlowNodeConfig&) = 0;

	//! Node-graph interface.
	virtual void SetRegularlyUpdated(TFlowNodeId, bool) = 0;
	virtual void RequestFinalActivation(TFlowNodeId) = 0;
	virtual void ActivateNode(TFlowNodeId) = 0;
	virtual void ActivatePortAny(SFlowAddress output, const TFlowInputData&) = 0;

	//! Special function to activate port from other dll module and pass
	//! const char* string to avoid passing string class across dll boundaries
	virtual void                  ActivatePortCString(SFlowAddress output, const char* cstr) = 0;

	virtual bool                  SetInputValue(TFlowNodeId node, TFlowPortId port, const TFlowInputData&) = 0;
	virtual bool                  IsOutputConnected(SFlowAddress output) = 0;
	virtual const TFlowInputData* GetInputValue(TFlowNodeId node, TFlowPortId port) = 0;
	virtual bool                  GetActivationInfo(const char* nodeName, IFlowNode::SActivationInfo& actInfo) = 0;

	// Temporary solutions.

	//! Suspended flow graphs were needed for AI Action flow graphs.
	//! Suspended flow graphs shouldn't be updated!
	//! Nodes in suspended flow graphs should ignore OnEntityEvent notifications!
	virtual void SetSuspended(bool suspend = true) = 0;

	//! Checks if the flow graph is suspended.
	virtual bool IsSuspended() const = 0;

	virtual bool IsInInitializationPhase() const = 0;

	// AI action related.

	//! Sets an AI Action
	virtual void SetAIAction(IAIAction* pAIAction) = 0;

	//! Gets an AI Action
	virtual IAIAction* GetAIAction() const = 0;

	//! Sets a Custom Action
	virtual void SetCustomAction(ICustomAction* pCustomAction) = 0;

	//! Gets a Custom Action
	virtual ICustomAction* GetCustomAction() const = 0;

	virtual void           GetMemoryUsage(ICrySizer* s) const = 0;
	// </interfuscator:shuffle>

	template<class T>
	ILINE void ActivatePort(const SFlowAddress output, const T& value)
	{
		DoActivatePort(output, NFlowSystemUtils::Wrapper<T>(value));
	}
	ILINE void ActivatePort(SFlowAddress output, const TFlowInputData& value)
	{
		ActivatePortAny(output, value);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Mono-specific Helpers.
	void ActivateStringOutput(TFlowNodeId nodeId, int nPort, const string& value)
	{
		SFlowAddress addr(nodeId, nPort, true);
		ActivatePort(addr, value);
	}
	void ActivateIntOutput(TFlowNodeId nodeId, int nPort, const int& value)
	{
		SFlowAddress addr(nodeId, nPort, true);
		ActivatePort(addr, value);
	}
	void ActivateBoolOutput(TFlowNodeId nodeId, int nPort, const bool& value)
	{
		SFlowAddress addr(nodeId, nPort, true);
		ActivatePort(addr, value);
	}
	void ActivateAnyOutput(TFlowNodeId nodeId, int nPort)
	{
		SFlowAddress addr(nodeId, nPort, true);
		TFlowInputData value;
		ActivatePort(addr, value);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! \cond INTERNAL
	//! Graph tokens are gametokens which are unique to a particular flow graph.
	struct SGraphToken
	{
		SGraphToken() : type(eFDT_Void) {}

		string         name;
		EFlowDataTypes type;
	};
	//! \endcond

	virtual size_t                         GetGraphTokenCount() const = 0; //! Get the number of graph tokens for this graph
	virtual const IFlowGraph::SGraphToken* GetGraphToken(size_t index) const = 0; //! Get a graph token by index
	virtual const char*                    GetGlobalNameForGraphToken(const char* tokenName) const = 0; //! Get the corresponding name for the GTS registry
	virtual bool                           AddGraphToken(const SGraphToken& token) = 0; //! Add a token from the SGraphToken definition to this graph and to the GTS
	virtual void                           RemoveGraphTokens() = 0; //! Delete all token definitions from the graph and the corresponding tokens from the GTS registry

	virtual TFlowGraphId                   GetGraphId() const = 0; //! ID with which this graph is registered in the IFlowSystem
};

struct IFlowNodeFactory : public _i_reference_target_t
{
	// <interfuscator:shuffle>
	virtual ~IFlowNodeFactory(){}
	virtual IFlowNodePtr Create(IFlowNode::SActivationInfo*) = 0;
	virtual void         GetMemoryUsage(ICrySizer* s) const = 0;
	virtual void         Reset() = 0;
	virtual bool         AllowOverride() const { return false; }
	// </interfuscator:shuffle>
};

TYPEDEF_AUTOPTR(IFlowNodeFactory);
typedef IFlowNodeFactory_AutoPtr IFlowNodeFactoryPtr;

//! Structure that permits to iterate through the node type. For each node type the ID and the name are stored.
struct IFlowNodeTypeIterator
{
	struct SNodeType
	{
		TFlowNodeTypeId typeId;
		const char*     typeName;
	};
	// <interfuscator:shuffle>
	virtual ~IFlowNodeTypeIterator(){}
	virtual void AddRef() = 0;
	virtual void Release() = 0;
	virtual bool Next(SNodeType& nodeType) = 0;
	// </interfuscator:shuffle>
};
TYPEDEF_AUTOPTR(IFlowNodeTypeIterator);
typedef IFlowNodeTypeIterator_AutoPtr IFlowNodeTypeIteratorPtr;

struct IFlowGraphInspector
{
	struct IFilter;
	TYPEDEF_AUTOPTR(IFilter);
	typedef IFilter_AutoPtr IFilterPtr;

	struct IFilter
	{
		enum EFilterResult
		{
			eFR_DontCare, //!< Lets others decide (not used currently).
			eFR_Block,    //!< Blocks flow record.
			eFR_Pass      //!< Lets flow record pass.
		};
		// <interfuscator:shuffle>
		virtual ~IFilter(){}
		virtual void AddRef() = 0;
		virtual void Release() = 0;

		//! Adds a new filter
		//! \note Filter can have subfilters. (these are generally AND'ed)
		virtual void AddFilter(IFilterPtr pFilter) = 0;

		//! Removes the specified filter.
		virtual void RemoveFilter(IFilterPtr pFilter) = 0;

		//! Applies filter(s).
		virtual IFlowGraphInspector::IFilter::EFilterResult Apply(IFlowGraph* pGraph, const SFlowAddress from, const SFlowAddress to) = 0;
		// </interfuscator:shuffle>
	};

	// <interfuscator:shuffle>
	virtual ~IFlowGraphInspector(){}

	virtual void AddRef() = 0;
	virtual void Release() = 0;

	//! Called once per frame before IFlowSystem::Update
	virtual void PreUpdate(IFlowGraph* pGraph) = 0;

	//! Called once per frame after  IFlowSystem::Update
	virtual void PostUpdate(IFlowGraph* pGraph) = 0;
	virtual void NotifyFlow(IFlowGraph* pGraph, const SFlowAddress from, const SFlowAddress to) = 0;
	virtual void NotifyProcessEvent(IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo, IFlowNode* pImpl) = 0;
	virtual void AddFilter(IFlowGraphInspector::IFilterPtr pFilter) = 0;
	virtual void RemoveFilter(IFlowGraphInspector::IFilterPtr pFilter) = 0;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;
	// </interfuscator:shuffle>
};

struct IFlowSystemContainer
{
	virtual ~IFlowSystemContainer() {}

	virtual void           AddItem(TFlowInputData item) = 0;
	virtual void           AddItemUnique(TFlowInputData item) = 0;

	virtual void           RemoveItem(TFlowInputData item) = 0;

	virtual TFlowInputData GetItem(int i) = 0;
	virtual void           Clear() = 0;

	virtual void           RemoveItemAt(int i) = 0;
	virtual int            GetItemCount() const = 0;
	virtual void           GetMemoryUsage(ICrySizer* s) const = 0;
	virtual void           Serialize(TSerialize ser) = 0;
};

TYPEDEF_AUTOPTR(IFlowGraphInspector);
typedef IFlowGraphInspector_AutoPtr           IFlowGraphInspectorPtr;

typedef std::shared_ptr<IFlowSystemContainer> IFlowSystemContainerPtr;

struct IFlowSystemEngineModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IFlowSystemEngineModule, "96b19348-6ad3-427f-9a8f-7764052a5536"_cry_guid);
};

struct IFlowSystem
{
	// <interfuscator:shuffle>
	virtual ~IFlowSystem(){}

	virtual void Release() = 0;

	//! Updates the flow system.
	//! \note IGameFramework calls this every frame.
	virtual void Update() = 0;

	//! Resets the flow system.
	virtual void Reset(bool unload) = 0;

	virtual void Enable(bool enable) = 0;

	//! Reload all FlowNode templates; only in Sandbox!
	virtual void ReloadAllNodeTypes() = 0;

	//! Creates a flowgraph.
	virtual IFlowGraphPtr CreateFlowGraph() = 0;

	//! Type registration and resolving.
	virtual TFlowNodeTypeId RegisterType(const char* typeName, IFlowNodeFactoryPtr factory) = 0;

	//! Unregister previously registered type, keeps internal type ids valid
	virtual bool UnregisterType(const char* typeName) = 0;

	//! Checks whether the flow system has finished registering all node types and sent ESYSTEM_EVENT_REGISTER_FLOWNODES
	//! If this is true flow nodes have to be manually registered via the RegisterType function.
	virtual bool HasRegisteredDefaultFlowNodes() = 0;

	//! Gets a type name from a flow node type ID.
	virtual const char* GetTypeName(TFlowNodeTypeId typeId) = 0;

	//! Gets a flow node type ID from a type name.
	virtual TFlowNodeTypeId GetTypeId(const char* typeName) = 0;

	//! Creates an iterator to visit the node type.
	virtual IFlowNodeTypeIteratorPtr CreateNodeTypeIterator() = 0;

	// Inspecting flowsystem.

	//! Registers an inspector for the flowsystem.
	virtual void RegisterInspector(IFlowGraphInspectorPtr pInspector, IFlowGraphPtr pGraph = 0) = 0;

	//! Unregisters an inspector for the flowsystem.
	virtual void UnregisterInspector(IFlowGraphInspectorPtr pInspector, IFlowGraphPtr pGraph = 0) = 0;

	//! Gets the default inspector for the flowsystem.
	virtual IFlowGraphInspectorPtr GetDefaultInspector() const = 0;

	//! Enables/disables inspectors for the flowsystem.
	virtual void EnableInspecting(bool bEnable) = 0;

	//! Returns true if inspectoring is enable, false otherwise.
	virtual bool IsInspectingEnabled() const = 0;

	//! Returns the graph with the specified ID (NULL if not found)
	virtual IFlowGraph* GetGraphById(TFlowGraphId graphId) = 0;

	//! Notify the nodes that an entityId has changed on an existing entity - generates eFE_SetEntityId event.
	virtual void OnEntityIdChanged(EntityId oldId, EntityId newId) = 0;

	//! Gets memory statistics.
	virtual void GetMemoryUsage(ICrySizer* s) const = 0;

	//! Gets the module manager.
	virtual IFlowGraphModuleManager* GetIModuleManager() = 0;

	//! Gets the game tokens system
	virtual IGameTokenSystem* GetIGameTokenSystem() = 0;

	//! Create, Delete and access flowsystem's global containers.
	virtual bool                    CreateContainer(TFlowSystemContainerId id) = 0;
	virtual void                    DeleteContainer(TFlowSystemContainerId id) = 0;

	virtual IFlowSystemContainerPtr GetContainer(TFlowSystemContainerId id) = 0;

	virtual void                    Serialize(TSerialize ser) = 0;

	//! Creates an instance of IFlowNode by specified type.
	virtual IFlowNodePtr            CreateNodeOfType(IFlowNode::SActivationInfo*, TFlowNodeTypeId typeId) = 0;
	// </interfuscator:shuffle>
};

ILINE bool IsPortActive(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	return pActInfo->pInputPorts[nPort].IsUserFlagSet();
}

ILINE EFlowDataTypes GetPortType(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	return (EFlowDataTypes)pActInfo->pInputPorts[nPort].GetType();
}

ILINE const TFlowInputData& GetPortAny(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	return pActInfo->pInputPorts[nPort];
}

ILINE bool GetPortBool(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	bool result;
	return pActInfo->pInputPorts[nPort].GetValueWithConversion(result) ? result : false;
}

ILINE int GetPortInt(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	int result;
	return pActInfo->pInputPorts[nPort].GetValueWithConversion(result) ? result : 0;
}

ILINE EntityId GetPortEntityId(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	EntityId result;
	return pActInfo->pInputPorts[nPort].GetValueWithConversion(result) ? result : INVALID_ENTITYID;
}

ILINE float GetPortFloat(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	float result;
	return pActInfo->pInputPorts[nPort].GetValueWithConversion(result) ? result : 0.0f;
}

ILINE Vec3 GetPortVec3(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	Vec3 result;
	return pActInfo->pInputPorts[nPort].GetValueWithConversion(result) ? result : Vec3Constants<float>::fVec3_Zero;
}

ILINE string GetPortString(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	string result;
	if (!pActInfo->pInputPorts[nPort].GetValueWithConversion(result))
		result.clear();
	return result;
}

ILINE bool IsBoolPortActive(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	if (IsPortActive(pActInfo, nPort) && GetPortBool(pActInfo, nPort))
		return true;
	else
		return false;
}

template<class T>
ILINE void ActivateOutput(IFlowNode::SActivationInfo* pActInfo, int nPort, const T& value)
{
	SFlowAddress addr(pActInfo->myID, nPort, true);
	if (!pActInfo->activateOutputCallback)
	{
		pActInfo->pGraph->ActivatePort(addr, value);
	}
	else
	{
		TFlowInputData valueData;
		valueData.SetUserFlag(true);
		valueData.SetValueWithConversion(value);
		pActInfo->activateOutputCallback(pActInfo,nPort,valueData);
	}
}

ILINE bool IsOutputConnected(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	SFlowAddress addr(pActInfo->myID, nPort, true);
	return pActInfo->pGraph->IsOutputConnected(addr);
}
