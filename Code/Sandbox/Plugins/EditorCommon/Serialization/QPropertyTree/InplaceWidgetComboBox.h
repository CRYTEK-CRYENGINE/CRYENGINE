// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QObject>
#include <QComboBox>
#include <QApplication>
#include <QMouseEvent>
#include "QPropertyTree.h"

#include "Serialization/PropertyTree/IUIFacade.h"

class QPropertyTree;

class InplaceWidgetComboBox : public QObject, public property_tree::InplaceWidget
{
	Q_OBJECT
public:
	InplaceWidgetComboBox(ComboBoxClientRow* client, QPropertyTree* tree)
	: InplaceWidget(tree)
	, comboBox_(new QComboBox(tree))
	, client_(client)
	, tree_(tree)
	{
		std::vector<std::string> values;
		int selectedIndex = 0;
		client_->populateComboBox(&values, &selectedIndex, tree_);
		for (size_t i = 0; i < values.size(); ++i)
			comboBox_->addItem(values[i].c_str());
		comboBox_->setCurrentIndex(selectedIndex);
		connect(comboBox_, SIGNAL(activated(int)), this, SLOT(onChange(int)));
	}

	void showPopup() override
	{
		// We could use QComboBox::showPopup() here, but in this case some of
		// the Qt themes (i.e. Fusion) are closing the combobox with following
		// mouse relase because internal timer wasn't fired during the mouse
		// click. We work around this by sending real click to the combo box.
		QSize size = comboBox_->size();
		QPoint centerPoint = QPoint(size.width() * 0.5f, size.height() * 0.5f);
		QMouseEvent ev(QMouseEvent::MouseButtonPress, centerPoint, comboBox_->mapToGlobal(centerPoint), Qt::LeftButton, Qt::LeftButton, Qt::KeyboardModifiers());
		QApplication::sendEvent(comboBox_, &ev);
	}

	~InplaceWidgetComboBox()
	{
		if (comboBox_) {
			comboBox_->hide();
			comboBox_->setParent(0);
			comboBox_->deleteLater();
			comboBox_ = 0;
		}
	}	

	void commit() override{}
	void* actualWidget() override{ return comboBox_; }
public slots:
	void onChange(int)
	{
		if (!client_->onComboBoxSelected(comboBox_->currentText().toUtf8().data(), tree_))
			tree_->_cancelWidget();
	}
protected:
	QComboBox* comboBox_;
	ComboBoxClientRow* client_;
	QPropertyTree* tree_;
};


