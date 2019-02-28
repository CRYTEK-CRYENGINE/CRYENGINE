// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>

#include <memory>

class QPropertyTree;

struct ITaskHost;
class CCreateMaterialTask;
class CMaterial;
class CMaterialElement;
class CMaterialSettings;
class CMaterialView;
class CTextureManager;
class CTextureModel;
class CSortedMaterialModel;
class CMaterialGenerator;

namespace MeshImporter
{
class CSceneManager;
struct SImportScenePayload;
}

struct IMaterial;

class QMenu;
class QPushButton;

class CMaterialPanel : public QWidget
{
	Q_OBJECT
public:
	CMaterialPanel(MeshImporter::CSceneManager* pSceneManager, ITaskHost* pTaskHost, QWidget* pParent = nullptr);
	~CMaterialPanel();

	_smart_ptr<CMaterial>            GetMaterial();
	IMaterial*                       GetMatInfo();

	void                             AssignScene(const MeshImporter::SImportScenePayload* pPayload);
	void                             UnloadScene();

	void                             GenerateMaterial(void* pUserData = nullptr);
	void                             GenerateMaterial(const QString& mtlFilePath, void* pUserData = nullptr);
	void                             GenerateTemporaryMaterial(void* pUserData = nullptr);

	CMaterialSettings*               GetMaterialSettings();
	QPropertyTree*                   GetMaterialSettingsTree();
	CSortedMaterialModel*            GetMaterialModel();
	CMaterialView*                   GetMaterialView();

	void                             ApplyMaterial();

	std::shared_ptr<CTextureManager> GetTextureManager();
	CTextureModel*                   GetTextureModel();
signals:
	void                             SigGenerateMaterial(void* pUserData);
private:
	void                             SetupUi();
	void                             ApplyMaterial_Int();

	void                             OnMaterialCreated(CMaterial* pMaterial, void* pUserData);

	MeshImporter::CSceneManager*       m_pSceneManager;
	ITaskHost*                         m_pTaskHost;
	QPushButton*                       m_pGenerateMaterialButton;

	std::unique_ptr<CMaterialGenerator> m_pMaterialGenerator;

	// Engine material.
	std::unique_ptr<CMaterialSettings> m_pMaterialSettings;
	QPropertyTree*                     m_pMaterialSettingsTree;

	// Materials.
	std::unique_ptr<CSortedMaterialModel> m_pMaterialModel;
	CMaterialView*                     m_pMaterialList;

	// Textures.
	std::shared_ptr<CTextureManager>   m_pTextureManager;
	std::unique_ptr<CTextureModel>     m_pTextureModel;

	bool                               m_bApplyingMaterials;
};

