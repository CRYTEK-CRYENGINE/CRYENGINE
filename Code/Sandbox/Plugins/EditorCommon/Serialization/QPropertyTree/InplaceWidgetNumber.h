// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QObject>
#include <QLineEdit>
#include <QApplication>
#include "QPropertyTree.h"
#include "Serialization/PropertyTree/IUIFacade.h"
#include "Serialization/PropertyTree/PropertyTreeModel.h"
#include "Serialization/PropertyTree/PropertyRowNumberField.h"
#include "Serialization/PropertyTree/MathUtils.h"

#include "Controls/QNumericBox.h"

class InplaceWidgetNumber : public QObject, public property_tree::InplaceWidget
{
	Q_OBJECT
public:
	inline InplaceWidgetNumber(PropertyRowNumberField* numberField, QPropertyTree* tree);

	~InplaceWidgetNumber()
	{
		if (QApplication::mouseButtons() == Qt::LeftButton)
		{
			QPoint pt = entry_->mapFromGlobal(QCursor::pos());
			QRect rect = entry_->rect();
			rect.adjust(rect.width() - rect.height(), 0, 0, 0);
			if (rect.contains(pt))
			{
				if (rect.top() - pt.y() > rect.height() / 2)
				{
					row_->addValue(tree_, -row_->singlestep());
				}
				else
				{
					row_->addValue(tree_, row_->singlestep());
				}
			}
		}

		entry_->setParent(0);
		entry_->deleteLater();
		entry_ = 0;
	}

	void commit();
	void* actualWidget() { return entry_; }
	bool autoFocus() const { return false; }

protected:
	void onShown();

	QNumericBox* entry_;
	PropertyRowNumberField* row_;
	QPropertyTree* tree_;
};

InplaceWidgetNumber::InplaceWidgetNumber(PropertyRowNumberField* row, QPropertyTree* tree)
: InplaceWidget(tree)
, row_(row)
, entry_(new QNumericBox(tree))
, tree_(tree)
{
	entry_->setValue(atof(row_->valueAsString().c_str()));
	entry_->setEditMode(true);
	connect(entry_, &QNumericBox::shown, this, &InplaceWidgetNumber::onShown);
}

inline void InplaceWidgetNumber::commit()
{
	QString str = QString::number(entry_->value());
	if (str != row_->valueAsString().c_str()  || row_->multiValue())
	{
		tree_->model()->rowAboutToBeChanged(row_);
		row_->setValueFromString(str.toUtf8().data());
		tree_->model()->rowChanged(row_);
	}
	else
	{
		tree_->_cancelWidget();
	}
}

inline void InplaceWidgetNumber::onShown()
{
	entry_->setEditMode(true);
	connect(entry_, &QNumericBox::valueSubmitted, this, &InplaceWidgetNumber::commit);
}

