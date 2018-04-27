// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DialogManager.h"
#include <CryString/CryPath.h>
#include <CryAudio/IAudioSystem.h>
#include <CryAudio/Dialog/IDialogSystem.h>
#include <ISourceControl.h>
#include <CryString/StringUtils.h>
#include "Util/FileUtil.h"

#define GameWarning       Warning
#define DIALOGSCRIPT_PATH "/Libs/Dialogs/"

namespace
{
static const CEditorDialogScript::TActorID MASK_ALL = ~CEditorDialogScript::TActorID(0);
static const CEditorDialogScript::TActorID MASK_01010101 = MASK_ALL / 3;
static const CEditorDialogScript::TActorID MASK_00110011 = MASK_ALL / 5;
static const CEditorDialogScript::TActorID MASK_00001111 = MASK_ALL / 17;

ILINE void bit_set(CEditorDialogScript::TActorID& n, int which)
{
	n |= (0x01 << which);
}

ILINE void bit_clear(CEditorDialogScript::TActorID& n, int which)
{
	n &= ~(0x01 << which);
}

ILINE bool is_bit_set(CEditorDialogScript::TActorID& n, int which)
{
	return (n & (0x01 << which)) != 0;
}

ILINE int bit_count(CEditorDialogScript::TActorID n)
{
	n = (n & MASK_01010101) + ((n >> 1) & MASK_01010101);
	n = (n & MASK_00110011) + ((n >> 2) & MASK_00110011);
	n = (n & MASK_00001111) + ((n >> 4) & MASK_00001111);
	return n % 255;
}
}

void CEditorDialogScript::SActorSet::SetActor(TActorID id)
{
	bit_set(m_actorBits, id);
}

void CEditorDialogScript::SActorSet::ResetActor(TActorID id)
{
	bit_clear(m_actorBits, id);
}

bool CEditorDialogScript::SActorSet::HasActor(TActorID id)
{
	return is_bit_set(m_actorBits, id);
}

int CEditorDialogScript::SActorSet::NumActors() const
{
	return bit_count(m_actorBits);
}

bool CEditorDialogScript::SActorSet::Matches(const CEditorDialogScript::SActorSet& other) const
{
	return m_actorBits == other.m_actorBits;
}

bool CEditorDialogScript::SActorSet::Satisfies(const CEditorDialogScript::SActorSet& other) const
{
	return (m_actorBits & other.m_actorBits) == other.m_actorBits;
}

////////////////////////////////////////////////////////////////////////////
CEditorDialogScript::CEditorDialogScript(const string& dialogID) : m_id(dialogID), m_reqActorSet(0), m_scFileAttributes(SCC_FILE_ATTRIBUTE_INVALID)
{
}

////////////////////////////////////////////////////////////////////////////
CEditorDialogScript::~CEditorDialogScript()
{
}

////////////////////////////////////////////////////////////////////////////
// Add one line after another
bool CEditorDialogScript::AddLine(const SScriptLine& line)
{
	if (line.m_actor < 0 || line.m_actor >= MAX_ACTORS)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "CEditorDialogScript::AddLine: Script='%s' Cannot add line because actorID %d is out of range [0..%d]", m_id.c_str(), line.m_actor, MAX_ACTORS - 1);
		return false;
	}

	if (line.m_lookatActor != NO_ACTOR_ID && line.m_lookatActor != STICKY_LOOKAT_RESET_ID && (line.m_lookatActor < 0 || line.m_lookatActor >= MAX_ACTORS))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "CEditorDialogScript::AddLine: Script='%s' Cannot add line because lookAtTargetID %d is out of range [0..%d]", m_id.c_str(), line.m_lookatActor, MAX_ACTORS - 1);
		return false;
	}

	m_lines.push_back(line);
	m_reqActorSet.SetActor(line.m_actor);
	if (line.m_lookatActor != NO_ACTOR_ID && line.m_lookatActor != STICKY_LOOKAT_RESET_ID)
		m_reqActorSet.SetActor(line.m_lookatActor);

	return true;
}

////////////////////////////////////////////////////////////////////////////
// Retrieves number of required actors
int CEditorDialogScript::GetNumRequiredActors() const
{
	return m_reqActorSet.NumActors();
}

////////////////////////////////////////////////////////////////////////////
// Get number of dialog lines
int CEditorDialogScript::GetNumLines() const
{
	return m_lines.size();
}

////////////////////////////////////////////////////////////////////////////
// Get a certain line
const CEditorDialogScript::SScriptLine* CEditorDialogScript::GetLine(int index) const
{
	assert(index >= 0 && index < m_lines.size());
	if (index >= 0 && index < m_lines.size())
		return &m_lines[index];
	else return 0;
}

////////////////////////////////////////////////////////////////////////////
CDialogManager::CDialogManager()
{

}

////////////////////////////////////////////////////////////////////////////
CDialogManager::~CDialogManager()
{
	DeleteAllScripts();
	SyncCryActionScripts();
}

////////////////////////////////////////////////////////////////////////////
void CDialogManager::SyncCryActionScripts()
{
	if (GetISystem()->GetIDialogSystem())
		GetISystem()->GetIDialogSystem()->ReloadScripts(NULL);
}

////////////////////////////////////////////////////////////////////////////
void CDialogManager::DeleteAllScripts()
{
	TEditorDialogScriptMap::iterator iter = m_scripts.begin();
	while (iter != m_scripts.end())
	{
		delete iter->second;
		++iter;
	}
	m_scripts.clear();
}

void MakeID(string& path)
{
	path.replace('/', '.');
}

void MakePath(string& id)
{
	id.replace('.', '/');
}

// Game relative path
CString CDialogManager::ScriptToFilename(const CString& id)
{
	CString filename = id;
	filename.Replace('.', '/');
	CString path = PathUtil::GetGameFolder() + DIALOGSCRIPT_PATH;
	path += filename;
	path += ".dlg";
	return path; // Path::GamePathToFullPath(path);
}

CString CDialogManager::FilenameToScript(const CString& id)
{
	CString filename = PathUtil::RemoveExtension(id.GetString()).c_str();
	filename = PathUtil::ToUnixPath(filename.GetString()).c_str();
	CString dataPath = PathUtil::GetGameFolder() + DIALOGSCRIPT_PATH;
	dataPath = PathUtil::ToUnixPath(dataPath.GetString()).c_str();
	if (filename.GetLength() > (dataPath.GetLength()) && strnicmp(filename, dataPath, dataPath.GetLength()) == 0)
	{
		filename = filename.Mid(dataPath.GetLength() + 1); // skip the slash...
	}
	filename.Replace('/', '.');
	return filename;
}

////////////////////////////////////////////////////////////////////////////
bool CDialogManager::ReloadScripts()
{
	DeleteAllScripts();

	string path = PathUtil::GetGameFolder() + DIALOGSCRIPT_PATH;

	std::vector<CFileUtil::FileDesc> files;
	CFileUtil::ScanDirectory(path, "*.dlg", files, true);
	for (int iFile = 0; iFile < files.size(); ++iFile)
	{
		string dlgID = PathUtil::ToUnixPath(files[iFile].filename.GetString());
		PathUtil::RemoveExtension(dlgID);
		MakeID(dlgID);

		string filename = path;
		filename += files[iFile].filename;

		// testing
		CString sID = FilenameToScript(files[iFile].filename.GetString());

		XmlNodeRef node = XmlHelpers::LoadXmlFromFile(filename);
		if (node && node->isTag("DialogScript"))
		{
			CEditorDialogScript* pScript = new CEditorDialogScript(dlgID);
			CEditorDialogScriptSerializer ser(pScript);
			bool bOK = ser.Load(node);
			if (bOK)
			{
				TEditorDialogScriptMap::iterator iter = m_scripts.find(pScript->GetID());
				if (iter != m_scripts.end())
					delete iter->second;
				m_scripts[pScript->GetID()] = pScript; // this overrides previous contents if any
			}
			else
			{
				delete pScript;
			}
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////
bool CDialogManager::SaveAllScripts()
{
	bool ok = true;
	TEditorDialogScriptMap::iterator iter = m_scripts.begin();
	while (iter != m_scripts.end())
	{
		ok &= SaveScript(iter->second, false);
		++iter;
	}
	SyncCryActionScripts();
	return ok;
}

////////////////////////////////////////////////////////////////////////////
CEditorDialogScript* CDialogManager::LoadScript(string& name, bool bReplace)
{
	CEditorDialogScript* pScript = GetScript(name, false);
	if (pScript && !bReplace)
		return pScript;
	if (pScript)
	{
		DeleteScript(pScript, false);
		pScript = 0;
	}

	string id = name;
	MakePath(id);
	string path = PathUtil::GetGameFolder() + DIALOGSCRIPT_PATH;
	path += id;
	path += ".dlg";

	// testing
	CString sPath = ScriptToFilename(CString(name));

	XmlNodeRef node = XmlHelpers::LoadXmlFromFile(path);
	if (node && node->isTag("DialogScript"))
	{
		pScript = new CEditorDialogScript(name);
		CEditorDialogScriptSerializer ser(pScript);
		bool bOK = ser.Load(node);
		if (bOK)
		{
			m_scripts.insert(TEditorDialogScriptMap::value_type(pScript->GetID(), pScript));
		}
		else
		{
			delete pScript;
			pScript = 0;
		}
	}
	return pScript;
}

////////////////////////////////////////////////////////////////////////////
bool CDialogManager::SaveScript(CEditorDialogScript* pScript, bool bSync, bool bBackup)
{
	CEditorDialogScriptSerializer ser(pScript);
	XmlNodeRef node = XmlHelpers::CreateXmlNode("DialogScript");
	bool bOK = ser.Save(node);
	if (bOK)
	{
		string idPath = pScript->GetID();
		MakePath(idPath);
		string path = PathUtil::GetGameFolder() + DIALOGSCRIPT_PATH;
		path += idPath;
		path += ".dlg";
		// testing
		CString sPath = ScriptToFilename(CString(pScript->GetID()));

		// backup file
		if (bBackup)
		{
			CFileUtil::BackupFile(path);
		}

		CFileUtil::CreateDirectory(PathUtil::GetPathWithoutFilename(path));
		bOK = XmlHelpers::SaveXmlNode(node, path);
		if (bSync)
		{
			SyncCryActionScripts();
		}
	}
	return bOK;
}

////////////////////////////////////////////////////////////////////////////
CEditorDialogScript* CDialogManager::GetScript(const string& name, bool bForceCreate)
{
	CEditorDialogScript* pScript = stl::find_in_map(m_scripts, name, 0);
	if (pScript == 0 && bForceCreate)
	{
		pScript = new CEditorDialogScript(name);
		m_scripts.insert(TEditorDialogScriptMap::value_type(name, pScript));
	}
	return pScript;
}

////////////////////////////////////////////////////////////////////////////
CEditorDialogScript* CDialogManager::GetScript(const CString& name, bool bForceCreate)
{
	string sName = name;
	return GetScript(sName, bForceCreate);
}

////////////////////////////////////////////////////////////////////////////
bool CDialogManager::DeleteScript(CEditorDialogScript* pScript, bool bDeleteFromDisk)
{
	TEditorDialogScriptMap::iterator iter = m_scripts.find(pScript->GetID());
	if (iter == m_scripts.end())
		return false;

	assert(pScript == iter->second);

	if (bDeleteFromDisk)
	{
		string idPath = pScript->GetID();
		MakePath(idPath);
		string path = PathUtil::GetGameFolder() + DIALOGSCRIPT_PATH;
		path += idPath;
		path += ".dlg";
		::DeleteFile(path);
	}

	delete pScript;
	m_scripts.erase(iter);

	if (bDeleteFromDisk)
	{
		SyncCryActionScripts();
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////
bool CDialogManager::RenameScript(CEditorDialogScript* pScript, string& newName, bool bDeleteFromDisk)
{
	TEditorDialogScriptMap::iterator iter = m_scripts.find(newName);
	if (iter != m_scripts.end())
		return false;

	// can be renamed...
	iter = m_scripts.find(pScript->GetID());
	assert(pScript == iter->second);

	string idPath = pScript->GetID();
	MakePath(idPath);
	string oldPath = PathUtil::GetGameFolder() + DIALOGSCRIPT_PATH;
	oldPath += idPath;
	oldPath += ".dlg";

	// rename script
	string newID = newName;
	pScript->SetID(newName);

	// save the script in new location
	bool bSaveOK = SaveScript(pScript);
	if (!bSaveOK)
		return false;

	// erase old from map and insert new
	m_scripts.erase(iter);
	m_scripts.insert(TEditorDialogScriptMap::value_type(newName, pScript));

	if (bDeleteFromDisk)
	{
		// delete old file from disk
		::DeleteFile(oldPath);
		SyncCryActionScripts();
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////
uint32 CDialogManager::GetFileAttrs(CEditorDialogScript* pScript, bool bUpdateFromFile)
{
	CString id = pScript->GetID();
	CString filename = ScriptToFilename(id);
	if (bUpdateFromFile || pScript->m_scFileAttributes == SCC_FILE_ATTRIBUTE_INVALID)
	{
		pScript->m_scFileAttributes = CFileUtil::GetAttributes(filename);
	}
	return pScript->m_scFileAttributes;
}

////////////////////////////////////////////////////////////////////////////
bool CDialogManager::CanModify(CEditorDialogScript* pScript)
{
	uint32 scAttr = GetFileAttrs(pScript, true);
	// If read only or in pack, do not save.
	if (scAttr & (SCC_FILE_ATTRIBUTE_READONLY | SCC_FILE_ATTRIBUTE_INPAK))
		return false;

	// Managed file must be checked out or not read-only

	if ((scAttr & SCC_FILE_ATTRIBUTE_MANAGED) && (scAttr & SCC_FILE_ATTRIBUTE_NORMAL) && !(scAttr & SCC_FILE_ATTRIBUTE_READONLY))
		return true;

	if ((scAttr & SCC_FILE_ATTRIBUTE_MANAGED) && !(scAttr & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////
CEditorDialogScriptSerializer::CEditorDialogScriptSerializer(CEditorDialogScript* pScript)
{
	assert(pScript != 0);
	m_pScript = pScript;
}

////////////////////////////////////////////////////////////////////////////
bool CEditorDialogScriptSerializer::Load(XmlNodeRef node)
{
	if (node->isTag("DialogScript") == false)
		return false;

	const char* storedId = node->getAttr("Name");
	if (storedId && stricmp(storedId, m_pScript->m_id.c_str()) == 0)
	{
		m_pScript->m_id = storedId;
	}

	m_pScript->m_description = node->getAttr("Description");
	CEditorDialogScript::SScriptLine line;
	for (int i = 0; i < node->getChildCount(); ++i)
	{
		XmlNodeRef lineNode = node->getChild(i);
		if (lineNode && lineNode->isTag("Line"))
		{
			// read line attributes
			line.Reset();
			if (ReadLine(lineNode, line) == true)
				m_pScript->AddLine(line);
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////
bool CEditorDialogScriptSerializer::Save(XmlNodeRef node)
{
	node->setAttr("Name", m_pScript->m_id);
	node->setAttr("Description", m_pScript->m_description);
	int nLineCount = m_pScript->GetNumLines();
	for (int i = 0; i < nLineCount; ++i)
	{
		XmlNodeRef lineNode = node->newChild("Line");
		const CEditorDialogScript::SScriptLine* pLine = m_pScript->GetLine(i);
		WriteLine(lineNode, *pLine);
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////
bool CEditorDialogScriptSerializer::ReadLine(const XmlNodeRef& lineNode, CEditorDialogScript::SScriptLine& line)
{
	bool tmp;
	lineNode->getAttr("actor", line.m_actor);
	lineNode->getAttr("lookatActor", line.m_lookatActor);
	if (lineNode->getAttr("flagLookAtSticky", tmp)) line.m_flagLookAtSticky = tmp;
	if (lineNode->getAttr("flagSoundStopsAnim", tmp)) line.m_flagSoundStopsAnim = tmp;
	if (lineNode->getAttr("flagAGSignal", tmp)) line.m_flagAGSignal = tmp;
	if (lineNode->getAttr("flagAGEP", tmp)) line.m_flagAGEP = tmp;
	//lineNode->getAttr("audioID", line.m_audioID);
	line.m_audioTriggerName = lineNode->getAttr("audioID");
	line.m_anim = lineNode->getAttr("anim");
	line.m_facial = lineNode->getAttr("facial");
	lineNode->getAttr("delay", line.m_delay);
	lineNode->getAttr("facialWeight", line.m_facialWeight);
	lineNode->getAttr("facialFadeTime", line.m_facialFadeTime);
	line.m_desc = lineNode->getAttr("desc");
	return true;
}

////////////////////////////////////////////////////////////////////////////
bool CEditorDialogScriptSerializer::WriteLine(XmlNodeRef lineNode, const CEditorDialogScript::SScriptLine& line)
{
	lineNode->setAttr("actor", line.m_actor);
	lineNode->setAttr("lookatActor", line.m_lookatActor);
	lineNode->setAttr("flagLookAtSticky", line.m_flagLookAtSticky);
	lineNode->setAttr("flagSoundStopsAnim", line.m_flagSoundStopsAnim);
	lineNode->setAttr("flagAGSignal", line.m_flagAGSignal);
	lineNode->setAttr("flagAGEP", line.m_flagAGEP);
	lineNode->setAttr("audioID", line.m_audioTriggerName);
	lineNode->setAttr("anim", line.m_anim);
	lineNode->setAttr("facial", line.m_facial);
	lineNode->setAttr("delay", line.m_delay);
	lineNode->setAttr("facialWeight", line.m_facialWeight);
	lineNode->setAttr("facialFadeTime", line.m_facialFadeTime);
	lineNode->setAttr("desc", line.m_desc);
	return true;
}

