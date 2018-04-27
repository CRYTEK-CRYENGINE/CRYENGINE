// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include <QString>
#include <QEvent>
#include <QVariantMap>
#include <QKeySequence>
#include <CryInput/IInput.h>

class QWidget;
class CBroadcastManager;
class CInspector;

class EDITOR_COMMON_API SandboxEvent : public QEvent
{
	friend class CEventLoopHandler;
public:

	enum EventType
	{
		First = QEvent::User,
		Command,
		GetBroadcastManager,
		CameraMovement,
		CryInput,
		MissedShortcut, // event is sent from parent-less tool windows to main frame to try to catch application wide shortcuts
		Max
	};

	SandboxEvent(EventType type, bool fallbackToParent);

	//Will send event to the widget that receives the keyboard events, this is the behavior you need for 99% of events
	void SendToKeyboardFocus();

	//Will send event to the widget that receives the mouse events,
	//which is the widget under cursor or the widgets that have grabbed mouse input (or captured during a drag operation for instance)
	//Double check if you really intend to use this behavior, this should be very rarely used
	void        SendToMouseFocus();

	static bool IsSandboxEvent(QEvent::Type type);

protected:

	bool m_beingHandled;
	bool m_fallbackToParent;
};

class EDITOR_COMMON_API CommandEvent : public SandboxEvent
{
public:
	CommandEvent(const char* command);

	const string& GetCommand() const { return m_command; }
private:

	string m_command;
};

class EDITOR_COMMON_API MissedShortcutEvent : public SandboxEvent
{
public:
	MissedShortcutEvent(QKeySequence sequence);

	const QKeySequence& GetSequence() const { return m_sequence; }
private:
	QKeySequence m_sequence;

};


class EDITOR_COMMON_API GetBroadcastManagerEvent : public SandboxEvent
{
public:
	GetBroadcastManagerEvent();

	CBroadcastManager* GetManager() const;
	void               SetManager(CBroadcastManager* const pManager);

private:
	CBroadcastManager* m_broadcastManager;
};

//
// Broadcast events
//

class EDITOR_COMMON_API BroadcastEvent : public QEvent
{
	friend class CBroadcastManager;
public:

	enum EventType
	{
		First = SandboxEvent::Max,
		All,
		PopulateInspector,
		CustomEditorEvent,
		EditCurve,
		AboutToQuit,
		Max
	};

	static bool IsBroadcastEvent(const QEvent::Type type);

	void        Broadcast(CBroadcastManager* pBroadcastManager);
	void        Broadcast(QWidget* pContextWidget);

protected:
	BroadcastEvent(EventType type);
};

//Sent by any UI component to populate the inspector in the current context.
class EDITOR_COMMON_API PopulateInspectorEvent : public BroadcastEvent
{
public: 
	typedef std::function<void(CInspector& inspector)> TCallback;

	PopulateInspectorEvent(const TCallback& factory, const char* szTitle = "")
		: BroadcastEvent(BroadcastEvent::PopulateInspector)
		, m_factory(factory)
		, m_szTitle(szTitle) {}

	const char* GetTitle() const { return m_szTitle.c_str(); }
	const TCallback GetCallback() const { return m_factory; }

private:
	const TCallback m_factory;
	string          m_szTitle;
};

//Clears all inspectors
class EDITOR_COMMON_API ClearInspectorEvent : public PopulateInspectorEvent
{
public:
	ClearInspectorEvent();
};

//This event can be used for any type of event that is not natively supported by the broadcasting system.
class EDITOR_COMMON_API CustomEditorEvent : public BroadcastEvent
{
public:
	CustomEditorEvent(const QString& action, const QVariantMap& params = QVariantMap())
		: BroadcastEvent(EventType::CustomEditorEvent)
		, m_action(action)
		, m_params(params)
	{}

	const QString&     GetAction() const { return m_action; }
	const QVariantMap& GetParams() const { return m_params; }

private:
	QString     m_action;
	QVariantMap m_params;
};

class CCurveEditorPanel;

//Sent by any UI component to populate a curve editor panel.
class EDITOR_COMMON_API EditCurveEvent : public BroadcastEvent
{
public:
	typedef std::function<void (CCurveEditorPanel&)> TSetupCallback;

	EditCurveEvent(const TSetupCallback& setup);

	void SetupEditor(CCurveEditorPanel&) const;

private:
	const TSetupCallback m_setup;
};

//! Send by the main frame when it's about to quit.
//! Receivers may ignore() this event to prevent quitting and add a changelist which contains any changes that should be saved first.
class EDITOR_COMMON_API AboutToQuitEvent : public BroadcastEvent
{
private:
	struct SChangeList
	{
		string              m_key;
		std::vector<string> m_changes;

		SChangeList(const string& key, const std::vector<string>& changes)
			: m_key(key)
			, m_changes(changes)
		{}
	};
public:
	explicit AboutToQuitEvent()
		: BroadcastEvent(BroadcastEvent::AboutToQuit)
	{}

	void AddChangeList(const string& key, const std::vector<string>& changes)
	{
		m_changeLists.emplace_back(key, changes);
	}

	size_t GetChangeListCount() const
	{
		return m_changeLists.size();
	}

	const string& GetChangeListKey(size_t index) const
	{
		return m_changeLists[index].m_key;
	}

	const std::vector<string>& GetChanges(size_t index) const
	{
		return m_changeLists[index].m_changes;
	}
private:
	std::vector<SChangeList> m_changeLists;
};

//! Generic translation/rotation message for renderviewport cameras. This can be used by 3D mouses or VR sets
//! and allows us to control all 6degrees of freedom of the camera/view
class EDITOR_COMMON_API CameraTransformEvent : public SandboxEvent
{
public:
	CameraTransformEvent();

	void        SetTranslation(Vec3& translation);
	void        SetRotation(Vec3& rotation);

	const Vec3& GetTranslation() { return m_translate; }
	const Vec3& GetRotation()    { return m_rotate; }

private:
	Vec3 m_translate;
	Vec3 m_rotate;
};

//! Allows for single system input events to be forwarded for interception and modification.
class EDITOR_COMMON_API CryInputEvent : public SandboxEvent
{
public:
	CryInputEvent(SInputEvent* pInput);

	SInputEvent* GetInputEvent() { return m_pInput; }

private:
	SInputEvent* m_pInput;
};

