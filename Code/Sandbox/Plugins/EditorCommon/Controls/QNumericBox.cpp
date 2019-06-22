// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Controls/QNumericBox.h"
#include "BoostPythonMacros.h"

#include <CryIcon.h>
#include <IEditor.h>

#include <QApplication>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleOption>
#include <QVBoxLayout>

class QNumericEditProxyStyle : public QProxyStyle
{
public:
	QNumericEditProxyStyle() : m_cursorWidth(0) {}
	virtual ~QNumericEditProxyStyle() override {}

	void        setCursorWidth(int width) { m_cursorWidth = width; }

	virtual int pixelMetric(PixelMetric metric, const QStyleOption* option = 0, const QWidget* widget = 0) const
	{
		if (metric == QStyle::PM_TextCursorWidth)
		{
			return m_cursorWidth;
		}

		return QProxyStyle::pixelMetric(metric, option, widget);
	}

private:
	int m_cursorWidth;
};

class QNumericEdit : public QLineEdit
{
public:
	//The type of editing operation the user is doing
	enum class EditState
	{
		//Typing a text in the line edit
		Text,
		//Using the slider to change the values
		Slider,
		//Line edit Is not being modified
		None
	};

	//This stores edit data and the previously set value so that we can undo when needed
	struct EditData
	{
		EditState editState;
		QString   previousValue;
	};

	QNumericEdit(QWidget* parent)
		: QLineEdit(parent)
		, m_lastCursor(-1)
		, m_editMode(false)
		, m_editStyle(new QNumericEditProxyStyle())
	{
		setStyleSheet("background-color: transparent");
		setTextEditMode(false);
		setStyle(m_editStyle);
		setText("0");
	}

	inline QString text() { return QLineEdit::text().replace(',', '.'); }

	virtual ~QNumericEdit() override {}

	virtual void focusInEvent(QFocusEvent* pEvent) override
	{
		if (pEvent->reason() == Qt::TabFocusReason ||
		    pEvent->reason() == Qt::BacktabFocusReason)
		{
			setTextEditMode(true);
		}

		QLineEdit::focusInEvent(pEvent);
	}

	virtual void focusOutEvent(QFocusEvent* pEvent) override
	{
		if (pEvent->reason() != Qt::PopupFocusReason)
		{
			setTextEditMode(false);
		}

		QLineEdit::focusOutEvent(pEvent);
	}

	virtual void mousePressEvent(QMouseEvent* pEvent) override
	{
		//if we are not in text edit mode setup for a slide edit operation
		if (!getTextEditMode() && pEvent->button() == Qt::LeftButton)
		{
			m_mouseDownLocation = QCursor::pos();
			setCursor(Qt::BlankCursor);
		}
		else
		{
			QLineEdit::mousePressEvent(pEvent);
		}
	}

	virtual void mouseMoveEvent(QMouseEvent* pEvent) override
	{
		//if we are not in text edit mode start a slide operation and save edit value
		if (!getTextEditMode() && QApplication::mouseButtons() == Qt::LeftButton)
		{

			//Apply sliding based on mouse movement
			QNumericBox* pNumbox = qobject_cast<QNumericBox*>(parentWidget());
			QPoint mouse = QCursor::pos();
			if (pNumbox && mouse != m_mouseDownLocation)
			{
				//Record text the first time the mouse moves,
				//This starts here instead of mouse press because we need to handle drag restart after escaping a change without releasing left mouse
				if (m_editState.editState == EditState::None)
				{
					m_editState.editState = EditState::Slider;
					m_editState.previousValue = text();
				}

				double add = 0.0;
				if (pNumbox->hasMinMax())
				{
					add = (pNumbox->maximum() - pNumbox->minimum()) * double((mouse.x() - m_mouseDownLocation.x())) / double(pNumbox->width());
				}
				else
				{
					add = pNumbox->singleStep() * double((mouse.x() - m_mouseDownLocation.x()));
				}

				if (Qt::ControlModifier == QApplication::keyboardModifiers())
				{
					add *= 0.01;
				}
				else if (Qt::ShiftModifier == QApplication::keyboardModifiers())
				{
					add *= 100.0;
				}

				pNumbox->setValue(pNumbox->value() + add);
				QCursor::setPos(m_mouseDownLocation);
			}
		}
		else
		{
			QLineEdit::mouseMoveEvent(pEvent);
		}
	}

	virtual void mouseReleaseEvent(QMouseEvent* pEvent) override
	{
		releaseMouse();
		//If we are releasing left mouse it might be the end of a sliding op or a selection on the line edit
		if (pEvent->button() == Qt::LeftButton)
		{
			setCursor(Qt::ArrowCursor);

			//slide op is finished, edit state can become none
			if (m_editState.editState == EditState::Slider)
			{
				if (m_editState.previousValue != text())
				{
					QLineEdit::editingFinished();
				}

				m_editState.editState = EditState::None;
			}
			else
			{
				//just a line edit selection
				selectAll();
			}
		}
		else
		{
			QLineEdit::mouseReleaseEvent(pEvent);
		}

	}

	virtual bool eventFilter(QObject* pObject, QEvent* pEvent) override
	{
		if (pObject != this && pEvent->type() == QEvent::MouseButtonPress)
		{
			QWidget* pWidget = qobject_cast<QWidget*>(pObject);
			if (pWidget && pWidget->parentWidget() != this)
			{
				QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);
				QNumericBox* pNumbox = qobject_cast<QNumericBox*>(parentWidget());
				if (pNumbox && pMouseEvent->buttons() == Qt::LeftButton)
				{
					QPoint numboxPoint = pNumbox->mapFromGlobal(pWidget->mapToGlobal(pMouseEvent->pos()));
					QRect numboxRect = pNumbox->rect();
					if (!numboxRect.contains(numboxPoint))
					{
						setTextEditMode(false);
					}
				}
			}
		}

		return false;
	}

	//If we start/end a text edit op
	void setTextEditMode(bool edit)
	{
		m_editMode = edit;
		m_editStyle->setCursorWidth(edit ? 1 : 0);
		setCursor(edit ? Qt::IBeamCursor : Qt::ArrowCursor);

		QApplication::instance()->removeEventFilter(this);
		if (edit)
		{
			//Starting a text edit, set the edit state to text
			m_editState.editState = EditState::Text;
			m_editState.previousValue = text();

			QApplication::instance()->installEventFilter(this);
		}
		else
		{
			//Text edit is ending, edit state can be nulled
			m_editState.editState = EditState::None;
			deselect();
		}

		update();
	}

	bool getTextEditMode() { return m_editMode; }

	void step(double multiplier)
	{
		QByteArray expression = text().toUtf8();
		int revCursor = text().length() - cursorPosition();
		int cursor = stepCursor();

		if (-1 != cursor)
		{
			int digitStart = cursor;
			int digitLen = 1;
			bool valid = true;

			for (int i = 0; i < digitStart; ++i)
			{
				if (' ' == text().at(i))
				{
					continue;
				}

				QString strValue = text().mid(i, 1 + digitStart - i);
				double value = strValue.toDouble(&valid);
				double avalue = atof(strValue.toUtf8().constData());

				if (valid && avalue == value)
				{
					digitLen = 1 + digitStart - i;
					digitStart = i;
					break;
				}
			}

			QString numString = text().mid(digitStart, digitLen);
			QString addString;
			int digitEnd = digitStart + digitLen;

			for (QChar c : numString)
			{
				addString.append(isxdigit(c.toLatin1()) ? '0' : c);
			}

			QByteArray tmpAddStr = addString.toUtf8();
			tmpAddStr[tmpAddStr.length() - 1] = '1';
			addString = QString(tmpAddStr);

			double result = numString.toDouble();
			double add = addString.toDouble();

			if (add < 0.0f)
			{
				add = -add;
			}

			result += add * multiplier;

			int decPoint = numString.indexOf('.');
			int precision = numString.length() - (decPoint + 1);
			QString outNumber = -1 != decPoint ? QString::number(result, 'f', precision) : QString::number(result, 'f', 0);
			QString outText = text().mid(0, digitStart) + outNumber + text().mid(digitEnd);
			setText(outText);
			setCursorPosition(text().length() - revCursor);
		}
	}

	virtual void keyPressEvent(QKeyEvent* pEvent) override
	{
		//We want to cancel a current edit operation
		if (pEvent->key() == Qt::Key_Escape)
		{
			//If state is none we have no op in progress, this means that we just need a deselect of current line edit
			if (m_editState.editState != EditState::None)
			{
				//We are reverting, set text to previous value
				setText(m_editState.previousValue);
				EditState prevType = m_editState.editState;

				//We can stop editing
				m_editState.editState = EditState::None;

				//Select all items in the line edit (ex: after end of slide or text edit)
				if (hasFocus())
				{
					selectAll();
				}

				//Exiting slide edit, select text and start editing that
				if (prevType == EditState::Slider)
				{
					setTextEditMode(true);
				}
				else
				{
					deselect();
				}

			}
			else
			{
				deselect();
			}

			//Lets the keypress go to parents, i.e. the parent dialog may close on Enter/ESC
			//Note that this behavior can be observed on QLineEdit
			pEvent->ignore();

			return;
		}

		//We are confirming a slide/text op or just selecting
		if (pEvent->key() == Qt::Key_Enter || pEvent->key() == Qt::Key_Return)
		{
			//if state is none this enter is just to select the field and start text edit
			if (m_editState.editState == EditState::None)
			{
				selectAll();
				setTextEditMode(true);
			}
			else
			{
				//Commit edited value
				if (m_editState.previousValue != text())
				{
					QLineEdit::editingFinished();
				}

				if (hasFocus())
				{
					selectAll();
				}

				//Enter confirms and clears edit state, all changes have been applied
				m_editState.editState = EditState::None;
			}

			//Lets the keypress go to parents, i.e. the parent dialog may close on Enter/ESC
			//Note that this behavior can be observed on QLineEdit
			pEvent->ignore();
			return;
		}

		if (pEvent->key() == Qt::Key_Up || pEvent->key() == Qt::Key_Down)
		{
			step(pEvent->key() == Qt::Key_Up ? 1.0f : -1.0f);
			QNumericBox* numbox = qobject_cast<QNumericBox*>(parentWidget());
			if (numbox)
			{
				numbox->valueChanged(numbox->value());
			}
		}

		//We are pressing a number on the keyboard, this needs to enter into edit mode immediately
		if (m_editState.editState == EditState::None)
		{
			selectAll();
			setTextEditMode(true);
		}
		else
		{
			//If we are sliding move to text edit
			if (m_editState.editState == EditState::Slider)
			{
				m_editState.editState = EditState::None;
				if (hasFocus())
				{
					selectAll();
				}
				setTextEditMode(true);
			}
		}

		//forward event to the line edit
		QLineEdit::keyPressEvent(pEvent);
	}

	int stepCursor()
	{
		if (hasSelectedText() || !hasFocus() || !getTextEditMode())
		{
			return -1;
		}

		int cursor = cursorPosition() - 1;
		if (cursor >= 0 && cursor < text().length())
		{
			char currentChar = text().at(cursor).toLatin1();
			if (isdigit(currentChar))
			{
				return cursor;
			}
		}
		return -1;
	}

	QRect stepCursorRect(int cursor)
	{
		int currentCursor = cursorPosition();
		if (m_previousStepText != text() || m_lastCursor != currentCursor)
		{
			int start = selectionStart();
			int length = selectedText().length();
			setCursorPosition(cursor);
			m_stepRect = cursorRect();
			setCursorPosition(currentCursor);
			setSelection(start, length);
			m_previousStepText = text();
			m_lastCursor = currentCursor;
		}
		return m_stepRect;
	}

private:

	inline bool isValidDigit(char c) { return isdigit(c) || '.' == c; }

	QString                 m_previousStepText;
	QRect                   m_stepRect;
	int                     m_lastCursor;
	bool                    m_editMode;
	QNumericEditProxyStyle* m_editStyle;
	QString                 m_previousText;
	QPoint                  m_mouseDownLocation;

	// Edit operation currently applied to this widget
	EditData m_editState;
};

QNumericButton::QNumericButton(QWidget* pParent)
	: QWidget(pParent)
	, m_spinClickTimerId(-1)
	, m_spinClickTimerInterval(100)
	, m_spinClickThresholdTimerId(-1)
	, m_spinClickThresholdTimerInterval(-1)
	, m_effectiveSpinRepeatRate(1)
	, m_acceleration(0)
	, m_accelerated(false)
{
	setStyleSheet("background-color: transparent");
	setAutoFillBackground(false);
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_NoSystemBackground);
}

void QNumericButton::paintEvent(QPaintEvent* pEvent)
{
	QPainter painter(this);
	m_icon.paint(&painter, rect());
}

void QNumericButton::mousePressEvent(QMouseEvent* pEvent)
{
	reset();
	if (Qt::LeftButton == pEvent->button())
	{
		singleStep();
		clicked();
		m_spinClickThresholdTimerId = startTimer(m_spinClickThresholdTimerInterval);
	}
	pEvent->accept();
}

void QNumericButton::changeEvent(QEvent* pEvent)
{
	switch (pEvent->type())
	{
	case QEvent::StyleChange:
		m_spinClickTimerInterval = style()->styleHint(QStyle::SH_SpinBox_ClickAutoRepeatRate, 0, this);
		m_spinClickThresholdTimerInterval = style()->styleHint(QStyle::SH_SpinBox_ClickAutoRepeatThreshold, 0, this);
		reset();
		break;

	case QEvent::EnabledChange:
		if (!isEnabled())
		{
			reset();
		}
		break;

	case QEvent::ActivationChange:
		if (!isActiveWindow())
		{
			reset();
		}
		break;

	default:
		break;
	}

	QWidget::changeEvent(pEvent);
}

void QNumericButton::mouseReleaseEvent(QMouseEvent* pEvent)
{
	reset();
	pEvent->accept();

	if (Qt::LeftButton == pEvent->button())
	{
		clicked();
	}
}

void QNumericButton::focusOutEvent(QFocusEvent* pEvent)
{
	reset();
	QWidget::focusOutEvent(pEvent);
}

void QNumericButton::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() == m_spinClickThresholdTimerId)
	{
		killTimer(m_spinClickThresholdTimerId);
		m_spinClickThresholdTimerId = -1;
		m_effectiveSpinRepeatRate = m_spinClickTimerInterval;
		m_spinClickTimerId = startTimer(m_effectiveSpinRepeatRate);
		singleStep();
	}
	else if (pEvent->timerId() == m_spinClickTimerId)
	{
		if (m_accelerated)
		{
			m_acceleration = m_acceleration + (int)(m_effectiveSpinRepeatRate * 0.05);
			if (m_effectiveSpinRepeatRate - m_acceleration >= 10)
			{
				killTimer(m_spinClickTimerId);
				m_spinClickTimerId = startTimer(m_effectiveSpinRepeatRate - m_acceleration);
			}
		}
		singleStep();
	}

	QWidget::timerEvent(pEvent);
}

void QNumericButton::reset()
{
	if (m_spinClickTimerId != -1)
	{
		killTimer(m_spinClickTimerId);
	}
	if (m_spinClickThresholdTimerId != -1)
	{
		killTimer(m_spinClickThresholdTimerId);
	}
	m_spinClickTimerId = m_spinClickThresholdTimerId = -1;
	m_acceleration = 0;
}

QNumericBox::QNumericBox(QWidget* pParent)
	: QWidget(pParent)
	, m_min(-DBL_MAX)
	, m_max(DBL_MAX)
	, m_step(0.1)
	, m_value(0)
	, m_precision(6)
{
	m_pEdit = new QNumericEdit(this);

	connect(m_pEdit, &QLineEdit::editingFinished, this, &QNumericBox::textEdited);

	QHBoxLayout* pHLayout = new QHBoxLayout(this);
	pHLayout->addWidget(m_pEdit);
	pHLayout->setContentsMargins(0, 0, 0, 0);
	pHLayout->setSpacing(0);

	QVBoxLayout* pVLayout = new QVBoxLayout();
	pHLayout->addLayout(pVLayout);

	m_pUpButton = new QNumericButton();
	m_pUpButton->setMaximumSize(QSize(20, 10));
	m_pUpButton->setMinimumSize(QSize(20, 10));
	m_pUpButton->setIcon(CryIcon("icons:General/Pointer_Up_Numeric.ico"));
	connect(m_pUpButton, &QNumericButton::singleStep, [=]() { step(1.0); });
	connect(m_pUpButton, &QNumericButton::clicked, [=]() { valueSubmitted(value()); });

	m_pDownButton = new QNumericButton();
	m_pDownButton->setMaximumSize(QSize(20, 10));
	m_pDownButton->setMinimumSize(QSize(20, 10));
	m_pDownButton->setIcon(CryIcon("icons:General/Pointer_Down_Numeric.ico"));
	connect(m_pDownButton, &QNumericButton::singleStep, [=]() { step(-1.0); });
	connect(m_pDownButton, &QNumericButton::clicked, [=]() { valueSubmitted(value()); });

	pVLayout->addWidget(m_pUpButton);
	pVLayout->addWidget(m_pDownButton);
	pVLayout->setContentsMargins(0, 0, 0, 0);
	pVLayout->setSpacing(0);

	setLayout(pHLayout);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QNumericBox::~QNumericBox()
{
	m_pEdit->removeEventFilter(this);
}

void QNumericBox::setRestrictToInt()
{
	setPrecision(0);
	setSingleStep(1);
}

void QNumericBox::setAlignment(Qt::Alignment alignment)
{
	m_pEdit->setAlignment(alignment);
}

void QNumericBox::setAccelerated(bool accelerated)
{
	m_pUpButton->setAccelerated(accelerated);
	m_pDownButton->setAccelerated(accelerated);
}

double QNumericBox::value() const
{
	// PyThreadState_GET() can return nullptr for some internal reasons (initialization, long operations, ...)
	if (!GetIEditor() || !GetIEditor()->GetIPythonManager() || !PyThreadState_GET())
	{
		return 0.0;
	}

	if (m_pEdit->text() == "")
	{
		return m_value;
	}
	GetIEditor()->GetIPythonManager()->Execute((QString("numeric_expression_result = ") + m_pEdit->text()).toUtf8().constData());
	double result = (double)GetIEditor()->GetIPythonManager()->GetAsFloat("numeric_expression_result");
	return result;
}

double QNumericBox::getSliderPosition() const
{
	if (!hasMinMax())
	{
		return 0.0;
	}

	if (m_pEdit->getTextEditMode())
	{
		return 0.0;
	}

	return clamp_tpl(double(value() - m_min) / (m_max - m_min), 0.0, 1.0);
}

void QNumericBox::setValue(double value)
{
	value = clamp_tpl(value, m_min, m_max);

	QString outText = QString::number(value, 'f', m_precision);
	int revCursor = m_pEdit->text().length() - m_pEdit->cursorPosition();
	int decPoint = outText.lastIndexOf('.');

	if (decPoint != -1)
	{
		for (int i = outText.length() - 1; i > decPoint; --i)
		{
			if ('0' == outText.at(i))
			{
				outText.remove(i, 1);
			}
			else
			{
				break;
			}
		}
	}

	if ('.' == outText.at(outText.length() - 1))
	{
		outText.remove(outText.length() - 1, 1);
	}

	m_value = value;
	m_pEdit->setText(outText);
	m_pEdit->setCursorPosition(m_pEdit->text().length() - revCursor);

	valueChanged(value);
}

void QNumericBox::setPrecision(int precision)
{
	if (m_precision != precision)
	{
		m_precision = precision;
		setValue(value());
	}
}

void QNumericBox::step(double multiplier)
{
	if (Qt::ControlModifier == QApplication::keyboardModifiers())
	{
		multiplier *= 0.01;
	}
	else if (Qt::ShiftModifier == QApplication::keyboardModifiers())
	{
		multiplier *= 100.0;
	}

	setValue(value() + m_step * multiplier);
}

void QNumericBox::textEdited()
{
	double val = value();
	setValue(val);
	valueSubmitted(val);
}

bool QNumericBox::hasMinMax() const
{
	return -DBL_MAX != m_min && DBL_MAX != m_max;
}

// QStyleOptionFrameV2 is deprecated, but if replace it with QStyleOption, widget's background will not be dark or sliderOverlayRect will be hidden
#pragma warning(disable : 4996)

void QNumericBox::paintEvent(QPaintEvent* pEvent)
{
	QPainter painter(this);
	QStyleOptionFrameV2 option;
	option.state = QStyle::State_Sunken;
	option.lineWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, 0);
	option.midLineWidth = 0;
	option.features = QStyleOptionFrameV2::None;
	if (m_pEdit->isEnabled())
	{
		option.state |= QStyle::State_Enabled;
	}
	option.rect = rect();
	option.palette = m_pEdit->palette();
	option.fontMetrics = m_pEdit->fontMetrics();

	painter.setPen(QPen(m_pEdit->palette().color(QPalette::WindowText)));
	painter.setBrush(QBrush(m_pEdit->palette().color(QPalette::Base)));

	static QLineEdit s_lineEdit;
	s_lineEdit.style()->drawPrimitive(QStyle::PE_PanelLineEdit, &option, &painter, &s_lineEdit);

	double sliderPos = getSliderPosition();
	if (sliderPos != 0.0)
	{
		QRect r = rect();
		QRect sliderOverlayRect(r.left(), r.top(), int(r.width() * sliderPos), r.height());
		QColor sliderOverlayColor = m_sliderColor;

		painter.setBrush(QBrush(sliderOverlayColor));
		painter.setPen(Qt::NoPen);
		painter.drawRoundedRect(sliderOverlayRect, 2, 2);
	}

	m_pDownButton->update();
	m_pUpButton->update();
}

#pragma warning(default : 4996)

void QNumericBox::setEditMode(bool edit)
{
	m_pEdit->setFocus(Qt::TabFocusReason);
}

void QNumericBox::alignText()
{
	m_pEdit->home(false);
}

void QNumericBox::setText(const QString& text)
{
	m_pEdit->setText(text);
}

QString QNumericBox::text() const
{
	return m_pEdit->text();
}

void QNumericBox::setPlaceholderText(const QString& text)
{
	m_pEdit->setPlaceholderText(text);
}

void QNumericBox::grabFocus()
{
	m_pEdit->setFocus();
}

void QNumericBox::wheelEvent(QWheelEvent* pEvent)
{
	step(sgn(pEvent->delta()));
}

void QNumericBox::showEvent(QShowEvent* pEvent)
{
	QWidget::showEvent(pEvent);
	shown();
}

void QNumericBox::resizeEvent(QResizeEvent* pEvent)
{
	QWidget::resizeEvent(pEvent);

	//Hide the buttons if the width is less than 8 characters
	const bool buttonsVisible = pEvent->size().width() > m_pEdit->fontMetrics().averageCharWidth() * 10;
	m_pUpButton->setVisible(buttonsVisible);
	m_pDownButton->setVisible(buttonsVisible);
}

void QNumericBox::focusInEvent(QFocusEvent* pEvent)
{
	m_pEdit->setFocus(pEvent->reason());
}
