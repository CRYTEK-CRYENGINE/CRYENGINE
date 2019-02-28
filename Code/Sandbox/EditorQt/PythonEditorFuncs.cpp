// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IEditorImpl.h"
#include "GameEngine.h"
#include "ViewManager.h"
#include <Preferences/ViewportPreferences.h>
#include "Util/BoostPythonHelpers.h"
#include "Dialogs/QStringDialog.h"
#include "Dialogs/ToolbarDialog.h"
#include "Dialogs/GenericSelectItemDialog.h"
#include "Util/Ruler.h"
#include "Util/Mailer.h"
#include "FileDialogs/SystemFileDialog.h"
#include "Controls/QuestionDialog.h"
#include "FilePathUtil.h"
#include "CryEditDoc.h"

namespace
{
//////////////////////////////////////////////////////////////////////////
const char* PyGetCVar(const char* pName)
{
	ICVar* pCVar = GetIEditorImpl()->GetSystem()->GetIConsole()->GetCVar(pName);
	if (!pCVar)
	{
		Warning("PyGetCVar: Attempt to access non-existent CVar '%s'", pName ? pName : "(null)");
		throw std::logic_error(string("\"") + pName + "\" is an invalid cvar.");
	}
	return pCVar->GetString();
}
//////////////////////////////////////////////////////////////////////////
void PySetCVar(const char* pName, pSPyWrappedProperty pValue)
{
	ICVar* pCVar = GetIEditorImpl()->GetSystem()->GetIConsole()->GetCVar(pName);
	if (!pCVar)
	{
		Warning("PySetCVar: Attempt to access non-existent CVar '%s'", pName ? pName : "(null)");
		throw std::logic_error(string("\"") + pName + " is an invalid cvar.");
	}

	if (pCVar->GetType() == CVAR_INT && pValue->type == SPyWrappedProperty::eType_Int)
	{
		pCVar->Set(pValue->property.intValue);
	}
	else if (pCVar->GetType() == CVAR_FLOAT && pValue->type == SPyWrappedProperty::eType_Float)
	{
		pCVar->Set(pValue->property.floatValue);
	}
	else if (pCVar->GetType() == CVAR_STRING && pValue->type == SPyWrappedProperty::eType_String)
	{
		pCVar->Set((LPCTSTR)pValue->stringValue);
	}
	else
	{
		Warning("PyGetCVar: Type mismatch while assigning CVar '%s'", pName ? pName : "(null)");
		throw std::logic_error("Invalid data type.");
	}

	CryLog("PySetCVar: %s set to %s", pName, pCVar->GetString());
}

//////////////////////////////////////////////////////////////////////////

void PySuspendGameInput()
{
	if (GetIEditorImpl()->GetGameEngine() && GetIEditorImpl()->IsInGameMode())
		GetIEditorImpl()->GetGameEngine()->ToggleGameInputSuspended();
}

void PyEnterGameMode()
{
	if (GetIEditorImpl()->GetGameEngine())
		GetIEditorImpl()->GetGameEngine()->RequestSetGameMode(true);
}

void PyExitGameMode()
{
	if (GetIEditorImpl()->GetGameEngine())
		GetIEditorImpl()->GetGameEngine()->RequestSetGameMode(false);
}

bool PyIsInGameMode()
{
	return GetIEditorImpl()->IsInGameMode();
}

//////////////////////////////////////////////////////////////////////////
const char* PyNewObject(const char* typeName, const char* fileName, const char* name, float x, float y, float z)
{
	CBaseObject* pObject = GetIEditorImpl()->NewObject(typeName, fileName);
	if (pObject)
	{
		if (name && strlen(name) > 0)
			pObject->SetName(name);
		pObject->SetPos(Vec3(x, y, z));
		return pObject->GetName().GetString();
	}
	else
		return "";
}

pPyGameObject PyCreateObject(const char* typeName, const char* fileName, const char* name, float x, float y, float z)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->NewObject(typeName, 0, fileName);

	if (pObject != NULL)
	{
		GetIEditorImpl()->SetModifiedFlag();

		if (strcmp(name, "") != 0)
			pObject->SetUniqName(name);

		pObject->SetPos(Vec3(x, y, z));
	}

	return PyScript::CreatePyGameObject(pObject);
}

//////////////////////////////////////////////////////////////////////////
const char* PyNewObjectAtCursor(const char* typeName, const char* fileName, const char* name)
{
	CUndo undo("Create new object");

	Vec3 pos(0, 0, 0);

	CPoint p;
	GetCursorPos(&p);
	CViewport* viewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint(p);
	if (viewport)
	{
		viewport->ScreenToClient(&p);
		pos = viewport->MapViewToCP(p, true, 1.f);
	}

	return PyNewObject(typeName, fileName, name, pos.x, pos.y, pos.z);
}

//////////////////////////////////////////////////////////////////////////
void PyStartObjectCreation(const char* typeName, const char* fileName)
{
	CUndo undo("Create new object");
	GetIEditorImpl()->StartObjectCreation(typeName, fileName);
}

//////////////////////////////////////////////////////////////////////////
void PyRunConsole(const char* text)
{
	GetIEditorImpl()->GetSystem()->GetIConsole()->ExecuteString(text);
}

//////////////////////////////////////////////////////////////////////////
void PyRunLua(const char* text)
{
	GetIEditorImpl()->GetSystem()->GetIScriptSystem()->ExecuteBuffer(text, strlen(text));
}

//////////////////////////////////////////////////////////////////////////
bool GetPythonScriptPath(const char* pFile, string& path)
{
	bool bRelativePath = true;
	char drive[_MAX_DRIVE];
	drive[0] = '\0';
	_splitpath(pFile, drive, 0, 0, 0);
	if (strlen(drive) != 0)
	{
		bRelativePath = false;
	}

	char workingDirectory[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, workingDirectory);
	string scriptFolder = workingDirectory + string("/");
	scriptFolder += GetIEditorImpl()->GetSystem()->GetIPak()->GetGameFolder() + string("/");
	PathUtil::ToUnixPath(scriptFolder.GetBuffer());

	if (bRelativePath)
	{
		// Try to open from user folder
		path = string(PathUtil::GetUserSandboxFolder().c_str()) + pFile;

		// If not found try editor folder
		if (!CFileUtil::FileExists(path))
		{
			path = scriptFolder + pFile;
		}
	}
	else
	{
		path = pFile;
	}

	PathUtil::ToUnixPath(path.GetBuffer());

	if (!CFileUtil::FileExists(path))
	{
		const string userSandboxFolder = string(PathUtil::GetUserSandboxFolder().c_str());
		string error = string("Could not find '") + pFile + "'\n in '" + userSandboxFolder + "'\n or '" + scriptFolder + "'\n";
		PyScript::PrintError(error);
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void GetPythonArgumentsVector(const char* pArguments, std::vector<string>& inputArguments)
{
	if (pArguments == NULL)
		return;

	string str(pArguments);
	int pos = 0;
	string token = str.Tokenize(" ", pos);
	while (!token.IsEmpty())
	{
		inputArguments.push_back(token);
		token = str.Tokenize(" ", pos);
	}
}

//////////////////////////////////////////////////////////////////////////
void PyRunFileWithParameters(const char* pFile, const char* pArguments)
{
	string path;
	std::vector<string> inputArguments;
	GetPythonArgumentsVector(pArguments, inputArguments);
	if (GetPythonScriptPath(pFile, path))
	{
		PyObject* pyFileObject = PyFile_FromString(const_cast<char*>(path.GetString()), "r");

		if (pyFileObject)
		{
			std::vector<const char*> argv;
			argv.reserve(inputArguments.size() + 1);
			argv.push_back(path.GetString());

			for (auto iter = inputArguments.begin(); iter != inputArguments.end(); ++iter)
			{
				argv.push_back(*iter);
			}

			PySys_SetArgv(argv.size(), const_cast<char**>(&argv[0]));
			PyRun_SimpleFile(PyFile_AsFile(pyFileObject), path);
			PyErr_Print();
		}
		Py_DECREF(pyFileObject);
	}
}

void PyRunFile(const char* pFile)
{
	PyRunFileWithParameters(pFile, nullptr);
}

//////////////////////////////////////////////////////////////////////////
void PyExecuteCommand(const char* cmdline)
{
	GetIEditorImpl()->GetCommandManager()->Execute(cmdline);
}

//////////////////////////////////////////////////////////////////////////
void PyLog(const char* pMessage)
{
	if (strcmp(pMessage, "") != 0)
		CryLogAlways(pMessage);
}

//////////////////////////////////////////////////////////////////////////
bool PyMessageBox(const char* pMessage)
{
	return QDialogButtonBox::StandardButton::Ok == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(pMessage), QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel);
}

bool PyMessageBoxYesNo(const char* pMessage)
{
	return QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(pMessage), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No);
}

bool PyMessageBoxOK(const char* pMessage)
{
	return QDialogButtonBox::StandardButton::Ok == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(pMessage), QDialogButtonBox::StandardButton::Ok);
}

string PyEditBox(const char* pTitle)
{
	QStringDialog stringDialog(pTitle);
	if (stringDialog.exec() == QDialog::Accepted)
	{
		return stringDialog.GetString().GetString();
	}
	return "";
}

SPyWrappedProperty PyEditBoxAndCheckSPyWrappedProperty(const char* pTitle)
{
	QStringDialog stringDialog(pTitle);
	SPyWrappedProperty value;
	stringDialog.SetString("");
	bool isValidDataType(false);

	while (!isValidDataType && stringDialog.exec() == QDialog::Accepted)
	{
		// detect data type
		string tempString = stringDialog.GetString();
		int countComa = 0;
		int countOpenRoundBraket = 0;
		int countCloseRoundBraket = 0;
		int countDots = 0;

		int posDots = 0;
		int posComa = 0;
		int posOpenRoundBraket = 0;
		int posCloseRoundBraket = 0;

		for (int i = 0; i < 3; i++)
		{
			if (tempString.Find(".", posDots) > -1)
			{
				posDots = tempString.Find(".", posDots) + 1;
				countDots++;
			}
			if (tempString.Find(",", posComa) > -1)
			{
				posComa = tempString.Find(",", posComa) + 1;
				countComa++;
			}
			if (tempString.Find("(", posOpenRoundBraket) > -1)
			{
				posOpenRoundBraket = tempString.Find("(", posOpenRoundBraket) + 1;
				countOpenRoundBraket++;
			}
			if (tempString.Find(")", posCloseRoundBraket) > -1)
			{
				posCloseRoundBraket = tempString.Find(")", posCloseRoundBraket) + 1;
				countCloseRoundBraket++;
			}
		}

		if (countDots == 3 && countComa == 2 && countOpenRoundBraket == 1 && countCloseRoundBraket == 1)
		{
			value.type = SPyWrappedProperty::eType_Vec3;
		}
		else if (countDots == 0 && countComa == 2 && countOpenRoundBraket == 1 && countCloseRoundBraket == 1)
		{
			value.type = SPyWrappedProperty::eType_Color;
		}
		else if (countDots == 1 && countComa == 0 && countOpenRoundBraket == 0 && countCloseRoundBraket == 0)
		{
			value.type = SPyWrappedProperty::eType_Float;
		}
		else if (countDots == 0 && countComa == 0 && countOpenRoundBraket == 0 && countCloseRoundBraket == 0)
		{
			if (stringDialog.GetString() == "False" || stringDialog.GetString() == "True")
			{
				value.type = SPyWrappedProperty::eType_Bool;
			}
			else
			{
				bool isString(false);

				if (stringDialog.GetString().IsEmpty())
				{
					isString = true;
				}

				char tempString[255];
				cry_strcpy(tempString, stringDialog.GetString());

				for (int i = 0; i < stringDialog.GetString().GetLength(); i++)
				{
					if (!isdigit(tempString[i]))
					{
						isString = true;
					}
				}

				if (isString)
				{
					value.type = SPyWrappedProperty::eType_String;
				}
				else
				{
					value.type = SPyWrappedProperty::eType_Int;
				}
			}
		}

		// initialize value
		if (value.type == SPyWrappedProperty::eType_Vec3)
		{
			string valueRed = stringDialog.GetString();
			int iStart = valueRed.Find("(");
			valueRed.Delete(0, iStart + 1);
			int iEnd = valueRed.Find(",");
			valueRed.Delete(iEnd, valueRed.GetLength());
			float fValueRed = atof(valueRed);

			string valueGreen = stringDialog.GetString();
			iStart = valueGreen.Find(",");
			valueGreen.Delete(0, iStart + 1);
			iEnd = valueGreen.Find(",");
			valueGreen.Delete(iEnd, valueGreen.GetLength());
			float fValueGreen = atof(valueGreen);

			string valueBlue = stringDialog.GetString();
			valueBlue.Delete(0, valueBlue.Find(",") + 1);
			valueBlue.Delete(0, valueBlue.Find(",") + 1);
			valueBlue.Delete(valueBlue.Find(")"), valueBlue.GetLength());
			float fValueBlue = atof(valueBlue);

			value.property.vecValue.x = fValueRed;
			value.property.vecValue.y = fValueGreen;
			value.property.vecValue.z = fValueBlue;
			isValidDataType = true;
		}
		else if (value.type == SPyWrappedProperty::eType_Color)
		{
			string valueRed = stringDialog.GetString();
			int iStart = valueRed.Find("(");
			valueRed.Delete(0, iStart + 1);
			int iEnd = valueRed.Find(",");
			valueRed.Delete(iEnd, valueRed.GetLength());
			int iValueRed = atoi(valueRed);

			string valueGreen = stringDialog.GetString();
			iStart = valueGreen.Find(",");
			valueGreen.Delete(0, iStart + 1);
			iEnd = valueGreen.Find(",");
			valueGreen.Delete(iEnd, valueGreen.GetLength());
			int iValueGreen = atoi(valueGreen);

			string valueBlue = stringDialog.GetString();
			valueBlue.Delete(0, valueBlue.Find(",") + 1);
			valueBlue.Delete(0, valueBlue.Find(",") + 1);
			valueBlue.Delete(valueBlue.Find(")"), valueBlue.GetLength());
			int iValueBlue = atoi(valueBlue);

			value.property.colorValue.r = iValueRed;
			value.property.colorValue.g = iValueGreen;
			value.property.colorValue.b = iValueBlue;
			isValidDataType = true;
		}
		else if (value.type == SPyWrappedProperty::eType_Int)
		{
			value.property.intValue = atoi(stringDialog.GetString());
			isValidDataType = true;
		}
		else if (value.type == SPyWrappedProperty::eType_Float)
		{
			value.property.floatValue = atof(stringDialog.GetString());
			isValidDataType = true;
		}
		else if (value.type == SPyWrappedProperty::eType_String)
		{
			value.stringValue = stringDialog.GetString();
			isValidDataType = true;
		}
		else if (value.type == SPyWrappedProperty::eType_Bool)
		{
			if (stringDialog.GetString() == "True" || stringDialog.GetString() == "False")
			{
				value.property.boolValue = stringDialog.GetString() == "True";
				isValidDataType = true;
			}
		}
		else
		{
			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Invalid data type."));
			isValidDataType = false;
		}
	}
	return value;
}

string PyOpenFileBox()
{
	CSystemFileDialog::RunParams runParams;
	runParams.title = "Select File";
	// runParams.hideReadOnly = true;

	QString file = CSystemFileDialog::RunImportFile(runParams, nullptr);
	return file.toLocal8Bit().data();
}

string PyComboBox(string title, std::vector<string> values, int selectedIdx = 0)
{
	string result;

	if (title.IsEmpty())
	{
		throw std::runtime_error("Incorrect title argument passed in. ");
		return result;
	}

	if (values.size() == 0)
	{
		throw std::runtime_error("Empty value list passed in. ");
		return result;
	}

	std::vector<CString> cvalues;
	for (const string& str : values)
		cvalues.push_back(str.GetString());

	CGenericSelectItemDialog pyDlg(AfxGetMainWnd());
	pyDlg.SetTitle(title.GetString());
	pyDlg.SetMode(CGenericSelectItemDialog::eMODE_LIST);
	pyDlg.SetItems(cvalues);
	pyDlg.PreSelectItem(cvalues[selectedIdx]);

	if (pyDlg.DoModal() == IDOK)
		result = pyDlg.GetSelectedItem().GetString();

	return result;
}

static void PyDrawLabel(int x, int y, float size, float r, float g, float b, float a, const char* pLabel)
{
	if (!pLabel)
	{
		throw std::logic_error("No label given.");
		return;
	}

	if (!r || !g || !b || !a)
	{
		throw std::logic_error("Invalid color parameters given.");
		return;
	}

	if (!x || !y || !size)
	{
		throw std::logic_error("Invalid position or size parameters given.");
	}
	else
	{
		float color[] = { r, g, b, a };
		IRenderAuxText::Draw2dLabel(x, y, size, color, false, pLabel);
	}
}

//////////////////////////////////////////////////////////////////////////
// Constrain
//////////////////////////////////////////////////////////////////////////
const char* PyGetAxisConstraint()
{
	AxisConstrains actualConstrain = GetIEditorImpl()->GetAxisConstrains();
	switch (actualConstrain)
	{
	case AXIS_X:
		return "X";
	case AXIS_Y:
		return "Y";
	case AXIS_Z:
		return "Z";
	case AXIS_XY:
		return "XY";
	case AXIS_XZ:
		return "XZ";
	case AXIS_YZ:
		return "YZ";
	case AXIS_XYZ:
		return "XYZ";
	default:
		throw std::logic_error("Invalid axes.");
	}
}

void PySetAxisConstraint(string pConstrain)
{
	if (pConstrain == "X")
	{
		GetIEditorImpl()->SetAxisConstrains(AXIS_X);
	}
	else if (pConstrain == "Y")
	{
		GetIEditorImpl()->SetAxisConstrains(AXIS_Y);
	}
	else if (pConstrain == "Z")
	{
		GetIEditorImpl()->SetAxisConstrains(AXIS_Z);
	}
	else if (pConstrain == "XY")
	{
		GetIEditorImpl()->SetAxisConstrains(AXIS_XY);
	}
	else if (pConstrain == "YZ")
	{
		GetIEditorImpl()->SetAxisConstrains(AXIS_YZ);
	}
	else if (pConstrain == "XZ")
	{
		GetIEditorImpl()->SetAxisConstrains(AXIS_XZ);
	}
	else if (pConstrain == "XYZ")
	{
		GetIEditorImpl()->SetAxisConstrains(AXIS_XYZ);
	}
	else
	{
		throw std::logic_error("Invalid axes.");
	}
}
//////////////////////////////////////////////////////////////////////////
// Edit Mode
//////////////////////////////////////////////////////////////////////////
const char* PyGetEditMode()
{
	int actualEditMode = GetIEditorImpl()->GetEditMode();
	switch (actualEditMode)
	{
	case eEditModeSelect:
		return "SELECT";
	case eEditModeSelectArea:
		return "SELECTAREA";
	case eEditModeMove:
		return "MOVE";
	case eEditModeRotate:
		return "ROTATE";
	case eEditModeScale:
		return "SCALE";
	case eEditModeTool:
		return "TOOL";
	default:
		throw std::logic_error("Invalid edit mode.");
	}
}

void PySetEditMode(string pEditMode)
{
	if (pEditMode == "MOVE")
	{
		GetIEditorImpl()->SetEditMode(eEditModeMove);
	}
	else if (pEditMode == "ROTATE")
	{
		GetIEditorImpl()->SetEditMode(eEditModeRotate);
	}
	else if (pEditMode == "SCALE")
	{
		GetIEditorImpl()->SetEditMode(eEditModeScale);
	}
	else if (pEditMode == "SELECT")
	{
		GetIEditorImpl()->SetEditMode(eEditModeSelect);
	}
	else if (pEditMode == "SELECTAREA")
	{
		GetIEditorImpl()->SetEditMode(eEditModeSelectArea);
	}
	else if (pEditMode == "TOOL")
	{
		GetIEditorImpl()->SetEditMode(eEditModeTool);
	}
	else if (pEditMode == "RULER")
	{
		CRuler* pRuler = GetIEditorImpl()->GetRuler();
		pRuler->SetActive(!pRuler->IsActive());
	}
	else
	{
		throw std::logic_error("Invalid edit mode.");
	}
}

//////////////////////////////////////////////////////////////////////////
const char* PyGetPakFromFile(const char* filename)
{
	ICryPak* pIPak = GetIEditorImpl()->GetSystem()->GetIPak();
	FILE* pFile = pIPak->FOpen(filename, "rb");
	if (!pFile)
	{
		throw std::logic_error("Invalid file name.");
	}
	const char* pArchPath = pIPak->GetFileArchivePath(pFile);
	pIPak->FClose(pFile);
	return pArchPath;
}

//////////////////////////////////////////////////////////////////////////
// HideMask controls
//////////////////////////////////////////////////////////////////////////
void PySetHideMaskAll()
{
	gViewportDebugPreferences.SetObjectHideMask(OBJTYPE_ANY);
}

//////////////////////////////////////////////////////////////////////////
void PySetHideMaskNone()
{
	gViewportDebugPreferences.SetObjectHideMask(0);
}

//////////////////////////////////////////////////////////////////////////
void PySetHideMaskInvert()
{
	uint32 hideMask = gViewportDebugPreferences.GetObjectHideMask();
	gViewportDebugPreferences.SetObjectHideMask(~hideMask);
}

void PySetHideMask(const char* pName, bool bAdd)
{
	uint32 hideMask = gViewportDebugPreferences.GetObjectHideMask();
	int hideType = 0;

	if (!stricmp(pName, "aipoints"))   hideType = OBJTYPE_AIPOINT;
	else if (!stricmp(pName, "brushes"))
		hideType = OBJTYPE_BRUSH;
	else if (!stricmp(pName, "decals"))
		hideType = OBJTYPE_DECAL;
	else if (!stricmp(pName, "entities"))
		hideType = OBJTYPE_ENTITY;
	else if (!stricmp(pName, "groups"))
		hideType = OBJTYPE_GROUP;
	else if (!stricmp(pName, "prefabs"))
		hideType = OBJTYPE_PREFAB;
	else if (!stricmp(pName, "other"))
		hideType = OBJTYPE_OTHER;
	else if (!stricmp(pName, "shapes"))
		hideType = OBJTYPE_SHAPE;
	else if (!stricmp(pName, "solids"))
		hideType = OBJTYPE_SOLID;
	else if (!stricmp(pName, "tagpoints"))
		hideType = OBJTYPE_TAGPOINT;
	else if (!stricmp(pName, "volumes"))
		hideType = OBJTYPE_VOLUME;
	else if (!stricmp(pName, "geomcaches"))
		hideType = OBJTYPE_GEOMCACHE;
	else if (!stricmp(pName, "roads"))
		hideType = OBJTYPE_ROAD;
	else if (!stricmp(pName, "rivers"))
		hideType = OBJTYPE_ROAD;

	if (bAdd)
	{
		hideMask |= hideType;
	}
	else
	{
		hideMask &= ~hideType;
	}

	gViewportDebugPreferences.SetObjectHideMask(hideMask);
}

bool PyGetHideMask(const char* pName)
{
	uint32 hideMask = gViewportDebugPreferences.GetObjectHideMask();

	if ((!stricmp(pName, "aipoints") && (hideMask & OBJTYPE_AIPOINT)) ||
	    (!stricmp(pName, "brushes") && (hideMask & OBJTYPE_BRUSH)) ||
	    (!stricmp(pName, "decals") && (hideMask & OBJTYPE_DECAL)) ||
	    (!stricmp(pName, "entities") && (hideMask & OBJTYPE_ENTITY)) ||
	    (!stricmp(pName, "groups") && (hideMask & OBJTYPE_GROUP)) ||
	    (!stricmp(pName, "prefabs") && (hideMask & OBJTYPE_PREFAB)) ||
	    (!stricmp(pName, "other") && (hideMask & OBJTYPE_OTHER)) ||
	    (!stricmp(pName, "shapes") && (hideMask & OBJTYPE_SHAPE)) ||
	    (!stricmp(pName, "solids") && (hideMask & OBJTYPE_SOLID)) ||
	    (!stricmp(pName, "tagpoints") && (hideMask & OBJTYPE_TAGPOINT)) ||
	    (!stricmp(pName, "volumes") && (hideMask & OBJTYPE_VOLUME)) ||
	    (!stricmp(pName, "geomcaches") && (hideMask & OBJTYPE_GEOMCACHE)) ||
	    (!stricmp(pName, "roads") && (hideMask & OBJTYPE_ROAD)) ||
	    (!stricmp(pName, "rivers") && (hideMask & OBJTYPE_ROAD)))    return true;

	return false;
}

void PySetSelectionMask(int mask)
{
	GetIEditorImpl()->GetObjectManager()->SetSelectionMask(mask);
}

void PySetCoordSys(int value)
{
	GetIEditorImpl()->SetReferenceCoordSys((RefCoordSys)value);
}

void PySetViewCoords()
{
	GetIEditorImpl()->SetReferenceCoordSys(COORDS_VIEW);
}

void PySetLocalCoords()
{
	GetIEditorImpl()->SetReferenceCoordSys(COORDS_LOCAL);
}

void PySetParentCoords()
{
	GetIEditorImpl()->SetReferenceCoordSys(COORDS_PARENT);
}

void PySetWorldCoords()
{
	GetIEditorImpl()->SetReferenceCoordSys(COORDS_WORLD);
}

void PySetCustomCoords()
{
	GetIEditorImpl()->SetReferenceCoordSys(COORDS_USERDEFINED);
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetCVar, general, get_cvar,
                                     "Gets a cvar value as a string.",
                                     "general.get_cvar(str cvarName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetCVar, general, set_cvar,
                                          "Sets a cvar value from an integer, float or string.",
                                          "general.set_cvar(str cvarName, [int, float, string] cvarValue)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyEnterGameMode, general, enter_game_mode,
                                     "Enters the editor game mode.",
                                     "general.enter_game_mode()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyExitGameMode, general, exit_game_mode,
                                     "Exits the editor game mode.",
                                     "general.exit_game_mode()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyIsInGameMode, general, is_in_game_mode,
                                     "Queries if it's in the game mode or not.",
                                     "general.is_in_game_mode()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyNewObject, general, new_object,
                                          "Creates a new object with given arguments and returns the name of the object.",
                                          "general.new_object(str entityTypeName, str cgfName, str entityName, float xValue, float yValue, float zValue)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyNewObjectAtCursor, general, new_object_at_cursor,
                                     "Creates a new object at a position targeted by cursor and returns the name of the object.",
                                     "general.new_object_at_cursor(str entityTypeName, str cgfName, str entityName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyStartObjectCreation, general, start_object_creation,
                                     "Creates a new object, which is following the cursor.",
                                     "general.start_object_creation(str entityTypeName, str cgfName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyRunConsole, general, run_console,
                                     "Runs a console command.",
                                     "general.run_console(str consoleCommand)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyRunLua, general, run_lua,
                                     "Runs a lua script command.",
                                     "general.run_lua(str luaName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyRunFile, general, run_file,
                                     "Runs a script file. A relative path from the editor user folder or an absolute path should be given as an argument",
                                     "general.run_file(str fileName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyRunFileWithParameters, general, run_file_parameters,
                                     "Runs a script file with parameters. A relative path from the editor user folder or an absolute path should be given as an argument. The arguments should be separated by whitespace.",
                                     "general.run_file_parameters(str fileName, str arguments)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyExecuteCommand, general, execute_command,
                                          "Executes a given string as an editor command.",
                                          "general.execute_command(str editorCommand)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyMessageBox, general, message_box,
                                          "Shows a confirmation message box with ok|cancel and shows a custom message.",
                                          "general.message_box(str message)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyMessageBoxYesNo, general, message_box_yes_no,
                                          "Shows a confirmation message box with yes|no and shows a custom message.",
                                          "general.message_box_yes_no(str message)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyMessageBoxOK, general, message_box_ok,
                                          "Shows a confirmation message box with only ok and shows a custom message.",
                                          "general.message_box_ok(str message)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyEditBox, general, edit_box,
                                          "Shows an edit box and returns the value as string.",
                                          "general.edit_box(str title)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyEditBoxAndCheckSPyWrappedProperty, general, edit_box_check_data_type,
                                          "Shows an edit box and checks the custom value to use the return value with other functions correctly.",
                                          "general.edit_box_check_data_type(str title)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyOpenFileBox, general, open_file_box,
                                          "Shows an open file box and returns the selected file path and name.",
                                          "general.open_file_box()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetAxisConstraint, general, get_axis_constraint,
                                          "Gets axis.",
                                          "general.get_axis_constraint()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetAxisConstraint, general, set_axis_constraint,
                                          "Sets axis.",
                                          "general.set_axis_constraint(str axisName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetEditMode, general, get_edit_mode,
                                          "Gets edit mode",
                                          "general.get_edit_mode()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetEditMode, general, set_edit_mode,
                                          "Sets edit mode",
                                          "general.set_edit_mode(str editModeName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetPakFromFile, general, get_pak_from_file,
                                          "Finds a pak file name for a given file",
                                          "general.get_pak_from_file(str fileName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyLog, general, log,
                                     "Prints the message to the editor console window.",
                                     "general.log(str message)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyDrawLabel, general, draw_label,
                                          "Shows a 2d label on the screen at the given position and given color.",
                                          "general.draw_label(int x, int y, float r, float g, float b, float a, str label)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyCreateObject, general, create_object,
                                          "Creates a new object with given arguments and returns the name of the object.",
                                          "general.create_object(str objectClass, str objectFile, str objectName, (float, float, float) position");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyComboBox, general, combo_box,
                                          "Shows an combo box listing each value passed in, returns string value selected by the user.",
                                          "general.combo_box(str title, [str] values, int selectedIndex)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetHideMaskAll, general, set_hidemask_all,
                                     "Sets the current hidemask to 'all'",
                                     "general.set_hidemask_all()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetHideMaskNone, general, set_hidemask_none,
                                     "Sets the current hidemask to 'none'",
                                     "general.set_hidemask_none()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetHideMaskInvert, general, set_hidemask_invert,
                                     "Inverts the current hidemask",
                                     "general.set_hidemask_invert()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetHideMask, general, set_hidemask,
                                     "Assigns a specified value to a specific object type in the current hidemask",
                                     "general.set_hidemask(str typeName, bool typeValue)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetHideMask, general, get_hidemask,
                                     "Gets the value of a specific object type in the current hidemask",
                                     "general.get_hidemask(str typeName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetSelectionMask, general, set_selection_mask,
                                     "Sets the selection mask for the level editor",
                                     "general.set_selection_mask(int mask)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetCoordSys, general, set_coord_sys,
                                     "Sets the coordinate system to use for level editor",
                                     "general.set_coord_sys(int value)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetViewCoords, general, set_view_coordinates,
                                     "Use view coordinates",
                                     "general.set_view_coordinates()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetLocalCoords, general, set_local_coordinates,
                                     "Use local coordinates",
                                     "general.set_local_coordinates()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetParentCoords, general, set_parent_coordinates,
                                     "Use parent coordinates",
                                     "general.set_parent_coordinates()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetWorldCoords, general, set_world_coordinates,
                                     "Use world coordinates",
                                     "general.set_world_coordinates()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetCustomCoords, general, set_custom_coordinates,
                                     "Use custom coordinates",
                                     "general.set_custom_coordinates()");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySuspendGameInput, general, suspend_game_input,
                                     "Suspend game input and allow editor to tweak properties",
                                     "general.suspend_game_input()");

