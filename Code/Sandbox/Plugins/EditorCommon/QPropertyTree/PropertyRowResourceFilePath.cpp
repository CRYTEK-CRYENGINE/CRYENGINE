// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PropertyRowResourceFilePath.h"
#include <CrySerialization/ClassFactory.h>
#include "Serialization/PropertyTree/IMenu.h"
#include <IEditor.h>
#include <QMenu>
#include "FileDialogs/EngineFileDialog.h"

ResourceFilePathMenuHandler::ResourceFilePathMenuHandler(PropertyTree* tree, PropertyRowResourceFilePath* self)
	: self(self), tree(tree)
{
}

void ResourceFilePathMenuHandler::onMenuClear()
{
	tree->model()->rowAboutToBeChanged(self);
	self->clear();
	tree->model()->rowChanged(self);
}

bool PropertyRowResourceFilePath::onActivateButton(int buttonIndex, const PropertyActivationEvent& e)
{
	if (userReadOnly())
		return false;
	if (!GetIEditor())
		return true;

	CEngineFileDialog::RunParams runParams;
	runParams.initialDir = startFolder_.c_str();
	runParams.initialFile = valueAsString();
	runParams.extensionFilters = CExtensionFilter::Parse(filter_.c_str());
	auto qFilename = CEngineFileDialog::RunGameOpen(runParams, e.tree->ui()->qwidget());
	string filename = qFilename.toLocal8Bit().data();

	//string filename = GetIEditor()->SelectFile(filter_.c_str(), startFolder_.c_str(), path_.c_str());
	if (filename.empty())
		return true;
	if (flags_ & ResourceFilePath::STRIP_EXTENSION)
	{
		size_t ext = filename.rfind('.');
		if (ext != filename.npos)
			filename.erase(ext, filename.length() - ext);
	}

	e.tree->model()->rowAboutToBeChanged(this);
	setValue(filename, searchHandle(), typeId());
	e.tree->model()->rowChanged(this);
	return true;
}

void PropertyRowResourceFilePath::setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar)
{
	ResourceFilePath* value = (ResourceFilePath*)ser.pointer();
	filter_ = value->filter.c_str();
	setValue(value->path->c_str(), searchHandle(), typeId());
	flags_ = value->flags;
	handle_ = value->path;
}

bool PropertyRowResourceFilePath::assignTo(const Serialization::SStruct& ser) const
{
	((ResourceFilePath*)ser.pointer())->SetPath(valueAsString());
	return true;
}

void PropertyRowResourceFilePath::serializeValue(Serialization::IArchive& ar)
{
	ar(filter_, "filter");

	string path = valueAsString();
	ar(path, "path");
	setValue(path, searchHandle(), typeId());

	ar(startFolder_, "startFolder");
}

Icon PropertyRowResourceFilePath::buttonIcon(const PropertyTree* tree, int index) const
{
	return Icon("icons:General/Folder.ico");
}

void PropertyRowResourceFilePath::clear()
{
	setValue("", searchHandle(), typeId());
}

bool PropertyRowResourceFilePath::onContextMenu(property_tree::IMenu& menu, PropertyTree* tree)
{
	yasli::SharedPtr<PropertyRow> selfPointer(this);

	ResourceFilePathMenuHandler* handler = new ResourceFilePathMenuHandler(tree, this);
	menu.addAction("Clear", userReadOnly() ? property_tree::MENU_DISABLED : 0, handler, &ResourceFilePathMenuHandler::onMenuClear);
	tree->addMenuHandler(handler);
	return true;
}

REGISTER_PROPERTY_ROW(ResourceFilePath, PropertyRowResourceFilePath);
DECLARE_SEGMENT(PropertyRowResourceFilePath)

