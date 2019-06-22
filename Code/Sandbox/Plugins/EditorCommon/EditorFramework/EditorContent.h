// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// EditorCommon
#include "StateSerializable.h"

// Qt
#include <QWidget>

// EditorCommon
class CAbstractMenu;
class CEditor;
class CToolBarAreaManager;

// Qt
class QBoxLayout;
class QToolBar;

class CEditorContent : public QWidget, public IStateSerializable
{
	Q_OBJECT
	Q_INTERFACES(IStateSerializable)
public:
	CEditorContent(CEditor* pEditor);

	void Initialize();

	const QWidget* GetContent() const { return m_pContent; }
	void SetContent(QWidget* pContent);
	void SetContent(QLayout* pContentLayout);

	bool CustomizeToolBar();
	bool ToggleToolBarLock();

	bool AddExpandingSpacer();
	bool AddFixedSpacer();

	// Layout management
	QVariantMap GetState() const override;
	void        SetState(const QVariantMap& state) override;
	QSize       GetMinimumSizeForOrientation(Qt::Orientation orientation) const;
	void        OnAdaptiveLayoutChanged(Qt::Orientation orientation);

protected:
	void paintEvent(QPaintEvent* pEvent) override;

private:
	CEditor*               m_pEditor;
	CToolBarAreaManager*   m_pToolBarAreaManager;

	std::vector<QToolBar*> m_toolbars;

	QBoxLayout*            m_pMainLayout;
	QBoxLayout*            m_pContentLayout;
	QWidget*               m_pContent;
};
