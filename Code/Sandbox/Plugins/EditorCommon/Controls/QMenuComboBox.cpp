// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QMenuComboBox.h"

#include <QLineEdit>
#include <QVBoxLayout>
#include <QMenu>
#include <QIcon>
#include <qevent.h> // contains sub events
#include <QFontMetrics>
#include <QSize>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>
#include <QToolTip>
#include <CryIcon.h>

//////////////////////////////////////////////////////////////////////////

struct QMenuComboBox::MenuAction : public QAction
{
	MenuAction(const QString& text, QObject* parent = nullptr) : QAction(text, parent) {}

	QString toolTip;
	int index;
};

class QMenuComboBox::Popup : public QMenu
{
public:
	Popup(QMenuComboBox* parent) : QMenu(parent), m_parent(parent)
	{}

	virtual bool event(QEvent* event) override
	{
		if (event->type() == QEvent::ToolTip)
		{
			auto helpEvent = static_cast<QHelpEvent*>(event);
			// all actions in this menu are internal menu actions.
			// cannot use QAction here because if a QAction tooltip
			// text is empty the toolTip() function of QAction alway
			// returns the text of the QAction which is unwanted here.
			// An empty tooltip should really be empty.
			auto action = static_cast<MenuAction*>(activeAction());
			if (action)
			{
				QToolTip::showText(helpEvent->globalPos(), action->toolTip);
			}
		}
		else if (QToolTip::isVisible())
		{
			QToolTip::hideText();
		}

		if (m_parent->m_multiSelect)
		{
			switch (event->type())
			{
			case QEvent::KeyPress:
				{
					QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
					switch (keyEvent->key())
					{
					case Qt::Key_Space:
					case Qt::Key_Return:
					case Qt::Key_Enter:
						break;
					default:
						return QMenu::event(event);
					}
				}
			//Intentional fall through
			case QEvent::MouseButtonRelease:
				{
					auto action = activeAction();
					if (action)
					{
						action->trigger();
						return true;
					}
				}
			}
		}

		return QMenu::event(event);
	}

	void DoPopup(const QPoint& p, QAction* atAction)
	{
		setMinimumSize(m_parent->size());
		popup(p, atAction);
	}

	QMenuComboBox* m_parent;
};

//////////////////////////////////////////////////////////////////////////

namespace Private_MenuComboBox
{
	static const int sWidthOffsetDefault = 36;
}

QMenuComboBox::QMenuComboBox(QWidget* parent)
	: QWidget(parent)
	, m_lineEdit(new QLineEdit(this))
	, m_popup(new Popup(this))
	, m_lastSelected(-1)
	, m_multiSelect(false)
	, m_emptySelect(false)
	, m_widthOffset(Private_MenuComboBox::sWidthOffsetDefault)
{
	m_lineEdit->installEventFilter(this);
	m_lineEdit->setReadOnly(true);
	m_lineEdit->setFocusProxy(this);
	// QLineEdit has Expand horizontal size policy by default. If size policy for
	// QMenuComboBox widget is set to Preferred horizontally it would still expand
	// because of the line edit
	m_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	QAction* showPopupAction = m_lineEdit->addAction(CryIcon("icons:General/Pointer_Down.ico"), QLineEdit::TrailingPosition);
	connect(showPopupAction, &QAction::triggered, this, &QMenuComboBox::ShowPopup);

	auto layout = new QVBoxLayout();
	layout->setMargin(0);
	layout->addWidget(m_lineEdit);
	setLayout(layout);

	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
	setFocusPolicy(Qt::WheelFocus);
}

QMenuComboBox::~QMenuComboBox()
{
}

void QMenuComboBox::Clear()
{
	m_popup->deleteLater();
	m_popup = new Popup(this);

	m_data.clear();

	m_lastSelected = -1;
	m_lineEdit->setText(QString());
	m_widthOffset = Private_MenuComboBox::sWidthOffsetDefault;
}

void QMenuComboBox::AddItem(const QString& str, const QVariant& userData)
{
	auto action = new MenuAction(str, this);
	action->setCheckable(true);
	action->setChecked(false);
	action->index = m_data.length();
	connect(action, &QAction::triggered, [action, this](bool checked) { OnToggled(action->index, checked); });

	ItemData itemData = { str, userData, action, QString() };
	m_data.push_back(itemData);

	if (action->index == 0)
	{
		if (!m_emptySelect)
		{
			m_popup->addAction(action);
			SetChecked(0);
			return;
		}
		else if (!m_multiSelect)
		{
			//Prepend "empty selection" action, not in the array but special handling
			auto actionEmpty = new MenuAction(QString(), this);
			actionEmpty->setCheckable(true);
			actionEmpty->setChecked(true);
			actionEmpty->index = -1;
			connect(actionEmpty, &QAction::triggered, [actionEmpty, this](bool checked) { OnToggled(actionEmpty->index, checked); });
			m_popup->addAction(actionEmpty);
			m_popup->addAction(action);
			return;
		}
	}

	m_popup->addAction(action);
	UpdateSizeHint(str);
}

void QMenuComboBox::AddItems(const QStringList& items)
{
	for (const QString& str : items)
	{
		AddItem(str);
	}
}

void QMenuComboBox::RemoveItem(int index)
{
	if (index < 0 || index >= m_data.size())
	{
		return;
	}

	bool wasSelected = m_data[index].action->isChecked();
	
	m_popup->removeAction(m_data[index].action);

	m_data.remove(index);

	for (auto& itemData : m_data)
	{
		if (itemData.action->index > index)
			itemData.action->index--;
	}

	if (wasSelected)
	{
		if (!m_multiSelect && !m_emptySelect && ! m_data.isEmpty())
		{
			bool wasLastSelected = m_lastSelected == 0;
			m_lastSelected = -1; // Invalidate last index so we can make sure a new item is selected and text is updated correctly
			SetChecked(0);
			if (wasLastSelected)
				signalCurrentIndexChanged(0);
		}
		else if (m_multiSelect && !m_multiSelect && GetCheckedItems().isEmpty())
		{
			SetChecked(0);
		}
		else if (m_data.empty())
		{
			m_lastSelected = -1;
			UpdateText();
			signalCurrentIndexChanged(-1);
		}
	}
}

QVariant QMenuComboBox::GetData(int index) const
{
	if (index < 0 || index >= m_data.size())
		return QVariant();
	return m_data[index].userData;
}

QString QMenuComboBox::GetItem(int index) const
{
	if (index < 0 || index >= m_data.size())
		return QString();
	return m_data[index].text;
}

QString QMenuComboBox::GetText() const
{
	return m_lineEdit->text();
}

QVariant QMenuComboBox::GetCurrentData() const
{
	const auto checkedItem = GetCheckedItem();
	if (checkedItem >= 0 && checkedItem < m_data.size())
	{
		return m_data[checkedItem].userData;
	}
	return QVariant();
}

int QMenuComboBox::GetWidthOffset() const
{
	return m_widthOffset;
}

void QMenuComboBox::SetText(const QString& text, const char* separator /*= ", "*/)
{
	QStringList list = text.split(separator, QString::SkipEmptyParts);

	if (!m_multiSelect)
	{
		CRY_ASSERT(list.size() <= 1);
		if (list.size() == 1)
		{
			SetChecked(list.first());
		}
	}
	else
	{
		UncheckAllItems();

		for (auto& str : list)
		{
			SetChecked(str, true);
		}
	}

}

void QMenuComboBox::SetChecked(int index, bool checked /*= true*/)
{
	const int itemCount = GetItemCount();
	if ((m_emptySelect && index < -1) || (!m_emptySelect && index < 0) || index >= itemCount)
	{
		return;
	}
	InternalSetChecked(index, checked);
	OnToggled(index, checked);
}

void QMenuComboBox::SetChecked(const QString& str, bool checked /*= true*/)
{
	for (int i = 0; i < m_data.length(); i++)
	{
		if (m_data[i].text == str)
		{
			SetChecked(i, checked);
			return;
		}
	}
}

void QMenuComboBox::SetCheckedByData(const QVariant& userData, bool checked)
{
	for (int i = 0; i < m_data.length(); i++)
	{
		if (m_data[i].userData == userData)
		{
			SetChecked(i, checked);
			return;
		}
	}
}

void QMenuComboBox::SetChecked(const QStringList& items)
{
	//clear everything
	for (int i = 0; i < GetItemCount(); i++)
	{
		InternalSetChecked(i, false);
	}

	//Check in the order of the list
	for (const QString& str : items)
	{
		for (int i = 0; i < m_data.length(); i++)
		{
			if (m_data[i].text == str && !IsChecked(i))
			{
				InternalSetChecked(i, true);
				break;
			}
		}
	}

	//Make sure not to break empty selection
	if (!m_emptySelect && GetCheckedItems().length() == 0)
		InternalSetChecked(m_multiSelect ? 0 : -1, true);

	UpdateText();
}

void QMenuComboBox::SetWidthOffset(int widthOffset)
{
	m_widthOffset = widthOffset;
}

void QMenuComboBox::SetItemToolTip(int index, const QString& toolTip)
{
	if (index < 0 || index >= m_data.size())
	{
		return;
	}
	m_data[index].action->toolTip = toolTip;
}

void QMenuComboBox::SetItemText(int index, const QString& text)
{
	if (index < 0 || index >= m_data.size())
	{
		return;
	}
	m_data[index].text = text;
	if (index == m_lastSelected)
	{
		UpdateText();
	}
}

void QMenuComboBox::InternalSetChecked(int index, bool checked /*= true*/)
{
	if (index < -1 || index >= m_data.size())
		return;

	if (index == -1)
	{
		if (m_emptySelect && !m_multiSelect)
		{
			m_popup->actions()[0]->setChecked(checked);
		}
	}
	else
	{
		m_data[index].action->setChecked(checked);
	}
}

bool QMenuComboBox::IsChecked(int index) const
{
	if (index == -1)
	{
		if (m_emptySelect && !m_multiSelect)
		{
			return m_popup->actions()[0]->isChecked();
		}
		else
		{
			return false;
		}
	}
	else
	{
		return m_data[index].action->isChecked();
	}
}

bool QMenuComboBox::IsChecked(const QString& str)
{
	for (int i = 0; i < m_data.length(); i++)
	{
		if (m_data[i].text == str)
			return IsChecked(i);
	}

	return false;
}

QList<int> QMenuComboBox::GetCheckedIndices() const
{
	QList<int> ret;
	for (int i = 0; i < m_data.length(); i++)
	{
		if (m_data[i].action->isChecked())
			ret.push_back(i);
	}
	return ret;
}

QStringList QMenuComboBox::GetCheckedItems() const
{
	QStringList ret;
	for (int i = 0; i < m_data.length(); i++)
	{
		if (m_data[i].action->isChecked())
		{
			ret.push_back(m_data[i].text);
		}
	}
	return ret;
}

int QMenuComboBox::GetCheckedItem() const
{
	return m_multiSelect ? -1 : m_lastSelected;
}

void QMenuComboBox::UncheckAllItems()
{
	auto checked = GetCheckedIndices();
	for (auto& i : checked)
	{
		SetChecked(i, false);
	}
}

QSize QMenuComboBox::sizeHint() const
{
	return m_sizeHint;
}

QSize QMenuComboBox::minimumSizeHint() const
{
	return m_sizeHint;
}

void QMenuComboBox::paintEvent(QPaintEvent*)
{
	QStyleOption styleOption;
	styleOption.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);
}

void QMenuComboBox::changeEvent(QEvent* pEvent)
{
	const auto type = pEvent->type();
	if (type == QEvent::StyleChange || type == QEvent::FontChange)
	{
		RecalculateSizeHint();
	}
	QWidget::changeEvent(pEvent);
}

void QMenuComboBox::focusOutEvent(QFocusEvent* event)
{
	m_lineEdit->event(event);
}

void QMenuComboBox::focusInEvent(QFocusEvent* event)
{
	m_lineEdit->event(event);
}

bool QMenuComboBox::event(QEvent* event)
{
	switch (event->type())
	{
	case QEvent::MouseButtonPress:
	{
		QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
		if (mouseEvent->button() == Qt::LeftButton)
		{
			ShowPopup();
			return true;
		}
		break;
	}
	case QEvent::Wheel:
	{
		QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
		ApplyDelta(-(wheelEvent->angleDelta().y() / 120));
		return true;
	}
	case QEvent::KeyPress:
	{
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		switch (keyEvent->key())
		{
		case Qt::Key_Up:
			ApplyDelta(-1);
			return true;
		case Qt::Key_Down:
			ApplyDelta(1);
			return true;
		case Qt::Key_Space:
			ShowPopup();
			return true;
		default:
			break;
		}
	}
	default:
		break;
	}

	return QWidget::event(event);
}

bool QMenuComboBox::eventFilter(QObject* object, QEvent* event)
{
	if (!isEnabled())
	{
		return false;
	}

	if (object == m_lineEdit)
	{
		switch (event->type())
		{
		case QEvent::MouseButtonPress:
		case QEvent::Wheel:
		case QEvent::KeyPress:
			return QMenuComboBox::event(event);
		}
	}

	return false;
}

void QMenuComboBox::ShowPopup()
{
	QPoint pt = mapToGlobal(QPoint(0, 0));
	QAction* focus = m_lastSelected != -1 ? m_data[m_lastSelected].action : nullptr;
	m_popup->DoPopup(pt, focus);
}

void QMenuComboBox::HidePopup()
{
	m_popup->hide();
}

void QMenuComboBox::UpdateText()
{
	if (m_multiSelect)
	{
		QStringList list = GetCheckedItems();
		m_lineEdit->setText(list.join(", "));
	}
	else if (m_lastSelected != -1)
	{
		m_lineEdit->setText(m_data[m_lastSelected].text);
	}
	else
	{
		m_lineEdit->setText(QString());
	}
}

void QMenuComboBox::ApplyDelta(int delta)
{
	if (m_multiSelect)
	{
		ShowPopup();
	}
	else
	{
		auto checkedIndex = std::min(std::max(m_lastSelected + delta, m_emptySelect ? -1 : 0), m_data.length() - 1);
		SetChecked(checkedIndex, true);
	}
}

void QMenuComboBox::OnToggled(int index, bool checked)
{
	if (m_multiSelect)
	{
		m_lastSelected = index;

		if (!m_emptySelect && GetCheckedIndices().length() == 0)
		{
			InternalSetChecked(index);
		}
		else
		{
			UpdateText();
			signalItemChecked(m_lastSelected, checked);
		}
	}
	else
	{
		if (index != m_lastSelected)
		{
			InternalSetChecked(m_lastSelected, false);
			m_lastSelected = index;
			InternalSetChecked(index, true);
			UpdateText();

			signalCurrentIndexChanged(m_lastSelected);
		}
		else if (!checked)
		{
			// single select and the last selected item was
			// selected again which leads to unchecking of
			// the item for comboboxes where m_emptySelect
			// is false
			if (!m_emptySelect)
			{
				m_data[index].action->setChecked(true);
			}
			// single select and last selected item was
			// selected again which leads to unchecking
			// of the item should select the empty action
			// at index 0
			else
			{
				OnToggled(-1, checked);
			}
		}
	}
}

void QMenuComboBox::UpdateSizeHint(const QString& entry)
{
	const auto fontMet = fontMetrics();
	m_sizeHint.setWidth(std::max(m_sizeHint.width(), fontMet.boundingRect(entry).width() + m_widthOffset));
	m_sizeHint.setHeight(m_lineEdit->sizeHint().height());
	updateGeometry();
}

void QMenuComboBox::RecalculateSizeHint()
{
	const auto fontMet = fontMetrics();
	m_sizeHint = QSize();
	for (int i = 0; i < m_data.size(); ++i)
	{
		m_sizeHint.setWidth(std::max(m_sizeHint.width(), fontMet.boundingRect(GetItem(i)).width() + m_widthOffset));
	}
	m_sizeHint.setHeight(m_lineEdit->sizeHint().height());
	updateGeometry();
}

