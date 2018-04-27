// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "BlockPalette.h"
#include "BlockPaletteContent.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>

namespace {

enum
{
	SELECTION_WIDTH       = 4,
	WIDGET_MARGIN         = 4,
	ELEMENT_PADDING       = 3,
	ELEMENT_MARGIN        = 5,
	MINIMAL_DROP_DISTANCE = 24
};
struct SLayoutItem
{
	QRect rect;
};
typedef std::vector<SLayoutItem> SLayoutItems;
}

struct BlockPaletteLayout
{
	int          height;
	SLayoutItems items;
};

static QRect AddElementToLayout(int* currentTop, int* currentLeft, int width, int elementWidth, int lineHeight, int padding)
{
	if (*currentLeft + padding + elementWidth < width)
	{
		QRect result(*currentLeft, *currentTop, elementWidth, lineHeight);
		*currentLeft += padding + elementWidth;
		return result;
	}
	else
	{
		*currentLeft = padding + elementWidth + padding;
		*currentTop += padding + lineHeight;
		return QRect(padding, *currentTop, elementWidth, lineHeight);
	}

}

static void CalculateLayout(BlockPaletteLayout* layout, const BlockPaletteContent& content, const QFont& font, int width)
{
	layout->items.clear();

	int padding = ELEMENT_PADDING;
	layout->height = padding;
	int currentLeft = padding;
	int margin = WIDGET_MARGIN;

	QFontMetrics metrics(font);
	int lineHeight = metrics.height() + margin * 2;
	if (lineHeight < 24)
		lineHeight = 24;

	layout->items.reserve(layout->items.size());
	for (size_t i = 0; i < content.items.size(); ++i)
	{
		const BlockPaletteItem& item = content.items[i];
		SLayoutItem litem;
		int elementWidth = metrics.width(QString::fromLocal8Bit(item.name.c_str())) + 2 * ELEMENT_MARGIN;

		litem.rect = AddElementToLayout(&layout->height, &currentLeft, width, elementWidth, lineHeight, padding);
		layout->items.push_back(litem);
	}

	layout->height += padding + lineHeight;
}

static int HitItem(const SLayoutItems& items, const QPoint& pos)
{
	for (size_t i = 0; i < items.size(); ++i)
	{
		const SLayoutItem& item = items[i];
		if (item.rect.contains(pos))
			return int(i);
	}
	return -1;
}

static void HitItems(std::vector<int>* selectedItems, const SLayoutItems& items, const QRect& rect)
{
	selectedItems->clear();
	for (size_t i = 0; i < items.size(); ++i)
	{
		const SLayoutItem& item = items[i];
		if (item.rect.intersects(rect))
			selectedItems->push_back(int(i));
	}
}

static int FindDropIndex(const SLayoutItems& items, const QPoint& p, int excludingIndex)
{
	if (items.empty())
		return -1;
	typedef std::vector<std::pair<int, int>> DistanceToIndex;
	DistanceToIndex distances;
	for (size_t i = 0; i < items.size(); ++i)
	{
		const QRect& r = items[i].rect;
		if (i < excludingIndex || i > excludingIndex + 1)
		{
			distances.push_back(std::make_pair((p - r.topLeft()).manhattanLength(), int(i)));
			distances.push_back(std::make_pair((p - r.bottomLeft()).manhattanLength(), int(i)));
		}
		if (i + 1 < excludingIndex || i + 1 > excludingIndex + 1)
		{
			distances.push_back(std::make_pair((p - r.topRight()).manhattanLength(), int(i + 1)));
			distances.push_back(std::make_pair((p - r.bottomRight()).manhattanLength(), int(i + 1)));
		}
	}
	if (distances.empty())
		return -1;
	std::sort(distances.begin(), distances.end());
	if (distances[0].first < MINIMAL_DROP_DISTANCE)
		return distances[0].second;
	else
		return -1;
}

static void DrawItem(QPainter& painter, const SLayoutItem& litem, const BlockPaletteItem& item, bool selected, bool hasFocus, const QPalette& palette, int hotkey)
{
	QRectF rect = QRectF(litem.rect.adjusted(1, 2, -1, -2));
	float ratio = rect.width() != 0 ? (rect.height() != 0 ? rect.width() / float(rect.height()) : 1.0f) : 1.0f;
	float radius = 0.2f;
	float rx = radius * 200.0f / ratio;
	float ry = radius * 200.0f;
	if (selected)
	{
		QRectF selectionRect = rect.adjusted(-SELECTION_WIDTH * 0.5f + 0.5f, -SELECTION_WIDTH * 0.5f + 0.5f, SELECTION_WIDTH * 0.5f - 0.5f, SELECTION_WIDTH * 0.5f - 0.5f);
		painter.setPen(QPen(palette.color(hasFocus ? QPalette::Highlight : QPalette::Shadow), SELECTION_WIDTH));
		painter.setBrush(QBrush(Qt::NoBrush));
		painter.drawRoundedRect(selectionRect, rx, ry, Qt::RelativeSize);
	}

	QRectF shadowRect = rect.adjusted(-1.0f, -0.5f, 1.0f, 1.5f);
	painter.setPen(QPen(QColor(0, 0, 0, 128), 2.0f));
	painter.setBrush(QBrush(Qt::NoBrush));
	painter.drawRoundedRect(shadowRect, rx, ry, Qt::RelativeSize);

	painter.setPen(QPen(QColor(item.color.r, item.color.g, item.color.b, 255)));
	painter.setBrush(QBrush(QColor(item.color.r, item.color.g, item.color.b, 255)));
	painter.drawRoundedRect(rect, rx, ry, Qt::RelativeSize);

	painter.setBrush(QColor(0, 0, 0));
	painter.setPen(QColor(0, 0, 0));
	painter.drawText(litem.rect.adjusted(hotkey != -1 ? ELEMENT_MARGIN : 0, 0, 0, 0), QString::fromLocal8Bit(item.name.c_str()), QTextOption(Qt::AlignCenter));

	if (hotkey != -1)
	{
		QRectF hotkeyRect = rect.adjusted(2, 1, -2, -1);
		QFont oldFont = painter.font();
		QFont font = oldFont;
		font.setBold(true);
		font.setPointSizeF(oldFont.pointSizeF() * 0.666f);
		painter.setFont(font);
		QString str;
		str.sprintf("%d", hotkey);
		painter.setBrush(QColor(0, 0, 0, 128));
		painter.drawText(hotkeyRect, str, QTextOption(Qt::AlignBottom | Qt::AlignLeft));
		painter.setFont(oldFont);
	}
}

struct BlockPalette::MouseHandler
{
	virtual ~MouseHandler() { Finish(); }
	virtual void MousePressEvent(QMouseEvent* ev) = 0;
	virtual void MouseMoveEvent(QMouseEvent* ev) = 0;
	virtual void MouseReleaseEvent(QMouseEvent* ev) = 0;
	virtual void Finish()                      {}
	virtual void PaintOver(QPainter& painter)  {}
	virtual void PaintUnder(QPainter& painter) {}
};

struct BlockPalette::SelectionHandler : MouseHandler
{
	BlockPalette* m_palette;
	QPoint        m_startPoint;
	QRect         m_rect;

	SelectionHandler(BlockPalette* palette)
		: m_palette(palette)
	{
	}

	void Finish() override
	{
		m_palette->SignalSelectionChanged();
	}

	void MousePressEvent(QMouseEvent* ev) override
	{
		m_startPoint = ev->pos();
		m_rect = QRect(m_startPoint, m_startPoint + QPoint(1, 1));
	}

	void MouseMoveEvent(QMouseEvent* ev) override
	{
		m_rect = QRect(m_startPoint, ev->pos() + QPoint(1, 1));
		std::vector<int> selectedItems;
		HitItems(&selectedItems, m_palette->m_layout->items, m_rect);
		if (m_palette->m_selectedItems.size() != selectedItems.size())
		{
			m_palette->m_selectedItems = selectedItems;
			m_palette->SignalSelectionChanged();
		}
	}

	void MouseReleaseEvent(QMouseEvent* ev) override
	{
		Finish();
	}

	void PaintOver(QPainter& painter)
	{
		painter.save();
		QRect rect = m_rect.intersected(QRect(1, 1, m_palette->width() - 2, m_palette->height() - 2));
		QColor highlightColor = m_palette->palette().color(QPalette::Highlight);
		QColor highlightColorA = QColor(highlightColor.red(), highlightColor.green(), highlightColor.blue(), 128);
		painter.setPen(QPen(highlightColor));
		painter.setBrush(QBrush(highlightColorA));
		painter.drawRect(QRectF(rect));
		painter.restore();
	}
};

struct BlockPalette::DragHandler : MouseHandler
{
	BlockPalette* m_palette;
	QPoint        m_startPoint;
	QPoint        m_lastPoint;
	bool          m_moved;
	int           m_dropIndex;

	DragHandler(BlockPalette* palette)
		: m_palette(palette)
		, m_moved(false)
		, m_dropIndex(-1)
	{
	}

	bool DragStarted() const
	{
		return (m_lastPoint - m_startPoint).manhattanLength() > 5;
	}

	void Finish() override
	{
		if (m_moved)
		{
			if (m_dropIndex != -1)
			{
				std::vector<int> oldToNewIndex;
				oldToNewIndex.resize(m_palette->m_content->items.size(), -1);
				BlockPaletteItems items;
				int selectionStart = 0;
				for (size_t i = 0; i < m_dropIndex; ++i)
					if (std::find(m_palette->m_draggedItems.begin(), m_palette->m_draggedItems.end(), i) == m_palette->m_draggedItems.end())
					{
						oldToNewIndex[i] = items.size();
						items.push_back(m_palette->m_content->items[i]);
						++selectionStart;
					}
				for (size_t i = 0; i < m_palette->m_draggedItems.size(); ++i)
				{
					int index = m_palette->m_draggedItems[i];
					oldToNewIndex[index] = items.size();
					items.push_back(m_palette->m_content->items[index]);
				}
				for (size_t i = m_dropIndex; i < m_palette->m_content->items.size(); ++i)
					if (std::find(m_palette->m_draggedItems.begin(), m_palette->m_draggedItems.end(), i) == m_palette->m_draggedItems.end())
					{
						oldToNewIndex[i] = items.size();
						items.push_back(m_palette->m_content->items[i]);
					}

				for (size_t i = 0; i < m_palette->m_hotkeys.size(); ++i)
				{
					int oldIndex = m_palette->m_hotkeys[i];
					if (size_t(oldIndex) < oldToNewIndex.size())
						m_palette->m_hotkeys[i] = oldToNewIndex[oldIndex];
				}

				m_palette->m_content->items.swap(items);
				m_palette->m_selectedItems.clear();
				for (size_t i = selectionStart; i < selectionStart + m_palette->m_draggedItems.size(); ++i)
					m_palette->m_selectedItems.push_back(i);
				m_palette->UpdateLayout();
				m_palette->SignalChanged();
				m_palette->SignalSelectionChanged();
			}
		}

		m_palette->m_draggedItems.clear();
	}

	void MousePressEvent(QMouseEvent* ev) override
	{
		m_startPoint = ev->pos();
		m_lastPoint = m_startPoint;
	}

	void MouseMoveEvent(QMouseEvent* ev) override
	{
		bool wasDraggingBefore = DragStarted();
		m_lastPoint = ev->pos();
		bool dragStarted = DragStarted();

		if (!wasDraggingBefore && DragStarted())
		{
			m_palette->m_draggedItems = m_palette->m_selectedItems;
			m_moved = true;
		}
		if (wasDraggingBefore && !DragStarted())
		{
			m_palette->m_draggedItems.clear();
		}

		if (dragStarted && !m_palette->m_selectedItems.empty())
			m_dropIndex = FindDropIndex(m_palette->m_layout->items, m_lastPoint, m_palette->m_selectedItems[0]);
		else
			m_dropIndex = -1;
	}

	void MouseReleaseEvent(QMouseEvent* ev) override
	{
		if (m_palette->m_addWithSingleClick && !m_moved)
		{
			int itemIndex = HitItem(m_palette->m_layout->items, ev->pos());
			if (itemIndex >= 0)
				m_palette->ItemClicked(itemIndex);
		}

		Finish();
	}

	void PaintUnder(QPainter& painter) override
	{
		if (m_dropIndex != -1)
		{
			QPoint p0, p1;
			if (m_dropIndex == m_palette->m_layout->items.size())
			{
				QRect itemRect = m_palette->m_layout->items.back().rect;
				p0 = itemRect.topRight();
				p1 = itemRect.bottomRight();
			}
			else
			{
				QRect itemRect = m_palette->m_layout->items[m_dropIndex].rect;
				p0 = itemRect.topLeft();
				p1 = itemRect.bottomLeft();
			}
			painter.setPen(QPen(m_palette->palette().color(QPalette::Highlight), 8, Qt::SolidLine, Qt::RoundCap));
			painter.drawLine(p0, p1);
		}
	}

	void PaintOver(QPainter& painter) override
	{
		if (DragStarted())
		{
			if (m_palette->m_selectedItems.empty())
				return;

			int index = m_palette->m_selectedItems[0];
			SLayoutItem litem = m_palette->m_layout->items[index];
			litem.rect = litem.rect.translated(m_lastPoint - m_startPoint);
			DrawItem(painter, litem, m_palette->m_content->items[index], true, true, m_palette->palette(), -1);
		}
	}
};

BlockPalette::BlockPalette(QWidget* parent)
	: QWidget(parent)
	, m_layout(new BlockPaletteLayout())
	, m_content(new BlockPaletteContent())
	, m_addWithSingleClick(false)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	setMinimumHeight(28);
	setFocusPolicy(Qt::StrongFocus);
}

BlockPalette::~BlockPalette()
{
}

void BlockPalette::UpdateLayout()
{
	CalculateLayout(m_layout.get(), *m_content, font(), width());
	update();
	setMinimumHeight(m_layout->height);
}

void BlockPalette::SetContent(const BlockPaletteContent& content)
{
	*m_content = content;
	UpdateLayout();
}

void BlockPalette::SetAddEventWithSingleClick(bool addWithSingleClick)
{
	m_addWithSingleClick = addWithSingleClick;
}

void BlockPalette::paintEvent(QPaintEvent* ev)
{
	bool hasFocus = this->hasFocus();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.translate(0.5f, 0.5f);

	if (m_mouseHandler)
		m_mouseHandler->PaintUnder(painter);

	for (size_t i = 0; i < m_layout->items.size(); ++i)
	{
		bool dragged = std::find(m_draggedItems.begin(), m_draggedItems.end(), int(i)) != m_draggedItems.end();
		;
		if (dragged)
			continue;
		bool selected = std::find(m_selectedItems.begin(), m_selectedItems.end(), int(i)) != m_selectedItems.end();
		const BlockPaletteItem& item = m_content->items[i];
		const SLayoutItem& litem = m_layout->items[i];

		int hotkey = -1;
		for (size_t j = 0; j < m_hotkeys.size(); ++j)
			if (m_hotkeys[j] == i)
			{
				hotkey = j;
				break;
			}

		DrawItem(painter, litem, item, selected, hasFocus, palette(), hotkey);
	}

	if (m_mouseHandler)
		m_mouseHandler->PaintOver(painter);
}

void BlockPalette::ItemClicked(int itemIndex)
{
	if (size_t(itemIndex) >= m_content->items.size())
		return;
	const BlockPaletteItem& item = m_content->items[itemIndex];
	SignalItemClicked(item);
}

void BlockPalette::AssignHotkey(int hotkey)
{
	if (size_t(hotkey) >= 10)
		return;
	m_hotkeys.resize(10, -1);
	for (size_t i = 0; i < m_hotkeys.size(); ++i)
		if (m_hotkeys[i] == m_selectedItems[0])
			m_hotkeys[i] = -1;

	m_hotkeys[hotkey] = m_selectedItems[0];

	update();
}

void BlockPalette::mousePressEvent(QMouseEvent* ev)
{
	if (ev->button() == Qt::LeftButton)
	{
		int hitItem = HitItem(m_layout->items, ev->pos());
		if (hitItem >= 0)
		{
			m_selectedItems.clear();
			m_selectedItems.push_back(hitItem);

			m_mouseHandler.reset(new DragHandler(this));
			m_mouseHandler->MousePressEvent(ev);
			SignalSelectionChanged();
			update();
		}
		else
		{
			m_selectedItems.clear();

			m_mouseHandler.reset(new SelectionHandler(this));
			m_mouseHandler->MousePressEvent(ev);
			SignalSelectionChanged();
			update();
		}
	}
	else if (ev->button() == Qt::RightButton)
	{
		int hitItem = HitItem(m_layout->items, ev->pos());
		if (hitItem >= 0)
		{
			if (std::find(m_selectedItems.begin(), m_selectedItems.end(), hitItem) == m_selectedItems.end())
			{
				m_selectedItems.clear();
				m_selectedItems.push_back(hitItem);
				SignalSelectionChanged();
			}
		}

		int selectionCount = m_selectedItems.size();

		QMenu contextMenu;
		contextMenu.setDefaultAction(contextMenu.addAction("Add", this, SLOT(OnMenuAdd())));
		contextMenu.addAction("Delete", this, SLOT(OnMenuDelete()))->setEnabled(selectionCount != 0);
		contextMenu.addSeparator();
		if (selectionCount == 1)
		{
			QMenu* hotkeyMenu = contextMenu.addMenu("Assign Hotkey");
			for (int i = 0; i < 10; ++i)
			{
				QString text;
				text.sprintf("%d", (i + 1) % 10);
				QString shortcut;
				shortcut.sprintf("Ctrl+%d", (i + 1) % 10);
				hotkeyMenu->addAction(text, this, SLOT(OnMenuAssignHotkey()), QKeySequence(shortcut))->setData(int((i + 1) % 10));
			}
		}
		else
			contextMenu.addAction("Assign Hotkey")->setEnabled(false);
		contextMenu.addSeparator();
		QAction* addWithSingleClick = contextMenu.addAction("Add Events with Single Click", this, SLOT(OnMenuAddWithSingleClick()));
		addWithSingleClick->setCheckable(true);
		addWithSingleClick->setChecked(m_addWithSingleClick);
		contextMenu.exec(QCursor::pos());
	}
}

void BlockPalette::mouseMoveEvent(QMouseEvent* ev)
{
	if (m_mouseHandler.get())
	{
		m_mouseHandler->MouseMoveEvent(ev);
		update();
	}
}

void BlockPalette::mouseReleaseEvent(QMouseEvent* ev)
{
	if (m_mouseHandler.get())
	{
		m_mouseHandler->MouseReleaseEvent(ev);
		m_mouseHandler.reset();
		update();
	}
}

void BlockPalette::mouseDoubleClickEvent(QMouseEvent* ev)
{
	int itemIndex = HitItem(m_layout->items, ev->pos());
	if (itemIndex >= 0)
	{
		if (!m_addWithSingleClick)
			ItemClicked(itemIndex);
	}
	else
		OnMenuAdd();
}

void BlockPalette::keyPressEvent(QKeyEvent* ev)
{
	if (!m_selectedItems.empty())
	{
		if ((ev->modifiers() & Qt::CTRL) != 0)
		{
			if (ev->key() >= Qt::Key_0 && ev->key() <= Qt::Key_9)
			{
				int keyIndex = int(ev->key()) - Qt::Key_0;
				AssignHotkey(keyIndex);
			}
		}
	}

	if ((ev->modifiers() & Qt::CTRL) == 0)
	{
		if (ev->key() >= Qt::Key_0 && ev->key() <= Qt::Key_9)
		{
			int keyIndex = int(ev->key()) - Qt::Key_0;
			size_t index = (size_t)m_hotkeys[keyIndex];
			if (index < m_content->items.size())
				ItemClicked(index);
		}
	}
}

void BlockPalette::resizeEvent(QResizeEvent* ev)
{
	UpdateLayout();
}

void BlockPalette::wheelEvent(QWheelEvent* ev)
{
}

void BlockPalette::GetSelectedIds(BlockPaletteSelectedIds* ids) const
{
	ids->resize(m_selectedItems.size());
	for (size_t i = 0; i < m_selectedItems.size(); ++i)
	{
		BlockPaletteSelectedId& id = (*ids)[i];
		int index = m_selectedItems[i];
		if (size_t(index) < m_content->items.size())
			id.userId = m_content->items[index].userId;
		else
			id.userId = 0;
	}
}

const BlockPaletteItem* BlockPalette::GetItemByHotkey(int hotkey) const
{
	if (size_t(hotkey) >= m_hotkeys.size())
		return 0;
	int index = m_hotkeys[hotkey];
	if (size_t(index) >= m_content->items.size())
		return 0;
	return &m_content->items[index];
}

void BlockPalette::OnMenuAdd()
{
	BlockPaletteItem item;
	item.name = "Preset";
	m_content->items.push_back(item);
	SignalChanged();
	m_selectedItems.clear();
	m_selectedItems.push_back(int(m_content->items.size()) - 1);
	SignalSelectionChanged();
	UpdateLayout();
}

void BlockPalette::OnMenuAddWithSingleClick()
{
	m_addWithSingleClick = !m_addWithSingleClick;
}

void BlockPalette::OnMenuDelete()
{
	std::vector<int> selectedItems;
	m_selectedItems.swap(selectedItems);
	std::sort(selectedItems.begin(), selectedItems.end());
	for (int i = int(selectedItems.size()) - 1; i >= 0; --i)
	{
		int index = selectedItems[i];
		m_content->items.erase(m_content->items.begin() + index);
		for (size_t j = 0; j < m_hotkeys.size(); ++j)
		{
			if (m_hotkeys[j] == index)
				m_hotkeys[j] = 0;
			else if (m_hotkeys[j] > index)
				--m_hotkeys[j];
		}
	}

	SignalChanged();
	SignalSelectionChanged();
	UpdateLayout();
}

void BlockPalette::OnMenuAssignHotkey()
{
	if (QAction* action = qobject_cast<QAction*>(sender()))
	{
		AssignHotkey(action->data().toInt());
	}
}

void BlockPalette::Serialize(Serialization::IArchive& ar)
{
	ar(m_addWithSingleClick, "addWithSingleClick");
	ar(m_hotkeys, "hotkeys");
}

