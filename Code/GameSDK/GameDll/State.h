// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/**********************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:

A Hierarchical State Machine

-------------------------------------------------------------------------
History:
- Created xx.xx.11: Created by Stephen M. North

*************************************************************************/
#pragma once

#ifndef __State_h__
#define __State_h__

#include <CryCore/CryFlags.h>
#include <CryRenderer/IRenderAuxGeom.h>

#include "AutoEnum.h"

#if !defined(_RELEASE) && !CRY_PLATFORM_ORBIS && !defined(DEDICATED_SERVER)
	#include "GameCVars.h"
	#define STATE_DEBUG
	struct SStateDebugContext;
#endif

#define eStateEvents(f) \
	f(STATE_EVENT_INIT) \
	f(STATE_EVENT_RELEASE) \
	f(STATE_EVENT_ENTER) \
	f(STATE_EVENT_EXIT) \
	f(STATE_EVENT_SERIALIZE) \
	f(STATE_EVENT_POST_SERIALIZE) \
	f(STATE_EVENT_NETSERIALIZE) \
	f(STATE_EVENT_DEBUG)

AUTOENUM_BUILDENUMWITHTYPE_WITHNUMEQUALS_WITHZERO( EStateEvent, eStateEvents, STATE_EVENT_CUSTOM, 100, EVENT_NONE );

#define MAX_NUM_PENDING_EVENTS 4
#define MAX_HSMEVENT_DATA	5


/*
enum EStateEvent
{
	EVENT_NONE = 0,
	EVENT_STATE_INIT,
	EVENT_STATE_RELEASE,
	EVENT_STATE_ENTER,
	EVENT_STATE_EXIT,
	EVENT_STATE_SERIALIZE,
	EVENT_STATE_NETSERIALIZE,
	EVENT_STATE_DEBUG,

	EVENT_CUSTOM = 100
};*/

template<typename HOST>
class CStateHierarchy;

//////////////////////////////////////////////////////////////////////////
// StateEvent

struct SStateEventData
{
	enum EType
	{
		eSEDT_undef,
		eSEDT_int,
		eSEDT_float,
		eSEDT_bool,
		eSEDT_voidptr,
	};

	union
	{
		int			m_int;
		float		m_float;
		bool		m_bool;
		const void*		m_pointer;
	};

	int8 m_type;

	SStateEventData() : m_pointer(NULL), m_type(eSEDT_undef) {}
	SStateEventData(int val) : m_int(val), m_type(eSEDT_int) {}
	SStateEventData(float val) : m_float(val), m_type(eSEDT_float) {}
	SStateEventData(bool val) : m_bool(val), m_type(eSEDT_bool) {}
	SStateEventData(const void* val) : m_pointer(val), m_type(eSEDT_voidptr) {}

	int GetInt() const { assert(m_type==eSEDT_int); return m_int; }
	float GetFloat() const { assert(m_type==eSEDT_float); return m_float; }
	bool GetBool() const { assert(m_type==eSEDT_bool); return m_bool; }
	const void* GetPtr() const { assert( (m_pointer==NULL) || (m_type==eSEDT_voidptr) ); return m_pointer; }
};

struct SStateEvent
{
public:

	typedef CryFixedArray<SStateEventData, MAX_HSMEVENT_DATA>			TStateEventData;

	SStateEvent()	: m_eventType(EVENT_NONE)
#ifdef STATE_DEBUG
		, m_debugContextAt(-1)
#endif
	{}
	SStateEvent(int type)	: m_eventType(type)
#ifdef STATE_DEBUG
		, m_debugContextAt(-1)
#endif
	{}
#ifdef STATE_DEBUG
	SStateEvent(int type, SStateDebugContext& context)	: m_eventType(type)
		, m_debugContextAt(-1) { AddDebugContext( context ); }
#endif

	SStateEvent( const SStateEvent& rhs )
		: m_eventType(rhs.m_eventType)
#ifdef STATE_DEBUG
		, m_debugContextAt(rhs.m_debugContextAt)
#endif
	{
		for( unsigned int i=0; i<rhs.GetDataSize(); ++i )
		{
			AddData( rhs.GetData(i) );
		}
	}

	void AddData(const SStateEventData& data)	{	m_data.push_back(data); }
	const SStateEventData& GetData(uint8 index) const {	return m_data[index];	}
	ILINE int GetEventId( ) const	{	return m_eventType;	}
	const unsigned int GetDataSize( ) const	{	return m_data.size();	}
	void ClearData( )	{	m_data.clear();	}

	static SStateEvent CreateStateEvent( int type, const SStateEventData& data ) { SStateEvent event(type); event.AddData(data); return event; }

#ifdef STATE_DEBUG
	mutable int m_debugContextAt;
	const SStateEvent& AddDebugContext(const SStateDebugContext& context ) const
	{
		if( m_debugContextAt == -1 && m_data.size()<m_data.max_size())
		{
			m_debugContextAt = m_data.size();
			SStateEventData tmpData( &context );
			m_data.push_back( tmpData );
		}
		return *this;
	}
#endif

private:

	int	m_eventType;
#ifdef STATE_DEBUG
	mutable
#endif
		TStateEventData	m_data;
};

struct SStateEventSerialize : public SStateEvent
{
	explicit SStateEventSerialize( TSerialize& ser )
		:	SStateEvent( STATE_EVENT_SERIALIZE )
	{
		AddData( &ser );
	}

	ILINE TSerialize& GetSerializer() const
	{
		// Work O Devil.
		const TSerialize* pSerialize = static_cast<const TSerialize*> (GetData(0).GetPtr());
		return *const_cast<TSerialize*> (pSerialize);
	}
};

#ifdef STATE_DEBUG

	#include <CryRenderer/IRenderer.h>
	static const float state_white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	static const float state_red[4]   = {1.0f, 0.0f, 0.0f, 1.0f};
	static const float state_green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
	static const float state_blue[4]  = {0.0f, 0.0f, 1.0f, 1.0f};

	struct SHistory
	{
		typedef string TDebugType;
		typedef CryFixedArray<TDebugType, 40> TStateHistory;

		SHistory()
		{
			Reset();
		}

		void Reset()
		{
			m_states.clear();

			while(m_states.size() < m_states.max_size())
			{
				m_states.push_back( string("null") );
			}

			m_front = 0;
			m_historyID = 0;
		}

		void AddToHistory( const char* subState, ... )
		{
			CRY_ASSERT( (UINT_PTR)subState != 0xcdcdcdcd );

			char tmp[128];
			va_list args;
			va_start(args,subState);
			cry_vsprintf( tmp, subState, args);
			va_end(args);

			CryFixedStringT<256> finalStr;
			finalStr.Format("%x %s", m_historyID++, tmp);

			m_states[m_front] = finalStr.c_str();
			m_front = (m_front < (int)(m_states.max_size() - 1)) ? m_front + 1 : 0;
		}

		ILINE int GetFront() const { return m_front - 1; }
		ILINE int GetBack() const { return (int)(m_states.size() - 1); }
		ILINE const TDebugType& GetStateAt(int index) const { return m_states[index]; }

	private:

		TStateHistory		m_states;
		int		m_front;
		int   m_historyID;
	};

	struct SStateDebugContext
	{
		SStateDebugContext(IRenderer* renderer, IRenderAuxGeom* renderAuxGeom)
			: m_renderer(renderer)
			, m_rendererAuxGeom(renderAuxGeom)
			, m_stateLevel(0)
			, m_baseHorizontal(50.0f)
			, m_currentVertical(50.0f)
		{
		}
		IRenderer*			m_renderer;
		IRenderAuxGeom*	m_rendererAuxGeom;
		uint32 m_stateLevel;
		float  m_baseHorizontal;
		mutable float  m_currentVertical;

		template<typename HOST>
		static const SStateEvent StateDebugAndLog( CStateHierarchy<HOST>* pState, const char* stateName, SStateEvent stateEvent );
	};

#define DebugInit( pDebugName ) { m_pDebugName = pDebugName; }

	#define STATE_DEBUG_LOG( n,m, ... )
	//#define STATE_DEBUG_LOG( n,m, ... ) CryLogAlways( m, __VA_ARGS__ );
/*	#define STATE_DEBUG_LOG( colour, horz, vert, n, ... ) \
		{\
			va_list args; \
			va_start(args,n); \
			IRenderAuxText::Draw2dLabel(horz, vert, 1.5f, state_white, false, n, args ); \
			va_end(args); \
		}*/

	#define STATE_DEBUG_EVENT_LOG( state, debugEvent, logit, colour, n, ... ) \
		{\
			const SStateDebugContext& stateDebugCtx = *static_cast<const SStateDebugContext*>(debugEvent.GetData(debugEvent.m_debugContextAt).GetPtr()); \
			IRenderAuxText::Draw2dLabel(stateDebugCtx.m_baseHorizontal, stateDebugCtx.m_currentVertical, 1.5f, colour, false, n, __VA_ARGS__ ); \
			stateDebugCtx.m_currentVertical += 15.f; \
			if( logit )	state->m_historyDebug.AddToHistory( n, __VA_ARGS__ ); \
		}\

	#define STATE_DEBUG_EVENT_LOG_TO_HISTORY( state, colour, n, ... ) \
		{\
			state->m_historyDebug.AddToHistory( n, __VA_ARGS__ ); \
		}\

	template<typename HOST>
	const SStateEvent STATE_DEBUG_RAW_EVENT_LOG( CStateHierarchy<HOST>* pState, const char* stateName, SStateEvent stateEvent )
	{
		return SStateDebugContext::StateDebugAndLog( pState, stateName, stateEvent );
	}

	template<typename HOST>
	const SStateEvent STATE_DEBUG_RAW_EVENT_LOG( CStateHierarchy<HOST>* pState, SStateEvent stateEvent )
	{
		return SStateDebugContext::StateDebugAndLog( pState, pState->m_currentState.m_pDebugName, stateEvent );
	}

	#define STATE_DEBUG_RAW_EVENT( state )\
		SStateEvent( state, &SStateDebugContext( gEnv->pRenderer, gEnv->pAuxGeomRenderer ) )

	#define STATE_DEBUG_APPEND_EVENT( event )\
		event.AddDebugContext( SStateDebugContext( gEnv->pRenderer, gEnv->pAuxGeomRenderer ) )

	#define STATE_DEBUG_EVENTONLY( name, e ) name, e

#else
	#define DebugInit( n )
	#define STATE_DEBUG_LOG( n,m, ... )
	#define STATE_DEBUG_EVENT_LOG( state, debugEvent, logit, colour, n, ... )
	#define STATE_DEBUG_EVENT_LOG_TO_HISTORY( state, colour, n, ... )
	#define STATE_DEBUG_APPEND_EVENT( e ) e
	#define STATE_DEBUG_EVENTONLY( name, e ) e

	template<typename HOST>
	const SStateEvent STATE_DEBUG_RAW_EVENT_LOG( CStateHierarchy<HOST>* pState, SStateEvent stateEvent )
	{
		return stateEvent;
	}

#endif

#include "Utility/CryHash.h"

struct SStateEvent;

enum EState
{
	STATE_DONE = 0,
	STATE_CONTINUE = 1,
	STATE_FIRST = 100,
};


//////////////////////////////////////////////////////////////////////////
// StateRegistration

template<typename HOST>
struct SStateIndex;

template<typename HOST, typename STATE>
class CStateHelper;

template<typename HOST>
class CStateMachine;

template<typename HOST>
class CStateHierarchy;

template<typename HOST>
class CStateMachineRegistration;

#define CALL_SUBSTATE_FN(object, subStateIndex) ((*object).*(subStateIndex.m_func))
#define CALL_SUBSTATE_PARENT_FN(object, subStateIndex)  ((*object).*(subStateIndex.m_parent->m_func))

#define CALL_STATE_CREATE_FN(subStateIndex)  (*(m_factories[trueStateID].m_createPtr))
#define CALL_STATE_DELETE_FN(subStateIndex)  (*(m_factories[trueStateID].m_deletePtr))

template<typename HOST>
class CStateProxy
{
public:
	typedef const SStateIndex<HOST> (CStateHierarchy<HOST>::*StatePtr)( HOST&, const SStateEvent& );
	typedef CStateHierarchy<HOST>* (*CreateStatePtr)( CStateMachineRegistration<HOST>& stateMachineReg );
	typedef void (*DeleteStatePtr)( CStateHierarchy<HOST>*& );
};

template< typename HOST >
class CStateMachineRegistration
{
	friend class CStateHelper<HOST, CStateHierarchy<HOST> >;
	friend class CStateMachine<HOST>;
	struct SStateFactory
	{
		typename CStateProxy<HOST>::CreateStatePtr m_createPtr;
		typename CStateProxy<HOST>::DeleteStatePtr m_deletePtr;

		SStateFactory() {}
		SStateFactory( typename CStateProxy<HOST>::CreateStatePtr createPtr, typename CStateProxy<HOST>::DeleteStatePtr deletePtr ) :
		m_createPtr(createPtr), m_deletePtr(deletePtr) {}
	};

	typedef std::vector<SStateFactory> TStateFactory;
	TStateFactory m_factories;

public:

	void RegisterState( typename CStateProxy<HOST>::CreateStatePtr createPtr, typename CStateProxy<HOST>::DeleteStatePtr deletePtr, const uint stateID )
	{
		const uint trueStateID = stateID - STATE_FIRST;

		if( (trueStateID + 1) > m_factories.size() )
		{
			m_factories.resize( trueStateID + 1 );
		}
		m_factories[trueStateID] = SStateFactory( createPtr, deletePtr );
	}

	void UnRegisterState( const uint stateID )
	{
		// TODO!
	}

	CStateHierarchy<HOST>* CreateState( const uint stateID )
	{
		const uint trueStateID = stateID - STATE_FIRST;
		if( trueStateID < m_factories.size() )
		{
			return CALL_STATE_CREATE_FN(trueStateID)( *this );
		}
		return NULL;
	}

	void DeleteState( CStateHierarchy<HOST>*& pState )
	{
		const uint trueStateID = pState->GetStateID() - STATE_FIRST;
		if( trueStateID < m_factories.size() )
		{
			CALL_STATE_DELETE_FN(trueStateID)( pState );
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// StateIndex

template<typename HOST>
struct SStateIndex
{
	enum { UNDEFINED = -1 };

	SStateIndex()
		: m_name(CryHash(UNDEFINED))
		, m_func(0)
		, m_parent(nullptr)
		, m_stateID(UNDEFINED)
		, m_hierarchy(UNDEFINED)
	{
		DebugInit("UNDEFINED_NEED_TO_ADD_STATE_TO_HIERARCHY_BEFORE_TRANSITIONING_TO_IT");
	}

	SStateIndex(CryHash hashName)
		: m_name(hashName)
		, m_func(0)
		, m_parent(nullptr)
		, m_stateID(0)
		, m_hierarchy(0)
	{
		DebugInit("UnknownHash");
	}

	explicit SStateIndex(const char* pName)
		: m_name(CryStringUtils::HashString(pName))
		, m_func(0)
		, m_parent(nullptr)
		, m_stateID(0)
		, m_hierarchy(0)
	{
		DebugInit(pName);
	}

	SStateIndex(const char* pName, typename CStateProxy<HOST>::StatePtr func, const SStateIndex<HOST>* parent, uint stateID)
		: m_name(CryStringUtils::HashString(pName))
		, m_func(func)
		, m_parent(parent)
		, m_stateID((1ULL << static_cast<uint64>(stateID)))
		, m_hierarchy(0)
	{
		DebugInit(pName);
		RecursiveGenerateHierarchy(*this, m_hierarchy);
	}

	SStateIndex(const SStateIndex& rhs)
		: m_name(rhs.m_name)
		, m_func(rhs.m_func)
		, m_parent(rhs.m_parent)
		, m_stateID(rhs.m_stateID)
		, m_hierarchy(rhs.m_hierarchy)
#ifdef STATE_DEBUG
		, m_pDebugName(rhs.m_pDebugName)
#endif
	{}

	bool operator==( const SStateIndex& rhs ) const { return( m_name == rhs.m_name ); }
	bool operator!=( const SStateIndex& rhs ) const { return( m_name != rhs.m_name ); }
	SStateIndex& operator=( const SStateIndex& rhs )
		{ m_name = rhs.m_name; m_func = rhs.m_func; m_parent = rhs.m_parent;  m_stateID = rhs.m_stateID; m_hierarchy = rhs.m_hierarchy;
#ifdef STATE_DEBUG
			m_pDebugName = rhs.m_pDebugName;
#endif
			return *this; }

	CryHash  m_name;
	typename CStateProxy<HOST>::StatePtr m_func;
	const SStateIndex<HOST>* m_parent;
	uint64 m_hierarchy;
	uint m_stateID;

#ifdef STATE_DEBUG
	const char* m_pDebugName;
#endif

private:

	void RecursiveGenerateHierarchy( typename CStateHierarchy<HOST>::TStateIndex currentSubState, uint64& hierarchy )
	{
		if( m_stateID > 0 )
		{
			hierarchy |= currentSubState.m_stateID;

			if( currentSubState.m_parent )
			{
				RecursiveGenerateHierarchy( *currentSubState.m_parent, hierarchy );
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// State

template<typename HOST, typename STATE>
class CStateHelper
{
public:
	static void StateInit( HOST& host, CStateMachineRegistration<HOST>& statemachineReg, STATE*& pState )
	{
		STATE_DEBUG_LOG( pState, "StateInit: Name: <%s>", pState->m_currentSubState.m_pDebugName );

		pState->InitState( host );

		RecursiveToCommonReverse( host, pState->m_currentState, 0, pState, STATE_EVENT_INIT );
		StateMachineHandleEventForState( host, statemachineReg, pState, STATE_DEBUG_RAW_EVENT_LOG( pState, STATE_EVENT_ENTER ), 0 );
	}

	static void StateRelease( HOST& host, CStateMachineRegistration<HOST>& statemachineReg, STATE*& pState )
	{
		STATE_DEBUG_LOG( pState, "StateRelease: Name: <%s>", pState->m_currentState.m_pDebugName );

		// Have to call Release before ~ for obvious reasons.
		// NOTE: this is not the mirror of StateInit.
		pState->ReleaseState( host, statemachineReg );

		RecursiveToCommon( host, pState->m_currentState, 0, pState, STATE_DEBUG_RAW_EVENT_LOG( pState, STATE_EVENT_EXIT ) );
		RecursiveToCommon( host, pState->m_currentState, 0, pState, STATE_EVENT_RELEASE );
	}

	static void StateTransition( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg, STATE*& pActiveState )
	{
		STATE_DEBUG_LOG( pActiveState, "StateTransition: From: <%s> To: <%s>", pActiveState->m_currentState.m_pDebugName, pActiveState->m_pTransitionState->m_currentState.m_pDebugName );

		// Beware, pointer ownership changes!
		STATE* pTransitionState = pActiveState->m_pTransitionStateHierarchy;
		pActiveState->m_pTransitionStateHierarchy = NULL;

		SStateEvent pendingEvent = pActiveState->m_pendingTransitionStateEvent;

		// Terminate the current state.
		StateRelease( host, stateMachineReg, pActiveState );

		// copy over the flags.
		pTransitionState->m_flags = pActiveState->m_flags;

		// cleanup the old state.
		StateDelete( host, stateMachineReg, pActiveState );

		// Set current state.
		pActiveState = pTransitionState;

		// Init the new state.
		StateInit( host, stateMachineReg, pActiveState );

		// dispatch the transition event!
		if( pendingEvent.GetEventId() != EVENT_NONE )
		{
			StateMachineHandleEventForState( host, stateMachineReg, pActiveState, pendingEvent, 0 );
		}
	}

	static uint64 GenerateCommonParent( SStateIndex<HOST> stateCurrent, SStateIndex<HOST> stateCommon )
	{
#ifdef STATE_DEBUG
		SStateIndex<HOST> stateCurrentOriginal = stateCurrent;
#endif

		uint64 stateResult = stateCurrent.m_hierarchy & stateCommon.m_hierarchy;
		if( stateResult != 0 )
		{
			do
			{
				if( (stateResult & stateCurrent.m_stateID) == stateCurrent.m_stateID )
				{
					STATE_DEBUG_LOG( NULL, "GenerateCommonParent: For: <%s> And: <%s> Is: <%s>", stateCurrentOriginal.m_pDebugName, stateCommon.m_pDebugName, stateCurrent.m_pDebugName );

					return stateCurrent.m_stateID;
				}

				if ( stateCurrent.m_parent )
				{
					stateCurrent = *stateCurrent.m_parent;
				}
			}
			while( stateCurrent.m_parent != NULL );
		}
		return stateCurrent.m_stateID;
	}

	static void RecursiveToCommonReverse( HOST& host, SStateIndex<HOST> stateCurrent, const uint64 stateCommonID, STATE* pState, const SStateEvent& event )
	{
		if ( stateCurrent.m_stateID != stateCommonID )
		{

			if( stateCurrent.m_parent )
			{
				RecursiveToCommonReverse( host, *stateCurrent.m_parent, stateCommonID, pState, event );
			}

			STATE_DEBUG_LOG( pState, "RecursiveToCommonReverse: Name: <%s>", stateCurrent.m_pDebugName );

			CALL_SUBSTATE_FN( pState, stateCurrent )( host, STATE_DEBUG_RAW_EVENT_LOG( pState, STATE_DEBUG_EVENTONLY( stateCurrent.m_pDebugName, event ) ) );
		}
	}

	static void RecursiveToCommon( HOST& host, SStateIndex<HOST> stateCurrent, const uint64 stateCommonID, STATE* pState, const SStateEvent& event )
	{
		if( stateCurrent.m_stateID != stateCommonID )
		{
			STATE_DEBUG_LOG( pState, "RecursiveToCommon: Name: <%s>", stateCurrent.m_pDebugName );

			const SStateIndex<HOST> stateReturn = CALL_SUBSTATE_FN( pState, stateCurrent )( host, STATE_DEBUG_RAW_EVENT_LOG( pState, STATE_DEBUG_EVENTONLY( stateCurrent.m_pDebugName, event ) ) );

			if( stateCurrent.m_parent && stateReturn != pState->State_Done )
			{
				RecursiveToCommon( host, *stateCurrent.m_parent, stateCommonID, pState, event );
			}
		}
	}

	static void StateMachineHandleEventForState( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg, STATE*& pState, const SStateEvent& event, const uint64 commonID )
	{
		STATE_DEBUG_LOG( pState, "HandleEvent: Name: <%s> Event: <%d>", pState->m_currentState.m_pDebugName, event.GetEventId() );

		typename STATE::TStateIndex currentState = pState->m_currentState;
		typename STATE::TStateIndex stateResult = STATE_DONE;
		if( commonID != currentState.m_stateID )
		{
			stateResult = CALL_SUBSTATE_FN( pState, currentState )( host, event );
		}

		do
		{
			STATE_DEBUG_LOG( pState, "HandleEvent: Name: <%s> Event: <%d>", pState->m_currentState.m_pDebugName, event.GetEventId() );

			switch( stateResult.m_name )
			{
			case STATE_DONE:
				break;
			case STATE_CONTINUE:
				if( currentState.m_parent != NULL && currentState.m_parent->m_stateID != commonID )
				{
					stateResult = CALL_SUBSTATE_PARENT_FN( pState, currentState )( host, event );

					currentState = *currentState.m_parent;
				}
				else
				{
					stateResult = STATE_DONE;
				}
				break;
			default:
				if ( pState->m_currentState != stateResult )
				{
					// transition to new sub state.
					pState->TransitionFromCurrentToSubState( host, stateMachineReg, stateResult );

					typename STATE::TStateIndex oldState = currentState;

					// set the current state to the transitioned state
					currentState = stateResult = pState->m_currentState;

					// then call the new state with the event which caused the transition,
					// unless it was a system event (e.g. STATE_ENTER)
					if( event.GetEventId() >= STATE_EVENT_CUSTOM )
					{
						const uint64 commonParent = CStateHelper<HOST, CStateHierarchy<HOST> >::GenerateCommonParent( oldState, pState->m_currentState );

						StateMachineHandleEventForState( host, stateMachineReg, pState, event, commonParent );
					}
				}
				else
				{
					stateResult = STATE_DONE;
				}
				break;
			}
		} while ( stateResult.m_name != STATE_DONE && (currentState.m_parent != NULL || stateResult.m_func != NULL) );

		if( pState->m_pTransitionStateHierarchy )
		{
			// Correctly transition to the new state.
			StateTransition( host, stateMachineReg, pState );
		}
	}

	static STATE* StateNew( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg, int stateIndex )
	{
		STATE* pState = stateMachineReg.CreateState( stateIndex );

		pState->m_currentState = pState->m_defaultState;

#ifdef STATE_DEBUG
		pState->m_historyDebug.Reset();
#endif

		return pState;
	}

	static void StateDelete( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg, STATE*& pState )
	{
		stateMachineReg.DeleteState( pState );
	}
};

template<typename HOST>
class CStateHierarchy
{
#ifdef STATE_DEBUG
	friend struct SStateDebugContext;
#endif
public:

	virtual ~CStateHierarchy() {}

	typedef SStateIndex<HOST> TStateIndex;

protected:

	ILINE int GetStateID() const { return m_stateID; }

	void RequestTransitionState( HOST& host, TStateIndex stateTransition, SStateEvent event )
	{
		CRY_ASSERT( !m_pTransitionStateHierarchy );

		m_pTransitionStateHierarchy = CStateHelper<HOST, CStateHierarchy<HOST> >::StateNew( host, m_stateMachineReg, stateTransition.m_name );
		m_pendingTransitionStateEvent = event;
	}

	void RequestTransitionState( HOST& host, TStateIndex stateTransition )
	{
		CRY_ASSERT( !m_pTransitionStateHierarchy );

		RequestTransitionState( host, stateTransition, EVENT_NONE );
	}

#ifdef STATE_DEBUG
	public:
#else
	protected:
#endif

	friend class CStateMachineRegistration<HOST>;
	friend class CStateHelper<HOST, CStateHierarchy<HOST> >;
	friend class CStateMachine<HOST>;

	CCryFlags<uint32> m_flags;

	const TStateIndex State_Done;
	const TStateIndex State_Continue;
	int m_stateID;

	// During a State Transition, we create the target State in order to initialize it.
	CStateHierarchy* m_pTransitionStateHierarchy;

	TStateIndex m_currentState;
	const TStateIndex& m_defaultState;
	CStateMachineRegistration<HOST>& m_stateMachineReg;

	typedef std::vector<SStateIndex<HOST>*> TStateIndexContainer;
	TStateIndexContainer m_stateIndexContainer;

#ifdef STATE_DEBUG
	SHistory m_historyDebug;
#endif

	CStateHierarchy( int stateID, const SStateIndex<HOST>& defaultState, CStateMachineRegistration<HOST>& stateMachineReg )
		: State_Done(CryHash(STATE_DONE))
		, State_Continue(CryHash(STATE_CONTINUE))
		, m_stateID( stateID )
		, m_pTransitionStateHierarchy(nullptr)
		, m_currentState(State_Done)
		, m_defaultState(defaultState)
		, m_stateMachineReg(stateMachineReg)
	{
	}

private:

	void InitState( HOST& host )
	{
		STATE_DEBUG_LOG( this, "InitState: Name: <%s>", m_currentState.m_pDebugName );

#ifdef STATE_DEBUG
		m_historyDebug.Reset();
#endif
	}

	void ReleaseState( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg )
	{
		STATE_DEBUG_LOG( this, "ReleaseState: Name: <%s>", m_currentState.m_pDebugName );
		if( m_pTransitionStateHierarchy )
		{
			STATE_DEBUG_LOG( this, "ReleaseState:DeletingPendingTransition: Name: <%s>; TransitionState: <%s> ", m_currentState.m_pDebugName, m_pTransitionStateHierarchy->m_currentState.m_pDebugName );
			CStateHelper<HOST, CStateHierarchy<HOST> >::StateDelete( host, stateMachineReg, m_pTransitionStateHierarchy );
		}
	}

	bool TransitionFromCurrentToSubState( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg, int64 stateHierarchy, int stateID, uint32 stateName )
	{
		if( (m_currentState.m_stateID != stateID) || (m_currentState.m_hierarchy != stateHierarchy) || (m_currentState.m_name != stateName) )
		{
			typename TStateIndexContainer::iterator it = m_stateIndexContainer.begin();
			const typename TStateIndexContainer::const_iterator iEnd = m_stateIndexContainer.end();
			for( ; it!=iEnd; ++it )
			{
				if( (*it)->m_stateID == stateID )
				{
					if( (*it)->m_hierarchy != stateHierarchy )
					{
						CryLog( "StateMachine: Failed to Serialize state as the hierarchy has changed!" );
						return false;
					}
					if( (*it)->m_name != stateName )
					{
						CryLog( "StateMachine: Failed to Serialize state as the name has changed!" );
						return false;
					}

					TransitionFromCurrentToSubState( host, stateMachineReg, *(*it) );
					return true;
				}
			}
		}
		return true;
	}

	void TransitionFromCurrentToSubState( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg, TStateIndex toSubState )
	{
		STATE_DEBUG_LOG( this, "TransitionFromCurrentToSubState: From: <%s> To: <%s>", m_currentState.m_pDebugName, toSubState.m_pDebugName );

#if defined(STATE_DEBUG)
		const char* debugFromStateName =  m_currentState.m_pDebugName;
		const char* debugIntoStateName = toSubState.m_pDebugName;
#endif
		const uint64 commonParent = CStateHelper<HOST, CStateHierarchy<HOST> >::GenerateCommonParent( m_currentState, toSubState );

		CStateHierarchy* pState = this;

		CStateHelper<HOST, CStateHierarchy<HOST> >::RecursiveToCommon( host, m_currentState, commonParent, pState, STATE_EVENT_EXIT );
		CStateHelper<HOST, CStateHierarchy<HOST> >::RecursiveToCommon( host, m_currentState, commonParent, pState, STATE_EVENT_RELEASE );

		m_currentState = toSubState;

		CStateHelper<HOST, CStateHierarchy<HOST> >::RecursiveToCommonReverse( host, m_currentState, commonParent, pState, STATE_EVENT_INIT );
		CStateHelper<HOST, CStateHierarchy<HOST> >::StateMachineHandleEventForState( host, stateMachineReg, pState, STATE_EVENT_ENTER, commonParent );
	}

	SStateEvent m_pendingTransitionStateEvent;
private: // DO_NOT_IMPLEMENT!
	CStateHierarchy();
	CStateHierarchy( const CStateHierarchy& );
	void operator=( const CStateHierarchy& );
};

//////////////////////////////////////////////////////////////////////////
// StateMachine

template<typename HOST>
class CStateMachine
{
	typedef CStateHelper<HOST, CStateHierarchy<HOST> > STATE_HELPER;
public:
	CStateMachine()
		:	m_pCurrentStateHierarchy(NULL)
		,	m_processingEvent(false)
	{
	}

	void StateMachineInit( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg )
	{
		CRY_ASSERT( !m_pCurrentStateHierarchy );
		m_pCurrentStateHierarchy = STATE_HELPER::StateNew( host, stateMachineReg, STATE_FIRST );

		STATE_HELPER::StateInit( host, stateMachineReg, m_pCurrentStateHierarchy );
	}

	void StateMachineRelease( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg )
	{
		if( m_pCurrentStateHierarchy )
		{
			if( m_pCurrentStateHierarchy->m_pTransitionStateHierarchy )
			{
				STATE_HELPER::StateDelete( host, stateMachineReg, m_pCurrentStateHierarchy->m_pTransitionStateHierarchy );
			}

			STATE_HELPER::StateRelease( host, stateMachineReg, m_pCurrentStateHierarchy );
			STATE_HELPER::StateDelete( host, stateMachineReg, m_pCurrentStateHierarchy );
		}
		stl::free_container( m_pendingEvents );
	}

	void StateMachineHandleEvent( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg, const SStateEvent& event )
	{
		if( !m_processingEvent )
		{
			m_processingEvent = true;
			STATE_HELPER::StateMachineHandleEventForState( host, stateMachineReg, m_pCurrentStateHierarchy, STATE_DEBUG_APPEND_EVENT( event ), 0 );
			m_processingEvent = false;

			while( !m_pendingEvents.empty() )
			{
				const SStateEvent pendingEvent = m_pendingEvents.front();
				m_pendingEvents.pop();
				StateMachineHandleEvent( host, stateMachineReg, pendingEvent );
			}
		}
		else
		{
			CRY_ASSERT( sizeof(event) == sizeof(SStateEvent) );

			m_pendingEvents.push( event );
		}
	}

	void StateMachineUpdateTime( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg, float frameTime, bool debug )
	{
		CRY_ASSERT( m_pCurrentStateHierarchy );

#ifdef STATE_DEBUG
		if( debug )
		{
			SStateDebugContext debugCtx(gEnv->pRenderer, gEnv->pAuxGeomRenderer);

			const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};

			//History
			debugCtx.m_baseHorizontal = 450.0f;
			debugCtx.m_currentVertical = 50.0f;
			IRenderAuxText::Draw2dLabel(debugCtx.m_baseHorizontal, debugCtx.m_currentVertical, 1.5f, white, false, "=== HISTORY ===" );

			debugCtx.m_baseHorizontal += 10.0f;

			for (int i = m_pCurrentStateHierarchy->m_historyDebug.GetFront(); i >= 0; --i)
			{
				debugCtx.m_currentVertical += 15.0f;

				IRenderAuxText::Draw2dLabel(debugCtx.m_baseHorizontal, debugCtx.m_currentVertical, 1.4f, white, false, "%s", m_pCurrentStateHierarchy->m_historyDebug.GetStateAt(i).c_str() );
			}

			for (int i = m_pCurrentStateHierarchy->m_historyDebug.GetBack(); i > m_pCurrentStateHierarchy->m_historyDebug.GetFront(); --i)
			{
				debugCtx.m_currentVertical += 15.0f;

				IRenderAuxText::Draw2dLabel(debugCtx.m_baseHorizontal, debugCtx.m_currentVertical, 1.4f, white, false, "%s", m_pCurrentStateHierarchy->m_historyDebug.GetStateAt(i).c_str() );
			}

			SStateEvent debugEvent( STATE_EVENT_DEBUG );
			StateMachineHandleEvent( host, stateMachineReg, debugEvent );
		}
#endif
	}

	void StateMachineSerialize( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg, const SStateEvent& event )
	{
		switch( event.GetEventId() )
		{
		case STATE_EVENT_SERIALIZE:
			{
				bool bShouldDispatchEvent = true;
				TSerialize& ser = static_cast<const SStateEventSerialize&> (event).GetSerializer();
				if( ser.IsWriting() )
				{
					int hierarchyID = m_pCurrentStateHierarchy->GetStateID();
					ser.Value( "HierarchyID", hierarchyID );
					ser.Value( "StateHierarchy",  m_pCurrentStateHierarchy->m_currentState.m_hierarchy );
					ser.Value( "StateID", m_pCurrentStateHierarchy->m_currentState.m_stateID );
					ser.Value( "StateName", m_pCurrentStateHierarchy->m_currentState.m_name );
				}
				else
				{
					int hierarchyID = 0;
					uint stateID = 0;
					uint32 stateName = 0;
					int64 stateHierarchy = 0;
					ser.Value( "HierarchyID", hierarchyID );
					ser.Value( "StateHierarchy",  stateHierarchy );
					ser.Value( "StateID", stateID );
					ser.Value( "StateName", stateName );

					// If we're not in the correct hierarchy, need to get there!
					if( m_pCurrentStateHierarchy->GetStateID() != hierarchyID )
					{
						if( m_pCurrentStateHierarchy->m_pTransitionStateHierarchy )
						{
							STATE_HELPER::StateDelete( host, stateMachineReg, m_pCurrentStateHierarchy->m_pTransitionStateHierarchy );
						}
						m_pCurrentStateHierarchy->m_pTransitionStateHierarchy = STATE_HELPER::StateNew( host, stateMachineReg, hierarchyID );
						STATE_HELPER::StateTransition( host, stateMachineReg, m_pCurrentStateHierarchy );
					}

					// Get into the required state.
					bShouldDispatchEvent = m_pCurrentStateHierarchy->TransitionFromCurrentToSubState( host, stateMachineReg, stateHierarchy, stateID, stateName );
				}

				// dispatch the serialize event, unless we failed to transition for some reason.
				if( bShouldDispatchEvent )
				{
					STATE_HELPER::RecursiveToCommonReverse( host, m_pCurrentStateHierarchy->m_currentState, 0, m_pCurrentStateHierarchy, event );
				}
			}
			break;
		case STATE_EVENT_NETSERIALIZE:
			{
				// TODO!!!
				// Need to also enforce the correct hierarchy/stateID.
				// Might just be the same code actually.
				// Hmm, I guess we don't need to check anything, we just transition as we're obviously the same code.
			}
			break;
		}
	}

	void StateMachineReset( HOST& host, CStateMachineRegistration<HOST>& stateMachineReg )
	{
		CRY_ASSERT( m_pCurrentStateHierarchy );

		if( m_pCurrentStateHierarchy->m_pTransitionStateHierarchy )
		{
			STATE_HELPER::StateDelete( host, stateMachineReg, m_pCurrentStateHierarchy->m_pTransitionStateHierarchy );
		}

		stl::free_container( m_pendingEvents );

		// clear flags!
		m_pCurrentStateHierarchy->m_flags.ClearAllFlags();

		// We must correctly follow the transition rules to reset the state machine.
		m_pCurrentStateHierarchy->m_pTransitionStateHierarchy = STATE_HELPER::StateNew( host, stateMachineReg, STATE_FIRST );

		STATE_HELPER::StateTransition( host, stateMachineReg, m_pCurrentStateHierarchy );
	}

	bool StateMachineActiveFlag( int flag ) const
	{
		CRY_ASSERT( m_pCurrentStateHierarchy );

		return( m_pCurrentStateHierarchy->m_flags.AreAllFlagsActive( flag ) );
	}

	bool StateMachineAnyActiveFlag( int flag ) const
	{
		CRY_ASSERT( m_pCurrentStateHierarchy );

		return( m_pCurrentStateHierarchy->m_flags.AreAnyFlagsActive( flag ) );
	}

	void StateMachineAddFlag( int flag )
	{
		CRY_ASSERT( m_pCurrentStateHierarchy );

		m_pCurrentStateHierarchy->m_flags.AddFlags( flag );
	}

	void StateMachineClearFlag( int flag )
	{
		CRY_ASSERT( m_pCurrentStateHierarchy );

		m_pCurrentStateHierarchy->m_flags.ClearFlags( flag );
	}

private:

	CStateHierarchy<HOST>* m_pCurrentStateHierarchy;

	typedef std::queue<SStateEvent> TEventQueue;
	TEventQueue m_pendingEvents;
	bool m_processingEvent;
};

#ifdef STATE_DEBUG
	template<typename HOST>
	const SStateEvent SStateDebugContext::StateDebugAndLog( CStateHierarchy<HOST>* pState, const char* stateName, SStateEvent stateEvent )
	{
		SStateEvent event( stateEvent );
		static SStateDebugContext debugContext( gEnv->pRenderer, gEnv->pAuxGeomRenderer );
		event.AddDebugContext( debugContext );
		if( stateEvent.GetEventId() < STATE_EVENT_CUSTOM )
		{
			AUTOENUM_BUILDNAMEARRAY( events, eStateEvents );
			STATE_DEBUG_EVENT_LOG_TO_HISTORY( pState, state_white, "State: %s; Event: %s", stateName, events[stateEvent.GetEventId()-1] );
		}
		else
		{
			STATE_DEBUG_EVENT_LOG_TO_HISTORY( pState, state_white, "State: %s; Event: Custom", stateName );
		}
		return event;
	}
#endif


	#define DECLARE_STATE_MACHINE( host, name ) \
		public: \
			static CStateMachineRegistration<host>* s_pStateMachineRegistration##name; \
			static void RegisterState( CStateProxy<host>::CreateStatePtr createPtr, CStateProxy<host>::DeleteStatePtr deletePtr, uint stateID ); \
			static void UnRegisterState( uint stateID ); \
			void StateMachineHandleEvent##name( const SStateEvent& event ); \
		private: \
			CStateMachine<host> m_stateMachine##name; \
			void StateMachineInit##name();\
			void StateMachineRelease##name();\
			void StateMachineReset##name();\
			void StateMachineUpdate##name( const float frameTime, const bool bShouldDebug = false );\
			void StateMachineSerialize##name( const SStateEvent& event );

	#define DEFINE_STATE_MACHINE( host, name )\
		void host::RegisterState( CStateProxy<host>::CreateStatePtr createPtr, CStateProxy<host>::DeleteStatePtr deletePtr, uint stateID ) \
		{\
			if( s_pStateMachineRegistration##name == NULL ) \
			{\
				s_pStateMachineRegistration##name = new CStateMachineRegistration<host>;\
			}\
			s_pStateMachineRegistration##name->RegisterState( createPtr, deletePtr, stateID ); \
		}\
		void host::UnRegisterState( uint stateID ) \
		{\
		}\
		CStateMachineRegistration<host>* host::s_pStateMachineRegistration##name = NULL; \
		void host::StateMachineHandleEvent##name( const SStateEvent& event ) \
		{\
			CRY_ASSERT_TRACE( s_pStateMachineRegistration##name, ("HSM: Somehow the registration class is NULL for the <%s> State Machine", #name) );\
			m_stateMachine##name.StateMachineHandleEvent( *this, *s_pStateMachineRegistration##name, event ); \
		}\
		void host::StateMachineInit##name()\
		{\
			CRY_ASSERT_TRACE( s_pStateMachineRegistration##name, ("HSM: Somehow the registration class is NULL for the <%s> State Machine", #name) );\
			m_stateMachine##name.StateMachineInit( *this, *s_pStateMachineRegistration##name );\
		}\
		void host::StateMachineRelease##name()\
		{\
			CRY_ASSERT_TRACE( s_pStateMachineRegistration##name, ("HSM: Somehow the registration class is NULL for the <%s> State Machine", #name) );\
			m_stateMachine##name.StateMachineRelease( *this, *s_pStateMachineRegistration##name );\
		}\
		void host::StateMachineReset##name()\
		{\
			CRY_ASSERT_TRACE( s_pStateMachineRegistration##name, ("HSM: Somehow the registration class is NULL for the <%s> State Machine", #name) );\
			m_stateMachine##name.StateMachineReset( *this, *s_pStateMachineRegistration##name );\
		}\
		void host::StateMachineUpdate##name( const float frameTime, const bool bShouldDebug )\
		{\
			CRY_ASSERT_TRACE( s_pStateMachineRegistration##name, ("HSM: Somehow the registration class is NULL for the <%s> State Machine", #name) );\
			m_stateMachine##name.StateMachineUpdateTime( *this, *s_pStateMachineRegistration##name, frameTime, bShouldDebug );\
		}\
		void host::StateMachineSerialize##name( const SStateEvent& event )\
		{\
			CRY_ASSERT_TRACE( s_pStateMachineRegistration##name, ("HSM: Somehow the registration class is NULL for the <%s> State Machine", #name) );\
			m_stateMachine##name.StateMachineSerialize( *this, *s_pStateMachineRegistration##name, event );\
		}

	#define DECLARE_STATE_CLASS_BEGIN( host, stateClass ) \
		private: \
			friend class host; \
			uint m_subStateIndex; \
		public:\
			stateClass( CStateMachineRegistration<host>& stateMachineReg ); \
			static CStateHierarchy<host>* Create( CStateMachineRegistration<host>& stateMachineReg ); \
			static void					 Delete( CStateHierarchy<host>*& pState ); \
			static uint					 Register(); \
			static void					 UnRegister(); \
		private:

	#define DECLARE_STATE_CLASS_ADD( host, stateName ) \
		const TStateIndex stateName( host& blah, const SStateEvent& event ); \
		TStateIndex State_##stateName;

	#define DECLARE_STATE_CLASS_ADD_DUMMY( host, stateName ) \
		TStateIndex State_##stateName;

	#define DECLARE_STATE_CLASS_END( host ) \
		DECLARE_STATE_CLASS_ADD( host, Root )

	#define DEFINE_STATE_CLASS_BEGIN( host, stateClass, stateId, defaultState )\
		CStateHierarchy<host>* stateClass::Create( CStateMachineRegistration<host>& stateMachineReg ) { return new stateClass(stateMachineReg); } \
		void					stateClass::Delete( CStateHierarchy<host>*& pState ) { SAFE_DELETE( pState ); } \
		uint stateClass::Register() \
		{ \
			host::RegisterState( &stateClass::Create, &stateClass::Delete, stateId );\
			return stateId; \
		}\
		void	stateClass::UnRegister()\
		{\
			host::UnRegisterState( stateId );\
		}\
		stateClass::stateClass( CStateMachineRegistration<host>& stateMachineReg )\
		:	CStateHierarchy<host>( stateId, State_##defaultState, stateMachineReg )\
		, m_subStateIndex(1) \
		{\
			State_Root = SStateIndex<host> ("Root", (CStateProxy<host>::StatePtr)&stateClass::Root, NULL, m_subStateIndex++ ); \
			m_stateIndexContainer.push_back( &State_Root );

	#define DEFINE_STATE_CLASS_ADD( host, stateClass, stateFunc, parentState )\
		  State_##stateFunc = SStateIndex<host> (#stateFunc, (CStateProxy<host>::StatePtr)&stateClass::stateFunc, &State_##parentState, m_subStateIndex++ );\
			m_stateIndexContainer.push_back( &State_##stateFunc );

	#define DEFINE_STATE_CLASS_ADD_DUMMY( host, stateClass, stateDummy, stateFunc, parentState )\
		  State_##stateDummy = SStateIndex<host> (#stateDummy, (CStateProxy<host>::StatePtr)&stateClass::stateFunc, &State_##parentState, m_subStateIndex++ );\
			m_stateIndexContainer.push_back( &State_##stateDummy );

	#define DEFINE_STATE_CLASS_END( host, stateClass )\
		}\
		uint id##host##stateClass = stateClass::Register();

#endif // __State_h__
