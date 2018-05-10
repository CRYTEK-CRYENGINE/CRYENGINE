// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QObject>
#include <QPointF>
#include <QSizeF>
#include <QColor>
#include <QNamespace.h>

#include <EditorFramework/Events.h>
#include <CryIcon.h>

#include "NodeGraphViewStyle.h"

class QWidget;
class QGraphicsItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneContextMenuEvent;

class CAbstractDictionary;

namespace CryGraphEditor {

class CNodeGraphViewGraphicsWidget;
typedef std::set<CNodeGraphViewGraphicsWidget*> GraphViewWidgetSet;

class CAbstractNodeGraphViewModelItem;
typedef std::set<CAbstractNodeGraphViewModelItem*> GraphItemSet;

typedef std::vector<QVariant>                      GraphItemIds;

// TODO: Move this into another header
class CConnectionPoint : public QObject
{
	Q_OBJECT

public:
	CConnectionPoint() {}
	~CConnectionPoint() {}

	QPointF GetPosition() const           { return m_connectionPoint; }
	void    SetPosition(QPointF position) { m_connectionPoint = position; SignalPositionChanged(); }

Q_SIGNALS:
	void SignalPositionChanged();

private:
	QPointF m_connectionPoint;
};
// ~TODO

enum EGraphViewWidgetType : int32
{
	eGraphViewWidgetType_Unset = 0,
	eGraphViewWidgetType_PinWidget,
	eGraphViewWidgetType_NodeWidget,
	eGraphViewWidgetType_ConnectionWidget,
	eGraphViewWidgetType_GroupWidget,

	eGraphViewWidgetType_UserType
};

enum EItemType : int32
{
	eItemType_Unset = 0,
	eItemType_Node,
	eItemType_Pin,
	eItemType_Connection,
	eItemType_Comment,
	eItemType_Group,

	eItemType_UserType
};

class CAbstractPinItem;

enum class EMouseEventReason
{
	ButtonPress,
	ButtonRelease,
	ButtonPressAndMove,
	HoverEnter,
	HoverMove,
	HoverLeave
};

struct SMouseInputEventArgs
{
	SMouseInputEventArgs(
	  EMouseEventReason reason,
	  Qt::MouseButton button,
	  Qt::MouseButtons buttons,
	  Qt::KeyboardModifiers modifiers,
	  QPointF localPos,
	  QPointF scenePos,
	  QPointF screenPos,
	  QPointF lastLocalPos,
	  QPointF lastScenePos,
	  QPointF lastScreenPos)
		: m_reason(reason)
		, m_button(button)
		, m_buttons(buttons)
		, m_modifiers(modifiers)
		, m_localPos(localPos)
		, m_scenePos(scenePos)
		, m_screenPos(screenPos)
		, m_lastLocalPos(lastLocalPos)
		, m_lastScenePos(lastScenePos)
		, m_lastScreenPos(lastScreenPos)
	{}

	SMouseInputEventArgs(SMouseInputEventArgs&& eventArgs)
	{
		m_reason = eventArgs.m_reason;
		m_button = eventArgs.m_button;
		m_buttons = eventArgs.m_buttons;
		m_modifiers = eventArgs.m_modifiers;
		m_localPos = eventArgs.m_localPos;
		m_scenePos = eventArgs.m_scenePos;
		m_screenPos = eventArgs.m_screenPos;
		m_lastLocalPos = eventArgs.m_lastLocalPos;
		m_lastScenePos = eventArgs.m_lastScenePos;
		m_lastScreenPos = eventArgs.m_lastScreenPos;
		m_isAccepted = true;
	}

	EMouseEventReason     GetReason() const            { return m_reason; }

	Qt::MouseButton       GetButton() const            { return m_button; }
	Qt::MouseButtons      GetButtons() const           { return m_buttons; }
	Qt::MouseEventFlags   GetFlags() const             { return m_flags; }
	Qt::KeyboardModifiers GetModifiers() const         { return m_modifiers; }

	const QPointF&        GetLocalPos() const          { return m_localPos; }
	const QPointF&        GetScenePos() const          { return m_scenePos; }
	const QPointF&        GetScreenPos() const         { return m_screenPos; }

	const QPointF&        GetLastLocalPos() const      { return m_lastLocalPos; }
	const QPointF&        GetLastScenePos() const      { return m_lastScenePos; }
	const QPointF&        GetLastScreenPos() const     { return m_lastScreenPos; }

	bool                  GetAccepted() const          { return m_isAccepted; }
	void                  SetAccepted(bool isAccepted) { m_isAccepted = isAccepted; }

private:
	EMouseEventReason     m_reason;

	Qt::MouseButton       m_button;
	Qt::MouseButtons      m_buttons;
	Qt::MouseEventFlags   m_flags;
	Qt::KeyboardModifiers m_modifiers;

	QPointF               m_localPos;
	QPointF               m_scenePos;
	QPointF               m_screenPos;

	QPointF               m_lastLocalPos;
	QPointF               m_lastScenePos;
	QPointF               m_lastScreenPos;

	bool                  m_isAccepted;
};

struct SNodeMouseEventArgs : public SMouseInputEventArgs
{
	SNodeMouseEventArgs(SMouseInputEventArgs& eventArgs)
		: SMouseInputEventArgs(static_cast<SMouseInputEventArgs &&>(eventArgs))
	{}
};

struct SPinMouseEventArgs : public SMouseInputEventArgs
{

	SPinMouseEventArgs(SMouseInputEventArgs& eventArgs)
		: SMouseInputEventArgs(static_cast<SMouseInputEventArgs &&>(eventArgs))
	{}
};

struct SConnectionMouseEventArgs : public SMouseInputEventArgs
{
	SConnectionMouseEventArgs(SMouseInputEventArgs& eventArgs, bool isInsideShape)
		: SMouseInputEventArgs(static_cast<SMouseInputEventArgs &&>(eventArgs))
		, m_isInsideShape(isInsideShape)
	{}

	bool IsInsideShape() const { return m_isInsideShape; }

private:
	bool m_isInsideShape;
};

struct SGroupMouseEventArgs : public SMouseInputEventArgs
{
	SGroupMouseEventArgs(SMouseInputEventArgs& eventArgs)
		: SMouseInputEventArgs(static_cast<SMouseInputEventArgs &&>(eventArgs))
	{}
};

struct SBeginCreateConnectionEventArgs
{
	CAbstractPinItem& sourcePin;

	SBeginCreateConnectionEventArgs(CAbstractPinItem& _sourcePin)
		: sourcePin(_sourcePin)
	{}
};

struct SEndCreateConnectionEventArgs
{
	CAbstractPinItem& sourcePin;
	CAbstractPinItem* pTargetPin;
	bool              wasCanceled : 1;
	bool              didFail     : 1;

	SEndCreateConnectionEventArgs(CAbstractPinItem& _sourcePin)
		: sourcePin(_sourcePin)
		, pTargetPin(nullptr)
		, wasCanceled(true)
	{}
};

// TODO: Get rid of this at some point.
struct IConnectionEventsListener
{
	virtual ~IConnectionEventsListener() {}

	virtual void OnBeginCreateConnection(QWidget* pSender, SBeginCreateConnectionEventArgs& args) = 0;
	virtual void OnEndCreateConnection(QWidget* pSender, SEndCreateConnectionEventArgs& args) = 0;
};
// ~TODO

struct SSelectionEventArgs
{
	QWidget*     pPropertiesWidget;
	GraphItemSet selecteditems;

	SSelectionEventArgs()
		: pPropertiesWidget(nullptr)
	{}
};

// TODO: Move this into CryGraph namespace?!
class CNodeGraphViewStyle;

struct INodeGraphRuntimeContext
{
	virtual ~INodeGraphRuntimeContext() {}

	virtual const char*                GetTypeName() const = 0;
	virtual CAbstractDictionary*       GetAvailableNodesDictionary() = 0;
	virtual const CNodeGraphViewStyle* GetStyle() const = 0;
};
// ~TODO

// TODO: Move this into a separate file.
template<uint32 TCount>
class CIconArray
{
public:
	enum EType
	{
		PinIcon,
		HeaderIcon,
	};

public:
	CIconArray<TCount>::CIconArray()
	{
		memset(m_pIcons, 0, sizeof(m_pIcons));
	}

	~CIconArray()
	{
		for (QPixmap* pIcon : m_pIcons)
		{
			if (pIcon)
				delete pIcon;
		}
	}

	QPixmap* GetIcon(uint32 index) const
	{
		return m_pIcons[index];
	}

	void SetIcon(uint32 index, CryIcon& icon, EType type)
	{
		QSizeF s;
		switch (type)
		{
		case EType::PinIcon:
			s = QSizeF(9.0, 9.0);
			break;
		case EType::HeaderIcon:
			s = QSizeF(16.0, 16.0);
			break;
		}

		m_pIcons[index] = new QPixmap(icon.pixmap(s.width(), s.height(), QIcon::Normal));
	}

private:
	QPixmap* m_pIcons[TCount];
};
// ~TODO

}

