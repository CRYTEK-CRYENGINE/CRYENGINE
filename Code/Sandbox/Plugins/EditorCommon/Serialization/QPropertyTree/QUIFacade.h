// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <memory>
#include "Serialization/PropertyTree/IDrawContext.h"
#include "Serialization/PropertyTree/IUIFacade.h"
#include "Serialization/PropertyTree/IMenu.h"
#include <QMenu>
#include "QPropertyTree.h"
#include <CrySerialization/yasli/Config.h>

#ifdef emit
#undef emit
#endif

class QPropertyTree;

namespace property_tree {

class QtAction : public QObject, public IMenuAction
{
	Q_OBJECT
public:
	QtAction(QAction* action)
	{
		connect(action, SIGNAL(triggered()), this, SLOT(onTriggered()));
	}
public slots:
	void onTriggered() { signalTriggered.emit(); }
private:
	QAction* action_;
};

class QPropertyTreeMenu : public QMenu
{
public:
	QPropertyTreeMenu(QWidget* parent) : QMenu(parent) {}

protected:
	virtual void leaveEvent(QEvent* e) override
	{
		// fix for Qt 5.6. bug, every mouse move calls leaveEvent, that sets the activeAction to nullptr
		// and on mouseRelease event no action is chosen, so before calling the original implementation
		// store the activeAction and sets it back
		QAction* pAction = activeAction();
		QMenu::leaveEvent(e);
		setActiveAction(pAction);
	}
};


class QtMenu : public IMenu
{
public:
	QtMenu(QPropertyTreeMenu* menu, QPropertyTree* tree, const char* text)
	: menu_(menu)
	, tree_(tree)
	, text_(text)
	{
	}

	~QtMenu()
	{
		// actions are freed with the QMenu
		for (size_t i = 0; i < menus_.size(); ++i)
			delete menus_[i];
	}

	bool isEmpty() override { return !menu_.get() || menu_->isEmpty(); }
	IMenu* addMenu(const char* text) override{ 
		QPropertyTreeMenu* qmenu = new QPropertyTreeMenu(menu_.get());
		qmenu->setTitle(text);
		menu_->addMenu(qmenu);
		menus_.push_back(new QtMenu(qmenu, tree_, text));
		return menus_.back();
	}
	IMenu* findMenu(const char* text) override{
		for (size_t i = 0; i < menus_.size(); ++i)
			if (menus_[i]->text() == text)
				return menus_[i];
		return 0;
	}
	void addSeparator() override{ menu_->addSeparator(); }
	QIcon nativeIcon(const Icon& icon)
	{
		return QIcon();
	}
	IMenuAction* addAction(const Icon& icon, const char* text, int flags = 0) override{
		QAction* qaction = menu_->addAction(text);
		qaction->setIcon(nativeIcon(icon));
		if (flags & MENU_DISABLED)
		{
			qaction->setEnabled(false);
		}
		QtAction* action = new QtAction(qaction);
		actions_.push_back(action);
		return action;
	}
	
	void exec(const Point& point) override{
		Point widgetPoint = tree_->_toWidget(point);
		menu_->exec(tree_->mapToGlobal(QPoint(widgetPoint.x(), widgetPoint.y())));
	}
	const yasli::string& text() const{ return text_; }
private:
	std::vector<QtAction*> actions_;
	std::vector<QtMenu*> menus_;
	std::auto_ptr<QMenu> menu_;
	yasli::string text_;
	QPropertyTree* tree_;
};

class QUIFacade : public IUIFacade
{
public:
	QUIFacade(QPropertyTree* tree) : tree_(tree) {}

	QWidget* qwidget() override;
	HWND hwnd() override;

	IMenu* createMenu() override;
	void setCursor(Cursor cursor) override;
	void unsetCursor() override;
	Point cursorPosition() override;

	Point screenSize() override;
	int textWidth(const char* text, Font font) override;
	int textHeight(int width, const char* text, Font font) override;

	InplaceWidget* createComboBox(ComboBoxClientRow* client) override;
	InplaceWidget* createNumberWidget(PropertyRowNumberField* row) override;
	InplaceWidget* createStringWidget(PropertyRowString* row) override;
private:
	QPropertyTree* tree_;
};

}

