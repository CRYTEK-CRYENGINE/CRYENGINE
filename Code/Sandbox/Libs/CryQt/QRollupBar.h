// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QObject>
#include <QWidget>
#include <QPoint>
#include <QMap>

#include "CryQtAPI.h"
#include "CryQtCompatibility.h"

class QScrollableBox;
class QCollapsibleFrame;

class CRYQT_API QRollupBar : public QWidget
{
	Q_OBJECT

public:
	QRollupBar(QWidget *parent);
	~QRollupBar();
	int addWidget(QWidget* widget);
	void insertWidget(QWidget* widget, int index);
	void removeWidget(QWidget* widget);
	void removeAt(int index);
	virtual int indexOf(QWidget* widget) const;
	int count() const;
	void clear();
	bool isDragHandle(QWidget*);
	QWidget* getDragHandleAt(int index);
	QWidget* getDragHandle(QWidget*);
	QWidget* widget(int index) const;
	void SetRollupsClosable(bool);
	void SetRollupsReorderable(bool);
	bool RollupsReorderable() { return m_canReorder; }
protected:
	QWidget* getDropTarget() const;
	bool eventFilter(QObject * o, QEvent *e);
	QCollapsibleFrame* rollupAt(int index) const;
	QCollapsibleFrame* rollupAt(QPoint pos) const;
	int rollupIndexAt(QPoint pos) const;
Q_SIGNALS:
	void RollupCloseRequested(int);

private Q_SLOTS:
	void OnRollupCloseRequested(QCollapsibleFrame*);

private:
	int attachNewWidget(QWidget* widget, int index=-1);

	QScrollableBox* m_pScrollBox;
	QPoint m_dragStartPosition;
	QWidget* m_pDragWidget;
	QMap<QWidget*, QCollapsibleFrame*> m_childRollups;
	QList<QCollapsibleFrame*> m_subFrames;
	QWidget* m_pDelimeter;
	int m_draggedId;
	bool m_DragInProgress;
	bool m_canReorder;
	bool m_rollupsClosable;
};

