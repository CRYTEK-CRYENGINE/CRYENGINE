// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorFramework/Editor.h"
#include "ILevelEditor.h"

class QMenu;
class CEditorDialog;
class CLevelExplorer;
class CQuickAssetBrowser;
class CTagLocations;

// This is a stub class that is meant to encapsulate all editor logic for the level editor that is currently in CryEdit.
// Ultimately the level editor will be just another window and all its logic should be encapsulated, while CryEdit should retain global functionality
// Currently the transition will only happen for the parts of the code being changed
class CLevelEditor : public CEditor, public ILevelEditor, public IAutoEditorNotifyListener
{
	Q_OBJECT

public:
	CLevelEditor();
	~CLevelEditor();

	void                OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	void                CreateRecentFilesMenu(QMenu* pRecentFilesMenu);
	virtual const char* GetEditorName() const override { return "Level Editor"; }

	// Snapping
	void EnableVertexSnapping(bool bEnable);
	void EnablePivotSnapping(bool bEnable);
	void EnableGridSnapping(bool bEnable);
	void EnableAngleSnapping(bool bEnable);
	void EnableScaleSnapping(bool bEnable);

	void EnableTerrainSnapping(bool bEnable);
	void EnableGeometrySnapping(bool bEnable);
	void EnableSurfaceNormalSnapping(bool bEnable);

	bool IsVertexSnappingEnabled() const;
	bool IsPivotSnappingEnabled() const;

	bool IsGridSnappingEnabled() const;
	bool IsAngleSnappingEnabled() const;
	bool IsScaleSnappingEnabled() const;
	bool IsTerrainSnappingEnabled() const;
	bool IsGeometrySnappingEnabled() const;
	bool IsSurfaceNormalSnappingEnabled() const;

	bool ConvertEditorXmlToLevelAssetType(const string& levelFolder, const char* const szName) const;

	//ILevelEditor interface
	virtual bool IsLevelLoaded() override;
	//end ILevelEditor

private:
	void InitActions();

	virtual bool IsOnlyBackend() const override { return true; }

	virtual bool OnFind() override;

	bool OnNew();
	bool OnOpen();
	bool OnSave();
	bool OnSaveAs();
	bool OnDelete();
	bool OnDuplicate();

	bool OnCut();
	bool OnCopy();
	bool OnPaste();
	bool OnSelectAll();

	void         OnShowInAssetBrowser(const char* asset);

	void         OnCopyInternal(bool isCut = false);
	void         OnToggleAssetBrowser();

	void         SaveCryassetFile(const string& levelPath);

Q_SIGNALS:
	void VertexSnappingEnabled(bool bEnable);
	void PivotSnappingEnabled(bool bEnable);
	void GridSnappingEnabled(bool bEnable);
	void AngleSnappingEnabled(bool bEnable);
	void ScaleSnappingEnabled(bool bEnable);

	void TerrainSnappingEnabled(bool bEnable);
	void GeometrySnappingEnabled(bool bEnable);
	void SurfaceNormalSnappingEnabled(bool bEnable);
	void LevelLoaded(const QString& levelPath);
	void HelpersDisplayEnabled(bool bEnable);

private:
	CLevelExplorer*     m_pLevelExplorer;
	CEditorDialog*      m_pFindDialog;
	CQuickAssetBrowser* m_pQuickAssetBrowser;
	CEditorDialog*      m_pQuickAssetBrowserDialog;
	CTagLocations*      m_pTagLocations;
};
