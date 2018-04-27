// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PropertyRowOutputFilePath.h"
#include "Serialization/PropertyTree/IMenu.h"
#include <CrySerialization/ClassFactory.h>
#include <Serialization/Decorators/IconXPM.h>
#ifndef SERIALIZATION_STANDALONE
	#include <IEditor.h>
	#include <CrySystem/File/ICryPak.h>
#endif
#include "FileDialogs/EngineFileDialog.h"
#include <QMenu>

OutputFilePathMenuHandler::OutputFilePathMenuHandler(PropertyTree* tree, PropertyRowOutputFilePath* self)
	: self(self), tree(tree)
{
}

void OutputFilePathMenuHandler::onMenuClear()
{
	tree->model()->rowAboutToBeChanged(self);
	self->clear();
	tree->model()->rowChanged(self);
}

QString convertMFCToQtFileFilter(QString* defaultSuffix, const char* mfcFilter)
{
	// convert filter from "All Files|*.*|Text files|*.txt||"
	//         format into "All files (*.*);;Text Files (*.txt)"
	QString filterMFC = QString::fromLocal8Bit(mfcFilter);
	QStringList filterItems = filterMFC.split("|");

	if (defaultSuffix && filterItems.size() > 1)
	{
		QString extensions = filterItems[1];
		QRegExp re("\\*\\.(\\w*)");
		if (extensions.indexOf(re) >= 0)
			*defaultSuffix = re.cap(1);
	}

	QString filter;
	for (int i = 0; i < int(filterItems.size()) / 2; ++i)
	{
		int bracketPos = filterItems[i].indexOf('(');
		QString desc = bracketPos >= 0 ? filterItems[i].left(bracketPos) : filterItems[i];
		int extIndex = i * 2 + 1;
		if (extIndex >= filterItems.size())
			break;
		if (!filter.isEmpty())
			filter += ";;";
		filter += desc;
		filter += " (";
		filter += filterItems[extIndex];
		filter += ")";
	}

	return filter;
}

bool PropertyRowOutputFilePath::onActivate(const PropertyActivationEvent& e)
{
	if (e.reason == e.REASON_RELEASE)
		return false;
#ifndef SERIALIZATION_STANDALONE
	if (!GetIEditor())
		return true;
#endif

	QString title;
	if (labelUndecorated())
		title = QString("Choose file for '%1'").arg(labelUndecorated());
	else
		title = "Choose file";

	CEngineFileDialog::RunParams runParams;
	runParams.title = title;
	runParams.initialFile = QString::fromLocal8Bit(path_.c_str());
	runParams.initialDir = QString::fromLocal8Bit(startFolder_.c_str());
	runParams.extensionFilters = CExtensionFilter::Parse(filter_.c_str());
	QString result = CEngineFileDialog::RunGameSave(runParams, static_cast<QPropertyTree*>(e.tree));
	if (!result.isEmpty())
	{
		e.tree->model()->rowAboutToBeChanged(this);
		path_ = result.toLocal8Bit().data();
		e.tree->model()->rowChanged(this);
	}
	return true;
}
void PropertyRowOutputFilePath::setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar)
{
	OutputFilePath* value = (OutputFilePath*)ser.pointer();
	path_ = value->path->c_str();
	filter_ = value->filter.c_str();
	startFolder_ = value->startFolder.c_str();
	handle_ = value->path;
}

bool PropertyRowOutputFilePath::assignTo(const Serialization::SStruct& ser) const
{
	((OutputFilePath*)ser.pointer())->SetPath(path_.c_str());
	return true;
}

Icon PropertyRowOutputFilePath::buttonIcon(const PropertyTree* tree, int index) const
{
#include "Serialization/PropertyTree/file_save.xpm"
	return Icon(yasli::IconXPM(file_save_xpm));
}

string PropertyRowOutputFilePath::valueAsString() const
{
	return path_;
}

void PropertyRowOutputFilePath::clear()
{
	path_.clear();
}

bool PropertyRowOutputFilePath::onContextMenu(IMenu& menu, PropertyTree* tree)
{
	yasli::SharedPtr<PropertyRow> selfPointer(this);

	OutputFilePathMenuHandler* handler = new OutputFilePathMenuHandler(tree, this);
	menu.addAction("Clear", "", 0, handler, &OutputFilePathMenuHandler::onMenuClear);
	return true;
}

void PropertyRowOutputFilePath::serializeValue(Serialization::IArchive& ar)
{
	ar(path_, "path");
	ar(filter_, "filter");
	ar(startFolder_, "startFolder");
}

REGISTER_PROPERTY_ROW(OutputFilePath, PropertyRowOutputFilePath);
DECLARE_SEGMENT(PropertyRowOutputFilePath)

