// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QNumericBox.h"

#include <BoostPythonMacros.h>

#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QPainter>
#include <QProxyStyle>
#include <QFocusEvent>
#include <QApplication>
#include <QStyleOption>
#include <CryIcon.h>

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
	QNumericEdit(QWidget* parent)
		: QLineEdit(parent)
		, m_lastCursor(-1)
		, m_editMode(false)
		, m_selectAllFromMouse(true)
		, m_editStyle(new QNumericEditProxyStyle())
		, m_mouseMoved(false)
	{
		setStyleSheet("background-color: transparent");
		setEditMode(false);
		setStyle(m_editStyle);
		setText("0");
	}

	inline QString text() { return QLineEdit::text().replace(',', '.'); }

	virtual ~QNumericEdit() override {}

	virtual void mouseDoubleClickEvent(QMouseEvent* e) override
	{
		selectAll();
	}

	virtual void focusInEvent(QFocusEvent* e) override
	{
		m_selectAllFromMouse = false;
		if (e->reason() == Qt::TabFocusReason ||
		    e->reason() == Qt::BacktabFocusReason)
		{
			setEditMode(true);
		}

		QLineEdit::focusInEvent(e);
	}

	virtual void focusOutEvent(QFocusEvent* e) override
	{
		if (e->reason() != Qt::PopupFocusReason)
		{
			setEditMode(false);
		}

		QLineEdit::focusOutEvent(e);
	}

	virtual void mousePressEvent(QMouseEvent* e) override
	{
		if (!getEditMode() && e->button() == Qt::LeftButton)
		{
			m_mouseDownLocation = QCursor::pos();
			m_mouseMoved = false;
			setCursor(Qt::BlankCursor);
		}
		else
		{
			if (m_selectAllFromMouse)
			{
				m_selectAllFromMouse = false;
				selectAll();
			}
			else
			{
				QLineEdit::mousePressEvent(e);
			}
		}
	}

	virtual void mouseMoveEvent(QMouseEvent* e) override
	{
		if (!getEditMode() && QApplication::mouseButtons() == Qt::LeftButton)
		{
			QNumericBox* numbox = qobject_cast<QNumericBox*>(parentWidget());
			QPoint mouse = QCursor::pos();
			if (numbox && mouse != m_mouseDownLocation)
			{
				m_mouseMoved = true;
				double add = 0.0;
				if (numbox->hasMinMax())
				{
					add = (numbox->maximum() - numbox->minimum()) * double((mouse.x() - m_mouseDownLocation.x())) / double(numbox->width());
				}
				else
				{
					add = numbox->singleStep() * double((mouse.x() - m_mouseDownLocation.x()));
				}

				if (Qt::ControlModifier == QApplication::keyboardModifiers())
				{
					add *= 0.01;
				}
				else if (Qt::ShiftModifier == QApplication::keyboardModifiers())
				{
					add *= 100.0;
				}

				numbox->setValue(numbox->value() + add);
				QCursor::setPos(m_mouseDownLocation);
			}
		}
		else
		{
			QLineEdit::mouseMoveEvent(e);
		}
	}

	virtual void mouseReleaseEvent(QMouseEvent* e) override
	{
		releaseMouse();
		if (!getEditMode() && e->button() == Qt::LeftButton)
		{
			setCursor(Qt::ArrowCursor);

			if (!m_mouseMoved)
			{
				setEditMode(true);
				selectAllFromMouse();
				QMouseEvent pe(QEvent::MouseButtonPress, e->pos(), e->button(), e->buttons(), e->modifiers());
				QMouseEvent re(QEvent::MouseButtonRelease, e->pos(), e->button(), e->buttons(), e->modifiers());
				QApplication::sendEvent(this, &pe);
				QApplication::sendEvent(this, &re);
			}
		}
		else
		{
			QLineEdit::mouseReleaseEvent(e);
		}
	}

	virtual bool eventFilter(QObject* object, QEvent* e) override
	{
		if (object != this && e->type() == QEvent::MouseButtonPress)
		{
			QWidget* widget = qobject_cast<QWidget*>(object);
			if (widget && widget->parentWidget() != this)
			{
				QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(e);
				QNumericBox* numbox = qobject_cast<QNumericBox*>(parentWidget());
				if (numbox && mouseEvent->buttons() == Qt::LeftButton)
				{
					QPoint numboxPoint = numbox->mapFromGlobal(widget->mapToGlobal(mouseEvent->pos()));
					QRect numboxRect = numbox->rect();
					if (!numboxRect.contains(numboxPoint))
					{
						setEditMode(false);
					}
				}
			}
		}

		return false;
	}

	void setEditMode(bool edit)
	{
		m_editMode = edit;
		m_editStyle->setCursorWidth(edit ? 1 : 0);
		setCursor(edit ? Qt::IBeamCursor : Qt::ArrowCursor);

		QApplication::instance()->removeEventFilter(this);
		if (edit)
		{
			QApplication::instance()->installEventFilter(this);
		}
		else
		{
			deselect();
		}

		update();
	}

	bool getEditMode()        { return m_editMode; }

	void selectAllFromMouse() { m_selectAllFromMouse = true; }

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

	virtual void keyPressEvent(QKeyEvent* e) override
	{
		if (e->key() == Qt::Key_Escape ||
		    e->key() == Qt::Key_Enter ||
		    e->key() == Qt::Key_Return)
		{
			QLineEdit::editingFinished();
			if (hasFocus())
			{
				selectAll();
			}

			//Lets the keypress go to parents, i.e. the parent dialog may close on Enter/ESC
			//Note that this behavior can be observed on QLineEdit
			e->ignore(); 
			return;
		}

		if (e->key() == Qt::Key_Up ||
		    e->key() == Qt::Key_Down)
		{
			step(e->key() == Qt::Key_Up ? 1.0f : -1.0f);
			QNumericBox* numbox = qobject_cast<QNumericBox*>(parentWidget());
			if (numbox)
			{
				numbox->valueChanged(numbox->value());
			}
		}
		else if (getEditMode())
		{
			QLineEdit::keyPressEvent(e);
		}
	}

	int stepCursor()
	{
		if (hasSelectedText() || !hasFocus() || !getEditMode())
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
		if (m_lastText != text() || m_lastCursor != currentCursor)
		{
			int start = selectionStart();
			int length = selectedText().length();
			setCursorPosition(cursor);
			m_stepRect = cursorRect();
			setCursorPosition(currentCursor);
			setSelection(start, length);
			m_lastText = text();
			m_lastCursor = currentCursor;
		}
		return m_stepRect;
	}

private:
	inline bool isValidDigit(char c) { return isdigit(c) || '.' == c; }

	QRect                   m_stepRect;
	QString                 m_lastText;
	int                     m_lastCursor;
	int                     m_editMode;
	QNumericEditProxyStyle* m_editStyle;
	bool                    m_selectAllFromMouse;
	bool                    m_mouseMoved;
	QPoint                  m_mouseDownLocation;
};

QNumericButton::QNumericButton(QWidget* parent)
	: QWidget(parent)
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

void QNumericButton::paintEvent(QPaintEvent* e)
{
	QPainter painter(this);
	m_icon.paint(&painter, rect());
}

void QNumericButton::mousePressEvent(QMouseEvent* e)
{
	reset();
	if (Qt::LeftButton == e->button())
	{
		singleStep();
		clicked();
		m_spinClickThresholdTimerId = startTimer(m_spinClickThresholdTimerInterval);
	}
	e->accept();
}

void QNumericButton::changeEvent(QEvent* e)
{
	switch (e->type())
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

	QWidget::changeEvent(e);
}

void QNumericButton::mouseReleaseEvent(QMouseEvent* e)
{
	reset();
	e->accept();

	if (Qt::LeftButton == e->button())
	{
		clicked();
	}
}

void QNumericButton::focusOutEvent(QFocusEvent* e)
{
	reset();
	QWidget::focusOutEvent(e);
}

void QNumericButton::timerEvent(QTimerEvent* e)
{
	if (e->timerId() == m_spinClickThresholdTimerId)
	{
		killTimer(m_spinClickThresholdTimerId);
		m_spinClickThresholdTimerId = -1;
		m_effectiveSpinRepeatRate = m_spinClickTimerInterval;
		m_spinClickTimerId = startTimer(m_effectiveSpinRepeatRate);
		singleStep();
	}
	else if (e->timerId() == m_spinClickTimerId)
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

	QWidget::timerEvent(e);
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

QNumericBox::QNumericBox(QWidget* parent)
	: QWidget(parent)
	, m_min(-DBL_MAX)
	, m_max(DBL_MAX)
	, m_step(0.1)
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
	connect(m_pDownButton, &QNumericButton::clicked, [=]() { step(-1.0); });
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
	// PyThreadState can be zeroed out in ShibokenWrapper => no requests to python code is available in this case
	if (!GetIEditor() || !GetIEditor()->GetIPythonManager() || nullptr == PyThreadState_GET())
	{
		return 0.0;
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

	if (m_pEdit->getEditMode())
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

	if (-1 != decPoint)
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

void QNumericBox::paintEvent(QPaintEvent* e)
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

void QNumericBox::wheelEvent(QWheelEvent* e)
{
	step(sgn(e->delta()));
}

void QNumericBox::showEvent(QShowEvent* e)
{
	QWidget::showEvent(e);
	shown();
}

void QNumericBox::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);

	//Hide the buttons if the width is less than 8 characters
	const bool buttonsVisible = event->size().width() > m_pEdit->fontMetrics().averageCharWidth() * 10;
	m_pUpButton->setVisible(buttonsVisible);
	m_pDownButton->setVisible(buttonsVisible);
}

void QNumericBox::focusInEvent(QFocusEvent* e)
{
	m_pEdit->setFocus(e->reason());
}


