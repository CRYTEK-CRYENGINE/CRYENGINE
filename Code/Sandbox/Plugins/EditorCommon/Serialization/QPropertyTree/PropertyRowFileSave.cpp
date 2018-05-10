/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include <CrySerialization/yasli/ClassFactory.h>

#include "Serialization/PropertyTree/IDrawContext.h"
#include "Serialization/PropertyTree/PropertyRowImpl.h"
#include "Serialization/PropertyTree/PropertyTree.h"
#include "Serialization/PropertyTree/IUIFacade.h"
#include "Serialization/PropertyTree/PropertyTreeModel.h"
#include "Serialization/PropertyTree/Serialization.h"
#include <CrySerialization/yasli/decorators/FileSave.h>
#include <CrySerialization/yasli/decorators/IconXPM.h>
#include <QFileDialog>
#include <QIcon>

#include "PropertyRowFileOpen.h"

using yasli::FileSave;

class PropertyRowFileSave : public PropertyRowImpl<FileSave>{
public:

	bool isLeaf() const override{ return true; }

	bool onActivate(const PropertyActivationEvent& e) override
	{
		QFileDialog dialog(e.tree->ui()->qwidget());
		dialog.setAcceptMode(QFileDialog::AcceptSave);
		dialog.setFileMode(QFileDialog::AnyFile);
		dialog.setDefaultSuffix(extractExtensionFromFilter(value().filter.c_str()).c_str());
		if (labelUndecorated())
			dialog.setWindowTitle(QString("Choose file for '") + labelUndecorated() + "'");		
		
		QDir directory(value().relativeToFolder.c_str());
		dialog.setNameFilter(value().filter.c_str());
		
		QString existingFile = value().path.c_str();
		if (!QDir::isAbsolutePath(existingFile))
			existingFile = directory.currentPath() + QDir::separator() + existingFile;

		if (!QFile::exists(existingFile))
			dialog.setDirectory(directory);
		else
			dialog.selectFile(QString(value().path.c_str()));

		if (dialog.exec() && !dialog.selectedFiles().isEmpty()) {
			e.tree->model()->rowAboutToBeChanged(this);
			QString filename = dialog.selectedFiles()[0];
			QString relativeFilename = directory.relativeFilePath(filename);
			value().path = relativeFilename.toLocal8Bit().data();
			e.tree->model()->rowChanged(this);
		}
		return true;
	}


	int buttonCount() const override{ return 1; }
	property_tree::Icon buttonIcon(const PropertyTree* tree, int index) const override{ 
		#include "Serialization/PropertyTree/file_save.xpm"
		return property_tree::Icon(yasli::IconXPM(file_save_xpm));
	}
	virtual bool usePathEllipsis() const override{ return true; }

	yasli::string valueAsString() const override{ return value().path; }
};

REGISTER_PROPERTY_ROW(FileSave, PropertyRowFileSave); 
DECLARE_SEGMENT(PropertyRowFileSave)

