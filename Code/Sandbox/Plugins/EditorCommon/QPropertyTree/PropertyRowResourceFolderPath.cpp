// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <Serialization/Decorators/IconXPM.h>
#include "Serialization/PropertyTree/IMenu.h"
#include "PropertyRowResourceFolderPath.h"
#include <CrySerialization/ClassFactory.h>
#include <IEditor.h>
#include <CrySystem/File/ICryPak.h>
#include <QMenu>
#include <shlobj.h>

#include "FileDialogs/EngineFileDialog.h"

ResourceFolderPathMenuHandler::ResourceFolderPathMenuHandler(PropertyTree* tree, PropertyRowResourceFolderPath* self)
	: self(self), tree(tree)
{
}

void ResourceFolderPathMenuHandler::onMenuClear()
{
	tree->model()->rowAboutToBeChanged(self);
	self->clear();
	tree->model()->rowChanged(self);
}

bool PropertyRowResourceFolderPath::onActivate(const PropertyActivationEvent& e)
{
	if (e.reason == e.REASON_RELEASE)
		return false;
	if (!GetIEditor())
		return true;

	if (userReadOnly())
		return false;

	QString title;
	if (labelUndecorated() && labelUndecorated()[0] != '\0')
	{
		title = QString("Choose folder for '%1'").arg(QString::fromLocal8Bit(labelUndecorated()));
	}
	CEngineFileDialog::RunParams runParams;
	runParams.title = title;
	runParams.initialFile = QString::fromLocal8Bit(path_.c_str());

	QString result = CEngineFileDialog::RunGameSelectDirectory(runParams, static_cast<QPropertyTree*>(e.tree));
	if (result.isEmpty())
	{
		return true;
	}
	e.tree->model()->rowAboutToBeChanged(this);
	path_ = result.toLocal8Bit().data();
	e.tree->model()->rowChanged(this);
	return true;
}
void PropertyRowResourceFolderPath::setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar)
{
	ResourceFolderPath* value = (ResourceFolderPath*)(ser.pointer());
	path_ = value->path->c_str();
	startFolder_ = value->startFolder.c_str();
	handle_ = value->path;
}

bool PropertyRowResourceFolderPath::assignTo(const Serialization::SStruct& ser) const
{
	((ResourceFolderPath*)ser.pointer())->SetPath(path_.c_str());
	return true;
}

property_tree::Icon PropertyRowResourceFolderPath::buttonIcon(const PropertyTree* tree, int index) const
{
#include "Serialization/PropertyTree/file_open.xpm"
	return property_tree::Icon(yasli::IconXPM(file_open_xpm));
}

string PropertyRowResourceFolderPath::valueAsString() const
{
	return path_;
}

void PropertyRowResourceFolderPath::clear()
{
	path_.clear();
}

void PropertyRowResourceFolderPath::serializeValue(Serialization::IArchive& ar)
{
	ar(path_, "path");
	ar(startFolder_, "startFolder");
}

bool PropertyRowResourceFolderPath::onContextMenu(IMenu& menu, PropertyTree* tree)
{
	ResourceFolderPathMenuHandler* handler = new ResourceFolderPathMenuHandler(tree, this);
	menu.addAction("Clear", userReadOnly() ? BUTTON_DISABLED : 0, handler, &ResourceFolderPathMenuHandler::onMenuClear);
	yasli::SharedPtr<PropertyRow> selfPointer(this);

	tree->addMenuHandler(handler);
	return true;
}

REGISTER_PROPERTY_ROW(ResourceFolderPath, PropertyRowResourceFolderPath);
DECLARE_SEGMENT(PropertyRowResourceFolderPath)

