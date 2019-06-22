// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "LevelEditor/LevelSettings.h"

#include "CryEditDoc.h"
#include "EnvironmentPresetsWidget.h"
#include "FileDialogs/SystemFileDialog.h"
#include "GameEngine.h"
#include "QT/QtMainFrame.h"
#include "RecursionLoopGuard.h"

#include <IUndoObject.h>
#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>
#include <Util/MFCUtil.h>

#include <QDir>
#include <QVBoxLayout>

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

private:
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
				}
			}

			void* pUserData = (void*)(UINT_PTR)((i << 16) | k);

			if (!stricmp(type, "int"))
			{
				CSmartVariable<int> intVar;
				CAutoSupressUpdateCallback<int> updateSupressor(intVar);
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
				CAutoSupressUpdateCallback<float> updateSupressor(floatVar);
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
				CAutoSupressUpdateCallback<Vec3> updateSupressor(vec3Var);
				AddVariable(group, vec3Var, child->getTag(), child->getTag(), strDescription, func, pUserData);
				Vec3 vValue(0, 0, 0);
				if (child->getAttr("value", vValue))
					vec3Var->Set(vValue);
			}
			else if (!stricmp(type, "bool"))
			{
				CSmartVariable<bool> bVar;
				CAutoSupressUpdateCallback<bool> updateSupressor(bVar);
				AddVariable(group, bVar, child->getTag(), child->getTag(), strDescription, func, pUserData);
				bool bValue = false;
				if (child->getAttr("value", bValue))
					bVar->Set(bValue);
			}
			else if (!stricmp(type, "texture"))
			{
				CSmartVariable<string> textureVar;
				CAutoSupressUpdateCallback<string> updateSupressor(textureVar);
				AddVariable(group, textureVar, child->getTag(), child->getTag(), strDescription, func, pUserData, IVariable::DT_TEXTURE);
				const char* textureName;
				if (child->getAttr("value", &textureName))
					textureVar->Set(textureName);
			}
			else if (!stricmp(type, "material"))
			{
				CSmartVariable<string> materialVar;
				CAutoSupressUpdateCallback<string> updateSupressor(materialVar);
				AddVariable(group, materialVar, child->getTag(), child->getTag(), strDescription, func, pUserData, IVariable::DT_MATERIAL);
				const char* materialName;
				if (child->getAttr("value", &materialName))
					materialVar->Set(materialName);
			}
			else if (!stricmp(type, "color"))
			{
				CSmartVariable<Vec3> colorVar;
				CAutoSupressUpdateCallback<Vec3> updateSupressor(colorVar);
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

class CSettingsWidget : public QWidget, public ISystemEventListener
{
public:
	CSettingsWidget(QWidget* pParent = nullptr)
		: QWidget(pParent)
		, m_pPropertyTree(new QPropertyTreeLegacy(this))
	{
		connect(m_pPropertyTree, &QPropertyTreeLegacy::signalAboutToSerialize, this, &CSettingsWidget::BeforeSerialization);
		connect(m_pPropertyTree, &QPropertyTreeLegacy::signalSerialized, this, &CSettingsWidget::AfterSerialization);
		connect(m_pPropertyTree, &QPropertyTreeLegacy::signalBeginUndo, this, &CSettingsWidget::OnBeginUndo);
		connect(m_pPropertyTree, &QPropertyTreeLegacy::signalEndUndo, this, &CSettingsWidget::OnEndUndo);


		setAttribute(Qt::WA_DeleteOnClose);

		m_pPropertyTree->setExpandLevels(2);
		m_pPropertyTree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
		ResetPropertyTree();
		setLayout(new QVBoxLayout());
		layout()->setContentsMargins(0, 0, 0, 0);
		layout()->addWidget(m_pPropertyTree);

		GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CLevelSettingsEditor");
	}

	virtual ~CSettingsWidget()
	{
		GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
	}

	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override
	{
		if (ESYSTEM_EVENT_ENVIRONMENT_SETTINGS_CHANGED == event)
		{
			ResetPropertyTree();
		}
	}

private:
	void OnBeginUndo()
	{
		GetIEditor()->GetIUndoManager()->Begin();
		GetIEditor()->GetIUndoManager()->RecordUndo(new Private_LevelSettings::CUndoLevelSettings());
	}

	void OnEndUndo(bool acceptUndo)
	{
		QString name(m_pPropertyTree->selectedRow()->label());
		name = "Modify " + name;
		if (acceptUndo)
		{
			GetIEditor()->GetIUndoManager()->Accept(name.toStdString().c_str());
		}
		else
		{
			GetIEditor()->GetIUndoManager()->Cancel();
		}
	}

	void BeforeSerialization(Serialization::IArchive& ar)
	{
		m_bIgnoreEvent = true;
	}

	void AfterSerialization(Serialization::IArchive& ar)
	{
		m_bIgnoreEvent = false;
	}

	void ResetPropertyTree()
	{
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

private:
	CVarBlockPtr   m_varBlock;
	QPropertyTreeLegacy* m_pPropertyTree = nullptr;
	bool           m_bIgnoreEvent = false;
};

REGISTER_VIEWPANE_FACTORY_AND_MENU(CLevelSettingsEditor, "Level Settings", "Tools", true, "Level Editor")
}

CLevelSettingsEditor::CLevelSettingsEditor(QWidget* parent)
	: CDockableEditor(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	RegisterActions();
}

void CLevelSettingsEditor::Initialize()
{
	CDockableEditor::Initialize();

	RegisterDockingWidgets();
	CreateMenu();
}

void CLevelSettingsEditor::RegisterActions()
{
	RegisterAction("general.import", &Private_LevelSettings::PyLoadLevelSettings);
	SetActionText("general.import", "Import Settings...");

	RegisterAction("general.export", &Private_LevelSettings::PySaveLevelSettings);
	SetActionText("general.export", "Export Settings...");
}

void CLevelSettingsEditor::CreateMenu()
{
	AddToMenu(CEditor::MenuItems::FileMenu);

	auto fileMenu = GetMenu("File");
	AddToMenu(fileMenu, "general.import");
	AddToMenu(fileMenu, "general.export");
}

void CLevelSettingsEditor::RegisterDockingWidgets()
{
	using namespace Private_LevelSettings;

	EnableDockingSystem();
	RegisterDockableWidget("Settings", [] { return new CSettingsWidget(); }, true, false);
	RegisterDockableWidget("Environment Presets", [] { return new CEnvironmentPresetsWidget(); }, true, false);
}

void CLevelSettingsEditor::CreateDefaultLayout(CDockableContainer* pSender)
{
	CRY_ASSERT(pSender);
	pSender->SpawnWidget("Settings");
	pSender->SpawnWidget("Environment Presets");
}
