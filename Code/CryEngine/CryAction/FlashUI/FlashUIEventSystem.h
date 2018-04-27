// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIEventSystem.h
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __FlashUIEventSystem_H__
#define __FlashUIEventSystem_H__

#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryCore/Containers/CryListenerSet.h>

class CFlashUIEventSystem
	: public IUIEventSystem
{
public:
	CFlashUIEventSystem(const char* sName, EEventSystemType eType) : m_sName(sName), m_eType(eType), m_listener(2) {};
	virtual ~CFlashUIEventSystem();

	virtual const char*                      GetName() const { return m_sName.c_str(); }
	virtual IUIEventSystem::EEventSystemType GetType() const { return m_eType; }

	virtual uint                             RegisterEvent(const SUIEventDesc& sEventDesc);

	virtual void                             RegisterListener(IUIEventListener* pListener, const char* name);
	virtual void                             UnregisterListener(IUIEventListener* pListener);

	virtual SUIArgumentsRet                  SendEvent(const SUIEvent& event);

	virtual const SUIEventDesc*              GetEventDesc(int index) const              { return index < m_eventDescriptions.size() ? m_eventDescriptions[index] : NULL; }
	virtual const SUIEventDesc*              GetEventDesc(const char* sEventName) const { return m_eventDescriptions(sEventName); }
	virtual int                              GetEventCount()  const                     { return m_eventDescriptions.size(); }

	virtual uint                             GetEventId(const char* sEventName);

	virtual void                             GetMemoryUsage(ICrySizer* s) const;

private:
	string           m_sName;
	EEventSystemType m_eType;
	TUIEventsLookup  m_eventDescriptions;

	typedef CListenerSet<IUIEventListener*> TEventListener;
	TEventListener m_listener;
};

typedef std::map<string, CFlashUIEventSystem*> TUIEventSystemMap;

struct CUIEventSystemIterator : public IUIEventSystemIterator
{
	CUIEventSystemIterator(TUIEventSystemMap* pMap);
	virtual void            AddRef();
	virtual void            Release();
	virtual IUIEventSystem* Next(string& sName);

private:
	int                         m_iRefs;
	TUIEventSystemMap::iterator m_currIter;
	TUIEventSystemMap::iterator m_endIter;
};

#endif // #ifndef __FlashUIEventSystem_H__
