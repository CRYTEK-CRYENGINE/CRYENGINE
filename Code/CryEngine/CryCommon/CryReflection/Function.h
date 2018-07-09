// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Cry {
namespace Reflection {

class CFunctionPtr
{
public:
	CFunctionPtr()
	{
		static_assert(sizeof(m_pointer) == (sizeof(uint64) * 2), "Please adapt code to buffer size.");
		uint64* ptr = reinterpret_cast<uint64*>(m_pointer);
		ptr[0] = ptr[1] = 0;
	}

	CFunctionPtr(const CFunctionPtr& functionPtr)
	{
		memcpy(m_pointer, &functionPtr, sizeof(m_pointer));
	}

	template<typename FUNCTION_TYPE>
	CFunctionPtr(FUNCTION_TYPE pFunction)
	{
		static_assert(sizeof(FUNCTION_TYPE) <= sizeof(m_pointer), "Buffer for function pointer too small.");
		memcpy(m_pointer, &pFunction, sizeof(FUNCTION_TYPE));
	}

	template<typename FUNCTION_TYPE>
	FUNCTION_TYPE Get() const
	{
		FUNCTION_TYPE pFunction;
		memcpy(&pFunction, m_pointer, sizeof(FUNCTION_TYPE));
		return pFunction;
	}

	bool IsValid() const
	{
		static_assert(sizeof(m_pointer) == (sizeof(uint64) * 2), "Please adapt code to buffer size.");
		const uint64* ptr = reinterpret_cast<const uint64*>(m_pointer);
		return ptr[0] || ptr[1];
	}

private:
	unsigned char m_pointer[sizeof(uint64) * 2];
};

// TODO: Code from old Schematyc reflection that needs to be integrated in new Schematyc.
struct CFunctionExecutionContext
{
public:
	CFunctionExecutionContext(void* pObject)
		: m_pObject(pObject)
	{}

	template<typename OBJECT_TYPE>
	OBJECT_TYPE* GetObject() const
	{
		return static_cast<OBJECT_TYPE*>(m_pObject);
	}

	size_t GetParamCount() const
	{
		return 0;//m_params.size();
	}

	template<typename TYPE>
	TYPE* GetParam(size_t index)
	{
		/*if (index < m_params.size())
		   {
		   return m_params.Get<TYPE>();
		   }*/
		return nullptr;
	}

	template<typename TYPE>
	void SetResult(const TYPE& returnValue)
	{
		//m_returnValue = returnValue;
	}

	/*template<typename TYPE>
	   const TYPE* GetResult() const
	   {
	   return m_returnValue.GetValue<TYPE>();
	   }

	   const CAnyValue& GetResult() const
	   {
	   return m_returnValue;
	   }*/

private:
	//CAnyValue            m_returnValue;
	void* m_pObject;

	//std::vector<CAnyPtr> m_params;
};
// ~TODO

class CFunction
{
public:
	typedef void (* ProxyFunc)(const CFunctionPtr& functPtr, CFunctionExecutionContext& context);

	CFunction()
		: m_proxyFunc(nullptr)
	{}

	CFunction(CFunctionPtr functionPtr, ProxyFunc proxyFunc)
		: m_functionPtr(functionPtr)
		, m_proxyFunc(proxyFunc)
	{}

	template<typename FUNCTION_TYPE>
	CFunction(FUNCTION_TYPE pFunction, ProxyFunc proxyFunc)
		: m_functionPtr(pFunction)
		, m_proxyFunc(proxyFunc)
	{}

	bool IsValid() const
	{
		return m_functionPtr.IsValid();
	}

	void Execute(CFunctionExecutionContext& context) const
	{
		CRY_ASSERT_MESSAGE(m_proxyFunc && m_functionPtr.IsValid(), "Pointer or proxy not set.");
		if (m_proxyFunc && m_functionPtr.IsValid())
		{
			m_proxyFunc(m_functionPtr, context);
		}
	}

	void Execute_Fast(CFunctionExecutionContext& context) const
	{
		m_proxyFunc(m_functionPtr, context);
	}

private:
	ProxyFunc    m_proxyFunc;
	CFunctionPtr m_functionPtr;
};

} // ~Reflection namespace
} // ~Cry namespace
