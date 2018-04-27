// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "BreadcrumbsBar.h"

#include <QBoxLayout>
#include <QToolButton>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>

CBreadcrumbsBar::CBreadcrumbsBar()
{
	m_dropDownButton = new QToolButton();
	m_dropDownButton->setIcon(CryIcon("icons:General/Pointer_Down.ico"));
	m_dropDownButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_dropDownButton->setVisible(false);
	connect(m_dropDownButton, &QToolButton::clicked, this, &CBreadcrumbsBar::OnDropDownClicked);

	m_breadCrumbsLayout = new QHBoxLayout();
	m_breadCrumbsLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	m_breadCrumbsLayout->setSpacing(0);
	m_breadCrumbsLayout->setMargin(0);

	m_textEdit = new QLineEdit();
	m_textEdit->setVisible(false);
	m_textEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	connect(m_textEdit, &QLineEdit::returnPressed, this, &CBreadcrumbsBar::OnEnterPressed);
	connect(m_textEdit, &QLineEdit::editingFinished, [=] {
		ToggleBreadcrumbsVisibility(true);
	});

	m_hoverWidget = new QWidget(this);
	m_hoverWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_hoverWidget->setObjectName("HoverWidget");

	auto layout = new QHBoxLayout();
	layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	layout->addWidget(m_dropDownButton);
	layout->addLayout(m_breadCrumbsLayout);
	layout->addWidget(m_hoverWidget);
	layout->addWidget(m_textEdit);
	layout->setSpacing(0);
	layout->setMargin(0);

	setFocusPolicy(Qt::ClickFocus);
	setMouseTracking(true);
	m_hoverWidget->setMouseTracking(true); // Forwards the mouseMoveEvent to the parent
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	setLayout(layout);
}

CBreadcrumbsBar::~CBreadcrumbsBar()
{

}

void CBreadcrumbsBar::showEvent(QShowEvent *event)
{
	UpdateVisibility();
}

void CBreadcrumbsBar::resizeEvent(QResizeEvent *event)
{
	UpdateVisibility();
}

void CBreadcrumbsBar::paintEvent(QPaintEvent*)
{
	QStyleOption styleOption;
	styleOption.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);
}

void CBreadcrumbsBar::AddBreadcrumb(const QString& text, const QVariant& data /*= QVariant()*/)
{
	if(!text.isEmpty())
	{
		Breadcrumb b = { text, data };
		AddBreadcrumb(b);
	}
}

void CBreadcrumbsBar::AddBreadcrumb(const Breadcrumb& b)
{
	m_breadcrumbs.push_back(b);
	UpdateWidgets();
}

void CBreadcrumbsBar::Clear()
{
	m_breadcrumbs.clear();
	UpdateWidgets();
}

void CBreadcrumbsBar::UpdateWidgets()
{
	const int widgetsCount = m_breadCrumbsLayout->count();
	for (int i = 0; i < m_breadcrumbs.count(); ++i)
	{
		Breadcrumb& b = m_breadcrumbs[i];
		if (i < widgetsCount)
		{
			QLayoutItem* const pItem = m_breadCrumbsLayout->itemAt(i);
			QToolButton* const pButton = static_cast<QToolButton*>(pItem->widget());
			b.widget = pButton;
			if (pButton->text() != b.text)
			{
				pButton->setText(b.text);
			}
			pButton->setVisible(true);
		}
		else
		{
			QToolButton* const pButton = new QToolButton();
			pButton->setText(b.text);
			pButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
			pButton->setMouseTracking(true); // Forwards the mouseMoveEvent to the parent
			connect(pButton, &QToolButton::clicked, [this, i]() { OnBreadcrumbClicked(i); });
			b.widget = pButton;
			m_breadCrumbsLayout->insertWidget(i, pButton);
		}
	}

	//hide remaining items in the layout
	for (int i = m_breadcrumbs.count(); i < m_breadCrumbsLayout->count(); ++i) 
	{
		QLayoutItem* const pItem = m_breadCrumbsLayout->itemAt(i);
		pItem->widget()->setVisible(false);
	}
}

QSize CBreadcrumbsBar::minimumSizeHint() const
{
	return m_dropDownButton->minimumSizeHint();
}

void CBreadcrumbsBar::UpdateVisibility()
{
	if (m_breadcrumbs.count() == 0)
		return;

	int remainingSize = CBreadcrumbsBar::size().width();
	const int spacing = m_breadCrumbsLayout->spacing();

	const int buttonHeight = m_breadcrumbs[0].widget->size().height();
	m_dropDownButton->setMaximumSize(QSize(buttonHeight, buttonHeight));

	bool showDropDown = false;

	const int count = m_breadcrumbs.count();
	for (int i = count - 1; i >= 0; i--)
	{
		auto& bc = m_breadcrumbs[i];
		CRY_ASSERT(bc.widget);

		remainingSize -= bc.widget->minimumSizeHint().width();
		if (remainingSize < i == 0 ? 0 : buttonHeight)
		{
			bc.widget->setVisible(false);
			showDropDown = true;
		}
		else
		{
			bc.widget->setVisible(true);
		}

		remainingSize -= spacing;
	}

	m_dropDownButton->setVisible(showDropDown);
}

void CBreadcrumbsBar::ToggleBreadcrumbsVisibility(bool bEnable)
{
	for each (Breadcrumb breadcrumb in m_breadcrumbs)
	{
		breadcrumb.widget->setVisible(bEnable);
	}
	m_hoverWidget->setVisible(bEnable);
	m_textEdit->setVisible(!bEnable);
}

void CBreadcrumbsBar::OnBreadcrumbClicked(int index)
{
	signalBreadcrumbClicked(m_breadcrumbs[index].text, m_breadcrumbs[index].data);
}

void CBreadcrumbsBar::OnDropDownClicked()
{
	//populate menu with the hidden breadcrumbs
	auto menu = new QMenu();

	const int count = m_breadcrumbs.count();
	for (int i = count - 1; i >= 0; i--)
	{
		auto& bc = m_breadcrumbs[i];
		if(!bc.widget->isVisible())
		{
			auto action = menu->addAction(bc.text);
			connect(action, &QAction::triggered, [this, i]() { OnBreadcrumbClicked(i); });
		}
	}

	menu->popup(m_dropDownButton->mapToGlobal(m_dropDownButton->rect().bottomLeft()));
}

void CBreadcrumbsBar::mousePressEvent(QMouseEvent* pEvent)
{
	if (pEvent->button() == Qt::LeftButton && m_hoverWidget->geometry().contains(pEvent->pos()) && !m_textEdit->isVisible())
	{
		ToggleBreadcrumbsVisibility(false);

		m_textEdit->setFocus();
		m_textEdit->setProperty("error", false);
		m_textEdit->style()->unpolish(m_textEdit);
		m_textEdit->style()->polish(m_textEdit);
		m_textEdit->setText(m_breadcrumbs.last().data.toString());
		m_textEdit->selectAll();
		return;
	}
	pEvent->ignore();
	QWidget::mousePressEvent(pEvent);
}

void CBreadcrumbsBar::keyPressEvent(QKeyEvent * pEvent)
{
	int keyCode = pEvent->key();
	if (keyCode == Qt::Key_Return || keyCode == Qt::Key_Enter && m_textEdit->isVisible())
	{
		OnEnterPressed();
		return;
	}
	if (keyCode == Qt::Key_Escape && m_textEdit->isVisible())
	{
		m_textEdit->editingFinished();
		return;
	}

	QWidget::keyPressEvent(pEvent);
}

void CBreadcrumbsBar::mouseMoveEvent(QMouseEvent* pEvent)
{
	if (m_hoverWidget->geometry().contains(pEvent->pos()))
	{
		// Only set the cursor if it's not already set because the setOverrideCursor method stacks the override cursors
		if((QApplication::overrideCursor() && QApplication::overrideCursor()->shape() != Qt::IBeamCursor) || !QApplication::overrideCursor())
			QApplication::setOverrideCursor(Qt::IBeamCursor);
	}
	else
	{
		QApplication::restoreOverrideCursor();
	}
	QWidget::mouseMoveEvent(pEvent);
}

void CBreadcrumbsBar::leaveEvent(QEvent* epEent)
{
	QApplication::restoreOverrideCursor(); //In case the cursors leaves the widget too fast for mouseMoveEvent to catch
}

void CBreadcrumbsBar::OnEnterPressed()
{
	QString enteredPath = m_textEdit->text();
	if (m_validator)
	{
		if (m_validator(enteredPath))
		{
			signalTextChanged(enteredPath);
		}
		else
		{
			bool result = m_textEdit->setProperty("error", true);
			m_textEdit->style()->unpolish(m_textEdit);
			m_textEdit->style()->polish(m_textEdit);
			ToggleBreadcrumbsVisibility(false);
			m_textEdit->setFocus();
		}
	}
	else
	{
		signalTextChanged(enteredPath);
	}
}

void CBreadcrumbsBar::SetValidator(std::function<bool(const QString&)> function)
{
	m_validator = function;
}
