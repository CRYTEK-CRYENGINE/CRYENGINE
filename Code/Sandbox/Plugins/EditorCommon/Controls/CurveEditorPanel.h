// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "QtViewPane.h"

#include <QPointer>
#include <QRect>
#include <QSize>
#include <QString>

class QWidget;
class QObject;
class QLabel;

class CBroadcastManager;
class BroadcastEvent;

class CCurveEditor;
struct SCurveEditorContent;

class EDITOR_COMMON_API CCurveEditorPanel : public CDockableWidget
{
	Q_OBJECT

public:
	CCurveEditorPanel(QWidget* pParent = nullptr);
	virtual ~CCurveEditorPanel();

	// CDockableWidget
	virtual const char* GetPaneTitle() const override { return "CurveEditor"; }
	virtual QRect       GetPaneRect() override        { return QRect(0, 0, 200, 200); }
	// ~CDockableWidget

	void                 SetTitle(const char* szTitle);

	SCurveEditorContent* GetEditorContent() const;
	void                 SetEditorContent(SCurveEditorContent* pContent);

	CCurveEditor& GetEditor() { return *m_pEditor; }

	void          ConnectToBroadcast(CBroadcastManager* pBroadcastManager);

	virtual QSize sizeHint() const override { return QSize(240, 600); }

protected:

	void OnPopulate(BroadcastEvent& event);
	void OnContentDestroyed(QObject* pObject);

	void Disconnect();

private:
	QLabel*                     m_pTitle;
	CCurveEditor*               m_pEditor;
	QPointer<CBroadcastManager> m_broadcastManager;
};

