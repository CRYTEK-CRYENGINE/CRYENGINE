// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CrySandbox/IEditorGame.h>
#include <CryGame/IGameFramework.h>
#include <ILevelSystem.h>
#include "CryEditDoc.h"
#include "GameEngine.h"
#include "Util/ImageTIF.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "FilePathUtil.h"
#include "Objects/DisplayContext.h"
#include "TerrainMiniMapTool.h"
#include <Viewport.h>
#include "IUndoManager.h"

#define MAP_SCREENSHOT_SETTINGS "MapScreenshotSettings.xml"
#define MAX_RESOLUTION_SHIFT    11

const float kMinExtend = 16.0f;

namespace
{
enum EMiniMapResolution
{
	eMiniMapResolution_8    = 8,
	eMiniMapResolution_16   = 16,
	eMiniMapResolution_32   = 32,
	eMiniMapResolution_64   = 64,
	eMiniMapResolution_128  = 128,
	eMiniMapResolution_256  = 256,
	eMiniMapResolution_512  = 512,
	eMiniMapResolution_1024 = 1024,
	eMiniMapResolution_2048 = 2048,
	eMiniMapResolution_4096 = 4096,
	//eMiniMapResolution_8192 = 8192, // res > 4096 not supported at the moment
};

SERIALIZATION_ENUM_BEGIN(EMiniMapResolution, "MiniMapResolution")
SERIALIZATION_ENUM(eMiniMapResolution_8, "8", "8");
SERIALIZATION_ENUM(eMiniMapResolution_16, "16", "16");
SERIALIZATION_ENUM(eMiniMapResolution_32, "32", "32");
SERIALIZATION_ENUM(eMiniMapResolution_64, "64", "64");
SERIALIZATION_ENUM(eMiniMapResolution_128, "128", "128");
SERIALIZATION_ENUM(eMiniMapResolution_256, "256", "256");
SERIALIZATION_ENUM(eMiniMapResolution_512, "512", "512");
SERIALIZATION_ENUM(eMiniMapResolution_1024, "1024", "1024");
SERIALIZATION_ENUM(eMiniMapResolution_2048, "2048", "2048");
SERIALIZATION_ENUM(eMiniMapResolution_4096, "4096", "4096");
//SERIALIZATION_ENUM(eMiniMapResolution_8192, "8192", "8192"); // res > 4096 not supported at the moment
SERIALIZATION_ENUM_END()
};

//////////////////////////////////////////////////////////////////////////
// class CUndoTerrainMiniMapTool
// Undo object for storing Mini Map states
//////////////////////////////////////////////////////////////////////////
class CUndoTerrainMiniMapTool : public IUndoObject
{
public:
	CUndoTerrainMiniMapTool()
	{
		m_Undo = GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetMinimap();
	}
protected:
	virtual const char* GetDescription() { return "MiniMap Params"; };

	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
			m_Redo = GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetMinimap();
		GetIEditorImpl()->GetDocument()->GetCurrentMission()->SetMinimap(m_Undo);
		if (bUndo)
			GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
	}
	virtual void Redo()
	{
		GetIEditorImpl()->GetDocument()->GetCurrentMission()->SetMinimap(m_Redo);
		GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
	}
private:
	SMinimapInfo m_Undo;
	SMinimapInfo m_Redo;
};

//////////////////////////////////////////////////////////////////////////
// CTerrainMiniMapTool
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTerrainMiniMapTool, CEditTool)

//////////////////////////////////////////////////////////////////////////
CTerrainMiniMapTool::CTerrainMiniMapTool()
	: m_bGenerationFinished(true)
{
	m_minimap = GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetMinimap();
	m_bDragging = false;
	b_stateScreenShot = false;
	m_exportDds = true;
	m_exportTif = true;

	m_path = PathUtil::GamePathToCryPakPath(GetIEditorImpl()->GetGameEngine()->GetLevelPath().GetString());
	if (strstr(m_path, ":\\") == nullptr)
	{
		char rootpath[_MAX_PATH];
		GetCurrentDirectory(sizeof(rootpath), rootpath);
		m_path = PathUtil::Make(rootpath, PathUtil::GamePathToCryPakPath(m_path.GetString()));
	}
	m_path = PathUtil::AddBackslash(m_path.GetString());
	m_filename = GetIEditorImpl()->GetGameEngine()->GetLevelName();

	GetIEditorImpl()->RegisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
CTerrainMiniMapTool::~CTerrainMiniMapTool()
{
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();

	GetIEditorImpl()->Get3DEngine()->SetScreenshotCallback(0);
	GetIEditorImpl()->UnregisterNotifyListener(this);
}

void CTerrainMiniMapTool::SetResolution(int nResolution)
{
	GetIEditorImpl()->GetIUndoManager()->Begin();
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoTerrainMiniMapTool());
	m_minimap.textureWidth = nResolution;
	m_minimap.textureHeight = nResolution;
	GetIEditorImpl()->GetDocument()->GetCurrentMission()->SetMinimap(m_minimap);
	GetIEditorImpl()->GetIUndoManager()->Accept("Mini Map Resolution");
}

void CTerrainMiniMapTool::SetCameraHeight(float fHeight)
{
	GetIEditorImpl()->GetIUndoManager()->Begin();
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoTerrainMiniMapTool());
	m_minimap.vExtends = Vec2(fHeight, fHeight);
	GetIEditorImpl()->GetDocument()->GetCurrentMission()->SetMinimap(m_minimap);
	GetIEditorImpl()->GetIUndoManager()->Accept("Mini Map Camera Height");
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainMiniMapTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseLDown || (event == eMouseMove && (flags & MK_LBUTTON)))
	{
		Vec3 pos = view->ViewToWorld(point, 0, true);

		if (!m_bDragging)
		{
			GetIEditorImpl()->GetIUndoManager()->Begin();
			m_bDragging = true;
		}
		else
		{
			GetIEditorImpl()->GetIUndoManager()->Restore();
		}

		if (CUndo::IsRecording())
			CUndo::Record(new CUndoTerrainMiniMapTool());

		m_minimap.vCenter.x = pos.x;
		m_minimap.vCenter.y = pos.y;
		GetIEditorImpl()->GetDocument()->GetCurrentMission()->SetMinimap(m_minimap);

		return true;
	}
	else
	{
		// Stop.
		if (m_bDragging)
		{
			m_bDragging = false;
			GetIEditorImpl()->GetIUndoManager()->Accept("Mini Map Position");
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMiniMapTool::Display(DisplayContext& dc)
{
	dc.SetColor(0, 0, 1);
	dc.DrawTerrainRect(m_minimap.vCenter.x - 0.5f, m_minimap.vCenter.y - 0.5f,
	                   m_minimap.vCenter.x + 0.5f, m_minimap.vCenter.y + 0.5f,
	                   1.0f);
	dc.DrawTerrainRect(m_minimap.vCenter.x - 5.f, m_minimap.vCenter.y - 5.f,
	                   m_minimap.vCenter.x + 5.f, m_minimap.vCenter.y + 5.f,
	                   1.0f);
	dc.DrawTerrainRect(m_minimap.vCenter.x - 50.f, m_minimap.vCenter.y - 50.f,
	                   m_minimap.vCenter.x + 50.f, m_minimap.vCenter.y + 50.f,
	                   1.0f);

	dc.SetColor(0, 1, 0);
	dc.SetLineWidth(2);
	dc.DrawTerrainRect(m_minimap.vCenter.x - m_minimap.vExtends.x, m_minimap.vCenter.y - m_minimap.vExtends.y,
	                   m_minimap.vCenter.x + m_minimap.vExtends.x, m_minimap.vCenter.y + m_minimap.vExtends.y, 1.1f);
	dc.SetLineWidth(0);
}

void CTerrainMiniMapTool::SendParameters(void* data, uint32 width, uint32 height, f32 minx, f32 miny, f32 maxx, f32 maxy)
{
	CWaitCursor wait;
	string levelName = GetIEditorImpl()->GetGameEngine()->GetLevelName();
	string dataFile = m_path + m_filename + ".xml";
	string imageTIFFile = m_path + levelName + ".tif";
	string imageDDSFile = m_path + levelName + ".dds";
	string imageDDSFileShort = levelName + ".dds";

	uint8* buf = (uint8*)data;

	CImageEx image;
	image.Allocate(width, height);

	if (m_exportDds)
	{
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				image.ValueAt(x, y) = RGBA8(buf[x * 3 + y * width * 3], buf[x * 3 + 1 + y * width * 3], buf[x * 3 + 2 + y * width * 3], 255);

		GetIEditorImpl()->GetRenderer()->WriteDDS((byte*)image.GetData(), width, height, 4, imageDDSFile, eTF_BC2, 1);
	}

	if (m_exportTif)
	{
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				image.ValueAt(x, y) = RGBA8(buf[x * 3 + 2 + y * width * 3], buf[x * 3 + 1 + y * width * 3], buf[x * 3 + y * width * 3], 255);

		CImageTIF imageTIF;
		imageTIF.SaveRAW(imageTIFFile, image.GetData(), image.GetWidth(), image.GetHeight(), 1, 4, false, "Minimap");
	}

	XmlNodeRef dataNode = GetISystem()->LoadXmlFromFile(dataFile);
	if (!dataNode)
		dataNode = GetISystem()->CreateXmlNode("MetaData");
	XmlNodeRef map = dataNode->findChild("MiniMap");
	if (!map)
	{
		map = GetISystem()->CreateXmlNode("MiniMap");
		dataNode->addChild(map);
	}
	map->setAttr("Filename", imageDDSFileShort);
	map->setAttr("startX", minx);
	map->setAttr("startY", miny);
	map->setAttr("endX", maxx);
	map->setAttr("endY", maxy);
	map->setAttr("width", width);
	map->setAttr("height", height);

	IEditorGame* pGame = GetIEditorImpl()->GetGameEngine()->GetIEditorGame();
	if (pGame)
	{
		pGame->GetAdditionalMinimapData(dataNode);
	}

	dataNode->saveToFile(dataFile);

	ILevelSystem* pLevelSystem = gEnv->pGameFramework->GetILevelSystem();
	string name = pLevelSystem->GetCurrentLevel() ? pLevelSystem->GetCurrentLevel()->GetName() : "";
	pLevelSystem->SetEditorLoadedLevel(name, true);

	m_bGenerationFinished = true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMiniMapTool::Generate(bool bHideProxy)
{
	m_ConstClearList.clear();

	if (bHideProxy)
		gEnv->SetIsEditorGameMode(true); // To hide objects with collision_proxy_nomaterialset and editor materials

	GetIEditorImpl()->SetConsoleVar("e_ScreenShotWidth", m_minimap.textureWidth);
	GetIEditorImpl()->SetConsoleVar("e_screenShotHeight", m_minimap.textureHeight);
	GetIEditorImpl()->SetConsoleVar("e_ScreenShotMapCamHeight", 100000.f);
	GetIEditorImpl()->SetConsoleVar("e_ScreenShotMapOrientation", m_minimap.orientation);

	GetIEditorImpl()->SetConsoleVar("e_ScreenShotMapCenterX", m_minimap.vCenter.x);
	GetIEditorImpl()->SetConsoleVar("e_ScreenShotMapCenterY", m_minimap.vCenter.y);

	GetIEditorImpl()->SetConsoleVar("e_ScreenShotMapSizeX", m_minimap.vExtends.x);
	GetIEditorImpl()->SetConsoleVar("e_ScreenShotMapSizeY", m_minimap.vExtends.y);

	XmlNodeRef root = GetISystem()->LoadXmlFromFile(string("%EDITOR%/") + MAP_SCREENSHOT_SETTINGS);
	if (root)
	{
		for (int i = 0, nChilds = root->getChildCount(); i < nChilds; ++i)
		{
			XmlNodeRef ChildNode = root->getChild(i);
			const char* pTagName = ChildNode->getTag();
			ICVar* pVar = gEnv->pConsole->GetCVar(pTagName);
			if (pVar)
			{
				m_ConstClearList[pTagName] = pVar->GetFVal();
				const char* pAttr = ChildNode->getAttr("value");
				string Value = pAttr;
				pVar->Set(Value.c_str());
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[MiniMap: %s] Console variable %s does not exist", MAP_SCREENSHOT_SETTINGS, pTagName);
			}
		}
	}
	else
	{
		m_ConstClearList["r_HDRRendering"] = gEnv->pConsole->GetCVar("r_HDRRendering")->GetFVal();

		m_ConstClearList["r_PostProcessEffects"] = gEnv->pConsole->GetCVar("r_PostProcessEffects")->GetFVal();
		m_ConstClearList["e_Lods"] = gEnv->pConsole->GetCVar("e_Lods")->GetFVal();
		m_ConstClearList["e_ViewDistRatio"] = gEnv->pConsole->GetCVar("e_ViewDistRatio")->GetFVal();
		m_ConstClearList["e_VegetationSpritesDistanceRatio"] = gEnv->pConsole->GetCVar("e_VegetationSpritesDistanceRatio")->GetFVal();
		m_ConstClearList["e_ViewDistRatioVegetation"] = gEnv->pConsole->GetCVar("e_ViewDistRatioVegetation")->GetFVal();
		m_ConstClearList["e_TerrainLodRatio"] = gEnv->pConsole->GetCVar("e_TerrainLodRatio")->GetFVal();
		m_ConstClearList["e_ScreenShotQuality"] = gEnv->pConsole->GetCVar("e_ScreenShotQuality")->GetFVal();
		m_ConstClearList["e_Vegetation"] = gEnv->pConsole->GetCVar("e_Vegetation")->GetFVal();

		gEnv->pConsole->GetCVar("r_HDRRendering")->Set(0);
		gEnv->pConsole->GetCVar("r_PostProcessEffects")->Set(0);
		gEnv->pConsole->GetCVar("e_ScreenShotQuality")->Set(0);
		gEnv->pConsole->GetCVar("e_ViewDistRatio")->Set(10000.f);
		gEnv->pConsole->GetCVar("e_VegetationSpritesDistanceRatio")->Set(20);
		gEnv->pConsole->GetCVar("e_ViewDistRatioVegetation")->Set(100.f);
		gEnv->pConsole->GetCVar("e_Lods")->Set(0);
		gEnv->pConsole->GetCVar("e_TerrainLodRatio")->Set(0.f);
		gEnv->pConsole->GetCVar("e_Vegetation")->Set(0);
	}
	gEnv->pConsole->GetCVar("e_ScreenShotQuality")->Set(0);

	GetIEditorImpl()->Get3DEngine()->SetScreenshotCallback(this);

	GetIEditorImpl()->SetConsoleVar("e_ScreenShot", 3);

	b_stateScreenShot = true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMiniMapTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
		{
			ResetToDefault();
			break;
		}
	case eNotify_OnInvalidateControls:
		m_minimap = GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetMinimap();
		signalPropertiesChanged(this);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMiniMapTool::LoadSettingsXML()
{
	string settingsXmlPath = m_path;
	settingsXmlPath += m_filename;
	settingsXmlPath += ".xml";
	XmlNodeRef dataNode = GetISystem()->LoadXmlFromFile(settingsXmlPath);
	if (!dataNode)
		return;
	XmlNodeRef mapNode = dataNode->findChild("MiniMap");
	if (!mapNode)
		return;
	float startX = 0, startY = 0, endX = 0, endY = 0;
	mapNode->getAttr("startX", startX);
	mapNode->getAttr("startY", startY);
	mapNode->getAttr("endX", endX);
	mapNode->getAttr("endY", endY);
	m_minimap.vCenter.x = 0.5f * (startX + endX);
	m_minimap.vCenter.y = 0.5f * (startY + endY);
	m_minimap.vExtends.x = max(0.5f * (endX - startX), kMinExtend);
	m_minimap.vExtends.y = max(0.5f * (endY - startY), kMinExtend);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMiniMapTool::ResetToDefault()
{
	if (b_stateScreenShot)
	{
		ICVar* pVar = gEnv->pConsole->GetCVar("e_ScreenShot");
		if (pVar && pVar->GetIVal() == 0)
		{
			for (std::map<string, float>::iterator it = m_ConstClearList.begin(); it != m_ConstClearList.end(); ++it)
			{
				ICVar* pVar = gEnv->pConsole->GetCVar(it->first.c_str());
				if (pVar)
					pVar->Set(it->second);
			}
			m_ConstClearList.clear();

			b_stateScreenShot = false;
			GetIEditorImpl()->Get3DEngine()->SetScreenshotCallback(0);
			gEnv->SetIsEditorGameMode(false);
		}
	}
}

void CTerrainMiniMapTool::Serialize(Serialization::IArchive& ar)
{
	EMiniMapResolution minimapRes = (EMiniMapResolution)m_minimap.textureWidth;
	if (ICVar *pResolution = gEnv->pConsole->GetCVar("e_ScreenShotMapResolution"))
	{
		pResolution->Set(m_minimap.textureWidth);
	}

	float extends = m_minimap.vExtends.x;
	string fileName(m_filename.GetBuffer());
	bool orientation = m_minimap.orientation > 0;

	if (ar.openBlock("minimap", "Mini Map"))
	{
		ar(minimapRes, "resolution", "Resolution");
		ar(Serialization::Range(extends, kMinExtend, 10000.0f), "size", "Size");
		ar(fileName, "filename", "File Name");
		ar(m_exportDds, "dds", "Export as DDS");
		ar(m_exportTif, "tif", "Export as TIF");

		if (ar.isInput())
		{
			m_minimap.textureWidth = minimapRes;
			m_minimap.textureHeight = minimapRes;
			m_minimap.vExtends.x = extends;
			m_minimap.vExtends.y = extends;
			m_filename = fileName;
			m_minimap.orientation = orientation ? 1 : 0;
		}

		if (ar.openBlock("action_buttons", "<Mini Map"))
		{
			ar(Serialization::ActionButton([this]
			{
				if (!m_bGenerationFinished)
					return;
				m_bGenerationFinished = false;
				Generate();
			}), "generate", "^Generate Mini Map");
			ar.closeBlock();
		}
	}
}

