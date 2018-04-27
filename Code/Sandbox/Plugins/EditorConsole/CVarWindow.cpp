// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CVarWindow.h"
#include "ConsolePlugin.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>

#include <QtUtil.h>

#define INITIAL_NAME_WIDTH 250

//constructor
CCVarWindow::CCVarWindow() : m_address(CConsolePlugin::GetInstance()->GetUniqueAddress()), m_bInitializing(true), m_model(this)
{
	//init UI
	SetupUI();
	m_pCVarList->setModel(&m_model);
	m_pCVarList->setColumnWidth(0, INITIAL_NAME_WIDTH);

	//connect event
	connect(m_pFilterInput, &QLineEdit::textChanged, this, &CCVarWindow::HandleFilterChanged);
	//initialize state by forcing a copy of all CVars to be delivered to us

	Messages::SCVarRequest req;
	req.address = m_address;

	//in case the request for information was completed in a synchronous fashion, we prevented a lot of layout messages
	//so send one now for the initial layout pass
	m_bInitializing = false;
	m_model.layoutChanged();
}

//handle change of filter expression
void CCVarWindow::HandleFilterChanged(const QString& filter)
{
	int index = 0;
	m_model.EnumerateItems([&](SCVarItem& item)
	{
		bool bHidden = false;
		if (!filter.isEmpty())
		{
		  bHidden = !item.name.contains(filter, Qt::CaseInsensitive);
		}
		m_pCVarList->setRowHidden(index++, bHidden);
	});
}

void CCVarWindow::SetupUI()
{
	setObjectName(QStringLiteral("CVarWindow"));
	resize(260, 222);
	QVBoxLayout* verticalLayout = new QVBoxLayout(this);
	verticalLayout->setSpacing(2);
	verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
	verticalLayout->setContentsMargins(2, 2, 2, 2);
	m_pCVarList = new QTableView(this);
	m_pCVarList->setObjectName(QStringLiteral("m_pCVarList"));
	m_pCVarList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_pCVarList->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pCVarList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	m_pCVarList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	m_pCVarList->setShowGrid(false);
	m_pCVarList->setWordWrap(false);
	m_pCVarList->setCornerButtonEnabled(false);
	m_pCVarList->horizontalHeader()->setStretchLastSection(true);
	m_pCVarList->verticalHeader()->setVisible(false);
	m_pCVarList->verticalHeader()->setDefaultSectionSize(16);
	m_pCVarList->verticalHeader()->setMinimumSectionSize(16);

	verticalLayout->addWidget(m_pCVarList);

	QWidget* m_pBottomContainer = new QWidget(this);
	m_pBottomContainer->setObjectName(QStringLiteral("m_pBottomContainer"));
	QHBoxLayout* horizontalLayout = new QHBoxLayout(m_pBottomContainer);
	horizontalLayout->setSpacing(2);
	horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
	horizontalLayout->setContentsMargins(2, 2, 2, 2);
	QLabel* m_pFilterLabel = new QLabel(m_pBottomContainer);
	m_pFilterLabel->setObjectName(QStringLiteral("m_pFilterLabel"));

	horizontalLayout->addWidget(m_pFilterLabel);

	m_pFilterInput = new QLineEdit(m_pBottomContainer);
	m_pFilterInput->setObjectName(QStringLiteral("m_pFilterInput"));

	horizontalLayout->addWidget(m_pFilterInput);
	verticalLayout->addWidget(m_pBottomContainer);
} // setupUi

//model constructor
CCVarWindow::CCVarModel::CCVarModel(CCVarWindow* pParent) : m_pParent(pParent)
{
	//load icons
	m_icons[SCVarItem::eType_Int] = QPixmap(QStringLiteral(":/int.png"));
	m_icons[SCVarItem::eType_Float] = QPixmap(QStringLiteral(":/float.png"));
	m_icons[SCVarItem::eType_String] = QPixmap(QStringLiteral(":/string.png"));
}

//handle updated CVar
void CCVarWindow::CCVarModel::AddOrUpdate(const Messages::SCVarUpdate& update, bool bSuppressLayout)
{
	//find item by index
	QString name = QtUtil::ToQString(update.name);
	SCVarItem* pItem = nullptr;
	bool bAdded = false;
	bool bChanged = false;
	for (std::vector<SCVarItem>::iterator i = m_items.begin(); i != m_items.end(); ++i)
	{
		if (name == i->name)
		{
			pItem = &*i;
			break;
		}
	}
	if (!pItem)
	{
		//add item because it doesn't exist yet
		SCVarItem item;
		qSwap(item.name, name);
		m_items.push_back(item);
		pItem = &m_items.back();
		bAdded = true;
	}

	//set item contents
	switch (update.type)
	{
	case Messages::eVarType_Int:
		{
			const Messages::SCVarUpdateInt& typed = *static_cast<const Messages::SCVarUpdateInt*>(&update);
			bChanged = pItem->value.intValue != typed.value;
			pItem->value.intValue = typed.value;
			pItem->stringValue = QString::number(typed.value);
			pItem->type = SCVarItem::eType_Int;
		}
		break;
	case Messages::eVarType_Float:
		{
			const Messages::SCVarUpdateFloat& typed = *static_cast<const Messages::SCVarUpdateFloat*>(&update);
			bChanged = pItem->value.floatValue != typed.value;
			pItem->value.floatValue = typed.value;
			pItem->stringValue = QString::number(typed.value);
			pItem->type = SCVarItem::eType_Float;
		}
		break;
	case Messages::eVarType_String:
		{
			const Messages::SCVarUpdateString& typed = *static_cast<const Messages::SCVarUpdateString*>(&update);
			QString value = QtUtil::ToQString(typed.value);
			bChanged = pItem->stringValue != value;
			pItem->value.intValue = 0;
			pItem->stringValue = value;
			pItem->type = SCVarItem::eType_String;
		}
		break;
	default:
		{
			bChanged = true;
			pItem->value.intValue = 0;
			pItem->stringValue = QStringLiteral("unknown type");
			pItem->type = SCVarItem::eType_Count;
		}
		break;
	}

	//signal change
	if ((bAdded || bChanged) && !bSuppressLayout)
	{
		layoutChanged();
	}
}

//handle destruction of CVar from engine side
void CCVarWindow::CCVarModel::Destroy(const Messages::SCVarDestroyed& message)
{
	//find item by index
	QString name = QtUtil::ToQString(message.name);
	for (std::vector<SCVarItem>::iterator i = m_items.begin(); i != m_items.end(); ++i)
	{
		if (name == i->name)
		{
			m_items.erase(i);
			layoutChanged();
			return;
		}
	}
}

//retrieve data for an item
QVariant CCVarWindow::CCVarModel::data(const QModelIndex& index, int role) const
{
	size_t row = index.row();
	size_t column = index.column();
	const SCVarItem* pItem = row < m_items.size() ? &m_items.at(row) : NULL;
	if (pItem)
	{
		switch (column)
		{
		case 0:
			switch (role)
			{
			case Qt::DisplayRole:
				return pItem->name;
			case Qt::DecorationRole:
				return m_icons[pItem->type];
			}
			break;
		case 1:
			switch (role)
			{
			case Qt::DisplayRole:
				return QVariant(pItem->stringValue);
			case Qt::EditRole:
				switch (pItem->type)
				{
				case SCVarItem::eType_Int:
					return QVariant(pItem->value.intValue);
				case SCVarItem::eType_Float:
					return QVariant((double)pItem->value.floatValue);
				case SCVarItem::eType_String:
					return QVariant(pItem->stringValue);
				}
			}
		}
	}
	return QVariant();
}

bool CCVarWindow::CCVarModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	//only the second column can be editted
	size_t column = index.column();
	assert(column == 1);
	if (column != 1) return false;

	size_t row = index.row();
	const SCVarItem* pItem = row < m_items.size() ? &m_items.at(row) : NULL;
	if (pItem)
	{
		switch (role)
		{
		case Qt::EditRole:
			switch (pItem->type)
			{
			case SCVarItem::eType_Int:
				if (value.type() == QVariant::Int)
				{
					Messages::SCVarUpdateInt message;
					message.value = value.toInt();
					if (message.value == pItem->value.intValue) return true;
					message.name = QtUtil::ToString(pItem->name);
					message.type = Messages::eVarType_Int;
					return m_pParent->SendCVarUpdate(message);
				}
				break;
			case SCVarItem::eType_Float:
				if (value.type() == QVariant::Double)
				{
					Messages::SCVarUpdateFloat message;
					message.value = value.toFloat();
					if (message.value == pItem->value.floatValue) return true;
					message.name = QtUtil::ToString(pItem->name);
					message.type = Messages::eVarType_Float;
					return m_pParent->SendCVarUpdate(message);
				}
				break;
			case SCVarItem::eType_String:
				if (value.type() == QVariant::String)
				{
					QString stringValue = value.toString();
					if (stringValue == pItem->stringValue) return true;
					Messages::SCVarUpdateString message;
					message.value = QtUtil::ToString(stringValue);
					message.name = QtUtil::ToString(pItem->name);
					message.type = Messages::eVarType_String;
					return m_pParent->SendCVarUpdate(message);
				}
				break;
			}
		}
	}
	return false;
}

