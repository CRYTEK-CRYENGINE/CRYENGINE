// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryFlowGraph/IFlowSystem.h>

struct IActor;

//////////////////////////////////////////////////////////////////////////
// Enum used for templating node base class
//////////////////////////////////////////////////////////////////////////
enum ENodeCloneType
{
	eNCT_Singleton, // node has only one instance; not cloned
	eNCT_Instanced, // new instance of the node will be created
};

//////////////////////////////////////////////////////////////////////////
// Auto registration for flow nodes. Handles both instanced and
//	singleton nodes
//////////////////////////////////////////////////////////////////////////
class CAutoRegFlowNodeBase : public IFlowNodeFactory
{
public:
	CAutoRegFlowNodeBase(const char* szClassName)
	{
		m_szClassName = szClassName;
		m_pNext = 0;
		if (!s_pLast)
		{
			s_pFirst = this;
		}
		else
		{
			s_pLast->m_pNext = this;
		}

		s_pLast = this;
	}

	virtual ~CAutoRegFlowNodeBase()
	{
		CAutoRegFlowNodeBase* pNode = s_pFirst;
		CAutoRegFlowNodeBase* pPrev = nullptr;
		while (pNode && pNode != this)
		{
			pPrev = pNode;
			pNode = pNode->m_pNext;
		}

		if (pNode)
		{
			if (pPrev)
				pPrev->m_pNext = m_pNext;
			else
				s_pFirst = m_pNext;

			if (s_pLast == this)
				s_pLast = pPrev;
		}
	}

	// overrides ref count behavior as auto reg nodes are statically allocated.
	virtual void AddRef() const override final {}
	virtual void Release() const override final {}

	virtual void Reset() override   {}

	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		SIZER_SUBCOMPONENT_NAME(s, "CAutoRegFlowNodeBase");
	}

	//////////////////////////////////////////////////////////////////////////
	const char*                  m_szClassName;
	CAutoRegFlowNodeBase*        m_pNext;
	static CAutoRegFlowNodeBase* s_pFirst;
	static CAutoRegFlowNodeBase* s_pLast;
	static bool                  s_bNodesRegistered;
	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
template<class T>
class CAutoRegFlowNode : public CAutoRegFlowNodeBase
{
public:
	CAutoRegFlowNode(const char* szClassName)
		: CAutoRegFlowNodeBase(szClassName)
	{
	}

	IFlowNodePtr Create(IFlowNode::SActivationInfo* pActInfo)
	{
		PREFAST_SUPPRESS_WARNING(6326)
		if (T::myCloneType == eNCT_Singleton)
		{
			if (!m_pInstance.get())
				m_pInstance = new T(pActInfo);

			return m_pInstance;
		}
		else if (T::myCloneType == eNCT_Instanced)
		{
			return new T(pActInfo);
		}
		else
		{
			CRY_ASSERT_MESSAGE(false, "Unsupported CloneType!");
		}
	}

private:
	IFlowNodePtr m_pInstance; // only applies for singleton nodes
};

#if CRY_PLATFORM_WINDOWS && defined(_LIB)
	#define CRY_EXPORT_STATIC_LINK_VARIABLE(Var)                        \
	  extern "C" { INT_PTR lib_func_ ## Var() { return (INT_PTR)&Var; } \
	  }                                                                 \
	  __pragma(message("#pragma comment(linker,\"/include:_lib_func_" # Var "\")"))
#else
	#define CRY_EXPORT_STATIC_LINK_VARIABLE(Var)
#endif

//////////////////////////////////////////////////////////////////////////
// Use this define to register a new flow node class. Handles
//	both instanced and singleton nodes
// Ex. REGISTER_FLOW_NODE( "Delay",CFlowDelayNode )
//////////////////////////////////////////////////////////////////////////
#define REGISTER_FLOW_NODE(FlowNodeClassName, FlowNodeClass)                     \
  CAutoRegFlowNode<FlowNodeClass> g_AutoReg ## FlowNodeClass(FlowNodeClassName); \
  CRY_EXPORT_STATIC_LINK_VARIABLE(g_AutoReg ## FlowNodeClass);

// macro similar to REGISTER_FLOW_NODE, but allows a different name for registration
#define REGISTER_FLOW_NODE_EX(FlowNodeClassName, FlowNodeClass, RegName)   \
  CAutoRegFlowNode<FlowNodeClass> g_AutoReg ## RegName(FlowNodeClassName); \
  CRY_EXPORT_STATIC_LINK_VARIABLE(g_AutoReg ## RegName);

//////////////////////////////////////////////////////////////////////////
// Internal base class for flow nodes. Node classes shouldn't be
//	derived directly from this, but should derive from CFlowBaseNode<>
//////////////////////////////////////////////////////////////////////////
class CFlowBaseNodeInternal : public IFlowNode
{
private:
	template<ENodeCloneType CLONE_TYPE> friend class CFlowBaseNode;
	// private ctor/dtor to prevent classes directly derived from this;
	//	the exception is CFlowBaseNode (friended above)
	CFlowBaseNodeInternal() {};
	virtual ~CFlowBaseNodeInternal() {}

public:
	//////////////////////////////////////////////////////////////////////////
	// IFlowNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)                          = 0;
	virtual bool         SerializeXML(SActivationInfo*, const XmlNodeRef&, bool)   { return true; }
	virtual void         Serialize(SActivationInfo*, TSerialize ser)               {}
	virtual void         PostSerialize(SActivationInfo*)                           {}
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) {};
};

//////////////////////////////////////////////////////////////////////////
// Base class that nodes can derive from.
//	eg1: Singleton node:
//	class CSingletonNode : public CFlowBaseNode<eNCT_Singleton>
//
//	eg2: Instanced node:
//	class CInstancedNode : public CFlowBaseNode<eNCT_Instanced>
//
//	Instanced nodes must override Clone (pure virtual in base), while
//	Singleton nodes cannot - will result in a compile error.
//////////////////////////////////////////////////////////////////////////

template<ENodeCloneType CLONE_TYPE>
class CFlowBaseNode : public CFlowBaseNodeInternal
{
public:
	CFlowBaseNode() : CFlowBaseNodeInternal() {}

	static const int myCloneType = CLONE_TYPE;
};

// specialization for instanced nodes: Clone() is purely virtual to force derived classes to override it
template<>
class CFlowBaseNode<eNCT_Instanced> : public CFlowBaseNodeInternal
{
public:
	CFlowBaseNode() : CFlowBaseNodeInternal() {}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) = 0;

	static const int myCloneType = eNCT_Instanced;
};

// specialization for singleton nodes: Clone() is marked final so that derived classes cannot override it.
template<>
class CFlowBaseNode<eNCT_Singleton> : public CFlowBaseNodeInternal
{
public:
	CFlowBaseNode() : CFlowBaseNodeInternal() {}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) final { return this; }

	static const int myCloneType = eNCT_Singleton;
};

inline void CryRegisterFlowNodes()
{
	if (gEnv->pFlowSystem && !CAutoRegFlowNodeBase::s_bNodesRegistered)
	{
		CAutoRegFlowNodeBase* pFactory = CAutoRegFlowNodeBase::s_pFirst;
		while (pFactory)
		{
			gEnv->pFlowSystem->RegisterType(pFactory->m_szClassName, pFactory);
			pFactory = pFactory->m_pNext;
		}
		CAutoRegFlowNodeBase::s_bNodesRegistered = true;
	}
}

inline void CryUnregisterFlowNodes()
{
	if (gEnv && gEnv->pFlowSystem && CAutoRegFlowNodeBase::s_bNodesRegistered)
	{
		CAutoRegFlowNodeBase* pFactory = CAutoRegFlowNodeBase::s_pFirst;
		while (pFactory)
		{
			gEnv->pFlowSystem->UnregisterType(pFactory->m_szClassName);
			pFactory = pFactory->m_pNext;
		}
		CAutoRegFlowNodeBase::s_bNodesRegistered = false;
	}
}