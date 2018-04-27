/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "PropertyRowFileOpen.h"
#include <CrySerialization/yasli/ClassFactory.h>
#include "Serialization/PropertyTree/IDrawContext.h"
#include "Serialization/PropertyTree/PropertyRowImpl.h"
#include "Serialization/PropertyTree/PropertyTreeModel.h"
#include "Serialization/PropertyTree/Serialization.h"
#include "Serialization/PropertyTree/PropertyTree.h"
#include "Serialization/PropertyTree/IUIFacade.h"
#include "Serialization/PropertyTree/IMenu.h"
#include <CrySerialization/yasli/decorators/FileOpen.h>
#include <CrySerialization/yasli/decorators/IconXPM.h>
#include <QFileDialog>
#include <QIcon>

static const char* getExtension(const char* path)
{
	const char* ext = strrchr(path, '.');
	if (!ext)
		return path + strlen(path);
	return ext;
}

static yasli::string removeExtension(const char* filename)
{
	const char* ext = getExtension(filename);
	return yasli::string(filename, ext);
}

yasli::string extractExtensionFromFilter(const char* mask)
{
	const char* start = strchr(mask, '(');
	const char* end = strchr(mask, ')');
	if (!start || !end)
		return yasli::string();
	++start;
	const char* ext = strchr(mask, '.');
	if (!ext)
		return yasli::string();
	if (ext > end)
		return yasli::string();
	++ext;
	const char* ext_end = ext;
	while (*ext_end && *ext_end != ' ' && *ext_end != ')' && ext_end < end) {
		if (*ext_end == '*' || *ext_end == '?')
			return yasli::string();
		++ext_end;
	}
	if (ext_end != end)
		++ext_end;
	return yasli::string(ext, ext_end);
}

using yasli::FileOpen;

class PropertyRowFileOpen : public PropertyRowImpl<FileOpen>{
public:

	bool isLeaf() const override{ return true; }

	bool onActivate(const PropertyActivationEvent& e) override
	{
		QFileDialog dialog(e.tree->ui()->qwidget());
		dialog.setAcceptMode(QFileDialog::AcceptOpen);
		dialog.setFileMode(QFileDialog::ExistingFile);
		if (labelUndecorated())
			dialog.setWindowTitle(QString("Choose file for '") + labelUndecorated() + "'");		
		
		QDir directory(value().relativeToFolder.c_str());
		dialog.setNameFilter(value().filter.c_str());
		
		QString existingFile = value().path.c_str();
        if (!QDir::isAbsolutePath(existingFile)) {
            existingFile = directory.currentPath();
            if (!value().relativeToFolder.empty()) {
                existingFile += QDir::separator();
                existingFile += value().relativeToFolder.c_str();
            }
            existingFile += QDir::separator();
            existingFile += value().path.c_str();
        }
        if (value().flags & FileOpen::STRIP_EXTENSION)
            existingFile += QString(extractExtensionFromFilter(value().filter.c_str()).c_str());

		if (!QFile::exists(existingFile))
			dialog.setDirectory(directory);
		else
            dialog.selectFile(existingFile);

		if (dialog.exec() && !dialog.selectedFiles().isEmpty()) {
			e.tree->model()->rowAboutToBeChanged(this);
			QString filename = dialog.selectedFiles()[0];
			QString relativeFilename = directory.relativeFilePath(filename);
			value().path = relativeFilename.toLocal8Bit().data();
			if (value().flags & FileOpen::STRIP_EXTENSION)
				value().path = removeExtension(value().path.c_str());
			e.tree->model()->rowChanged(this);
		}
		return true;
	}

	void clear(PropertyTree* tree)
	{
		tree->model()->rowAboutToBeChanged(this);
		value().path.clear();
		tree->model()->rowChanged(this);
	}

	bool onContextMenu(IMenu &menu, PropertyTree* tree) override
	{
		FileOpenMenuHandler* handler = new FileOpenMenuHandler(this, tree);
		tree->addMenuHandler(handler);

		menu.addAction("Choose File...", MENU_DEFAULT, handler, &FileOpenMenuHandler::onMenuActivate);
		menu.addAction("Clear", 0, handler, &FileOpenMenuHandler::onMenuClear);

		return PropertyRow::onContextMenu(menu, tree);
	}

	int buttonCount() const override{ return 1; }
	virtual property_tree::Icon buttonIcon(const PropertyTree* tree, int index) const override{ 
		#include "Serialization/PropertyTree/file_open.xpm"
		return property_tree::Icon(yasli::IconXPM(file_open_xpm));
	}
	virtual bool usePathEllipsis() const override{ return true; }

	yasli::string valueAsString() const override{ return value().path; }
};

REGISTER_PROPERTY_ROW(FileOpen, PropertyRowFileOpen); 
DECLARE_SEGMENT(PropertyRowFileOpen)

// ---------------------------------------------------------------------------

void FileOpenMenuHandler::onMenuActivate()
{
	PropertyActivationEvent e;
	e.reason = e.REASON_CONTEXT_MENU;
	e.tree = tree;
	self->onActivate(e);
}

void FileOpenMenuHandler::onMenuClear()
{
	self->clear(tree);
}


