// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DialogCommon.h"
#include "Viewport.h"

#include <map>

class CMaterialGenerator;
class CRcInOrderCaller;
class CTextureManager;

struct ICharacterInstance;

class QTemporaryDir;

class QPropertyTree;

namespace Serialization
{
class CContextList;
} // namespace Serialization

namespace FbxMetaData
{

struct SMetaData;

} // namespace FbxMetaData

namespace MeshImporter
{

namespace Private_DialogCAF
{

struct SProperties;
struct SAnimationClip;
class CRootMotionNodePanel;

} // namespace Private_DialogCAF

class CDialogCAF : public CBaseDialog, public QViewportConsumer
{
private:
	struct SAnimation
	{
		std::unique_ptr<QTemporaryDir> m_pTempDir;
		std::map<QString, QString> m_clipFilenames;  // Maps encoded animation clips to filenames.

		QString m_currentClip;
		bool m_bIsPlaying;

		SAnimation();
		~SAnimation();
	};

	struct SCharacter
	{
		_smart_ptr<ICharacterInstance> m_pCharacterInstance;
		std::unique_ptr<QTemporaryDir> m_pTempDir;
		QString                        m_filePath;

		SCharacter();
		~SCharacter();
	};

	struct SSkin
	{
		std::unique_ptr<QTemporaryDir> m_pTempDir;
		QString                        m_filePath;

		SSkin();
		~SSkin();
	};

	struct SScene : CSceneModel<SScene>
	{
		SAnimation                     m_animation;
		SCharacter                     m_defaultCharacter;
		SSkin                          m_defaultSkin;
		string                         m_material;
		CMaterial* m_pMaterial;
		_smart_ptr<ICharacterInstance> m_pCharInstance;

		bool                           m_bAnimation;
		bool                           m_bSkeleton;

		const FbxTool::SNode* m_pRootMotionNode;

		void SetRootMotionNode(const FbxTool::SNode* pNode);
		const FbxTool::SNode* GetRootMotionNode() const;

		SScene();
		~SScene();
	};
public:
	CDialogCAF();
	~CDialogCAF();

	bool Serialize(Serialization::IArchive& ar, Private_DialogCAF::SAnimationClip& anim);

	// CBaseDialog implementation.
	virtual void         AssignScene(const SImportScenePayload* pUserData) override;
	virtual void         OnIdleUpdate() override;
	virtual void         OnReleaseResources() override;
	virtual void         OnReloadResources() override;
	virtual const char*  GetDialogName() const override;
	virtual bool         MayUnloadScene() override;
	virtual int GetToolButtons() const override;

	virtual QString ShowSaveAsDialog();
	virtual bool         SaveAs(SSaveContext& ctx) override;

	virtual int GetOpenFileFormatFlags() override { return eFileFormat_I_CAF; }

	// QViewportConsumer implementation.
	virtual void OnViewportRender(const SRenderContext& rc) override;
private:
	void MakeNameUnique(Private_DialogCAF::SAnimationClip& clip) const;
	void MakeNamesUnique();

	void         CreateMetaDataCaf(FbxMetaData::SMetaData& metaData, const Private_DialogCAF::SAnimationClip& clip) const;
	void         CreateMetaDataChr(FbxMetaData::SMetaData& metaData) const;
	void         CreateMetaDataSkin(FbxMetaData::SMetaData& metaData) const;

	QString      GetSkeletonFilePath() const;
	QString      GetSkinFilePath() const;

	void ReloadAnimationSet();
	void StartAnimation(const QString& animFilename);

	void PreviewAnimationClip(const Private_DialogCAF::SAnimationClip& clip);
	void UpdateAnimation(const Private_DialogCAF::SAnimationClip& clip);
	void UpdateCurrentAnimation();
	void UpdateSkeleton();
	void UpdateSkin();
	void UpdateMaterial();
	void UpdateCharacter();

	void RcCaller_Assign(CRcInOrderCaller* pCaller, const QString& filePath, void* pUserData);
	void RcCaller_ChrAssign(CRcInOrderCaller* pCaller, const QString& filePath, void* pUserData);
	void RcCaller_SkinAssign(CRcInOrderCaller* pCaller, const QString& filePath, void* pUserData);
	void AssignMaterial(CMaterial* pMat, void* pUserData);

	void SetupUI();

	bool IsCurrentScene(int sceneId) const;

	SJointsViewSettings                                     m_viewSettings;
	CSplitViewportContainer*                                m_pViewportContainer;
	Private_DialogCAF::CRootMotionNodePanel* m_pRootMotionNodePanel;
	QPropertyTree* m_pPropertyTree;
	std::unique_ptr<Serialization::CContextList> m_pSerializationContextList;
	std::unique_ptr<Private_DialogCAF::SProperties> m_pSkeletonProperties;
	std::unique_ptr<SScene>                                 m_pScene;
	int                                                     m_sceneCounter;
	std::unique_ptr<CRcInOrderCaller>                       m_pAnimationRcCaller;
	std::unique_ptr<CRcInOrderCaller>                       m_pSkeletonRcCaller;
	std::unique_ptr<CRcInOrderCaller>                       m_pSkinRcCaller;
	std::shared_ptr<CTextureManager> m_pTextureManager;
	std::unique_ptr<CMaterialGenerator> m_pMaterialGenerator;
};

} // namespace MeshImporter

