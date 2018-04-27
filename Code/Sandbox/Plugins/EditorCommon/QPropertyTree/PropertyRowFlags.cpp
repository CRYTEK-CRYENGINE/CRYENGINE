// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryCore/Platform/platform.h>

#include <QObject>
#include <QMenu>
#include <QCheckBox>
#include <QWidgetAction>
#include <QHBoxLayout>

#include <Serialization/PropertyTree/PropertyTree.h>
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include <Serialization/PropertyTree/PropertyRowImpl.h>
#include <Serialization/PropertyTree/IDrawContext.h>
#include <CrySerialization/yasli/BitVectorImpl.h>
#include <CrySerialization/StringList.h>

#include "Serialization/QPropertyTree/QPropertyTree.h"

class QAction;

namespace Private_MenuFlags
{
class QCheckboxHandler : public QObject
{
public:
	QCheckboxHandler(QMenu* pMenu, PropertyTree* tree, PropertyRow* flagComboboxRow, yasli::string& value)
		: QObject(pMenu), tree_(tree), flagComboboxRow_(flagComboboxRow), value_(value), menu_(pMenu)
	{
	}

	void onCheckChange(int state);

private:
	PropertyRow*   flagComboboxRow_;
	yasli::string& value_;
	PropertyTree*  tree_;
	QMenu* menu_;
};

int widgetSizeMinHelper(const PropertyTree* tree, const PropertyRow* row, RowWidthCache& widthCache)
{
	if (row->userWidgetSize() >= 0)
	{
		return row->userWidgetSize();
	}

	const int width =
	  row->userWidgetToContent()
	  ? widthCache.getOrUpdate(tree, row, tree->_defaultRowHeight())
	  : 80;

	return width;
}

void addCheckbox(QMenu* menu, QString name, bool checked, QCheckboxHandler* pComboboxHandler)
{
	QCheckBox *checkBox = new QCheckBox(menu);
	QWidgetAction *checkableAction = new QWidgetAction(menu);
	QHBoxLayout *layout = new QHBoxLayout(menu);
	QWidget* widget = new QWidget(menu);

	const int margin = 2;

	layout->setContentsMargins(margin, margin, margin, margin);
	checkBox->setText(name);
	checkBox->setChecked(checked);
	layout->addWidget(checkBox);
	widget->setLayout(layout);
	checkableAction->setDefaultWidget(widget);

	QObject::connect(checkBox, &QCheckBox::stateChanged, pComboboxHandler, &QCheckboxHandler::onCheckChange);

	menu->addAction(checkableAction);
}

bool showPopup(QPropertyTree* pTree, Rect rect, PropertyRow* pPropertyRow, const yasli::StringList& stringList, yasli::string& value)
{
	QPoint menuPoint = pTree->_toScreen(rect.topLeft());
	QPoint menuEndPoint = pTree->_toScreen(Point(rect.right(), rect.top()));
	QMenu* pMenu = new QMenu(pTree);
	pMenu->setMinimumWidth(menuEndPoint.x() - menuPoint.x());

	QCheckboxHandler* pComboboxHandler = new QCheckboxHandler(pMenu, pTree, pPropertyRow, value);
	yasli::StringList checkedItems;
	splitStringList(&checkedItems, value.c_str(), ',');

	for (const yasli::string& item : stringList)
	{
		bool checked = yasli::StringList::npos != checkedItems.find(item.c_str());
		addCheckbox(pMenu, item.c_str(), checked, pComboboxHandler);
	}

	pMenu->popup(menuPoint);
	return true;
}

class PropertyRowFlags : public PropertyRowImpl<Serialization::StringListStaticValue>
{
public:
	PropertyRowFlags() : value_(nullptr), description_(nullptr) {}

	bool onActivate(const PropertyActivationEvent& e)
	{
		if (e.reason == e.REASON_RELEASE)
			return false;
		if (userReadOnly())
			return false;

		property_tree::Rect rect = widgetRect(e.tree);

		if (!rect.contains(e.clickPoint))
			return false;

		QPropertyTree* pTree = (QPropertyTree*)e.tree;
		yasli::StringList names;
		for (const char* name : description_->names())
			names.push_back(name);

		return showPopup(pTree, rect, this, names, value_);
	}

	yasli::string valueAsString() const override { return value_.c_str(); }

	bool          assignTo(const Serializer& ser) const override
	{
		if (yasli::BitVectorWrapper* wrapper = ser.cast<yasli::BitVectorWrapper>())
		{
			yasli::StringList values;
			splitStringList(&values, value_.c_str(), ',');
			yasli::StringList::iterator it;
			wrapper->value = 0;
			for (it = values.begin(); it != values.end(); ++it)
			{
				if (!it->empty())
				{
					if (description_)
						wrapper->value |= description_->value(it->c_str());
					else
						wrapper->value |= wrapper->description->value(it->c_str());

				}
			}
			return true;
		}
		return false;
	}

	void setValueAndContext(const Serializer& ser, yasli::Archive& ar) override
	{
		yasli::BitVectorWrapper* wrapper = ser.cast<yasli::BitVectorWrapper>();
		description_ = wrapper->description;

		if (description_) {
			yasli::StringListStatic values = description_->nameCombination(wrapper->value);
			joinStringList(&value_, values, ',');
		}
		else {
			YASLI_ASSERT(0);
			typeName_ = "";
			value_ = "";
		}
	}

	bool            isLeaf() const override                                { return true; }
	bool            isStatic() const override                              { return false; }
	int             widgetSizeMin(const PropertyTree* tree) const override { return widgetSizeMinHelper(tree, this, widthCache_); }
	WidgetPlacement widgetPlacement() const override                       { return WIDGET_VALUE; }
	const void*     searchHandle() const override                          { return handle_; }
	yasli::TypeID   typeId() const override                                { return type_; }

	void            redraw(IDrawContext& context) override
	{
		if (multiValue())
			context.drawEntry(context.widgetRect, " ... ", false, true, 0);
		else
			context.drawComboBox(context.widgetRect, valueAsString().c_str(), userReadOnly());
	}

	void serializeValue(yasli::Archive& ar) override
	{
		ar(value_, "value", "Value");
	}

private:
	const yasli::EnumDescription* description_;
	yasli::string         value_;
	const void*           handle_;
	yasli::TypeID         type_;
	mutable RowWidthCache widthCache_;
};

void QCheckboxHandler::onCheckChange(int state)
{
	if (!tree_ || !flagComboboxRow_)
		return;

	QString value;
	QList<QCheckBox*> checkboxes = menu_->findChildren<QCheckBox*>();
	for (QCheckBox* pCheckBox : checkboxes)
	{
		if (pCheckBox->isChecked())
			value += "," + pCheckBox->text();
	}

	if (!value.isEmpty())
		value = QString(value.toUtf8().constData() + 1);

	yasli::string newValue(value.toUtf8().constData());
	if (value_ != newValue)
	{
		tree_->model()->rowAboutToBeChanged(flagComboboxRow_);
		value_ = newValue;
		tree_->model()->rowChanged(flagComboboxRow_);
	}
}

REGISTER_PROPERTY_ROW(yasli::BitVectorWrapper, PropertyRowFlags)
}

