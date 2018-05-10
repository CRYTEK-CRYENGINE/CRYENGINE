// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 19:05:2009: Created by Federico Rebora
*************************************************************************/

#pragma once

#ifndef GAME_FLOW_BASE_NODE_H_INCLUDED
#define GAME_FLOW_BASE_NODE_H_INCLUDED

#include <CryFlowGraph/IFlowSystem.h>

//#include "GameEnvironmentForwards.h"
#include "GameEnvironment/GameEnvironment.h"

class CG2AutoRegFlowNodeBase : public IFlowNodeFactory
{
public:
	CG2AutoRegFlowNodeBase( const char *sClassName );
	void AddRef();
	void Release();

	const char* m_className;
	CG2AutoRegFlowNodeBase* m_next;
	static CG2AutoRegFlowNodeBase* m_first;
	static CG2AutoRegFlowNodeBase* m_last;
};

//////////////////////////////////////////////////////////////////////////
template <class T>
class CG2AutoRegFlowNode : public CG2AutoRegFlowNodeBase
{
public:
	CG2AutoRegFlowNode( const char *sClassName ) : CG2AutoRegFlowNodeBase( sClassName ) {}
	IFlowNodePtr Create( IFlowNode::SActivationInfo * pActInfo ) { return new T(pActInfo); }
	void GetMemoryStatistics(ICrySizer * s)
	{ 
		SIZER_SUBCOMPONENT_NAME(s, "CG2AutoRegFlowNode");
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
template <class T>
class CG2AutoRegFlowNodeWithEnvironment : public CG2AutoRegFlowNodeBase
{
public:
	CG2AutoRegFlowNodeWithEnvironment( const char *className )
	: CG2AutoRegFlowNodeBase( className )
	{
	}
	
	IFlowNodePtr Create( IFlowNode::SActivationInfo * activationInfo )
	{
		return new T(GetEnvironment(), activationInfo);
	}
	
	void GetMemoryStatistics(ICrySizer * sizer)
	{ 
		SIZER_SUBCOMPONENT_NAME(sizer, "CG2AutoRegFlowNode");
		sizer->Add(*this);
	}

	IGameEnvironment& GetEnvironment()
	{
		if (m_environment.get() == 0)
		{
			m_environment = IGameEnvironment::Create();;
		}

		return *m_environment;
	}

private:	
	IGameEnvironmentPtr m_environment;
};

//////////////////////////////////////////////////////////////////////////
class CGameFlowBaseNode;

template <class T>
class CG2AutoRegFlowNodeSingleton : public CG2AutoRegFlowNodeBase
{
public:
	CG2AutoRegFlowNodeSingleton( const char *sClassName ) : CG2AutoRegFlowNodeBase( sClassName )
	{
		// this makes sure, the derived class DOES NOT implement a Clone method
		typedef IFlowNodePtr (CGameFlowBaseNode::*PtrToMemFunc) (IFlowNode::SActivationInfo*);
		static const PtrToMemFunc f = &T::Clone; // likely to get optimized away
	}
	IFlowNodePtr Create( IFlowNode::SActivationInfo * pActInfo ) 
	{ 
		if (!m_pInstance)
			m_pInstance = new T(pActInfo);
		return m_pInstance;
	}
	void GetMemoryStatistics(ICrySizer * s)
	{ 
		SIZER_SUBCOMPONENT_NAME(s, "CG2AutoRegFlowNodeSingleton");
		s->Add(*this);
	}
private:
	IFlowNodePtr m_pInstance;
};

//////////////////////////////////////////////////////////////////////////
// Use this define to register a new flow node class.
// Ex. REGISTER_FLOW_NODE( "Delay",CFlowDelayNode )
//////////////////////////////////////////////////////////////////////////
#define REGISTER_FLOW_NODE( FlowNodeClassName,FlowNodeClass ) \
	CG2AutoRegFlowNode<FlowNodeClass> g_AutoReg##FlowNodeClass ( FlowNodeClassName );

#define REGISTER_FLOW_NODE_WITH_ENVIRONMENT( FlowNodeClassName,FlowNodeClass ) \
	CG2AutoRegFlowNodeWithEnvironment<FlowNodeClass> g_AutoReg##FlowNodeClass ( FlowNodeClassName );

#define REGISTER_FLOW_NODE_EX( FlowNodeClassName,FlowNodeClass,RegName ) \
	CG2AutoRegFlowNode<FlowNodeClass> g_AutoReg##RegName ( FlowNodeClassName );

#define REGISTER_FLOW_NODE_SINGLETON( FlowNodeClassName,FlowNodeClass ) \
	CG2AutoRegFlowNodeSingleton<FlowNodeClass> g_AutoReg##FlowNodeClass ( FlowNodeClassName );

#define REGISTER_FLOW_NODE_SINGLETON_EX( FlowNodeClassName,FlowNodeClass,RegName ) \
	CG2AutoRegFlowNodeSingleton<FlowNodeClass> g_AutoReg##RegName ( FlowNodeClassName );


class CGameFlowBaseNode : public IFlowNode
{
public:
	CGameFlowBaseNode();
	virtual ~CGameFlowBaseNode();

	//////////////////////////////////////////////////////////////////////////
	// IFlowNode
	virtual void AddRef();
	virtual void Release();

	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo );
	virtual bool SerializeXML( SActivationInfo *, const XmlNodeRef&, bool );
	virtual void Serialize(SActivationInfo *, TSerialize ser);
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo );

	//////////////////////////////////////////////////////////////////////////

	virtual void ProcessActivateEvent(SActivationInfo* activationInfo);

	//////////////////////////////////////////////////////////////////////////
	// Common functions to use in derived classes.
	//////////////////////////////////////////////////////////////////////////
	bool IsPortActive( SActivationInfo *pActInfo,int nPort ) const;
	bool IsBoolPortActive( SActivationInfo *pActInfo,int nPort ) const;
	EFlowDataTypes GetPortType( SActivationInfo *pActInfo,int nPort ) const;

	const TFlowInputData& GetPortAny( SActivationInfo *pActInfo,int nPort ) const;

	bool GetPortBool( SActivationInfo *pActInfo,int nPort ) const;
	int GetPortInt( SActivationInfo *pActInfo,int nPort ) const;
	EntityId GetPortEntityId( SActivationInfo *pActInfo,int nPort );
	float GetPortFloat( SActivationInfo *pActInfo,int nPort ) const;
	Vec3 GetPortVec3( SActivationInfo *pActInfo,int nPort ) const;
	EntityId GetPortEntityId( SActivationInfo *pActInfo,int nPort ) const;
	const string& GetPortString( SActivationInfo *pActInfo,int nPort ) const;

	//////////////////////////////////////////////////////////////////////////
	// Sends data to output port.
	//////////////////////////////////////////////////////////////////////////
	template <class T>
		void ActivateOutput( SActivationInfo *pActInfo,int nPort, const T &value )
	{
		SFlowAddress addr( pActInfo->myID, nPort, true );
		pActInfo->pGraph->ActivatePort( addr, value );
	}
	//////////////////////////////////////////////////////////////////////////
	bool IsOutputConnected( SActivationInfo *pActInfo,int nPort ) const;
	//////////////////////////////////////////////////////////////////////////
	
protected:
  virtual bool InputEntityIsLocalPlayer( const SActivationInfo* const pActInfo ) const;
	CActor2* GetInputActor( const SActivationInfo* const pActInfo ) const;

private:
	int m_refs;
};

#endif
