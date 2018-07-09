// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "VegetationEraseTool.h"

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CVegetationEraseTool_ClassDesc : public IClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.VegetationErase"; };

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char*    Category()        { return "Terrain"; };

	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVegetationEraseTool); }
};

REGISTER_CLASS_DESC(CVegetationEraseTool_ClassDesc);

IMPLEMENT_DYNCREATE(CVegetationEraseTool, CVegetationPaintTool)

//////////////////////////////////////////////////////////////////////////
CVegetationEraseTool::CVegetationEraseTool()
{
	// set base class member to activate erase mode
	m_eraseMode = true;
	m_isModeSwitchAllowed = false;
	m_isEraseTool = true;
}

QEditToolButtonPanel::SButtonInfo CVegetationEraseTool::CreateEraseToolButtonInfo()
{
	QEditToolButtonPanel::SButtonInfo eraseToolButtonInfo;
	eraseToolButtonInfo.name = string(QObject::tr("Erase").toStdString().c_str()); // string copies contents of assignment operand
	eraseToolButtonInfo.toolTip = string(QObject::tr("Erases all vegetation objects selected in the tree").toStdString().c_str());
	eraseToolButtonInfo.pToolClass = RUNTIME_CLASS(CVegetationEraseTool);
	eraseToolButtonInfo.icon = "icons:Vegetation/Vegetation_Erase.ico";
	return eraseToolButtonInfo;
}

