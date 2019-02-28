// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IGameUIEventSystem.h
//  Version:     v1.00
//  Created:     19/03/2012 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __IGameUIEventSystem__
#define __IGameUIEventSystem__

#include <IViewSystem.h>
#include <IPlayerProfiles.h>

struct IUIPseudoRTTI
{
	virtual ~IUIPseudoRTTI() {}
	// do not override this function,
	// use UIEVENTSYSTEM( name ) instead
	virtual const char* GetTypeName() const = 0;
};

struct IUIGameEventSystem : public IUIPseudoRTTI
{
	virtual ~IUIGameEventSystem() {}
	
	virtual void InitEventSystem() = 0;
	virtual void UnloadEventSystem() = 0;

	virtual void UpdateView( const SViewParams &viewParams ) {}
	virtual void OnUpdate( float fDelta ) {}
};
typedef std::shared_ptr<IUIGameEventSystem> TUIEventSystemPtr;

struct IUIEventSystemFactory
{
	IUIEventSystemFactory()
	{
		m_pNext = 0;
		if (!s_pLast)
			s_pFirst = this;
		else
			s_pLast->m_pNext = this;
		s_pLast = this;
	}

	virtual ~IUIEventSystemFactory(){}
	virtual TUIEventSystemPtr Create() = 0;

	static IUIEventSystemFactory* GetFirst() { return s_pFirst; }
	IUIEventSystemFactory* GetNext() const { return m_pNext; }

private:
	IUIEventSystemFactory* m_pNext;
	static IUIEventSystemFactory* s_pFirst;
	static IUIEventSystemFactory* s_pLast;
};

template <class T>
struct SAutoRegUIEventSystem : public IUIEventSystemFactory
{
	virtual TUIEventSystemPtr Create() { return std::make_shared<T>(); }
};

#define UIEVENTSYSTEM(name) \
virtual const char* GetTypeName() const override { return GetTypeNameS(); } \
static const char* GetTypeNameS() { return name; }


#if CRY_PLATFORM_WINDOWS && defined(_LIB)
	#define CRY_EXPORT_STATIC_LINK_VARIABLE(Var)                        \
	  extern "C" { INT_PTR lib_func_ ## Var() { return (INT_PTR)&Var; } \
	  }                                                                 \
	  __pragma(message("#pragma comment(linker,\"/include:_lib_func_" # Var "\")"))
#else
	#define CRY_EXPORT_STATIC_LINK_VARIABLE(Var)
#endif

#define REGISTER_UI_EVENTSYSTEM( UIEventSystemClass ) \
	SAutoRegUIEventSystem<UIEventSystemClass> g_AutoRegUIEvent##UIEventSystemClass; \
	CRY_EXPORT_STATIC_LINK_VARIABLE( g_AutoRegUIEvent##UIEventSystemClass );

#endif
