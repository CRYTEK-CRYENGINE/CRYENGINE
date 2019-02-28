// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include <QWidget>

class QToolButton;

//! BreadcrumbsBar is typically used to display a path/address and enable the user to click on any part of the breadcrumb to go to that part of the path
//! A typical use case is windows explorer's  address bar (win 7/10)
class EDITOR_COMMON_API CBreadcrumbsBar : public QWidget
{
	struct Breadcrumb
	{
		QString text;
		QVariant data;
		QWidget* widget;
	};

	Q_OBJECT

public:
	CBreadcrumbsBar();
	virtual ~CBreadcrumbsBar();

	void AddBreadcrumb(const QString& text, const QVariant& data = QVariant());
	
	void Clear();

	QSize minimumSizeHint() const override;
	void mousePressEvent(QMouseEvent* pEvent) override;
	void keyPressEvent(QKeyEvent* pEvent) override;
	void mouseMoveEvent(QMouseEvent* pEvent) override;
	void leaveEvent(QEvent* epEent) override;

	void SetValidator(std::function<bool(const QString&)> function);

	CCrySignal<void(const QString&, const QVariant&)> signalBreadcrumbClicked;
	CCrySignal<void(const QString&)> signalTextChanged;

private:
	void showEvent(QShowEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent*) override;

	void AddBreadcrumb(const Breadcrumb& b);
	void UpdateWidgets();
	void UpdateVisibility();
	void ToggleBreadcrumbsVisibility(bool bEnable);

	void OnBreadcrumbClicked(int index);
	void OnDropDownClicked();
	void OnEnterPressed();

	QVector<Breadcrumb>                 m_breadcrumbs;
	QHBoxLayout*		                m_breadCrumbsLayout;
	QToolButton*		                m_dropDownButton;
	QLineEdit*                          m_textEdit;
	std::function<bool(const QString&)> m_validator;
	QWidget*							m_hoverWidget;
};
