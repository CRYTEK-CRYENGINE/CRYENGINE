// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// CryCommon
#include <CrySandbox/CrySignal.h>

// Qt
#include <QWidget>

//EditorCommon
class CDragHandle;
class CToolBarArea;

// Qt
class QBoxLayout;
class QToolBar;

class CToolBarAreaItem : public QWidget
{
	Q_OBJECT
public:
	enum class Type
	{
		ToolBar,
		Spacer,
	};

	void               SetArea(CToolBarArea* pArea) { m_pArea = pArea; }
	virtual void       SetLocked(bool isLocked);
	virtual void       SetOrientation(Qt::Orientation orientation);

	virtual Type       GetType() const = 0;
	CToolBarArea*      GetArea() const        { return m_pArea; }
	Qt::Orientation    GetOrientation() const { return m_orientation; }
	virtual QSize      GetMinimumSize() const { return minimumSize(); }
	static const char* GetMimeType()          { return "CToolBarAreaItem"; }

protected:
	CToolBarAreaItem(CToolBarArea* pArea, Qt::Orientation orientation);
	void SetContent(QWidget* pNewContent);

	void paintEvent(QPaintEvent* pEvent) override;

private:
	void OnDragStart();
	void OnDragEnd();

public:
	CCrySignal<void(CToolBarAreaItem*)> signalDragStart;
	CCrySignal<void(CToolBarAreaItem*)> signalDragEnd;

protected:
	CToolBarArea*   m_pArea;
	CDragHandle*    m_pDragHandle;
	QWidget*        m_pContent;
	QBoxLayout*     m_pLayout;
	Qt::Orientation m_orientation;
};

class CToolBarItem : public CToolBarAreaItem
{
	Q_OBJECT
public:
	void      ReplaceToolBar(QToolBar* pNewToolBar);
	void      SetOrientation(Qt::Orientation orientation) override;

	virtual QSize      GetMinimumSize() const override;
	Type      GetType() const override { return Type::ToolBar; }
	QString   GetTitle() const;
	QString   GetName() const;
	QToolBar* GetToolBar() const;

protected:
	void paintEvent(QPaintEvent* pEvent) override;

private:
	friend CToolBarArea;
	CToolBarItem(CToolBarArea* pArea, QToolBar* pToolBar, Qt::Orientation orientation);

private:
	QToolBar* m_pToolBar;
};

class CSpacerItem : public CToolBarAreaItem
{
	Q_OBJECT
public:
	enum class SpacerType
	{
		Expanding,
		Fixed,
	};

	void       SetLocked(bool isLocked) override;
	void       SetOrientation(Qt::Orientation orientation) override;
	void       SetSpacerType(SpacerType spacerType);

	Type       GetType() const override { return Type::Spacer; }
	SpacerType GetSpacerType() const    { return m_spacerType; }

private:
	friend CToolBarArea;
	CSpacerItem(CToolBarArea* pArea, SpacerType spacerType, Qt::Orientation orientation);

protected:
	QSize sizeHint() const override { return QSize(24, 24); }
	void  paintEvent(QPaintEvent* pEvent) override;

private:
	SpacerType m_spacerType;
};
