// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialManager.h"

#include "Material.h"
#include "MaterialLibrary.h"

#include "Viewport.h"
#include "ModelViewport.h"
#include "MaterialSender.h"

#include <CryAnimation/ICryAnimation.h>
#include "ISourceControl.h"

#include "Util/BoostPythonHelpers.h"

#include "Terrain/Layer.h"
#include "Terrain/TerrainManager.h"
#include "Terrain/SurfaceType.h"

#include "QT/QtMainFrame.h"

#include <CryString/CryPath.h>
#include <FilePathUtil.h>
#include <Preferences/ViewportPreferences.h>
#include <QAbstractNativeEventFilter>
#include <QApplication>

static const char* MATERIALS_LIBS_PATH = "Materials/";
static unsigned int s_highlightUpdateCounter = 0;

struct SHighlightMode
{
	float m_colorHue;
	float m_period;
	bool  m_continuous;
};

static SHighlightMode g_highlightModes[] = {
	{ 0.70f, 0.8f,  true  }, // purple
	{ 0.25f, 0.75f, false }, // green
	{ 0.0,   0.75f, true  }  // red
};

class CMaterialHighlighter
{
public:
	void Start(CMaterial* pMaterial, int modeFlag);
	void Stop(CMaterial* pMaterial, int modeFlag);
	void GetHighlightColor(ColorF* color, float* intensity, int flags);

	void ClearMaterials() { m_materials.clear(); };
	void RestoreMaterials();
	void Update();
private:
	struct SHighlightOptions
	{
		int m_modeFlags;
	};

	typedef std::map<CMaterial*, SHighlightOptions> Materials;
	Materials m_materials;
};

void CMaterialHighlighter::Start(CMaterial* pMaterial, int modeFlag)
{
	Materials::iterator it = m_materials.find(pMaterial);
	if (it == m_materials.end())
	{
		SHighlightOptions& options = m_materials[pMaterial];
		options.m_modeFlags = modeFlag;
	}
	else
	{
		SHighlightOptions& options = it->second;
		options.m_modeFlags |= modeFlag;
	}
}

void CMaterialHighlighter::Stop(CMaterial* pMaterial, int modeFlag)
{
	if (pMaterial)
		pMaterial->SetHighlightFlags(0);

	Materials::iterator it = m_materials.find(pMaterial);
	if (it == m_materials.end())
		return;

	SHighlightOptions& options = it->second;
	MAKE_SURE((options.m_modeFlags & modeFlag) != 0, return );

	options.m_modeFlags &= ~modeFlag;
	if (options.m_modeFlags == 0)
		m_materials.erase(it);
}

void CMaterialHighlighter::RestoreMaterials()
{
	for (Materials::iterator it = m_materials.begin(); it != m_materials.end(); ++it)
	{
		if (it->first)
			it->first->SetHighlightFlags(0);
	}
}

void CMaterialHighlighter::Update()
{
	unsigned int counter = s_highlightUpdateCounter;

	Materials::iterator it;
	for (it = m_materials.begin(); it != m_materials.end(); ++it)
	{
		// Only update each material every 4 frames
		if (counter++ % 4 == 0)
		{
			it->first->SetHighlightFlags(it->second.m_modeFlags);
		}
	}

	s_highlightUpdateCounter = (s_highlightUpdateCounter + 1) % 4;
}

void CMaterialHighlighter::GetHighlightColor(ColorF* color, float* intensity, int flags)
{
	MAKE_SURE(color != 0, return );
	MAKE_SURE(intensity != 0, return );

	*intensity = 0.0f;

	if (flags == 0)
		return;

	int flagIndex = 0;
	while (flags)
	{
		if ((flags & 1) != 0)
			break;
		flags = flags >> 1;
		++flagIndex;
	}

	MAKE_SURE(flagIndex < sizeof(g_highlightModes) / sizeof(g_highlightModes[0]), return );

	const SHighlightMode& mode = g_highlightModes[flagIndex];
	float t = GetTickCount() / 1000.0f;
	float h = mode.m_colorHue;
	float s = 1.0f;
	float v = 1.0f;

	color->fromHSV(h + sinf(t * g_PI2 * 5.0f) * 0.025f, s, v);
	color->a = 1.0f;

	if (mode.m_continuous)
		*intensity = fabsf(sinf(t * g_PI2 / mode.m_period));
	else
		*intensity = max(0.0f, sinf(t * g_PI2 / mode.m_period));
}

boost::python::list PyGetMaterials(string materialName = "", bool selectedOnly = false)
{
	boost::python::list result;

	GetIEditorImpl()->OpenDataBaseLibrary(EDB_TYPE_MATERIAL, NULL);
	CMaterialManager* pMaterialMgr = GetIEditorImpl()->GetMaterialManager();

	if (!materialName.IsEmpty())
	{
		result.append(PyScript::CreatePyGameMaterial((CMaterial*)pMaterialMgr->FindItemByName(materialName)));
	}
	else if (selectedOnly)
	{
		if (materialName.IsEmpty() && pMaterialMgr->GetSelectedItem() != NULL)
		{
			result.append(PyScript::CreatePyGameMaterial((CMaterial*)pMaterialMgr->GetSelectedItem()));
		}
	}
	else
	{
		// Acquire all of the materials via iterating across the objects.
		CBaseObjectsArray objects;
		GetIEditorImpl()->GetObjectManager()->GetObjects(objects);
		for (int i = 0; i < objects.size(); i++)
		{
			result.append(PyScript::CreatePyGameMaterial((CMaterial*)objects[i]->GetMaterial()));
		}
	}
	return result;
}

BOOST_PYTHON_FUNCTION_OVERLOADS(pyGetMaterialsOverload, PyGetMaterials, 0, 2);
REGISTER_PYTHON_OVERLOAD_COMMAND(PyGetMaterials, general, get_materials, pyGetMaterialsOverload,
                                 "Get all, subgroup, or selected materials in the material editor.",
                                 "general.get_materials(str materialName=\'\', selectedOnly=False, levelOnly=False)");

//////////////////////////////////////////////////////////////////////////
// CMaterialManager implementation.
//////////////////////////////////////////////////////////////////////////
CMaterialManager::CMaterialManager()
	: m_pHighlighter(new CMaterialHighlighter)
	, m_highlightMask(eHighlight_All & ~(eHighlight_Breakable | eHighlight_NoSurfaceType))
	, m_currentFolder("")
{
	m_bUniqGuidMap = false;
	m_bUniqNameMap = true;
	m_bShadersEnumerated = false;

	m_bMaterialsLoaded = false;
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);

	m_MatSender = new CMaterialSender(true);

	if (gEnv->p3DEngine)
		gEnv->p3DEngine->GetMaterialManager()->SetListener(this);
	gViewportDebugPreferences.debugFlagsChanged.Connect(this, &CMaterialManager::OnDebugFlagsChanged);
}

//////////////////////////////////////////////////////////////////////////
CMaterialManager::~CMaterialManager()
{
	gViewportDebugPreferences.debugFlagsChanged.DisconnectObject(this);

	delete m_pHighlighter;
	m_pHighlighter = 0;

	if (gEnv->p3DEngine)
		gEnv->p3DEngine->GetMaterialManager()->SetListener(NULL);

	if (m_MatSender)
	{
		delete m_MatSender;
		m_MatSender = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::ClearAll()
{
	SetCurrentMaterial(NULL);
	CBaseLibraryManager::ClearAll();

	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::CreateMaterial(const string& sMaterialName, XmlNodeRef& node, int nMtlFlags, unsigned long nLoadingFlags)
{
	CMaterial* pMaterial = new CMaterial(sMaterialName, nMtlFlags);

	if (node)
	{
		CBaseLibraryItem::SerializeContext serCtx(node, true);
		serCtx.bUniqName = true;
		pMaterial->Serialize(serCtx);
	}
	if (!pMaterial->IsPureChild() && !(pMaterial->GetFlags() & MTL_FLAG_UIMATERIAL))
	{
		RegisterItem(pMaterial);
	}

	return pMaterial;
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::CreateMaterial(const char* sMaterialName, XmlNodeRef& node, int nMtlFlags, unsigned long nLoadingFlags)
{
	return CreateMaterial(string(sMaterialName), node, nMtlFlags, nLoadingFlags);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Export(XmlNodeRef& node)
{
	XmlNodeRef libs = node->newChild("MaterialsLibrary");
	for (int i = 0; i < GetLibraryCount(); i++)
	{
		IDataBaseLibrary* pLib = GetLibrary(i);
		// Level libraries are saved in in level.
		XmlNodeRef libNode = libs->newChild("Library");

		// Export library.
		libNode->setAttr("Name", pLib->GetName());
	}
}

//////////////////////////////////////////////////////////////////////////
int CMaterialManager::ExportLib(CMaterialLibrary* pLib, XmlNodeRef& libNode)
{
	int num = 0;
	// Export library.
	libNode->setAttr("Name", pLib->GetName());
	libNode->setAttr("File", pLib->GetFilename());
	libNode->setAttr("SandboxVersion", (const char*)GetIEditorImpl()->GetFileVersion().ToFullString());
	// Serialize prototypes.
	for (int j = 0; j < pLib->GetItemCount(); j++)
	{
		CMaterial* pMtl = (CMaterial*)pLib->GetItem(j);

		// Only export real used materials.
		if (pMtl->IsDummy() || pMtl->IsPureChild())
			continue;

		XmlNodeRef itemNode = libNode->newChild("Material");
		itemNode->setAttr("Name", pMtl->GetName());
		num++;
	}
	return num;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetSelectedItem(IDataBaseItem* pItem)
{
	m_pSelectedItem = (CBaseLibraryItem*)pItem;
	SetCurrentMaterial((CMaterial*)pItem);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetCurrentMaterial(CMaterial* pMtl)
{
	if (m_pCurrentMaterial)
	{
		// Changing current material. save old one.
		if (m_pCurrentMaterial->IsModified())
			m_pCurrentMaterial->Save();
	}

	m_pCurrentMaterial = pMtl;
	if (m_pCurrentMaterial)
	{
		m_pCurrentMaterial->OnMakeCurrent();
		m_pCurrentEngineMaterial = m_pCurrentMaterial->GetMatInfo();
	}
	else
	{
		m_pCurrentEngineMaterial = 0;
	}

	m_pSelectedItem = pMtl;
	m_pSelectedParent = pMtl ? pMtl->GetParent() : NULL;

	NotifyItemEvent(m_pCurrentMaterial, EDB_ITEM_EVENT_SELECTED);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetCurrentFolder(const string& folder)
{
	m_currentFolder = folder;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetMarkedMaterials(const std::vector<_smart_ptr<CMaterial>>& markedMaterials)
{
	m_markedMaterials = markedMaterials;
}

void CMaterialManager::OnLoadShader(CMaterial* pMaterial)
{
	RemoveFromHighlighting(pMaterial, eHighlight_All);
	AddForHighlighting(pMaterial);
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::GetCurrentMaterial() const
{
	return m_pCurrentMaterial;
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CMaterialManager::MakeNewItem()
{
	CMaterial* pMaterial = new CMaterial("", 0);
	return pMaterial;
}
//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CMaterialManager::MakeNewLibrary()
{
	return new CMaterialLibrary(this);
}
//////////////////////////////////////////////////////////////////////////
string CMaterialManager::GetRootNodeName()
{
	return "MaterialsLibs";
}
//////////////////////////////////////////////////////////////////////////
string CMaterialManager::GetLibsPath()
{
	if (m_libsPath.IsEmpty())
		m_libsPath = MATERIALS_LIBS_PATH;
	return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::ReportDuplicateItem(CBaseLibraryItem* pItem, CBaseLibraryItem* pOldItem)
{
	string sLibName;
	if (pOldItem->GetLibrary())
		sLibName = pOldItem->GetLibrary()->GetName();

	CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,
	           "Material %s with the duplicate name to the loaded material %s ignored",
	           (const char*)pItem->GetName(), (const char*)pOldItem->GetName());
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Serialize(XmlNodeRef& node, bool bLoading)
{
	//CBaseLibraryManager::Serialize( node,bLoading );
	if (bLoading)
	{
	}
	else
	{
	}
}

int CMaterialManager::GetShaderCount()
{
	if (!m_bShadersEnumerated)
		EnumerateShaders();

	return m_shaderList.size();
}

const std::vector<string>& CMaterialManager::GetShaderList()
{
	if (!m_bShadersEnumerated)
		EnumerateShaders();

	return m_shaderList;
}

void CMaterialManager::EnumerateShaders()
{
	if (!m_bShadersEnumerated)
	{
		IRenderer* renderer = GetIEditor()->GetSystem()->GetIRenderer();
		if (!renderer)
			return;

		int numShaders = 0;

		m_shaderList.clear();
		m_shaderList.reserve(100);

		//! Enumerate Shaders.
		int nNumShaders = 0;
		string* files = renderer->EF_GetShaderNames(nNumShaders);
		
		for (int i = 0; i < nNumShaders; i++)
		{
			m_shaderList.push_back(files[i]);
		}

		XmlNodeRef root = GetISystem()->GetXmlUtils()->LoadXmlFromFile("%EDITOR%/Materials/ShaderList.xml");
		if (root)
		{
			for (int i = 0; i < root->getChildCount(); ++i)
			{
				XmlNodeRef ChildNode = root->getChild(i);
				const char* pTagName = ChildNode->getTag();
				if (!stricmp(pTagName, "Shader"))
				{
					string name;
					if (ChildNode->getAttr("name", name) && !name.IsEmpty())
					{
						// make sure there is no duplication
						bool bFound = false;
						for (const auto& shader : m_shaderList)
						{
							if (stricmp(name, shader) == 0)
							{
								bFound = true;
								break;
							}
						}

						if(!bFound)
						{
							m_shaderList.push_back(name);
						}
					}
				}
			}
		}

		std::sort(m_shaderList.begin(), m_shaderList.end(), [](const string &s1, const string &s2) { return stricmp(s1, s2) < 0; });

		//capitalize shader names
		for (string& shaderName : m_shaderList)
		{
			CRY_ASSERT(shaderName.length() > 0);
			shaderName.SetAt(0, toupper(shaderName[0]));
		}

		m_bShadersEnumerated = true;
	}
}

namespace
{

struct SMaterialManagerFilter : QAbstractNativeEventFilter
{
	virtual bool nativeEventFilter(const QByteArray& eventType, void* pMessage, long* pResult) override
	{
		if (eventType != "windows_generic_MSG")
		{
			return false;
		}

		const MSG* const pTheMessage = (MSG*)pMessage;
		if (pTheMessage->message != WM_MATEDITSEND)
		{
			return false;
		}

		const CWnd* const pDlg = GetIEditorImpl()->OpenView("Material Editor Legacy");
		if (pDlg)
		{
			GetIEditorImpl()->GetMaterialManager()->SyncMaterialEditor();

			if (pResult)
			{
				*pResult = 0;
			}

			return true;
		}

		return false;
	}

	static SMaterialManagerFilter& GetInstance()
	{
		static SMaterialManagerFilter theInstance;
		return theInstance;
	}
};

} // namespace

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	CBaseLibraryManager::OnEditorNotifyEvent(event);
	switch (event)
	{
	case eNotify_OnMainFrameCreated:
		InitMatSender();
		if (!GetIEditorImpl()->IsInMatEditMode())
		{
			qApp->installNativeEventFilter(&SMaterialManagerFilter::GetInstance());
		}
		break;
	case eNotify_OnIdleUpdate:
		m_pHighlighter->Update();
		break;
	case eNotify_OnBeginGameMode:
		m_pHighlighter->RestoreMaterials();
		break;
	case eNotify_OnBeginNewScene:
		SetCurrentMaterial(0);
		break;
	case eNotify_OnBeginSceneOpen:
		SetCurrentMaterial(0);
		break;
	case eNotify_OnMissionChange:
		SetCurrentMaterial(0);
		break;
	case eNotify_OnClearLevelContents:
		SetCurrentMaterial(0);
		m_pHighlighter->ClearMaterials();
		break;
	case eNotify_OnQuit:
		SetCurrentMaterial(0);
		if (gEnv->p3DEngine)
			gEnv->p3DEngine->GetMaterialManager()->SetListener(NULL);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::LoadMaterial(const string& sMaterialName, bool bMakeIfNotFound)
{
	LOADING_TIME_PROFILE_SECTION(GetISystem());

	string sMaterialNameClear(sMaterialName);
	int nExtLen = strlen(MATERIAL_FILE_EXT);
	if (sMaterialNameClear.Right(nExtLen) == MATERIAL_FILE_EXT)
		sMaterialNameClear = sMaterialNameClear.Left(sMaterialNameClear.GetLength() - nExtLen);

	// Load material with this name if not yet loaded.
	CMaterial* pMaterial = (CMaterial*)FindItemByName(sMaterialNameClear);
	if (pMaterial)
	{
		return pMaterial;
	}

	string filename = PathUtil::MakeGamePath(MaterialToFilename(sMaterialNameClear).GetString()).c_str();
	if (filename.GetLength() - nExtLen < sMaterialNameClear.GetLength() - 1)
	{
		// Remove game folder in the begin of material name (bad material names in cgf)
		int nNewLenMaterialName = filename.GetLength() - nExtLen;
		if (sMaterialNameClear[sMaterialNameClear.GetLength() - nNewLenMaterialName - 1] == '/' &&
		    !stricmp(sMaterialNameClear.Right(nNewLenMaterialName), filename.Left(nNewLenMaterialName)))
		{
			sMaterialNameClear = sMaterialNameClear.Right(nNewLenMaterialName);
			filename = PathUtil::MakeGamePath(MaterialToFilename(sMaterialNameClear).GetString()).c_str();
		}
	}

	// Try to load material with this name again if not yet loaded.
	pMaterial = (CMaterial*)FindItemByName(sMaterialNameClear);
	if (pMaterial)
	{
		return pMaterial;
	}

	XmlNodeRef mtlNode = GetISystem()->LoadXmlFromFile(filename);
	if (mtlNode)
	{
		pMaterial = CreateMaterial(sMaterialNameClear, mtlNode);
	}
	else
	{
		if (bMakeIfNotFound)
		{
			pMaterial = new CMaterial(sMaterialNameClear);
			pMaterial->SetDummy(true);
			RegisterItem(pMaterial);

			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Material %s not found", (const char*)sMaterialNameClear);
		}
	}
	//

	return pMaterial;
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::LoadMaterial(const char* sMaterialName, bool bMakeIfNotFound)
{
	return LoadMaterial(string(sMaterialName), bMakeIfNotFound);
}

//////////////////////////////////////////////////////////////////////////
static bool MaterialRequiresSurfaceType(CMaterial* pMaterial)
{
	// Do not enforce Surface Type...

	// ...over editor UI materials
	if ((pMaterial->GetFlags() & MTL_FLAG_UIMATERIAL) != 0)
		return false;

	// ...over SKY
	if (pMaterial->GetShaderName() == "DistanceCloud" ||
	    pMaterial->GetShaderName() == "Sky" ||
	    pMaterial->GetShaderName() == "SkyHDR")
		return false;
	// ...over terrain materials
	if (pMaterial->GetShaderName() == "Terrain.Layer")
		return false;
	// ...over vegetation
	if (pMaterial->GetShaderName() == "Vegetation")
		return false;

	// ...over decals
	CVarBlock* pShaderGenParams = pMaterial->GetShaderGenParamsVars();
	if (pShaderGenParams)
		if (IVariable* pVar = pShaderGenParams->FindVariable("Decal"))
		{
			int value = 0;
			pVar->Get(value);
			if (value)
				return false;
		}

	return true;
}

//////////////////////////////////////////////////////////////////////////
int CMaterialManager::GetHighlightFlags(CMaterial* pMaterial) const
{
	if (pMaterial == NULL)
		return 0;

	if ((pMaterial->GetFlags() & MTL_FLAG_NODRAW) != 0)
		return 0;

	int result = 0;

	if (pMaterial == m_pHighlightMaterial)
		result |= eHighlight_Pick;

	const string& surfaceTypeName = pMaterial->GetSurfaceTypeName();
	if (surfaceTypeName.IsEmpty() && MaterialRequiresSurfaceType(pMaterial))
		result |= eHighlight_NoSurfaceType;

	if (ISurfaceTypeManager* pSurfaceManager = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->GetSurfaceTypeManager())
	{
		const ISurfaceType* pSurfaceType = pSurfaceManager->GetSurfaceTypeByName(surfaceTypeName);
		if (pSurfaceType && pSurfaceType->GetBreakability() != 0)
			result |= eHighlight_Breakable;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::AddForHighlighting(CMaterial* pMaterial)
{
	if (pMaterial == NULL)
	{
		return;
	}

	int highlightFlags = (GetHighlightFlags(pMaterial) & m_highlightMask);
	if (highlightFlags != 0)
	{
		m_pHighlighter->Start(pMaterial, highlightFlags);
	}

	int count = pMaterial->GetSubMaterialCount();
	for (int i = 0; i < count; ++i)
	{
		CMaterial* pChild = pMaterial->GetSubMaterial(i);
		if (!pChild)
			continue;

		AddForHighlighting(pChild);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::RemoveFromHighlighting(CMaterial* pMaterial, int mask)
{
	if (pMaterial == NULL)
		return;

	m_pHighlighter->Stop(pMaterial, mask);

	int count = pMaterial->GetSubMaterialCount();
	for (int i = 0; i < count; ++i)
	{
		CMaterial* pChild = pMaterial->GetSubMaterial(i);
		if (!pChild)
			continue;

		RemoveFromHighlighting(pChild, mask);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::UpdateHighlightedMaterials()
{
	IDataBaseItemEnumerator* pEnum = CBaseLibraryManager::GetItemEnumerator();
	if (!pEnum)
		return;

	CMaterial* pMaterial = (CMaterial*)pEnum->GetFirst();
	while (pMaterial)
	{
		RemoveFromHighlighting(pMaterial, eHighlight_All);
		AddForHighlighting(pMaterial);
		pMaterial = (CMaterial*)pEnum->GetNext();
	}

	pEnum->Release();
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMaterialManager::OnLoadMaterial(const char* sMtlName, bool bForceCreation, unsigned long nLoadingFlags)
{
	_smart_ptr<CMaterial> pMaterial = LoadMaterial(sMtlName, bForceCreation);
	if (pMaterial)
	{
		AddForHighlighting(pMaterial);
		return pMaterial->GetMatInfo();
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnRequestMaterial(IMaterial* pMatInfo)
{
	const char* pcName = pMatInfo->GetName();
	CMaterial* pMaterial = (CMaterial*) pMatInfo->GetUserData();

	// LoadMaterial() will return registered items if they match
	// It will also register items if the item is not found and the XML exists and it's not a pure child
	if (!pMaterial && pcName && *pcName)
		pMaterial = LoadMaterial(pcName, false);

	if (pMaterial)
	{
		IMaterial* pNewMatInfo = pMaterial->GetMatInfo(true);
		assert(pNewMatInfo == pMatInfo);

		// RegisterItem() can be called through multiple request of the same materials or through a cascade
		// of calls (see above), thus prevent warning messages by calling it conditionally.
		string fullName = pMaterial->GetFullName();
		if (!fullName.IsEmpty() && !FindItemByName(fullName))
			RegisterItem(pMaterial);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnCreateMaterial(IMaterial* pMatInfo)
{
	if (!(pMatInfo->GetFlags() & MTL_FLAG_PURE_CHILD) && !(pMatInfo->GetFlags() & MTL_FLAG_UIMATERIAL))
	{
		CMaterial* pMaterial = new CMaterial(pMatInfo->GetName());
		pMaterial->SetFromMatInfo(pMatInfo);
		RegisterItem(pMaterial);

		AddForHighlighting(pMaterial);
	}

}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnDeleteMaterial(IMaterial* pMaterial)
{
	CMaterial* pMtl = (CMaterial*)pMaterial->GetUserData();
	if (pMtl)
	{
		RemoveFromHighlighting(pMtl, eHighlight_All);
		pMtl->ClearMatInfo();
	}
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::FromIMaterial(IMaterial* pMaterial)
{
	if (!pMaterial)
		return 0;
	CMaterial* pMtl = (CMaterial*)pMaterial->GetUserData();
	return pMtl;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SaveAllLibs()
{
}

//////////////////////////////////////////////////////////////////////////
string CMaterialManager::FilenameToMaterial(const string& filename)
{
	string name = PathUtil::RemoveExtension(filename.GetString()).c_str();
	name.Replace('\\', '/');

	string sDataFolder = PathUtil::GetGameFolder();
	// Remove "DATA_FOLDER/" sub path from the filename.
	if (name.GetLength() > (sDataFolder.GetLength()) && strnicmp(name, sDataFolder, sDataFolder.GetLength()) == 0)
	{
		name = name.Mid(sDataFolder.GetLength() + 1); // skip the slash...
	}

	/*
	   // Remove "materials/" sub path from the filename.
	   if (name.GetLength() > sizeof(MATERIALS_PATH)-1 && strnicmp(name,MATERIALS_PATH,sizeof(MATERIALS_PATH)-1) == 0)
	   {
	   //name = name.Mid(sizeof(MATERIALS_PATH)+1);
	   }
	 */
	return name;
}

//////////////////////////////////////////////////////////////////////////
string CMaterialManager::MaterialToFilename(const string& sMaterialName, bool bForWriting)
{
	string filename = PathUtil::ReplaceExtension(sMaterialName.GetString(), MATERIAL_FILE_EXT).c_str();
	return filename;
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialManager::DeleteMaterial(CMaterial* pMtl)
{
	assert(pMtl);
	_smart_ptr<CMaterial> _ref(pMtl);
	if (pMtl == GetCurrentMaterial())
		SetCurrentMaterial(NULL);

	DeleteItem(pMtl);

	// Unassign this material from all objects.
	CBaseObjectsArray objects;
	GetIEditorImpl()->GetObjectManager()->GetObjects(objects);
	int i;
	for (i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];
		if (pObject->GetMaterial() == pMtl)
		{
			pObject->SetMaterial(NULL);
		}
	}
	// Delete it from all sub materials.
	for (i = 0; i < m_pLevelLibrary->GetItemCount(); i++)
	{
		CMaterial* pMultiMtl = (CMaterial*)m_pLevelLibrary->GetItem(i);
		if (pMultiMtl->IsMultiSubMaterial())
		{
			for (int slot = 0; slot < pMultiMtl->GetSubMaterialCount(); slot++)
			{
				if (pMultiMtl->GetSubMaterial(slot) == pMultiMtl)
				{
					// Clear this sub material slot.
					pMultiMtl->SetSubMaterial(slot, 0);
				}
			}
		}
	}
	bool bRes = true;
	// Delete file on disk.
	if (!pMtl->GetFilename().IsEmpty())
	{
		if (!::DeleteFile(pMtl->GetFilename()))
			bRes = false;
	}

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialManager::SelectSaveMaterial(string& itemName, const char* defaultStartPath)
{
	string startPath;
	if (defaultStartPath && defaultStartPath[0] != '\0')
	{
		startPath = defaultStartPath;
	}
	else
	{
		startPath = string(PathUtil::GetGameFolder()) + "/Materials";
	}

	string filename;
	if (!CFileUtil::SelectSaveFile("Material Files (*.mtl)|*.mtl", "mtl", startPath, filename))
	{
		return false;
	}

	filename = PathUtil::ToDosPath(filename.GetString()).c_str();

	itemName = PathUtil::AbsolutePathToGamePath(filename.GetString()).c_str();
	itemName = PathUtil::RemoveExtension(itemName.GetString()).c_str();
	if (itemName.IsEmpty())
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::SelectNewMaterial(int nMtlFlags, const char* szStartPath)
{
	string path;
	if (szStartPath)
	{
		path = szStartPath;
	}
	else if (m_pCurrentMaterial)
	{
		path = PathUtil::GetPathWithoutFilename(m_pCurrentMaterial->GetFilename());
	}
	else
	{
		path = m_currentFolder;
	}
	string itemName;
	if (!SelectSaveMaterial(itemName, path))
		return 0;

	if (FindItemByName(itemName))
	{
		Warning("Material with name %s already exist", (const char*)itemName);
		return 0;
	}

	_smart_ptr<CMaterial> mtl = CreateMaterial(itemName, XmlNodeRef(), nMtlFlags);
	mtl->Update();
	mtl->Save();
	SetCurrentMaterial(mtl);
	return mtl;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_Create()
{
	SelectNewMaterial(0);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_CreateMulti()
{
	SelectNewMaterial(MTL_FLAG_MULTI_SUBMTL);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_ConvertToMulti()
{
	CMaterial* pMaterial = GetCurrentMaterial();

	if (pMaterial && pMaterial->GetSubMaterialCount() == 0)
	{
		pMaterial->ConvertToMultiMaterial();
		pMaterial->Save();
		pMaterial->Reload();
		SetSelectedItem(pMaterial->GetSubMaterial(0));
	}
	else
	{
		Warning(pMaterial ? "material.convert_to_multi called on invalid material setup" : "material.convert_to_multi called while no material selected");
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_Duplicate()
{
	CMaterial* pSrcMtl = GetCurrentMaterial();

	if (!pSrcMtl)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "material.duplicate_current called while no materials selected");
		return;
	}

	if (GetIEditorImpl()->IsSourceControlAvailable())
	{
		uint32 attrib = pSrcMtl->GetFileAttributes();

		if ((attrib & SCC_FILE_ATTRIBUTE_INPAK) && (attrib & SCC_FILE_ATTRIBUTE_MANAGED) && !(attrib & SCC_FILE_ATTRIBUTE_NORMAL))
		{
			// Get latest for making folders with right case
			GetIEditorImpl()->GetSourceControl()->GetLatestVersion(pSrcMtl->GetFilename());
		}
	}

	if (pSrcMtl != 0 && !pSrcMtl->IsPureChild())
	{
		string name = MakeUniqItemName(pSrcMtl->GetName());
		// Create a new material.
		_smart_ptr<CMaterial> pMtl = DuplicateMaterial(name, pSrcMtl);
		if (pMtl)
		{
			pMtl->Save();
			SetSelectedItem(pMtl);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::DuplicateMaterial(const char* newName, CMaterial* pOriginal)
{
	if (!newName)
	{
		assert(0 && "NULL newName passed into " __FUNCTION__);
		return 0;
	}
	if (!pOriginal)
	{
		assert(0 && "NULL pOriginal passed into " __FUNCTION__);
		return 0;
	}

	XmlNodeRef node = GetISystem()->CreateXmlNode("Material");
	CBaseLibraryItem::SerializeContext ctx(node, false);
	ctx.bCopyPaste = true;
	pOriginal->Serialize(ctx);

	return CreateMaterial(newName, node, pOriginal->GetFlags());
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_Merge()
{
	string itemName;
	string defaultMaterialPath;
	if (m_pCurrentMaterial)
		defaultMaterialPath = PathUtil::GetPathWithoutFilename(m_pCurrentMaterial->GetFilename());
	if (!SelectSaveMaterial(itemName, defaultMaterialPath))
		return;

	_smart_ptr<CMaterial> pNewMaterial = CreateMaterial(itemName, XmlNodeRef(), MTL_FLAG_MULTI_SUBMTL);

	size_t numRecords = m_markedMaterials.size();
	size_t subMaterialIndex = 0;
	for (size_t i = 0; i < numRecords; ++i)
	{
		_smart_ptr<CMaterial>& pMaterial = m_markedMaterials[i];
		int subMaterialCount = pMaterial->GetSubMaterialCount();
		pNewMaterial->SetSubMaterialCount(pNewMaterial->GetSubMaterialCount() + subMaterialCount);
		for (size_t j = 0; j < subMaterialCount; ++j)
		{
			CMaterial* pSubMaterial = pMaterial->GetSubMaterial(j);
			CMaterial* pNewSubMaterial = 0;
			if (pSubMaterial)
			{
				// generate unique name
				string name = pSubMaterial->GetName();
				size_t nameIndex = 0;

				bool nameUpdated = true;
				while (nameUpdated)
				{
					nameUpdated = false;
					for (size_t k = 0; k < subMaterialIndex; ++k)
					{
						if (pNewMaterial->GetSubMaterial(k)->GetName() == name)
						{
							++nameIndex;
							name.Format("%s%02d", pSubMaterial->GetName(), nameIndex);
							nameUpdated = true;
							break;
						}
					}
				}
				pNewSubMaterial = DuplicateMaterial(name, pSubMaterial);
			}
			pNewMaterial->SetSubMaterial(subMaterialIndex, pNewSubMaterial);
			++subMaterialIndex;
		}
	}

	pNewMaterial->Update();
	pNewMaterial->Save();
	SetCurrentMaterial(pNewMaterial);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_Delete()
{
	CMaterial* pMtl = GetCurrentMaterial();
	if (pMtl)
	{
		CUndo undo("Delete Material");
		string str;
		str.Format(_T("Delete Material %s?\r\nNote: Material file %s will also be deleted."),
		           (const char*)pMtl->GetName(), (const char*)pMtl->GetFilename());
		if (MessageBox(AfxGetMainWnd()->GetSafeHwnd(), str, _T("Delete Confirmation"), MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			DeleteMaterial(pMtl);
			SetCurrentMaterial(0);
		}
	}
}

void CMaterialManager::Command_CreateTerrainLayer()
{
	CMaterial* pMaterial = GetCurrentMaterial();

	if (!pMaterial)
		return;

	CLayer* pLayer = 0;
	string sMaterialPath = pMaterial ? pMaterial->GetName() : "";
	bool bFound = false;
	CTerrainManager* terrainManager = GetIEditorImpl()->GetTerrainManager();

	for (int i = 0, n = terrainManager->GetLayerCount(); i < n; ++i)
	{
		CLayer* pCurLayer = terrainManager->GetLayer(i);
		CSurfaceType* pSurfaceType = pCurLayer->GetSurfaceType();

		if (pSurfaceType && stricmp(pSurfaceType->GetMaterial(), sMaterialPath.GetBuffer()) == 0)
		{
			pLayer = pCurLayer;
			break;
		}
	}

	if (pLayer)
	{
		MessageBox(AfxGetMainWnd()->GetSafeHwnd(), _T("There's already a terrain layer with that material."), _T("Existing layer"), MB_ICONEXCLAMATION);
	}
	else
	{
		pLayer = new CLayer;
		pLayer->SetLayerName(PathUtil::GetFile(sMaterialPath.c_str()));
		pLayer->LoadTexture(string(PathUtil::GetGameFolder() + "/Textures/Terrain/Default.dds"));
		pLayer->GetOrRequestLayerId();
		terrainManager->AddLayer(pLayer);
		pLayer->AssignMaterial(sMaterialPath);
		terrainManager->ReloadSurfaceTypes(true, false);
	}

	for (int i = 0, n = terrainManager->GetLayerCount(); i < n; ++i)
		terrainManager->GetLayer(i)->SetSelected(false);

	pLayer->SetSelected(true);
	GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_AssignToSelection()
{
	CMaterial* pMtl = GetCurrentMaterial();
	if (pMtl)
	{
		CUndo undo("Assign Material");
		const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
		if (pMtl->IsPureChild())
		{
			int nSelectionCount = pSel->GetCount();
			bool bAllDesignerObject = nSelectionCount == 0 ? false : true;
			for (int i = 0; i < nSelectionCount; ++i)
			{
				if (pSel->GetObject(i)->GetType() != OBJTYPE_SOLID)
				{
					bAllDesignerObject = false;
					break;
				}
			}
			if (!bAllDesignerObject)
			{
				if (MessageBox(AfxGetMainWnd()->GetSafeHwnd(), _T("You can assign submaterials to objects only for preview purpose. This assignment will not be saved with the level and will not be exported to the game."), _T("Assign Submaterial"), MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL)
					return;
			}
		}
		if (!pSel->IsEmpty())
		{
			for (int i = 0; i < pSel->GetCount(); i++)
			{
				pSel->GetObject(i)->SetMaterial(pMtl);
				pSel->GetObject(i)->UpdateGroup();
				pSel->GetObject(i)->UpdatePrefab();
			}
		}
	}
	CViewport* pViewport = GetIEditorImpl()->GetActiveView();
	if (pViewport)
	{
		pViewport->Drop(CPoint(-1, -1), pMtl);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_ResetSelection()
{
	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	if (!pSel->IsEmpty())
	{
		CUndo undo("Reset Material");
		for (int i = 0; i < pSel->GetCount(); i++)
		{
			CBaseObject* pObject = pSel->GetObject(i);
			pObject->SetMaterial(0);
			pObject->UpdatePrefab();
		}
	}
	CViewport* pViewport = GetIEditorImpl()->GetActiveView();
	if (pViewport)
	{
		pViewport->Drop(CPoint(-1, -1), 0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_SelectAssignedObjects()
{
	CMaterial* pMtl = GetCurrentMaterial();
	if (pMtl)
	{
		CUndo undo("Select Object(s)");
		CBaseObjectsArray objects;
		GetIEditorImpl()->GetObjectManager()->GetObjects(objects);
		for (int i = 0; i < objects.size(); i++)
		{
			CBaseObject* pObject = objects[i];
			if (pObject->GetMaterial() == pMtl || pObject->GetRenderMaterial() == pMtl)
			{
				if (pObject->IsHidden() || pObject->IsFrozen())
					continue;
				GetIEditorImpl()->GetObjectManager()->SelectObject(pObject);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_SelectFromObject()
{
	if (GetIEditorImpl()->IsInPreviewMode())
	{
		CViewport* pViewport = GetIEditorImpl()->GetActiveView();
		if (pViewport && pViewport->GetType() == ET_ViewportModel)
		{
			CMaterial* pMtl = static_cast<CMaterial*>(((CModelViewport*)pViewport)->GetMaterial());
			SetCurrentMaterial(pMtl);
		}
		return;
	}

	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	if (pSel->IsEmpty())
		return;

	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CMaterial* pMtl = (CMaterial*)pSel->GetObject(i)->GetRenderMaterial();
		if (pMtl)
		{
			SetCurrentMaterial(pMtl);
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::PickPreviewMaterial(HWND hWndCaller)
{
	XmlNodeRef data = XmlHelpers::CreateXmlNode("ExportMaterial");
	CMaterial* pMtl = GetCurrentMaterial();
	if (!pMtl)
		return;

	if (pMtl->IsPureChild() && pMtl->GetParent())
		pMtl = pMtl->GetParent();

	if (pMtl->GetFlags() & MTL_FLAG_WIRE)
		data->setAttr("Flag_Wire", 1);
	if (pMtl->GetFlags() & MTL_FLAG_2SIDED)
		data->setAttr("Flag_2Sided", 1);

	data->setAttr("Name", pMtl->GetName());
	data->setAttr("FileName", pMtl->GetFilename());

	XmlNodeRef node = data->newChild("Material");

	CBaseLibraryItem::SerializeContext serCtx(node, false);
	pMtl->Serialize(serCtx);

	if (!pMtl->IsMultiSubMaterial())
	{
		XmlNodeRef texturesNode = node->findChild("Textures");
		if (texturesNode)
		{
			for (int i = 0; i < texturesNode->getChildCount(); i++)
			{
				XmlNodeRef texNode = texturesNode->getChild(i);
				string file;
				if (texNode->getAttr("File", file))
				{

					if (file.size() >= 2 && file.c_str()[0] == '.' && (file.c_str()[1] == '/' || file.c_str()[1] == '\\'))
					{
						// Texture file location is relative to material file folder
						file = PathUtil::Make(PathUtil::GetPathWithoutFilename(pMtl->GetFilename()), &file.c_str()[2]);
					}

					char sFullFilenameLC[MAX_PATH];
					GetCurrentDirectory(MAX_PATH, sFullFilenameLC);
					cry_strcat(sFullFilenameLC, "\\");
					cry_strcat(sFullFilenameLC, PathUtil::GamePathToCryPakPath(file.GetString()));
					texNode->setAttr("File", sFullFilenameLC);
				}
			}
		}
	}
	else
	{
		XmlNodeRef childsNode = node->findChild("SubMaterials");
		if (childsNode)
		{
			int nSubMtls = childsNode->getChildCount();
			for (int i = 0; i < nSubMtls; i++)
			{
				XmlNodeRef node = childsNode->getChild(i);
				XmlNodeRef texturesNode = node->findChild("Textures");
				if (texturesNode)
				{
					for (int i = 0; i < texturesNode->getChildCount(); i++)
					{
						XmlNodeRef texNode = texturesNode->getChild(i);
						string file;
						if (texNode->getAttr("File", file))
						{

							if (file.size() >= 2 && file.c_str()[0] == '.' && (file.c_str()[1] == '/' || file.c_str()[1] == '\\'))
							{
								// Texture file location is relative to material file folder
								file = PathUtil::Make(PathUtil::GetPathWithoutFilename(pMtl->GetFilename()), &file.c_str()[2]);
							}

							char sFullFilenameLC[MAX_PATH];
							GetCurrentDirectory(MAX_PATH, sFullFilenameLC);
							cry_strcat(sFullFilenameLC, "\\");
							cry_strcat(sFullFilenameLC, PathUtil::GamePathToCryPakPath(file.GetString()));
							texNode->setAttr("File", sFullFilenameLC);
							//texNode->setAttr( "File", Path::GamePathToFullPath(file) );
						}
					}
				}
			}
		}
	}

	m_MatSender->SendMessage(eMSM_GetSelectedMaterial, data);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SyncMaterialEditor()
{
	if (!m_MatSender)
		return;

	if (!m_MatSender->GetMessage())
		return;

	if (m_MatSender->m_h.msg == eMSM_Create)
	{
		XmlNodeRef node = m_MatSender->m_node->findChild("Material");
		if (!node)
			return;

		string sMtlName;
		string sMaxFile;

		XmlNodeRef root = m_MatSender->m_node;
		root->getAttr("Name", sMtlName);
		root->getAttr("MaxFile", sMaxFile);

		int IsMulti = 0;
		root->getAttr("IsMulti", IsMulti);

		int nMtlFlags = 0;
		if (IsMulti)
			nMtlFlags |= MTL_FLAG_MULTI_SUBMTL;

		if (root->haveAttr("Flag_Wire"))
			nMtlFlags |= MTL_FLAG_WIRE;
		if (root->haveAttr("Flag_2Sided"))
			nMtlFlags |= MTL_FLAG_2SIDED;

		_smart_ptr<CMaterial> pMtl = SelectNewMaterial(nMtlFlags, PathUtil::GetPathWithoutFilename(sMaxFile));

		if (!pMtl)
			return;

		if (!IsMulti)
		{
			node->delAttr("Shader");   // Remove shader attribute.
			XmlNodeRef texturesNode = node->findChild("Textures");
			if (texturesNode)
			{
				for (int i = 0; i < texturesNode->getChildCount(); i++)
				{
					XmlNodeRef texNode = texturesNode->getChild(i);
					string file;
					if (texNode->getAttr("File", file))
					{
						//make path relative to the project specific game folder
						string newfile = PathUtil::AbsolutePathToGamePath(file.GetString()).c_str();
						if (newfile.GetLength() > 0)
							file = newfile;
						texNode->setAttr("File", file);
					}
				}
			}
		}
		else
		{
			XmlNodeRef childsNode = node->findChild("SubMaterials");
			if (childsNode)
			{
				int nSubMtls = childsNode->getChildCount();
				for (int i = 0; i < nSubMtls; i++)
				{
					XmlNodeRef node = childsNode->getChild(i);
					node->delAttr("Shader");   // Remove shader attribute.
					XmlNodeRef texturesNode = node->findChild("Textures");
					if (texturesNode)
					{
						for (int i = 0; i < texturesNode->getChildCount(); i++)
						{
							XmlNodeRef texNode = texturesNode->getChild(i);
							string file;
							if (texNode->getAttr("File", file))
							{
								//make path relative to the project specific game folder
								string newfile = PathUtil::AbsolutePathToGamePath(file.GetString()).c_str();
								if (newfile.GetLength() > 0)
									file = newfile;
								texNode->setAttr("File", file);
							}
						}
					}
				}
			}
		}

		CBaseLibraryItem::SerializeContext ctx(node, true);
		ctx.bUndo = true;
		pMtl->Serialize(ctx);

		pMtl->Update();

		SetCurrentMaterial(0);
		SetCurrentMaterial(pMtl);
	}

	if (m_MatSender->m_h.msg == eMSM_GetSelectedMaterial)
	{
		PickPreviewMaterial(m_MatSender->m_h.GetMaxHWND());
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::InitMatSender()
{
	//MatSend(true);

	HWND mainWnd = (HWND)CEditorMainFrame::GetInstance()->winId();
	CRY_ASSERT(mainWnd);

	m_MatSender->Create();
	m_MatSender->SetupWindows(mainWnd, mainWnd);
	XmlNodeRef node = XmlHelpers::CreateXmlNode("Temp");
	m_MatSender->SendMessage(eMSM_Init, node);
}

void CMaterialManager::OnDebugFlagsChanged()
{
	int debugFlags = gViewportDebugPreferences.GetDebugFlags();

	int mask = GetHighlightMask();
	if (debugFlags & DBG_HIGHLIGHT_BREAKABLE)
		mask |= eHighlight_Breakable;
	else
		mask &= ~eHighlight_Breakable;

	if (debugFlags & DBG_HIGHLIGHT_MISSING_SURFACE_TYPE)
		mask |= eHighlight_NoSurfaceType;
	else
		mask &= ~eHighlight_NoSurfaceType;

	SetHighlightMask(mask);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::GotoMaterial(CMaterial* pMaterial)
{
	if (pMaterial)
		GetIEditorImpl()->OpenDataBaseLibrary(EDB_TYPE_MATERIAL, pMaterial);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::GotoMaterial(IMaterial* pMtl)
{
	if (pMtl)
	{
		CMaterial* pEdMaterial = FromIMaterial(pMtl);
		if (pEdMaterial)
			GetIEditorImpl()->OpenDataBaseLibrary(EDB_TYPE_MATERIAL, pEdMaterial);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetHighlightedMaterial(CMaterial* pMtl)
{
	if (m_pHighlightMaterial)
		RemoveFromHighlighting(m_pHighlightMaterial, eHighlight_Pick);

	m_pHighlightMaterial = pMtl;
	if (m_pHighlightMaterial)
		AddForHighlighting(m_pHighlightMaterial);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::HighlightedMaterialChanged(CMaterial* pMtl)
{
	if (!pMtl)
		return;

	RemoveFromHighlighting(pMtl, eHighlight_All);
	AddForHighlighting(pMtl);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetHighlightMask(int highlightMask)
{
	if (m_highlightMask != highlightMask)
	{
		m_highlightMask = highlightMask;

		UpdateHighlightedMaterials();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::GatherResources(IMaterial* pMaterial, CUsedResources& resources)
{
	if (!pMaterial)
		return;

	int nSubMtlCount = pMaterial->GetSubMtlCount();
	if (nSubMtlCount > 0)
	{
		for (int i = 0; i < nSubMtlCount; i++)
		{
			GatherResources(pMaterial->GetSubMtl(i), resources);
		}
	}
	else
	{
		SShaderItem& shItem = pMaterial->GetShaderItem();
		if (shItem.m_pShaderResources)
		{
			SInputShaderResourcesPtr res = gEnv->pRenderer->EF_CreateInputShaderResource();
			shItem.m_pShaderResources->ConvertToInputResource(res);

			for (int i = 0; i < EFTT_MAX; i++)
			{
				if (!res->m_Textures[i].m_Name.empty())
				{
					resources.Add(res->m_Textures[i].m_Name.c_str());
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CMaterialManager::GetHighlightColor(ColorF* color, float* intensity, int flags)
{
	MAKE_SURE(m_pHighlighter, return );
	m_pHighlighter->GetHighlightColor(color, intensity, flags);
}

void CMaterialManager::CreateTerrainLayerFromMaterial(CMaterial* pMaterial)
{
	if (!pMaterial)
		return;

	CLayer* pLayer = 0;
	string sMaterialPath = pMaterial ? pMaterial->GetName() : "";
	bool bFound = false;
	CTerrainManager* terrainManager = GetIEditorImpl()->GetTerrainManager();

	for (int i = 0, n = terrainManager->GetLayerCount(); i < n; ++i)
	{
		CLayer* pCurLayer = terrainManager->GetLayer(i);
		CSurfaceType* pSurfaceType = pCurLayer->GetSurfaceType();

		if (pSurfaceType && stricmp(pSurfaceType->GetMaterial(), sMaterialPath.c_str()) == 0)
		{
			pLayer = pCurLayer;
			break;
		}
	}

	if (!pLayer)
	{
		pLayer = new CLayer;
		pLayer->SetLayerName(PathUtil::GetFile(sMaterialPath.c_str()));
		pLayer->LoadTexture(string(PathUtil::GetGameFolder() + "/Textures/Terrain/Default.dds"));
		pLayer->GetOrRequestLayerId();
		terrainManager->AddLayer(pLayer);
		pLayer->AssignMaterial(sMaterialPath);
		terrainManager->ReloadSurfaceTypes(true, false);
	}

	for (int i = 0, n = terrainManager->GetLayerCount(); i < n; ++i)
		terrainManager->GetLayer(i)->SetSelected(false);

	pLayer->SetSelected(true);
	GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
}

