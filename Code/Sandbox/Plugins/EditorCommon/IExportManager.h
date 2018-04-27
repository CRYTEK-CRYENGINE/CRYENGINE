// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Export/ExportDataTypes.h>

struct IStatObj;

// IExporter: interface to present an exporter
// Exporter is responding to export data from object of IData type
// to file with specified format
// Exporter could be provided by user through plug-in system
struct IExporter
{
	// Get file extension of exporter type, f.i. "obj"
	virtual const char* GetExtension() const = 0;

	// Get short format description for showing it in FileSave dialog
	// Example: "Object format"
	virtual const char* GetShortDescription() const = 0;

	// Implementation of en exporting data to the file
	virtual bool ExportToFile(const char* filename, const SExportData* pData) = 0;
	virtual bool ImportFromFile(const char* filename, SExportData* pData) = 0;

	// Before Export Manager is destroyed Release will be called
	virtual void Release() = 0;
};

// IExportManager: interface to export manager
struct IExportManager
{
	//! Register exporter
	//! return true if succeed, otherwise false
	virtual bool       RegisterExporter(IExporter* pExporter) = 0;

	virtual IExporter* GetExporterForExtension(const char* szExtension) const = 0;

	virtual bool       ExportSingleStatObj(IStatObj* pStatObj, const char* filename) = 0;

	//! Export specified geometry
	//! return true if succeed, otherwise false
	virtual bool Export(const char* defaultName, const char* defaultExt = "", const char* defaultPath = "", bool isSelectedObjects = true,
		bool isSelectedRegionObjects = false, bool isTerrain = false, bool isOccluder = false, bool bAnimationExport = false) = 0;

};

