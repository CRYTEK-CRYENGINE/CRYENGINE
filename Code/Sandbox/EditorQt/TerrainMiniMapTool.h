// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TerrainMiniMapTool_h__
#define __TerrainMiniMapTool_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Mission.h"

//////////////////////////////////////////////////////////////////////////
class CTerrainMiniMapTool : public CEditTool, public IEditorNotifyListener, public IScreenshotCallback
{
	DECLARE_DYNCREATE(CTerrainMiniMapTool)
public:
	CTerrainMiniMapTool();
	virtual ~CTerrainMiniMapTool();

	//////////////////////////////////////////////////////////////////////////
	// CEditTool
	//////////////////////////////////////////////////////////////////////////
	virtual string GetDisplayName() const override { return "Minimap Tool"; }
	virtual void   Display(DisplayContext& dc);
	virtual bool   MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);
	virtual void   DeleteThis() { delete this; };
	//////////////////////////////////////////////////////////////////////////

	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

	SMinimapInfo GetMinimap()                         { return m_minimap; }
	string      GetPath()                            { return m_path; }
	void         SetPath(const string& path)         { m_path = path; }
	string      GetFilename()                        { return m_filename; }
	void         SetFilename(const string& filename) { m_filename = filename; }
	void         SetOrientation(int orientation)      { m_minimap.orientation = orientation; }

	void         SetResolution(int nResolution);
	void         SetCameraHeight(float fHeight);
	void         AddToLevelFolder();
	void         Generate(bool bHideProxy = true);

	// IScreenshotCallback
	void         SendParameters(void* data, uint32 width, uint32 height, f32 minx, f32 miny, f32 maxx, f32 maxy);

	void         LoadSettingsXML();

	virtual void ResetToDefault();

	virtual void Serialize(Serialization::IArchive& ar) override;

protected:
	SMinimapInfo m_minimap;

private:

	bool                    m_bDragging;

	string                 m_path;
	string                 m_filename; // without path and extension

	std::map<string, float> m_ConstClearList;
	bool                    b_stateScreenShot;
	bool                    m_exportDds;
	bool                    m_exportTif;
	bool                    m_bGenerationFinished;
};

#endif // __TerrainMiniMapTool_h__

