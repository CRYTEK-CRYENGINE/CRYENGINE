// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "LevelEditor/LevelSettings.h"

#include <QMenuBar>
#include <QLayout>
#include <QDir>

#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <Preferences/LightingPreferences.h>
#include <QtUtil.h>
#include <CrySerialization/IArchiveHost.h>
#include "GameEngine.h"
#include "Mission.h"
#include "FileDialogs/SystemFileDialog.h"
#include "QT/QtMainFrame.h"
#include "RecursionLoopGuard.h"
#include "CryEditDoc.h"
#include "Util/MFCUtil.h"

namespace Private_LevelSettings
{

class CUndoLevelSettings : public IUndoObject
{
public:
	CUndoLevelSettings()
	{
		m_old_settings = XmlHelpers::CreateXmlNode("Environment");
		CXmlTemplate::SetValues(GetIEditorImpl()->GetDocument()->GetEnvironmentTemplate(), m_old_settings);
	}

	virtual ~CUndoLevelSettings() {}

protected:
	void Undo(bool bUndo)
	{
		if (bUndo && !m_new_settings)
		{
			m_new_settings = XmlHelpers::CreateXmlNode("Environment");
			CXmlTemplate::SetValues(GetIEditorImpl()->GetDocument()->GetEnvironmentTemplate(), m_new_settings);
		}

		CXmlTemplate::GetValues(GetIEditorImpl()->GetDocument()->GetEnvironmentTemplate(), m_old_settings);
		GetIEditorImpl()->GetGameEngine()->ReloadEnvironment();
	}

	void Redo()
	{
		CXmlTemplate::GetValues(GetIEditorImpl()->GetDocument()->GetEnvironmentTemplate(), m_new_settings);
		GetIEditorImpl()->GetGameEngine()->ReloadEnvironment();
	}

	const char* GetDescription() { return "Modify Level Settings"; }

private:

	XmlNodeRef m_new_settings;
	XmlNodeRef m_old_settings;
};

void PyLoadLevelSettings()
{
	if (!GetIEditorImpl()->GetDocument() || !GetIEditorImpl()->GetDocument()->IsDocumentReady())
		return;

	QDir dir(GetIEditorImpl()->GetLevelDataFolder().GetBuffer());

	CSystemFileDialog::RunParams runParams;
	runParams.initialDir = dir.absolutePath();
	runParams.title = CEditorMainFrame::tr("Open Level Settings");
	runParams.extensionFilters << CExtensionFilter(CEditorMainFrame::tr("XML files"), "xml");

	const QString path = CSystemFileDialog::RunImportFile(runParams, CEditorMainFrame::GetInstance());
	if (!path.isEmpty())
	{
		XmlNodeRef environment = XmlHelpers::LoadXmlFromFile(path.toUtf8().constData());
		CXmlTemplate::GetValues(GetIEditorImpl()->GetDocument()->GetEnvironmentTemplate(), environment);
		GetIEditorImpl()->GetGameEngine()->ReloadEnvironment();
	}
}

void PySaveLevelSettings()
{
	if (!GetIEditorImpl()->GetDocument() || !GetIEditorImpl()->GetDocument()->IsDocumentReady())
		return;

	QDir dir(GetIEditorImpl()->GetLevelDataFolder().GetBuffer());
	CSystemFileDialog::RunParams runParams;
	runParams.title = CEditorMainFrame::tr("Save Level Settings");
	runParams.initialDir = dir.absolutePath();
	runParams.extensionFilters << CExtensionFilter(CEditorMainFrame::tr("XML files"), "xml");

	const QString path = CSystemFileDialog::RunExportFile(runParams, CEditorMainFrame::GetInstance());
	if (!path.isEmpty())
	{
		XmlNodeRef environment = XmlHelpers::CreateXmlNode("Environment");
		CXmlTemplate::SetValues(GetIEditorImpl()->GetDocument()->GetEnvironmentTemplate(), environment);
		environment->saveToFile(path.toUtf8().constData());
	}
}

// this XML->IVariables automatic generation is obsolete and will be replaced by yasli serialization soon
// do not use it on other places, use yasli serialization
void AddVariable(CVariableBase& varArray, CVariableBase& var, const char* varName, const char* humanVarName, const char* description, IVariable::OnSetCallback func, void* pUserData, char dataType = IVariable::DT_SIMPLE)
{
	if (varName)
		var.SetName(varName);

	if (humanVarName)
		var.SetHumanName(humanVarName);

	if (description)
		var.SetDescription(description);
	var.SetDataType(dataType);
	var.SetUserData(pUserData);
	var.AddOnSetCallback(func);
	varArray.AddVariable(&var);
}

void CreateItems(XmlNodeRef& node, CVarBlockPtr& outBlockPtr, IVariable::OnSetCallback func)
{
	outBlockPtr = new CVarBlock;
	for (int i = 0, iGroupCount(node->getChildCount()); i < iGroupCount; ++i)
	{
		XmlNodeRef groupNode = node->getChild(i);
		CSmartVariableArray group;

		if (groupNode->getTag())
			group->SetName(groupNode->getTag());
		if (groupNode->getTag())
			group->SetHumanName(groupNode->getTag());

		group->SetDescription("");
		group->SetDataType(IVariable::DT_SIMPLE);
		outBlockPtr->AddVariable(&*group);

		for (int k = 0, iChildCount(groupNode->getChildCount()); k < iChildCount; ++k)
		{
			XmlNodeRef child = groupNode->getChild(k);

			const char* type;
			if (!child->getAttr("type", &type))
				continue;

			// read parameter description from the tip tag and from associated console variable
			string strDescription;
			child->getAttr("tip", strDescription);
			string strTipCVar;
			child->getAttr("TipCVar", strTipCVar);
			if (!strTipCVar.IsEmpty())
			{
				strTipCVar.Replace("*", child->getTag());
				if (ICVar* pCVar = gEnv->pConsole->GetCVar(strTipCVar))
				{
					if (!strDescription.IsEmpty())
						strDescription += string("\r\n");
					strDescription = pCVar->GetHelp();

#ifdef FEATURE_SVO_GI
					// Hide or unlock experimental items
					if ((pCVar->GetFlags() & VF_EXPERIMENTAL) && !gLightingPreferences.bTotalIlluminationEnabled && strstr(groupNode->getTag(), "Total_Illumination"))
						continue;
#endif
				}
			}

			void* pUserData = (void*)(UINT_PTR)((i << 16) | k);

			if (!stricmp(type, "int"))
			{
				CSmartVariable<int> intVar;
				AddVariable(group, intVar, child->getTag(), child->getTag(), strDescription, func, pUserData);
				int nValue(0);
				if (child->getAttr("value", nValue))
					intVar->Set(nValue);

				int nMin(0), nMax(0);
				if (child->getAttr("min", nMin) && child->getAttr("max", nMax))
					intVar->SetLimits(nMin, nMax);
			}
			else if (!stricmp(type, "float"))
			{
				CSmartVariable<float> floatVar;
				AddVariable(group, floatVar, child->getTag(), child->getTag(), strDescription, func, pUserData);
				float fValue(0.0f);
				if (child->getAttr("value", fValue))
					floatVar->Set(fValue);

				float fMin(0), fMax(0);
				if (child->getAttr("min", fMin) && child->getAttr("max", fMax))
					floatVar->SetLimits(fMin, fMax);
			}
			else if (!stricmp(type, "vector"))
			{
				CSmartVariable<Vec3> vec3Var;
				AddVariable(group, vec3Var, child->getTag(), child->getTag(), strDescription, func, pUserData);
				Vec3 vValue(0, 0, 0);
				if (child->getAttr("value", vValue))
					vec3Var->Set(vValue);
			}
			else if (!stricmp(type, "bool"))
			{
				CSmartVariable<bool> bVar;
				AddVariable(group, bVar, child->getTag(), child->getTag(), strDescription, func, pUserData);
				bool bValue(false);
				if (child->getAttr("value", bValue))
					bVar->Set(bValue);
			}
			else if (!stricmp(type, "texture"))
			{
				CSmartVariable<string> textureVar;
				AddVariable(group, textureVar, child->getTag(), child->getTag(), strDescription, func, pUserData, IVariable::DT_TEXTURE);
				const char* textureName;
				if (child->getAttr("value", &textureName))
					textureVar->Set(textureName);
			}
			else if (!stricmp(type, "material"))
			{
				CSmartVariable<string> materialVar;
				AddVariable(group, materialVar, child->getTag(), child->getTag(), strDescription, func, pUserData, IVariable::DT_MATERIAL);
				const char* materialName;
				if (child->getAttr("value", &materialName))
					materialVar->Set(materialName);
			}
			else if (!stricmp(type, "color"))
			{
				CSmartVariable<Vec3> colorVar;
				AddVariable(group, colorVar, child->getTag(), child->getTag(), strDescription, func, pUserData, IVariable::DT_COLOR);
				ColorB color;
				if (child->getAttr("value", color))
				{
					ColorF colorLinear = CMFCUtils::ColorGammaToLinear(RGB(color.r, color.g, color.b));
					Vec3 colorVec3(colorLinear.r, colorLinear.g, colorLinear.b);
					colorVar->Set(colorVec3);
				}
			}
		}
	}
}

REGISTER_VIEWPANE_FACTORY_AND_MENU(CLevelSettingsEditor, "Level Settings", "Tools", true, "Level Editor")

// Level Settings commands
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyLoadLevelSettings, general, import_level_settings,
                                     "Loads and sets the global level settings.",
                                     "general.import_level_settings()");
REGISTER_EDITOR_COMMAND_TEXT(general, import_level_settings, "Import Settings...");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySaveLevelSettings, general, export_level_settings,
                                     "Saves the global level settings.",
                                     "general.export_level_settings()");
REGISTER_EDITOR_COMMAND_TEXT(general, export_level_settings, "Export Settings...");

};

CLevelSettingsEditor::CLevelSettingsEditor(QWidget* parent)
	: CDockableEditor(parent)
	, m_pPropertyTree(nullptr)
	, m_bIgnoreEvent(false)
{
	setAttribute(Qt::WA_DeleteOnClose);
	InitMenu();
	m_pPropertyTree = new QPropertyTree(this);

	connect(m_pPropertyTree, &QPropertyTree::signalAboutToSerialize, this, &CLevelSettingsEditor::BeforeSerialization);
	connect(m_pPropertyTree, &QPropertyTree::signalSerialized, this, &CLevelSettingsEditor::AfterSerialization);
	connect(m_pPropertyTree, &QPropertyTree::signalPushUndo, this, &CLevelSettingsEditor::PushUndo);

	m_pPropertyTree->setExpandLevels(2);
	m_pPropertyTree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	ReloadFromTemplate();
	layout()->addWidget(m_pPropertyTree);

	if (GetISystem()->GetISystemEventDispatcher())
		GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CLevelSettingsEditor");
}

CLevelSettingsEditor::~CLevelSettingsEditor()
{
	if (GetISystem()->GetISystemEventDispatcher())
		GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
}

void CLevelSettingsEditor::InitMenu()
{
	AddToMenu(CEditor::MenuItems::FileMenu);

	auto fileMenu = GetMenu("File");
	AddToMenu(fileMenu, "general.import_level_settings");
	AddToMenu(fileMenu, "general.export_level_settings");
}

void CLevelSettingsEditor::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	if (ESYSTEM_EVENT_ENVIRONMENT_SETTINGS_CHANGED == event)
		ReloadFromTemplate();
}

void CLevelSettingsEditor::PushUndo()
{
	QString name(m_pPropertyTree->selectedRow()->label());
	name = "Modify " + name;
	CUndo undo(name.toUtf8().constData());
	CUndo::Record(new Private_LevelSettings::CUndoLevelSettings());
}

void CLevelSettingsEditor::BeforeSerialization(Serialization::IArchive& ar)
{
	m_bIgnoreEvent = true;
}

void CLevelSettingsEditor::AfterSerialization(Serialization::IArchive& ar)
{
	m_bIgnoreEvent = false;
}

void CLevelSettingsEditor::ReloadFromTemplate()
{
	if (!m_pPropertyTree)
		return;

	RECURSION_GUARD(m_bIgnoreEvent)

	m_pPropertyTree->detach();

	if (!GetIEditorImpl()->GetDocument())
		return;

	XmlNodeRef node = GetIEditorImpl()->GetDocument()->GetEnvironmentTemplate();
	Private_LevelSettings::CreateItems(node, m_varBlock, functor(*GetIEditorImpl()->GetDocument(), &CCryEditDoc::OnEnvironmentPropertyChanged));
	Serialization::SStructs structs;
	structs.push_back(Serialization::SStruct(*m_varBlock));
	m_pPropertyTree->attach(structs);
}

