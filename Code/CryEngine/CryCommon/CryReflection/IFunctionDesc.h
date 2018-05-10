// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryReflection/IReflection.h>
#include <CryExtension/CryGUID.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include <tuple>

#include "IReflection.h"
#include "Function.h"
#include "IDescExtension.h"

namespace Cry {
namespace Reflection {

struct ITypeDesc;

enum class EFunctionParamFlags : uint32
{
	Empty       = 0,
	IsReference = BIT(0),
	IsConst     = BIT(1),
};
typedef CEnumFlags<EFunctionParamFlags> FunctionParamFlags;

struct IFunctionParameterDesc
{
	virtual ~IFunctionParameterDesc() {}

	virtual CryTypeId          GetTypeId() const = 0;
	virtual uint32             GetIndex() const = 0;

	virtual FunctionParamFlags GetFlags() const = 0;
	virtual void               SetFlags(FunctionParamFlags flags) = 0;

	virtual bool               IsConst() const = 0;
	virtual bool               IsReference() const = 0;
};

struct IFunctionDesc : public IExtensibleDesc
{
	virtual ~IFunctionDesc() {}

	virtual const CryGUID&                GetGuid() const = 0;

	virtual const char*                   GetLabel() const = 0;
	virtual void                          SetLabel(const char* szLabel) = 0;

	virtual const char*                   GetDescription() const = 0;
	virtual void                          SetDescription(const char* szDescription) = 0;

	virtual bool                          IsMemberFunction() const = 0;

	virtual const CFunction&              GetFunction() const = 0;

	virtual const ITypeDesc*              GetObjectTypeDesc() const = 0;
	virtual CryTypeId                     GetReturnTypeId() const = 0;

	virtual size_t                        GetParamCount() const = 0;
	virtual const IFunctionParameterDesc* GetParam(size_t index) const = 0;
};

template<typename FUNCTION_TYPE>
struct CFunctionDesc;

template<typename RETURN_TYPE, typename OBJECT_TYPE, typename ... PARAM_TYPES>
struct CFunctionDesc<RETURN_TYPE (OBJECT_TYPE::*)(PARAM_TYPES ...)>
{
	typedef RETURN_TYPE (OBJECT_TYPE::* FuncPtrType)(PARAM_TYPES ...);

	typedef RETURN_TYPE                 ReturnType;
	typedef OBJECT_TYPE                 ObjectType;
	typedef std::tuple<PARAM_TYPES ...> ParameterTypes;

	template<size_t INDEX>
	using ParamType = typename std::tuple_element<INDEX, ParameterTypes>::type;

	constexpr static bool IsConst() { return false; }
};

template<typename RETURN_TYPE, typename OBJECT_TYPE, typename ... PARAM_TYPES>
struct CFunctionDesc<RETURN_TYPE (OBJECT_TYPE::*)(PARAM_TYPES ...) const>
{
	typedef RETURN_TYPE (OBJECT_TYPE::* FuncPtrType)(PARAM_TYPES ...) const;

	typedef RETURN_TYPE                 ReturnType;
	typedef OBJECT_TYPE                 ObjectType;
	typedef std::tuple<PARAM_TYPES ...> ParameterTypes;

	template<size_t INDEX>
	using ParamType = typename std::tuple_element<INDEX, ParameterTypes>::type;

	constexpr static bool IsConst() { return true; }
};

struct SFunctionParameterDesc
{
	SFunctionParameterDesc(CryTypeId _typeId, uint32 _index)
		: typeId(_typeId)
		, index(_index)
	{}

	FunctionParamFlags flags;
	CryTypeId          typeId;
	uint32             index;
};

typedef std::vector<SFunctionParameterDesc> ParamArray;

class CMemberFunction
{
public:
	CMemberFunction()
		: m_isConst(false)
	{}

	bool              IsValid() const         { return m_function.IsValid(); }
	bool              IsConst() const         { return m_isConst; }

	const CryTypeId   GetReturnTypeId() const { return m_returnTypeId; }
	const CFunction&  GetFunction() const     { return m_function; }

	ParamArray&       GetParams()             { return m_params; }
	const ParamArray& GetParams() const       { return m_params; }

protected:
	CryTypeId  m_returnTypeId;
	CFunction  m_function;
	ParamArray m_params;
	bool       m_isConst;
};

namespace Helper {

template<typename RETURN_TYPE>
class SReturnType
{
public:
	SReturnType()
		: m_typeId(TypeIdOf<RETURN_TYPE>())
	{}

	operator CryTypeId() const
	{
		return m_typeId;
	}

private:
	CryTypeId m_typeId;
};

template<>
class SReturnType<void>
{
public:
	SReturnType() {}

	operator CryTypeId() const
	{
		return CryTypeId();
	}
};

template<typename FUNCTION_DESC, size_t INDEX, bool IS_END>
struct ParamIterator
{
	typedef FUNCTION_DESC FunctionDesc;

	static void Append(ParamArray& params)
	{
		params.emplace_back(Cry::TypeIdOf<typename FunctionDesc::template ParamType<INDEX>>(), INDEX);
		SFunctionParameterDesc& param = params.back();

		if (std::is_const<typename FunctionDesc::template ParamType<INDEX>>::value)
			param.flags.Add(EFunctionParamFlags::IsConst);
		if (std::is_reference<typename FunctionDesc::template ParamType<INDEX>>::value || std::is_pointer<typename FunctionDesc::template ParamType<INDEX>>::value)
			param.flags.Add(EFunctionParamFlags::IsReference);

		ParamIterator < FUNCTION_DESC, INDEX + 1, (INDEX + 1) < std::tuple_size<typename FunctionDesc::ParameterTypes>::value > ::Append(params);
	}
};

template<typename FUNCTION_DESC, size_t INDEX>
struct ParamIterator<FUNCTION_DESC, INDEX, false>
{
	static void Append(ParamArray& params) {}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE, size_t PARAM_COUNT>
struct SProxy
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		//static_assert(false, "Unsupported amount of parameters.");
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 0>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		CRY_ASSERT_MESSAGE(context.GetParamCount() == 0, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())();
		context.SetResult(result);
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 1>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 1 && pParam0, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0);
		context.SetResult(result);
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 2>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 2 && pParam0 && pParam1, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1);
		context.SetResult(result);
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 3>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 3 && pParam0 && pParam1 && pParam2, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2);
		context.SetResult(result);
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 4>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 4 && pParam0 && pParam1 && pParam2 && pParam3, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3);
		context.SetResult(result);
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 5>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 5 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4);
		context.SetResult(result);
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 6>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);
		typename FUNCTION_DESC::template ParamType<5>* const pParam5 = context.GetParam<typename FUNCTION_DESC::template ParamType<5>>(5);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 6 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4 && pParam5, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4, *pParam5);
		context.SetResult(result);
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 7>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);
		typename FUNCTION_DESC::template ParamType<5>* const pParam5 = context.GetParam<typename FUNCTION_DESC::template ParamType<5>>(5);
		typename FUNCTION_DESC::template ParamType<6>* const pParam6 = context.GetParam<typename FUNCTION_DESC::template ParamType<6>>(6);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 7 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4 && pParam5 && pParam6, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4, *pParam5, *pParam6);
		context.SetResult(result);
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 8>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);
		typename FUNCTION_DESC::template ParamType<5>* const pParam5 = context.GetParam<typename FUNCTION_DESC::template ParamType<5>>(5);
		typename FUNCTION_DESC::template ParamType<6>* const pParam6 = context.GetParam<typename FUNCTION_DESC::template ParamType<6>>(6);
		typename FUNCTION_DESC::template ParamType<7>* const pParam7 = context.GetParam<typename FUNCTION_DESC::template ParamType<7>>(7);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 8 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4 && pParam5 && pParam6 && pParam7, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4, *pParam5, *pParam6, *pParam7);
		context.SetResult(result);
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 9>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);
		typename FUNCTION_DESC::template ParamType<5>* const pParam5 = context.GetParam<typename FUNCTION_DESC::template ParamType<5>>(5);
		typename FUNCTION_DESC::template ParamType<6>* const pParam6 = context.GetParam<typename FUNCTION_DESC::template ParamType<6>>(6);
		typename FUNCTION_DESC::template ParamType<7>* const pParam7 = context.GetParam<typename FUNCTION_DESC::template ParamType<7>>(7);
		typename FUNCTION_DESC::template ParamType<8>* const pParam8 = context.GetParam<typename FUNCTION_DESC::template ParamType<8>>(8);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 9 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4 && pParam5 && pParam6 && pParam7 && pParam8, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4, *pParam5, *pParam6, *pParam7, *pParam8);
		context.SetResult(result);
	}
};

template<typename FUNCTION_DESC, typename RETURN_TYPE>
struct SProxy<FUNCTION_DESC, RETURN_TYPE, 10>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);
		typename FUNCTION_DESC::template ParamType<5>* const pParam5 = context.GetParam<typename FUNCTION_DESC::template ParamType<5>>(5);
		typename FUNCTION_DESC::template ParamType<6>* const pParam6 = context.GetParam<typename FUNCTION_DESC::template ParamType<6>>(6);
		typename FUNCTION_DESC::template ParamType<7>* const pParam7 = context.GetParam<typename FUNCTION_DESC::template ParamType<7>>(7);
		typename FUNCTION_DESC::template ParamType<8>* const pParam8 = context.GetParam<typename FUNCTION_DESC::template ParamType<8>>(8);
		typename FUNCTION_DESC::template ParamType<9>* const pParam9 = context.GetParam<typename FUNCTION_DESC::template ParamType<9>>(9);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 10 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4 && pParam5 && pParam6 && pParam7 && pParam8 && pParam9, "Parameter missmatch.");

		const RETURN_TYPE result = (context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4, *pParam5, *pParam6, *pParam7, *pParam8, *pParam9);
		context.SetResult(result);
	}
};

//////////////////////////////////////////////////////////////////////////
// RETURN_TYPE == void
//////////////////////////////////////////////////////////////////////////

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 0>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		CRY_ASSERT_MESSAGE(context.GetParamCount() == 0, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())();
	}
};

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 1>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 1 && pParam0, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0);
	}
};

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 2>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 2 && pParam0 && pParam1, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1);
	}
};

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 3>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 3 && pParam0 && pParam1 && pParam2, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2);
	}
};

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 4>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 4 && pParam0 && pParam1 && pParam2 && pParam3, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3);
	}
};

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 5>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 5 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4);
	}
};

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 6>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);
		typename FUNCTION_DESC::template ParamType<5>* const pParam5 = context.GetParam<typename FUNCTION_DESC::template ParamType<5>>(5);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 6 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4 && pParam5, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4, *pParam5);
	}
};

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 7>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);
		typename FUNCTION_DESC::template ParamType<5>* const pParam5 = context.GetParam<typename FUNCTION_DESC::template ParamType<5>>(5);
		typename FUNCTION_DESC::template ParamType<6>* const pParam6 = context.GetParam<typename FUNCTION_DESC::template ParamType<6>>(6);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 7 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4 && pParam5 && pParam6, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4, *pParam5, *pParam6);
	}
};

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 8>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);
		typename FUNCTION_DESC::template ParamType<5>* const pParam5 = context.GetParam<typename FUNCTION_DESC::template ParamType<5>>(5);
		typename FUNCTION_DESC::template ParamType<6>* const pParam6 = context.GetParam<typename FUNCTION_DESC::template ParamType<6>>(6);
		typename FUNCTION_DESC::template ParamType<7>* const pParam7 = context.GetParam<typename FUNCTION_DESC::template ParamType<7>>(7);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 8 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4 && pParam5 && pParam6 && pParam7, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4, *pParam5, *pParam6, *pParam7);
	}
};

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 9>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);
		typename FUNCTION_DESC::template ParamType<5>* const pParam5 = context.GetParam<typename FUNCTION_DESC::template ParamType<5>>(5);
		typename FUNCTION_DESC::template ParamType<6>* const pParam6 = context.GetParam<typename FUNCTION_DESC::template ParamType<6>>(6);
		typename FUNCTION_DESC::template ParamType<7>* const pParam7 = context.GetParam<typename FUNCTION_DESC::template ParamType<7>>(7);
		typename FUNCTION_DESC::template ParamType<8>* const pParam8 = context.GetParam<typename FUNCTION_DESC::template ParamType<8>>(8);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 9 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4 && pParam5 && pParam6 && pParam7 && pParam8, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4, *pParam5, *pParam6, *pParam7, *pParam8);
	}
};

template<typename FUNCTION_DESC>
struct SProxy<FUNCTION_DESC, void, 10>
{
	static void Execute(const CFunctionPtr& funcPtr, CFunctionExecutionContext& context)
	{
		typename FUNCTION_DESC::template ParamType<0>* const pParam0 = context.GetParam<typename FUNCTION_DESC::template ParamType<0>>(0);
		typename FUNCTION_DESC::template ParamType<1>* const pParam1 = context.GetParam<typename FUNCTION_DESC::template ParamType<1>>(1);
		typename FUNCTION_DESC::template ParamType<2>* const pParam2 = context.GetParam<typename FUNCTION_DESC::template ParamType<2>>(2);
		typename FUNCTION_DESC::template ParamType<3>* const pParam3 = context.GetParam<typename FUNCTION_DESC::template ParamType<3>>(3);
		typename FUNCTION_DESC::template ParamType<4>* const pParam4 = context.GetParam<typename FUNCTION_DESC::template ParamType<4>>(4);
		typename FUNCTION_DESC::template ParamType<5>* const pParam5 = context.GetParam<typename FUNCTION_DESC::template ParamType<5>>(5);
		typename FUNCTION_DESC::template ParamType<6>* const pParam6 = context.GetParam<typename FUNCTION_DESC::template ParamType<6>>(6);
		typename FUNCTION_DESC::template ParamType<7>* const pParam7 = context.GetParam<typename FUNCTION_DESC::template ParamType<7>>(7);
		typename FUNCTION_DESC::template ParamType<8>* const pParam8 = context.GetParam<typename FUNCTION_DESC::template ParamType<8>>(8);
		typename FUNCTION_DESC::template ParamType<9>* const pParam9 = context.GetParam<typename FUNCTION_DESC::template ParamType<9>>(9);

		CRY_ASSERT_MESSAGE(context.GetParamCount() == 10 && pParam0 && pParam1 && pParam2 && pParam3 && pParam4 && pParam5 && pParam6 && pParam7 && pParam8 && pParam9, "Parameter missmatch.");

		(context.GetObject<typename FUNCTION_DESC::ObjectType>()->*funcPtr.Get<typename FUNCTION_DESC::FuncPtrType>())(*pParam0, *pParam1, *pParam2, *pParam3, *pParam4, *pParam5, *pParam6, *pParam7, *pParam8, *pParam9);
	}
};

}   // ~Helper namespace

template<typename FUNCTION_DESC>
class CMemberFunctionCreator : public CMemberFunction
{
public:
	typedef FUNCTION_DESC FunctionDesc;

	CMemberFunctionCreator(typename FunctionDesc::FuncPtrType pFunction)
	{
		m_returnTypeId = Helper::SReturnType<typename FunctionDesc::ReturnType>();

		m_params.reserve(std::tuple_size<typename FunctionDesc::ParameterTypes>::value);
		Helper::ParamIterator < FunctionDesc, 0, 0 < std::tuple_size<typename FunctionDesc::ParameterTypes>::value > ::Append(m_params);

		m_function = CFunction(pFunction, &Helper::SProxy<FunctionDesc, typename FunctionDesc::ReturnType, std::tuple_size<typename FunctionDesc::ParameterTypes>::value>::Execute);
	}

};

} // ~Cry namespace
} // ~Reflection namespace
