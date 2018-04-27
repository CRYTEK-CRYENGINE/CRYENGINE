// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <CrySandbox/CrySignal.h>
#include "Events.h"
#include <QObject>

class QEvent;

//! Broadcast Manager is intended to be the primary communication method between panels that should otherwise not depend or have direct dependency on each other.
//! This enables architectures such as an inspector being able to handle displaying properties from any panel, without the other panel knowing about the inspector, or even if there is one or multiple inspectors.
//! Broadcasting is also useful because anything can register and listen to messages, not only UI components, and some messages may want to be handled anywhere. 
//! Examples of this can be the "Save All" command, which anything may want to handle, or the "Application is about to quit" event to be able to collect and display the unsaved changes.
//!
//!	For this purpose there are several "contexts" of broadcasting. There is a global broadcast context used for the latter, and accessible through GetIEditor()->GetGlobalBroadcastManager();
//! And there is a more "local" context to connect the different UI components that belong to a similar context. This is conveniently implemented by CEditor class, which encapsulates one editing context.
//! This means different level editor panels are connected to the same broadcast manager but another editor with its own subpanels has its own context and does not interfere with the level editor, and vice versa.
//!	CEditor tries to ensure lifetime of the local broadcast manager to be longer than its children widgets, however as this is dependent on Qt and platforms, it is good practice to have safety measures
//
// How to use the broadcast manager:
//
// - Get broadcast manager for current context:
//	 This needs a widget as context is determined by widget hierarchy. Children of CEditor or panels of CEditor will work automatically, otherwise pass the parent CEditor.
//	 If you need to store a pointer to the broadcast manager, it is good practice to do it in the form of QPointer<CBroadcastManager>.
//	 This will prevent crashes if the broadcast manager is deleted before the listener and the listener tries to disconnect subsequently.
//
//   CBroadcastManager* pBroadcastManager = CBroadcastManager::Get(this);
//   if (pBroadcastManager)
//   {
//	   // Store in QPointer<CBroadcastManager> member in order to disconnect later
//     // Do stuff...
//   }
//
// - Connect:
//   Listeners can connect either to a single event type or for all event types at once. For the latter simply connect to BroadcastEvent::All.
//   When connecting a non-member function you need to pass an ID with it. This ID must be unique and always the same for that specific function.
//
//     // Connect lambda
//     pBroadcastManager->Connect(BroadcastEvent::SomeType, [](BroadcastEvent& event) { /* Do useful stuff */ }, 12345);
//
//     // Connect member function
//     pBroadcastManager->Connect(BroadcastEvent::SomeType, this, &MyClass::Foo);
//
// - Disconnect:
//	 Do not forget to disconnect  or before destruction of your listener or risk a crash. 
//	 If the listener may not be able to reach its broadcast manager through CBroadcastManager::Get, you need to store a reference to the connected broadcast manager. Use QPointer<CBroadcastManager> for good practice.
//   To disconnect all listeners that share the same id/type/object at once you can use the DisconnectById/DisconnectByType/DisconnectByObject functions.
//   Individual listeners are removed by calling Disconnect with the type that they are connected to and their id or object pointer.
//
//     // Disconnect from specific event type by ID
//     pBroadcastManager->Disconnect(BroadcastEvent::SomeType, 12345);
//
//     // Disconnect from all connected types by ID
//     pBroadcastManager->DisconnectById(12345);
//
//     // Disconnect object from specific event type
//     pBroadcastManager->Disconnect(BroadcastEvent::SomeType, this);
//
//     // Disconnect object from all connected types
//     pBroadcastManager->DisconnectByObject(this);
//
// - Send events:
//   To broadcast an event you can call CBroadcastManager::Broadcast() or equivalent helper methods of BroadcastEvent
//
//     SomeBroadcastEvent broadcastEvent;
//     pBroadcastManager->Broadcast(broadcastEvent);
//		// or equivalent syntax
//	   broadcastEvent.Broadcast(pBroadcastManager);	
//

class EDITOR_COMMON_API CBroadcastManager : public QObject
{
	Q_OBJECT
public:

	CBroadcastManager();
	~CBroadcastManager();

	template<typename Function, typename std::enable_if<IsCallable<Function>::value,int>::type* = 0>
	void Connect(BroadcastEvent::EventType type, Function&& func, uintptr_t id = 0)
	{
		m_listeners[type].Connect(func, id);
	}

	template<typename Object, typename MemberFunction, typename std::enable_if<CryMemFunTraits<MemberFunction>::isMemberFunction, int>::type* = 0>
	void Connect(BroadcastEvent::EventType type, Object* pObject, MemberFunction function, uintptr_t forceId = 0)
	{
		m_listeners[type].Connect(pObject, function, forceId);
	}

	void DisconnectById(BroadcastEvent::EventType type, uintptr_t id);

	template<typename Object>
	void DisconnectObject(BroadcastEvent::EventType type, Object* pObject)
	{
		DisconnectById(type, reinterpret_cast<uintptr_t>(pObject));
	}

	void                      DisconnectAll();
	void                      DisconnectById(uintptr_t id);

	template<typename Object>
	void                      DisconnectObject(Object* pObject) { DisconnectById(reinterpret_cast<uintptr_t>(pObject)); }

	static CBroadcastManager* Get(QWidget* const pContextWidget);

	//! Broadcasts this event to all listeners
	void Broadcast(BroadcastEvent& event);

private:

	typedef std::map<BroadcastEvent::EventType, CCrySignal<void(BroadcastEvent&)>> ListenerMap;
	ListenerMap m_listeners;
};

