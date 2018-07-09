// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExportTool.h"
#include "DesignerEditor.h"
#include "CryEdit.h"
#include "GameEngine.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "FileDialogs/SystemFileDialog.h"
#include "Export/ExportManager.h"
#include "Objects/ObjectLoader.h"

#include <QFileDialog>

using Serialization::ActionButton;

namespace Designer
{
void ExportTool::ExportToCgf()
{
	std::vector<MainContext> selections;
	GetSelectedObjectList(selections);

	if (selections.size() == 1)
	{
		CSystemFileDialog::RunParams runParams;
		runParams.title = QFileDialog::tr("Export CGF");
		runParams.initialDir = CFileUtil::FormatInitialFolderForFileDialog(GetIEditor()->GetGameEngine()->GetLevelPath().GetString()).GetString();
		runParams.extensionFilters << CExtensionFilter(QFileDialog::tr("CRYENGINE Geometry Files"), "cgf");

		const QString filePath = CSystemFileDialog::RunExportFile(runParams, nullptr);
		if (!filePath.isEmpty())
			selections[0].pCompiler->SaveToCgf(filePath.toUtf8().constData());

	}
	else
	{
		MessageBox("Warning", "Only one object must be selected to save it to cgf file.");
	}
}

void ExportTool::ExportToGrp()
{
	CSystemFileDialog::RunParams runParams;
	runParams.title = QFileDialog::tr("Export CGF");
	runParams.initialDir = CFileUtil::FormatInitialFolderForFileDialog(GetIEditor()->GetGameEngine()->GetLevelPath().GetString()).GetString();
	runParams.extensionFilters << CExtensionFilter(QFileDialog::tr("Object Group Files"), "grp");

	const QString filePath = CSystemFileDialog::RunExportFile(runParams, nullptr);
	if (filePath.isEmpty())
	{
		return;
	}
	const ISelectionGroup* sel = GetIEditor()->GetISelectionGroup();
	XmlNodeRef root = XmlHelpers::CreateXmlNode("Objects");
	CObjectArchive ar(GetIEditor()->GetObjectManager(), root, false);
	for (int i = 0; i < sel->GetCount(); i++)
		ar.SaveObject(sel->GetObject(i));

	XmlHelpers::SaveXmlNode(root, filePath.toUtf8().constData());
}

void ExportTool::ExportToObj()
{
	string filename = "untitled";
	CBaseObject* pObj = GetIEditor()->GetSelectedObject();
	if (pObj)
	{
		filename = pObj->GetName();
	}
	else
	{
		string levelName = GetIEditor()->GetGameEngine()->GetLevelName();
		if (!levelName.IsEmpty())
			filename = levelName;
	}
	string levelPath = GetIEditor()->GetGameEngine()->GetLevelPath();

	IExportManager* pExportManager = GetIEditor()->GetExportManager();
	pExportManager->Export(filename, "obj", levelPath);
}

void ExportTool::Serialize(Serialization::IArchive& ar)
{
	ar(ActionButton([this] { ExportToObj();
	                }), "OBJ", ".OBJ");
	ar(ActionButton([this] { ExportToCgf();
	                }), "CGF", ".CGF");
	ar(ActionButton([this] { ExportToGrp();
	                }), "GRP", ".GRP");
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Export, eToolGroup_Misc, "Export", ExportTool,
                                                           export, "runs export tool", "designer.export")

