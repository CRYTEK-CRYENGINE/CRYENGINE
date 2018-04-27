// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// FbxTool main dialog
#pragma once
#include "AsyncHelper.h"
#include "FbxMetaData.h"
#include "FbxScene.h"
#include "Viewport.h"
#include "DialogCommon.h"
#include "DisplayOptions.h"

#include <Cry3DEngine/IStatObj.h>
#include <Cry3DEngine/IIndexedMesh.h>

#include <memory> // unique_ptr

class CSceneElementCommon;
class CSceneModel;
class CMaterialElement;

class CAsyncImportSceneTask;
class CCallRcTask;
class CRcInOrderCaller;

class CSplitViewportContainer;
class CSceneViewContainer;
class CSceneViewCgf;
class CMaterialViewContainer;
class CTargetMeshView;
class CMaterialPanel;
class CModelProperties;

class CGlobalImportSettings;

class CProxyData;
class CProxyGenerator;
class CPhysProxiesControlsWidget;

namespace DialogMesh
{

class CSceneUserData;

} // namespace DialogMesh
struct SEditorMetaData;

namespace Private_MainDialog
{
class CShowMeshesModeWidget;
}

#include <QAbstractItemModel>

class QAbstractItemView;

class CMainDialog
	: public MeshImporter::CBaseDialog
	  , public QViewportConsumer
	  , public FbxTool::CScene::IObserver
{
	Q_OBJECT
private:
	class CViewportHeader;
	class CSceneContextMenu;
public:
	struct SNodeRenderInfo
	{
		float selectionHeat;
		bool  bSelected;

		SNodeRenderInfo()
			: selectionHeat(0.0f)
			, bSelected(false)
		{}
	};

	struct SStatObjChild
	{
		_smart_ptr<IStatObj> pStatObj;
		const FbxTool::SNode* pNode;
		Matrix34              localToWorld;
		string                geoName;

		const FbxTool::SNode* GetNode() const         { return pNode; }
		Matrix34              GetLocalToWorld() const { return localToWorld; }
	};
public:
	struct SDisplayScene;

	CMainDialog(QWidget* pParent = nullptr);
	~CMainDialog();

	virtual void        AssignScene(const MeshImporter::SImportScenePayload* pUserData) override;

	virtual const char* GetDialogName() const override;

	void                UnloadScene();

	// Returns true if auto-lod generation must be enabled when updating the RC mesh.
	bool NeedAutoLod() const;

	// Create meta-data for all loaded scenes.
	
	enum ECreateMetaDataFlags
	{
		eCreateMetaDataFlags_OmitAutoLods = BIT(0)
	};

	// flags is combination of EMetaDataFlags.
	bool                CreateMetaData(FbxMetaData::SMetaData& metaData, const QString& sourceFilename, int flags = 0);

	void                ResetCamera();

	// FbxTool::CScene::IObserver implementation.
	virtual void OnNodeUserDataChanged(const FbxTool::SNode* pNode) override;
	virtual void OnMaterialUserDataChanged() override;
protected:
	// QWidget implementation.
	virtual bool eventFilter(QObject* pObject, QEvent* pEvent) override;
private:
	std::shared_ptr<SDisplayScene> m_displayScene;

	struct SHitInfo
	{
		const char* szLabel;

		SHitInfo()
		{
			Clear();
		}

		void Clear()
		{
			szLabel = "";
		}
	};
	SHitInfo m_sourceHitInfo;
	SHitInfo m_targetHitInfo;

	void setupUi(CMainDialog* MainDialog);
	void retranslateUi(CMainDialog* MainDialog);

	void Init();
	void InitMaterials();
	void ApplyMaterial();

	bool SaveCgf(const std::shared_ptr<QTemporaryDir>& pTempDir, const string& file, const QString& sourceFilename);
	bool SaveSkin(const std::shared_ptr<QTemporaryDir>& pTempDir, const string& file, const QString& sourceFilename);

	void ApplyMetaDataCommon(const FbxMetaData::SMetaData& metaData);
	void ApplyMetaDataSkin(const FbxMetaData::SMetaData& metaData);
	bool ApplyMetaData(const FbxMetaData::SMetaData& metaData);

	void            OnGlobalImportsTreeChanged();

	// Scene context menu.
	void            AddSourceNodeElementContextMenu(const QModelIndex& index, QMenu* pMenu);
	void            AddExpandCollapseChildrenContextMenu(const QModelIndex& index, QMenu* pMenu);
	void            AddExpandCollapseAllContextMenu(QMenu* pMenu);
	void            AddSkinFilterContextMenu(QMenu* pMenu);

	void            CreateMaterialContextMenu(QMenu* pMenu, CMaterialElement* pMaterialElement);

	void            SelectMaterialsOnMesh(const FbxTool::SMesh* pMesh);
	void            SelectMeshesUsingMaterial(const CMaterialElement* pMaterialGroup);
	void            SelectMeshesUsingMaterial(const FbxTool::SMaterial* pNeedle, const CSceneElementCommon* pHaystack);

	CSceneElementCommon* GetSelectedSceneElement();

	virtual void    OnViewportRender(const SRenderContext& rc) override;
	virtual void    OnViewportMouse(const SMouseEvent& ev) override;

	void            ClearSelectionHighlighting();
	void            MarkVisibleNodes();
	void            RenderCommon();
	void            RenderTargetView(const SRenderContext& rc);
	void            RenderSourceView(const SRenderContext& rc);

	void RenderStaticMesh(const SRenderContext& rc);
	void RenderSkin(const SRenderContext& rc);

	void            MouseTargetView(const SMouseEvent& ev);
	void            MouseSourceView(const SMouseEvent& ev);
	bool            IsNodeVisible(const FbxTool::SNode* pNode) const;

	bool            MayUnloadScene();
	virtual int     GetToolButtons() const override;

	virtual int     GetOpenFileFormatFlags() override;

	virtual QString ShowSaveAsDialog();

	virtual bool    SaveAs(SSaveContext& saveContext) override;

	virtual void    OnIdleUpdate() override;
	virtual void    OnReleaseResources() override;

	virtual void ReimportFile() override;
	void            OnOpenMetaData();

	void            TouchLastJson();

	TSmartPtr<CMaterial> m_pAutoMaterial;

	void     CreateUnmergedContent();

	bool                         m_bRefreshRcMesh;

	std::vector<SStatObjChild>   m_rcMeshUnmergedContent;
	std::vector<SNodeRenderInfo> m_nodeRenderInfoMap;
	std::vector<int>             m_nodeVisibilityMap;

	//! @name Mesh, UnmergedMesh, and unmerged content.
	//! @{
	//! When the preview needs to be updated, we create a new stat obj with the current metadata for preview.
	//! Here, we call this the "mesh". For selection highlighting of individual nodes, we always need an unmerged stat obj.
	//! Therefore, if the mesh is merged, we need to create the "unmerged mesh" as well.
	//! "Static meshes" always refers to both "mesh" and "unmerged meshes".

	std::unique_ptr<CTempRcObject> m_pMeshRcObject;
	std::unique_ptr<CTempRcObject> m_pUnmergedMeshRcObject;

	IStatObj* m_pMeshStatObj;
	IStatObj* m_pUnmergedMeshStatObj;
	IStatObj** m_ppSelectionMesh;

	//! Returns null, if there is no unmerged stat obj. Otherwise, the returned stat obj is either m_pMeshStatObj or m_pUnmergedMeshStatObj.
	IStatObj* FindUnmergedMesh();

	void UpdateStaticMeshes();
	void ReleaseStaticMeshes();

	void CreateMeshFromFile(const string& filePath);
	void CreateUnmergedMeshFromFile(const string& filePath);

	//! @}

	// Skin.
	std::unique_ptr<CTempRcObject> m_pSkinRcObject;
	TSmartPtr<ICharacterInstance> m_pCharacterInstance;

	void CreateSkinFromFile(const string& filePath);

	void CreateSkinMetaData(FbxMetaData::SMetaData& metaData);
	void UpdateSkin();

	//! Either creates static meshes, or skin, depending on current selection.
	void UpdateResources();

	// UI-thread only
	QString                              m_loadedMaterialFile;
	bool                                 m_bApplyingMaterials;
	std::vector<QMetaObject::Connection> m_connections;
	QString                              m_targetCgfFile;

	bool                                 m_bCameraNeedsReset;

	// We partition the source materials into groups of size MAX_SUB_MATERIALS. For every group,
	// an uber-material of type IMaterial is created that contains a sub-material for every source
	// material in its group.
	std::vector<TSmartPtr<IMaterial>> m_uberMaterials;

	// Indexed by FbxTool::SMaterial::id. Stores the engine material for each source material.
	std::vector<TSmartPtr<IMaterial>> m_materials;

	TSmartPtr<IMaterial>              m_proxyMaterial;

	TSmartPtr<IMaterial> m_pVertexColorMaterial;
	TSmartPtr<IMaterial> m_pVertexAlphaMaterial;

	std::unique_ptr<DialogMesh::CSceneUserData> m_pSceneUserData;

	// If true, we opened an already existing CGF file that references its material by relative path.
	// When overwriting the CGF in the same place, we try to preserve this setting.
	bool m_bMaterialNameWasRelative;

	// Models.
	std::unique_ptr<CSceneModel>           m_pSceneModel;

	CSceneViewContainer*                   m_pSceneViewContainer;
	CSceneViewCgf*                         m_pSceneTree;    // TODO: Remove this.
	CMaterialViewContainer*                m_pMaterialViewContainer;
	CTargetMeshView*                       m_pTargetMeshView;
	CSplitViewportContainer*               m_pViewportContainer;
	Private_MainDialog::CShowMeshesModeWidget* m_pShowMeshesModeWidget;
	QPropertyTree*                         m_pGlobalImportSettingsTree;
	QPropertyTree*                         m_pPropertyTree; // General properties of selected view item.
	std::unique_ptr<CSceneContextMenu> m_pSceneContextMenu;

	std::unique_ptr<CGlobalImportSettings> m_pGlobalImportSettings;

	std::unique_ptr<CAutoLodSettings>      m_pAutoLodSettings;
	bool m_bHasLods;
	bool m_bAutoLodCheckViewSettingsFlag;  // If true, changing the LOD viewport setting should trigger an update of the rc mesh.

	bool            m_bGlobalAabbNeedsRefresh;

	QString         m_initialFilePath;

	SViewSettings   m_viewSettings;

	CMaterialPanel* m_pMaterialPanel;

	std::unique_ptr<CModelProperties> m_pModelProperties;

	// Proxy generation.
	std::unique_ptr<CProxyData> m_pProxyData;
	std::unique_ptr<CProxyGenerator> m_pProxyGenerator;
	CPhysProxiesControlsWidget* m_pPhysProxiesControls;
};

