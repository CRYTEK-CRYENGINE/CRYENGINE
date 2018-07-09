// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>
#include <QToolButton>
#include <QBoxLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyleOption>
#include <QEvent>
#include <QMouseEvent>
#include <QMimeData>
#include <QApplication>
#include <QDrag>

#include "CryIcon.h"
#include "CryQtAPI.h"
#include "CryQtCompatibility.h"
#include <memory>
#include <functional>

class QPushButton;
class QLabel;

//! This is a reusable UI component used for grouping widgets together with a title and the group can be collapsed
class CRYQT_API QCollapsibleFrame : public QWidget
{
	Q_OBJECT;

	friend class CCollapsibleFrameHeader;
public:
	explicit QCollapsibleFrame(QWidget* pParent = nullptr);
	explicit QCollapsibleFrame(const QString& title, QWidget* pParent = nullptr);
	virtual ~QCollapsibleFrame();

	QWidget* GetWidget() const;
	QWidget* GetDragHandler() const;

	//! Sets or replaces the widget in the frame. The previous widget will be queued for deletion.
	void     SetWidget(QWidget* pWidget);
	void	 SetClosable(bool closable);
	bool	 Closable() const;
	void     SetTitle(const QString& title);
	bool	 Collapsed() const;
	void     SetCollapsed(bool bCollapsed);

	void SetCollapsedStateChangeCallback(std::function<void(bool bCollapsed)> callback);

protected:
	virtual void paintEvent(QPaintEvent*) override;
	void SetHeaderWidget(CCollapsibleFrameHeader* pHeader);

	QFrame*					 m_pContentsFrame;
	CCollapsibleFrameHeader* m_pHeaderWidget;
	QWidget*                 m_pWidget;

protected slots:
	void OnCloseRequested();

signals:
	void CloseRequested(QCollapsibleFrame* caller);
};

// This class is an implementation detail of QCollapsibleFrame and
// also used so that the frame header is styleable. It should not
// be used directly which is why it has non-public members only.
// This class is not placed in the cpp file because then the moc
// file would have to be included. It cannot be nested in QCollapsibleFrame
// either because moc does not allow that
class CRYQT_API CCollapsibleFrameHeader : public QWidget
{
	Q_OBJECT;

	Q_PROPERTY(QSize iconSize READ GetIconSize WRITE SetIconSize)

	friend class QCollapsibleFrame;
public:
	CCollapsibleFrameHeader(const QString& title, QCollapsibleFrame* pParentCollapsible, const QString& icon = QString(), bool bCollapsed = false);

	QSize GetIconSize() const;
	void  SetIconSize(const QSize& iconSize);
	void  SetClosable(bool closable);
	bool  Closable() const;

	void SetCollapsed(bool bCollapsed);

	std::function<void(bool bCollapsed)> m_onCollapsedStateChanged;

protected:
	virtual void paintEvent(QPaintEvent*) override;
	virtual bool eventFilter(QObject* pWatched, QEvent* pEvent) override;

	void  SetupCollapseButton();
	void  SetTitle(const QString& title);
	void  SetupMainLayout();

	QCollapsibleFrame*           m_pParentCollapsible;
	QLabel*                      m_pTitleLabel;
	QLabel*                      m_pIconLabel;
	QLabel*                      m_pCollapseIconLabel;
	QPushButton*                 m_pCollapseButton;
	QToolButton*				 m_pCloseButton;
	QSize						 m_iconSize;
	QIcon						 m_collapsedIcon;
	QIcon						 m_expandedIcon;
	bool                         m_bCollapsed;
	static const CryIconColorMap s_colorMap;

protected Q_SLOTS:
	void OnCollapseButtonClick();
};

