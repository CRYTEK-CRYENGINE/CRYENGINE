// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QDrawContext.h"
#include "Serialization/PropertyTree/IDrawContext.h"
#include "Serialization/PropertyTree/Rect.h"
#include "Serialization/PropertyTree/PropertyTreeStyle.h"
#include "Serialization/PropertyTree/Unicode.h"
#include "Serialization/PropertyTree/Color.h"
#include "Serialization/PropertyTree/ValidatorBlock.h"
#include "Serialization/PropertyTree/MathUtils.h"
#include "QPropertyTree.h"
#include "PropertyGroupBox.h"
#include "PropertySplitter.h"
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QIcon>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>

#include "Controls/QNumericBox.h"

namespace property_tree {

void drawRoundRectangle(QPainter& p, const QRect &_r, unsigned int color, int radius, int width)
{
	QRect r = _r;
	int dia = 2*radius;

	p.setPen(QColor(color));
	p.drawRoundedRect(r, dia, dia);
}

void fillRoundRectangle(QPainter& p, const QBrush& brush, const QRect& _r, const QColor& border, int radius)
{
	bool wasAntialisingSet = p.renderHints().testFlag(QPainter::Antialiasing);
	p.setRenderHints(QPainter::Antialiasing, true);

	p.setBrush(brush);
	p.setPen(QPen(border, 1.0));
	QRectF adjustedRect = _r;
	adjustedRect.adjust(0.5f, 0.5f, -0.5f, -0.5f);
	p.drawRoundedRect(adjustedRect, radius, radius);

	p.setRenderHints(QPainter::Antialiasing, wasAntialisingSet);
}

static const QFont* translateFont(QPropertyTree* tree, Font font)
{
	return font == FONT_NORMAL ? &tree->font() : &tree->boldFont();
}

// ---------------------------------------------------------------------------

static QColor interpolateColor(const QColor& a, const QColor& b, float k)
{
	float mk = 1.0f - k;
	return QColor(a.red() * mk  + b.red() * k,
				  a.green() * mk + b.green() * k,
				  a.blue() * mk + b.blue() * k,
				  a.alpha() * mk + b.alpha() * k);
}

static QColor toQColor(const Color& color)
{
	return QColor(color.r, color.g, color.b, color.a);
}


QRect toQRect(const Rect& r)
{
	return QRect(r.x, r.y, r.w, r.h);
}

Rect fromQRect(const QRect& r)
{
	return Rect(r.left(), r.top(), r.width(), r.height());
}

QPoint toQPoint(const Point& p)
{
	return QPoint(p.x(), p.y());
}

Point fromQPoint(const QPoint& p)
{
	return Point(p.x(), p.y());
}

void QDrawContext::drawColor(const Rect& rect, const Color& treeColor)
{
	
	static QImage checkboardPattern;
	if (checkboardPattern.isNull())
	{
		int size = 12;		
		static vector<int> pixels(size * size);
		for (int i = 0; i < pixels.size(); ++i)
			pixels[i] = ((i / size) / (size / 2) + (i % size) / (size / 2)) % 2 ? 0xffffffff : 0x000000ff;
		checkboardPattern = QImage((unsigned char*)pixels.data(), size, size, size * 4, QImage::Format_RGBA8888);
	}

	QRect r = toQRect(rect.adjusted(0, 0, 0, -1));

	painter_->save();
	painter_->setPen(QPen(Qt::NoPen));
	painter_->setRenderHint(QPainter::Antialiasing, true);
	painter_->setBrush(tree_->palette().color(QPalette::Dark));
	painter_->setPen(Qt::NoPen);
	painter_->drawRoundedRect(r, 2, 2);
	r = r.adjusted(1,1,-1,-1);
	QRect cr = r.adjusted(0, 0, -r.width() / 2, 0);
	painter_->setBrushOrigin(cr.topRight() + QPoint(1,0));
	painter_->setBrush(QBrush(checkboardPattern));

	painter_->setRenderHint(QPainter::Antialiasing, false);	
	painter_->drawRoundedRect(r, 2, 2);

	QColor color(treeColor.r, treeColor.g, treeColor.b, treeColor.a);

	painter_->setPen(QPen(Qt::NoPen));
	painter_->setClipRect(cr);
	painter_->setBrush(QBrush(color));
	painter_->drawRoundedRect(r, 2, 2);

	cr = r.adjusted(r.width() / 2, 0, 0, 0);
	painter_->setClipRect(cr);
	painter_->setBrush(QBrush(QColor(color.red(), color.green(), color.blue(), 255)));
	painter_->drawRoundedRect(r, 2, 2);
	painter_->restore();
}

void QDrawContext::drawComboBox(const Rect& rect, const char* text, bool disabled)
{
	// the Qt drawing functions doesn't paint controls like comboboxes/checkboxes/lineedits etc.
	// with the correct style until there is passed as an argument some existing control,
	// from which is the style internally determined - the control can be any control of the specified type,
	// that's why I added here these static controls in these drawing functions (Tomas Oresky)
	static QComboBox s_comboBox;

	QStyleOptionComboBox option;
	option.editable = false;
	option.frame = true;
	option.currentText = QString(text);

	if (!disabled)
		option.state |= QStyle::State_Enabled;

	option.rect = QRect(0, 0, rect.width(), rect.height());
	// we have to translate painter here to work around bug in some themes
	painter_->translate(rect.left(), rect.top());
	s_comboBox.style()->drawComplexControl(QStyle::CC_ComboBox, &option, painter_, &s_comboBox);
	painter_->setPen(QPen(s_comboBox.palette().color(QPalette::WindowText)));
	QRect textRect = tree_->style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, 0);
	textRect.adjust(1, 0, -1, 0);
	tree_->_drawRowValue(*painter_, text, &s_comboBox.font(), textRect, s_comboBox.palette().color(QPalette::WindowText), false, false);
	painter_->translate(-rect.left(), -rect.top());
}

void QDrawContext::drawSelection(const Rect& rect, bool inlinedRow)
{
	if (tree->treeStyle().selectionRectangle)
	{
		if (inlinedRow) {
			QColor color1(tree_->palette().button().color());
			QColor color2(tree_->palette().highlight().color());
			QColor brushColor = tree_->hasFocusOrInplaceHasFocus() ? interpolateColor(color1, color2, 0.33f) : tree_->palette().shadow().color();
			fillRoundRectangle(*painter_, QBrush(brushColor), toQRect(rect), brushColor, 3);
		}
		else {
			QBrush brush(tree_->hasFocusOrInplaceHasFocus() ? tree_->palette().highlight() : tree_->palette().shadow());
			QColor brushColor = brush.color();
			QColor borderColor(brushColor.red(), brushColor.green(), brushColor.blue(), brushColor.alpha() / 4);
			fillRoundRectangle(*painter_, brush, toQRect(rect), borderColor, 3);
		}
	}
}

void QDrawContext::drawRowLine(const Rect& rect)
{
	QLinearGradient gradient(rect.left(), rect.top(), rect.right(), rect.top());
	gradient.setColorAt(0.0f, tree_->palette().color(QPalette::Button));
	gradient.setColorAt(0.6f, tree_->palette().color(QPalette::Light));
	gradient.setColorAt(0.95f, tree_->palette().color(QPalette::Light));
	gradient.setColorAt(1.0f, tree_->palette().color(QPalette::Button));
	QBrush brush(gradient);
	painter_->fillRect(toQRect(rect), brush);
}

void QDrawContext::drawHorizontalLine(const Rect& rect)
{
	// Not implemented
}

void QDrawContext::drawPlus(const Rect& rect, bool expanded, bool selected, bool grayed)
{	
	QStyleOption option;
	option.rect = toQRect(rect);
	option.state = QStyle::State_Enabled | QStyle::State_Children;
	if (expanded)
		option.state |= QStyle::State_Open;
	painter_->setPen(QPen());
	painter_->setBrush(QBrush());

	if (expanded)
	{
		if (!tree_->getBranchOpened().isNull())
			tree_->getBranchOpened().paint(painter_, toQRect(rect));
		else
			tree_->style()->drawPrimitive(QStyle::PE_IndicatorBranch, &option, painter_, tree_);
	}
	else
	{
		if (!tree_->getBranchClosed().isNull())
			tree_->getBranchClosed().paint(painter_, toQRect(rect));
		else
			tree_->style()->drawPrimitive(QStyle::PE_IndicatorBranch, &option, painter_, tree_);
	}
}

void QDrawContext::drawSplitter(const Rect& rect)
{
	static QPropertySplitter s_propertySplitter;

	QStyleOption option;
	option.state |= QStyle::State_Enabled;
	
	option.rect = QRect(0, 0, tree_->rightBorder() - rect.left(), rect.height());
	// we have to translate painter here to work around bug in some themes
	painter_->translate(rect.left(), rect.top());
	s_propertySplitter.style()->drawPrimitive(QStyle::PE_Widget, &option, painter_, &s_propertySplitter);
	painter_->translate(-rect.left(), -rect.top());
}

void QDrawContext::drawGroupBox(const Rect& rect, int headerHeight)
{
	static QPropertyGroupBox s_groupBox;
	static QPropertyGroupHeader s_groupHeader;

	QStyleOption option;
	option.state |= QStyle::State_Enabled;
	option.rect = QRect(0, 0, rect.width(), rect.height());
	// we have to translate painter here to work around bug in some themes
	painter_->translate(rect.left(), rect.top());
	s_groupBox.style()->drawPrimitive(QStyle::PE_Widget, &option, painter_, &s_groupBox);
	option.rect.setHeight(headerHeight);
	s_groupHeader.style()->drawPrimitive(QStyle::PE_Widget, &option, painter_, &s_groupHeader);
	painter_->translate(-rect.left(), -rect.top());
}

void QDrawContext::drawLabel(const char* text, Font font, const Rect& rect, bool selected, bool disabled)
{
	const QFont* qfont = translateFont(tree_, font);
	QColor textColor = tree_->palette().buttonText().color();
	if (disabled)
	{
		textColor = tree_->palette().dark().color();
	}
	else if(selected)
	{
		textColor = tree_->treeStyle().selectionRectangle ? tree_->palette().highlightedText().color() : tree_->palette().highlight().color();
	}
	else
	{
		const float contrast = 110.0f;
		if (font == FONT_BOLD)
			textColor = textColor.lighter(contrast);
		else
			textColor = textColor.darker(contrast);
	}

	tree_->drawFilteredString(*painter_, text, QPropertyTree::RowFilter::NAME, qfont, toQRect(rect), textColor, false, false);
}

void QDrawContext::drawNumberEntry(const char* text, const Rect& rect, int flags, double minValue, double maxValue)
{
	static QNumericBox s_lineEdit;
	QRect rt = toQRect(rect);

	s_lineEdit.setGeometry(rt);
	s_lineEdit.setMinimum(minValue);
	s_lineEdit.setMaximum(maxValue);
	s_lineEdit.setValue(atof(text));
	s_lineEdit.alignText();
	s_lineEdit.setEnabled(!(flags & FIELD_DISABLED));

	QPixmap w1Pix = s_lineEdit.grab();
	painter_->drawPixmap(rt, w1Pix);
}

void QDrawContext::drawIcon(const Rect& rect, const Icon& icon, IconEffect effect)
{
	QImage* image = iconCache_->getImageForIcon(icon);
	if (!image)
		return;
	int x = rect.left() + (rect.width() - image->width()) / 2;
	int y = rect.top() + (rect.height() - image->height()) / 2;
	// TODO effect
	if (effect == ICON_DISABLED)
	{
		painter_->drawImage(x, y, *image,0,0,-1,-1,Qt::MonoOnly);
	}
	else
	{
		painter_->drawImage(x, y, *image);
	}
}

void QDrawContext::drawCheck(const Rect& rect, bool disabled, CheckState checked)
{
	static QCheckBox s_checkBox;

	QStyleOptionButton option;
	if (!disabled)
		option.state |= QStyle::State_Enabled;
	if (checked == CHECK_SET)
		option.state |= QStyle::State_On;
	else if (checked == CHECK_IN_BETWEEN)
		option.state |= QStyle::State_NoChange;
	else
		option.state |= QStyle::State_Off;

	QSize checkboxSize = s_checkBox.style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, &s_checkBox).size();
	option.rect = QRect(toQRect(rect).center().x() - checkboxSize.width() / 2, toQRect(rect).center().y() - checkboxSize.height() / 2, checkboxSize.width(), checkboxSize.height());

	s_checkBox.style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &option, painter_, &s_checkBox);
}

void QDrawContext::drawControlButton(const Rect& rect, const char* text, int buttonFlags, property_tree::Font font_name, const Color* colorOverride)
{
	drawButton(rect, text, buttonFlags, font_name, colorOverride);
}

void QDrawContext::drawButton(const Rect& rect, const char* text, int buttonFlags, property_tree::Font font_name, const Color* colorOverride)
{
	static QPushButton s_defaultButton;

	bool pressed = (buttonFlags & BUTTON_PRESSED) != 0;
	bool focused = (buttonFlags & BUTTON_FOCUSED) != 0;
	bool center = (buttonFlags & BUTTON_CENTER_TEXT) != 0;
	bool dropDownArrow = (buttonFlags & BUTTON_DROP_DOWN) != 0;
	bool enabled = (buttonFlags & BUTTON_DISABLED) == 0;
	const QFont* font = translateFont(tree_, font_name);
	QStyleOptionButton option;
	if (enabled)
		option.state |= QStyle::State_Enabled;
	else {
		option.state |= QStyle::State_ReadOnly;
		option.palette.setCurrentColorGroup(QPalette::Disabled);
	}
	if (pressed) {
		option.state |= QStyle::State_On;
		option.state |= QStyle::State_Sunken;
	}
	else
		option.state |= QStyle::State_Raised;

	if (focused)
		option.state |= QStyle::State_HasFocus;
	option.rect = toQRect(rect.adjusted(0, 0, -1, -1));

	if (colorOverride) {
		QPalette& palette = option.palette;
		palette.setCurrentColorGroup(QPalette::Normal);
		QColor tintTarget(colorOverride->r, colorOverride->g, colorOverride->b, colorOverride->a);

		QPalette::ColorRole groups[] = { QPalette::Button, QPalette::Light, QPalette::Dark, QPalette::Midlight, QPalette::Mid, QPalette::Shadow };
		for (int i = 0; i < sizeof(groups) / sizeof(groups[0]); ++i)
			palette.setColor(groups[i], interpolateColor(palette.color(groups[i]), tintTarget, 0.11f));

		s_defaultButton.style()->drawControl(QStyle::CE_PushButton, &option, painter_, &s_defaultButton);
	}
	else
		s_defaultButton.style()->drawControl(QStyle::CE_PushButton, &option, painter_, &s_defaultButton);

	QRect textRect;
	if (enabled && dropDownArrow)
	{
		QStyleOption arrowOption;
		arrowOption.rect = QRect(rect.right() - 11, rect.top(), 8, rect.height());
		arrowOption.state |= QStyle::State_Enabled;
		tree_->style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &arrowOption, painter_, 0);
		textRect = toQRect(rect.adjusted(0, 0, -8, 0));
	}
	else
		textRect = toQRect(rect);

	if (pressed)
		textRect = textRect.adjusted(1, 0, 1, 0);
	if (!center)
		textRect.adjust(4, 0, -5, 0);

	QColor textColor;
	if (colorOverride && enabled)
		textColor = interpolateColor(s_defaultButton.palette().color(QPalette::Normal, QPalette::ButtonText), 
									 QColor(colorOverride->r, colorOverride->g, colorOverride->b, colorOverride->a), 0.4f);
	else
		textColor = s_defaultButton.palette().color(enabled ? QPalette::Normal : QPalette::Disabled, QPalette::ButtonText);
	tree_->_drawRowValue(*painter_, text, font, textRect, textColor, false, center);
}

void QDrawContext::drawButtonWithIcon(const Icon& icon, const Rect& rect, const char* text, int buttonFlags, property_tree::Font font)
{
	bool pressed = (buttonFlags & BUTTON_PRESSED) != 0;
	bool focused = (buttonFlags & BUTTON_FOCUSED) != 0;
	bool center = (buttonFlags & BUTTON_CENTER_TEXT) != 0;
	bool dropDownArrow = (buttonFlags & BUTTON_DROP_DOWN) != 0;
	bool enabled = (buttonFlags & BUTTON_DISABLED) == 0;
	bool showButtonFrame = (buttonFlags & BUTTON_NO_FRAME) == 0;

	QStyleOptionButton option;
	if (enabled)
		option.state |= QStyle::State_Enabled;
	else
		option.state |= QStyle::State_ReadOnly;
	if (pressed) {
		option.state |= QStyle::State_On;
		option.state |= QStyle::State_Sunken;
	}
	else
		option.state |= QStyle::State_Raised;

	if (focused)
		option.state |= QStyle::State_HasFocus;
	option.rect = toQRect(rect.adjusted(0, 0, -1, -1));

	if (showButtonFrame)
		tree_->style()->drawControl(QStyle::CE_PushButton, &option, painter_);

	int iconSize = 16;
	QRect iconRect(toQPoint(rect.topLeft()), QPoint(rect.left() + iconSize, rect.bottom()));
	QRect textRect;
	if (enabled)
		textRect = toQRect(rect.adjusted(iconSize, 0, -8, 0));
	else
		textRect = toQRect(rect.adjusted(iconSize, 0, 0, 0));

	if (pressed)
	{
		textRect.adjust(5, 0, 1, 0);
		iconRect.adjust(4, 0, 4, 0);
	}
	else
	{
		textRect.adjust(4, 0, 0, 0);
		iconRect.adjust(3, 0, 3, 0);
	}
	drawIcon(fromQRect(iconRect), icon, ICON_NORMAL);

	QColor textColor = tree_->palette().color(enabled ? QPalette::Active : QPalette::Disabled, pressed && !showButtonFrame ? QPalette::HighlightedText :		QPalette::ButtonText);
	static_cast<const QPropertyTree*>(tree)->_drawRowValue(*painter_, text, translateFont(tree_, font), textRect, textColor, false, false);
}

void QDrawContext::drawValueText(bool highlighted, const char* text)
{
	QColor textColor;
	if (tree_->treeStyle().selectionRectangle)
		textColor = highlighted ? tree_->palette().highlightedText().color() : tree_->palette().buttonText().color();
	else
		textColor = highlighted ? tree_->palette().highlight().color() : tree_->palette().buttonText().color();
	QRect textRect(widgetRect.left() + 3, widgetRect.top() + 2, widgetRect.width() - 6, widgetRect.height() - 4);
	tree_->_drawRowValue(*painter_, text, &tree_->font(), textRect, textColor, false, false);
}

void QDrawContext::drawEntry(const Rect& rect, const char* text, bool pathEllipsis, bool grayBackground, int trailingOffset)
{
	QLineEdit s_lineEdit;
	QRect rt = toQRect(rect);
	rt.adjust(0, 0, -trailingOffset, -1);
	QStyleOptionFrameV2 option;
	option.state = QStyle::State_Sunken;
	option.lineWidth = tree_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, 0);
	option.midLineWidth = 0;
	option.features = QStyleOptionFrameV2::None;
	if (!grayBackground)
		option.state |= QStyle::State_Enabled;
	if (captured)
	  option.state |= QStyle::State_HasFocus;
	option.rect = rt; // option.rect is the rectangle to be drawn on.
	QRect textRect = tree_->style()->subElementRect(QStyle::SE_LineEditContents, &option, tree_);
	if (!textRect.isValid()) {
		textRect = rt;
		textRect.adjust(3, 1, -3, -2);
	}
	else {
		textRect.adjust(2, 1, -2, -1);
	}

	s_lineEdit.style()->drawPrimitive(QStyle::PE_PanelLineEdit, &option, painter_, &s_lineEdit);
	tree_->_drawRowValue(*painter_, text, &s_lineEdit.font(),  textRect,  s_lineEdit.palette().foreground().color(), pathEllipsis, false);
}

static const QIcon& GetValidatorErrorIcon()
{
	static const property_tree::PropertyIcon errorIcon(":/QPropertyTree/validator_error.png");
	return errorIcon;
}

static const QIcon& GetValidatorWarningIcon()
{
	static const property_tree::PropertyIcon warningIcon(":/QPropertyTree/validator_warning.png");
	return warningIcon;
}

void QDrawContext::drawValidators(PropertyRow* row, const Rect& totalRect)
{
	using namespace property_tree;
	QPropertyTree* tree = tree_;
	const int padding = tree->_defaultRowHeight() * 0.1f;
	int offset = padding;
	auto drawFunc = [&](PropertyRow* row) {
		QFontMetrics fm(tree->font());
		if (const ValidatorEntry* validatorEntries = tree->_validatorBlock()->getEntry(row->validatorIndex(), row->validatorCount())) {
			for (int i = 0; i < row->validatorCount(); ++i) {
				const ValidatorEntry* validatorEntry = validatorEntries + i;
				bool isError = validatorEntry->type == VALIDATOR_ENTRY_ERROR;

				const char* text = validatorEntry->message.c_str();
				const QIcon& icon = isError ? GetValidatorErrorIcon() : GetValidatorWarningIcon();
				QColor brushColor = isError ? QColor(255, 64, 64, 192) : QPalette().color(QPalette::ToolTipBase);
				QColor penColor = isError ? QColor(64, 0, 0, 255) : QPalette().color(QPalette::ToolTipText);

				QRect rect(totalRect.left(), totalRect.top() + offset,
						  totalRect.width(), totalRect.height() - offset);

				QRect textRect = rect.adjusted(tree->_defaultRowHeight() + padding, padding, -padding, -padding);
				int textHeight = max(tree->_defaultRowHeight(),
					fm.boundingRect(textRect, Qt::TextWordWrap, QString::fromUtf8(text), 0, 0).height() + padding * 2);
				rect.setHeight(textHeight + padding * 2);
				textRect.setHeight(textHeight);

				QPen pen(penColor);
				pen.setWidth(1);
				painter_->setPen(QPen(penColor));
				painter_->setRenderHint(QPainter::Antialiasing);
				painter_->setBrush(brushColor);
				painter_->translate(-0.5f, -0.5f);
				painter_->drawRoundedRect(rect, 2, 2, Qt::AbsoluteSize);
				painter_->translate(0.5f, 0.5f);
				painter_->setPen(penColor);
				painter_->setBrush(QBrush());
				QTextOption opt;
				opt.setWrapMode(QTextOption::WordWrap);
				opt.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
				painter_->drawText(textRect, text, opt);
				textRect.setHeight(0xffff);
				QRect iconRect(rect.left(), rect.top(), tree->_defaultRowHeight(), rect.height());
				icon.paint(&*painter_, iconRect, Qt::AlignCenter);
				offset += rect.height() + padding;
			}
		}
	};
	drawFunc(row);
	visitPulledRows(row, drawFunc);
}

static void DrawValidatorIcon(const Rect& rect, const bool isError, QPainter& painter)
{
	const QIcon& icon = isError ? GetValidatorErrorIcon() : GetValidatorWarningIcon();
	const QRect iconRect(rect.left(), rect.top(), rect.width(), rect.height());
	icon.paint(&painter, iconRect, Qt::AlignCenter);
}

void QDrawContext::drawValidatorErrorIcon(const Rect& rect) 
{
	DrawValidatorIcon(rect, true, *painter_);
}

void QDrawContext::drawValidatorWarningIcon(const Rect& rect) 
{
	DrawValidatorIcon(rect, false, *painter_);
}


}

