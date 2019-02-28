// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef INCLUDING_FROM_TEMPLATE_UTILS_DELEGATE_HEADER
#error This file should only be included from TemplateUtils_Delegate.h!
#endif //INCLUDING_FROM_TEMPLATE_UTILS_DELEGATE_HEADER

#define PARAM_SEPARATOR                    PP_COMMA_IF(PARAM_COUNT)
#define TEMPLATE_PARAMS                    PP_ENUM_PARAMS(PARAM_COUNT, typename PARAM)
#define PROTOTYPE_PARAMS                   PP_ENUM_PARAMS(PARAM_COUNT, PARAM)
#define CAST_TEMPLATE_PARAMS               PP_ENUM_PARAMS(PARAM_COUNT, typename CAST_PARAM)
#define CAST_PROTOTYPE_PARAMS              PP_ENUM_PARAMS(PARAM_COUNT, CAST_PARAM)
#define INPUT_PARAMS_ELEMENT(i, user_data) PARAM##i param##i	
#define INPUT_PARAMS                       PP_COMMA_ENUM(PARAM_COUNT, INPUT_PARAMS_ELEMENT, PP_EMPTY)
#define OUTPUT_PARAMS                      PP_ENUM_PARAMS(PARAM_COUNT, param)

namespace TemplateUtils
{
	template <typename RETURN PARAM_SEPARATOR TEMPLATE_PARAMS> class CDelegate<RETURN (PROTOTYPE_PARAMS)>
	{
		template<typename TYPE> friend class CDelegate;

	public:

		inline CDelegate()
			: m_pStub(nullptr)
			, m_pContext(nullptr)
		{}

		inline CDelegate(const CDelegate& rhs)
		{
			Copy(rhs);
		}

		inline ~CDelegate()
		{
			Release();
		}

		inline CDelegate& operator = (const CDelegate& rhs)
		{
			Release();
			Copy(rhs);
			return *this;
		}

		inline operator bool () const
		{
			return m_pStub != nullptr;
		}

		inline RETURN operator() (INPUT_PARAMS) const
		{
			return (*m_pStub)(m_pContext PARAM_SEPARATOR OUTPUT_PARAMS);
		}

		inline bool operator < (const CDelegate& rhs) const
		{
			return (m_pContext < rhs.m_pContext) || ((m_pContext == rhs.m_pContext) && (m_pStub < rhs.m_pStub));
		}

		inline bool operator > (const CDelegate& rhs) const
		{
			return (m_pContext > rhs.m_pContext) || ((m_pContext == rhs.m_pContext) && (m_pStub > rhs.m_pStub));
		}

		inline bool operator <= (const CDelegate& rhs) const
		{
			return operator < (*this, rhs) || operator == (*this, rhs);
		}

		inline bool operator >= (const CDelegate& rhs) const
		{
			return operator > (*this, rhs) || operator == (*this, rhs);
		}

		inline bool operator == (const CDelegate& rhs) const
		{
			return (m_pStub == rhs.m_pStub) && (m_pContext == rhs.m_pContext);
		}

		inline bool operator != (const CDelegate& rhs) const
		{
			return (m_pStub != rhs.m_pStub) || (m_pContext != rhs.m_pContext);
		}

		template <RETURN (*FUNCTION_PTR)(PROTOTYPE_PARAMS)> static CDelegate FromGlobalFunction()
		{
			return CDelegate(GlobalFunctionStub<FUNCTION_PTR>);
		}

		template <class OBJECT, RETURN (OBJECT::*MEMBER_FUNCTION_PTR)(PROTOTYPE_PARAMS)> static CDelegate FromMemberFunction(OBJECT& object)
		{
			return CDelegate(MemberFunctionStub<OBJECT, MEMBER_FUNCTION_PTR>, &object);
		}

		template <class OBJECT, RETURN (OBJECT::*CONST_MEMBER_FUNCTION_PTR)(PROTOTYPE_PARAMS) const> static CDelegate FromConstMemberFunction(const OBJECT& object)
		{
			return CDelegate(ConstMemberFunctionStub<OBJECT, CONST_MEMBER_FUNCTION_PTR>, const_cast<OBJECT*>(&object));
		}

		template <typename CLOSURE> static inline CDelegate FromLambdaFunction(const CLOSURE& closure)
		{
			return CDelegate(LambdaFunctionStub<CLOSURE>, closure);
		}

		template <typename CAST_RETURN PARAM_SEPARATOR CAST_TEMPLATE_PARAMS> static CDelegate Cast(const CDelegate<CAST_RETURN (CAST_PROTOTYPE_PARAMS)>& rhs)
		{ 
			static_assert(Conversion<CAST_RETURN, RETURN>::Exists, "Cast return type must match or inherit from delegate return type!");
			static_assert(ListConversion<typename TL::BuildTypelist<CAST_PROTOTYPE_PARAMS>::Result, typename TL::BuildTypelist<PROTOTYPE_PARAMS>::Result>::Exists, "Cast parameters must match or inherit from delegate parameters!");
			return CDelegate(reinterpret_cast<const CDelegate&>(rhs));
		}

	private:

		typedef RETURN (DELEGATE_FASTCALL *StubPtr)(void* pObject PARAM_SEPARATOR PROTOTYPE_PARAMS);

		inline CDelegate(StubPtr pStub)
			: m_pStub(pStub)
			, m_pContext(nullptr)
		{}

		template <typename OBJECT> inline CDelegate(StubPtr pStub, OBJECT* pObject)
			: m_pStub(pStub)
			, m_pContext(pObject)
		{}

		template <typename CLOSURE> inline CDelegate(StubPtr pStub, const CLOSURE& closure)
			: m_pStub(pStub)
		{
			m_allocator.SetType<CLOSURE>();
			m_pContext = m_allocator.Clone(&closure);
		}

		inline void Copy(const CDelegate& rhs)
		{
			m_pStub     = rhs.m_pStub;
			m_allocator = rhs.m_allocator;
			m_pContext  = m_allocator ? m_allocator.Clone(rhs.m_pContext) : rhs.m_pContext;
		}

		inline void Release()
		{
			if(m_allocator)
			{
				m_allocator.Delete(m_pContext);
			}
		}

		template <RETURN (*FUNCTION_PTR)(PROTOTYPE_PARAMS)> static RETURN DELEGATE_FASTCALL GlobalFunctionStub(void*PARAM_SEPARATOR INPUT_PARAMS)
		{
			return (FUNCTION_PTR)(OUTPUT_PARAMS);
		}

		template <class OBJECT, RETURN (OBJECT::*MEMBER_FUNCTION_PTR)(PROTOTYPE_PARAMS)> static RETURN DELEGATE_FASTCALL MemberFunctionStub(void* pObject PARAM_SEPARATOR INPUT_PARAMS)
		{
			return ((static_cast<OBJECT*>(pObject))->*MEMBER_FUNCTION_PTR)(OUTPUT_PARAMS);
		}

		template <class OBJECT, RETURN (OBJECT::*CONST_MEMBER_FUNCTION_PTR)(PROTOTYPE_PARAMS) const> static RETURN DELEGATE_FASTCALL ConstMemberFunctionStub(void* pObject PARAM_SEPARATOR INPUT_PARAMS)
		{
			return ((const_cast<OBJECT*>(static_cast<const OBJECT*>(pObject)))->*CONST_MEMBER_FUNCTION_PTR)(OUTPUT_PARAMS);
		}

		template <typename CLOSURE> static RETURN DELEGATE_FASTCALL LambdaFunctionStub(void* pClosure PARAM_SEPARATOR INPUT_PARAMS)
		{
			return (*reinterpret_cast<CLOSURE*>(pClosure))(OUTPUT_PARAMS);
		}

		StubPtr                   m_pStub;
		void*                     m_pContext;
		CDelegateContextAllocator m_allocator;
	};

	template <typename OBJECT, typename RETURN PARAM_SEPARATOR TEMPLATE_PARAMS, RETURN (OBJECT::*FUNCTION_PTR)(PROTOTYPE_PARAMS)> struct SMakeDelegate<RETURN (OBJECT::*)(PROTOTYPE_PARAMS), FUNCTION_PTR>
	{
		inline CDelegate<RETURN (PROTOTYPE_PARAMS)> operator () (OBJECT& object) const
		{
			return CDelegate<RETURN (PROTOTYPE_PARAMS)>::template FromMemberFunction<OBJECT, FUNCTION_PTR>(object);
		}
	};

	template <typename OBJECT, typename RETURN PARAM_SEPARATOR TEMPLATE_PARAMS, RETURN (OBJECT::*FUNCTION_PTR)(PROTOTYPE_PARAMS) const> struct SMakeDelegate<RETURN (OBJECT::*)(PROTOTYPE_PARAMS) const, FUNCTION_PTR>
	{
		inline CDelegate<RETURN (PROTOTYPE_PARAMS)> operator () (const OBJECT& object) const
		{
			return CDelegate<RETURN (PROTOTYPE_PARAMS)>::template FromConstMemberFunction<OBJECT, FUNCTION_PTR>(object);
		}
	};
}

#undef PARAM_SEPARATOR
#undef TEMPLATE_PARAMS
#undef PROTOTYPE_PARAMS
#undef CAST_TEMPLATE_PARAMS
#undef CAST_PROTOTYPE_PARAMS
#undef INPUT_PARAMS_ELEMENT
#undef INPUT_PARAMS
#undef OUTPUT_PARAMS