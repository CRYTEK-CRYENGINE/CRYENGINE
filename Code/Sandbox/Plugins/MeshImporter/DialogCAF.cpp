// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DialogCAF.h"
#include "AsyncHelper.h"
#include "AnimationHelpers/AnimationHelpers.h"
#include "RcLoader.h"
#include "RcCaller.h"
#include "FbxMetaData.h"
#include "FbxScene.h"
#include "RenderHelpers.h"
#include "DisplayOptions.h"
#include "MaterialSettings.h"
#include "MaterialHelpers.h"
#include "MaterialGenerator/MaterialGenerator.h"
#include "TextureManager.h"
#include "SceneModelSingleSelection.h"
#include "SceneView.h"
#include "SaveRcObject.h"
#include "ImporterUtil.h"

// EditorCommon
#include <CrySerialization/Decorators/Resources.h>
#include <FilePathUtil.h>
#include <ThreadingUtils.h>

// EditorQt
#include "QViewportSettings.h"

#include <CryAnimation/ICryAnimation.h>
#include <Material\Material.h>
#include <Material\MaterialManager.h>

#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <QPropertyTree/ContextList.h>

#include "Serialization/Decorators/EditorActionButton.h"

// Qt
#include <QLabel>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace MeshImporter
{

namespace Private_DialogCAF
{

const char* const s_defaultTake = "Default take";

static const QString GetRcOptions()
{
	return QtUtil::ToQString(CRcCaller::OptionOverwriteExtension("fbx"));
}

struct SAnimationClip
{
	string outputName;
	string takeName;
	int    startFrame;
	int    endFrame;

	// We store the number of clips in every Serialize() call, so that we can detect add operations.
	int clipCount;

	SAnimationClip()
		: takeName()
		, startFrame(-1)
		, endFrame(-1)
		, clipCount(-1)
	{}

	void Serialize(Serialization::IArchive& ar)
	{
		CDialogCAF* pOwner = ar.context<CDialogCAF>();
		CRY_ASSERT(pOwner);

		pOwner->Serialize(ar, *this);
	}
};

struct SRcPayload
{
	std::unique_ptr<QTemporaryFile> m_pMetaDataFile;
	int                             m_sceneId;
	QString                         m_compiledFilename;
	SAnimationClip                  m_clip;
	bool                            m_bSaveOnly;

	SRcPayload() : m_bSaveOnly(false) {}
};

struct SProperties
{
	std::vector<SAnimationClip> m_clips;

	void                        Serialize(Serialization::IArchive& ar)
	{
		ar(m_clips, "slips", "Animation clips");
	}
};

SAnimationClip CreateClip(const FbxTool::SAnimationTake* const pTake)
{
	using namespace Private_DialogCAF;

	SAnimationClip clip;
	clip.takeName = pTake->name.empty() ? s_defaultTake : pTake->name;
	clip.outputName = MakeAlphaNum(clip.takeName);
	clip.startFrame = pTake->startFrame;
	clip.endFrame = pTake->endFrame;
	return clip;
}

void CreateDefaultClips(std::vector<SAnimationClip>& clips, const FbxTool::CScene* pScene)
{
	const int takeCount = pScene->GetAnimationTakeCount();

	clips.clear();
	clips.reserve(takeCount);

	SAnimationClip clip;
	for (int i = 0; i < takeCount; ++i)
	{
		const FbxTool::SAnimationTake* const pTake = pScene->GetAnimationTakeByIndex(i);
		clips.push_back(CreateClip(pTake));
	}
}

string DottedTail(string str, int len = 20)
{
	const int d = (int)str.length() - len;
	if (d > 0)
	{
		return "..." + str.substr(d + 3);
	}
	return str;
}

QString Encode(const SAnimationClip& anim)
{
	return QString("%1%2%3").arg(anim.takeName.c_str()).arg(anim.startFrame).arg(anim.endFrame);
}

// ==================================================
// CRootMotionNodePanel
// ==================================================

class CRootMotionNodePanel : public QWidget
{
public:
	CRootMotionNodePanel(QWidget* pParent = nullptr);

	void Initialize(
		FbxTool::CScene* pScene,
		const CSceneModelSingleSelection::SetNode& setNode,
		const CSceneModelSingleSelection::GetNode& getNode);

	void Reset();
private:
	typedef std::unique_ptr<CSceneModelSingleSelection, std::function<void(CSceneModelSingleSelection*)>> SceneModelPointer;

	void SetupUi();

	SceneModelPointer m_pSceneModel;
	CSceneViewCommon* m_pSceneView;
	QLabel* m_pLabel;
};

CRootMotionNodePanel::CRootMotionNodePanel(QWidget* pParent /* = nullptr */)
	: QWidget(pParent)
{
	SetupUi();
}

void CRootMotionNodePanel::Initialize(
	FbxTool::CScene* pScene,
	const CSceneModelSingleSelection::SetNode& setNode,
	const CSceneModelSingleSelection::GetNode& getNode)
{
	if (!pScene)
	{
		return;
	}

	m_pSceneModel = SceneModelPointer(new CSceneModelSingleSelection(), [](CSceneModelSingleSelection* p) { p->deleteLater(); });
	m_pSceneModel->SetPseudoRootVisible(false);
	m_pSceneModel->SetNodeAccessors(setNode, getNode);
	m_pSceneModel->SetScene(pScene);

	auto setLabel = [this, getNode]()
	{
		const FbxTool::SNode* const pNode = getNode();
		const QString label = tr("Root motion node: %1").arg(pNode ? pNode->szName : "auto-detect");
		m_pLabel->setText(label);
	};

	connect(m_pSceneModel.get(), &CSceneModelSingleSelection::modelReset, setLabel);
	setLabel();  // Set initial text.

	m_pSceneView->setModel(m_pSceneModel.get());
}

void CRootMotionNodePanel::Reset()
{
	if (m_pSceneModel)
	{
		m_pSceneModel->Reset();
	}
}

void CRootMotionNodePanel::SetupUi()
{
	m_pSceneView = new CSceneViewCommon();

	m_pLabel = new QLabel(tr("Root motion node: "));

	QVBoxLayout* const pMainLayout = new QVBoxLayout();
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(m_pLabel);
	pMainLayout->addWidget(m_pSceneView);
	setLayout(pMainLayout);
}

} // namespace Private_DialogCAF

CDialogCAF::SAnimation::SAnimation()
	: m_bIsPlaying(false)
{
}

CDialogCAF::SAnimation::~SAnimation() {}

CDialogCAF::SCharacter::SCharacter()
{
}

CDialogCAF::SCharacter::~SCharacter() {}

CDialogCAF::SSkin::SSkin()
{
}

CDialogCAF::SSkin::~SSkin() {}

CDialogCAF::SScene::SScene()
	: m_bAnimation(false)
	, m_bSkeleton(false)
	, m_pRootMotionNode(nullptr)
	, m_pMaterial(nullptr)
{}

CDialogCAF::SScene::~SScene() {}

void CDialogCAF::SScene::SetRootMotionNode(const FbxTool::SNode* pNode)
{
	if (pNode != m_pRootMotionNode)
	{
		m_pRootMotionNode = pNode;
		Notify();
	}
}

const FbxTool::SNode* CDialogCAF::SScene::GetRootMotionNode() const
{
	return m_pRootMotionNode;
}

CDialogCAF::CDialogCAF()
	: m_pViewportContainer(nullptr)
{
	using namespace Private_DialogCAF;

	m_pTextureManager.reset(new CTextureManager());

	// Material generator.
	m_pMaterialGenerator.reset(new CMaterialGenerator());
	m_pMaterialGenerator->SetTextureManager(m_pTextureManager);
	m_pMaterialGenerator->SetSceneManager(&GetSceneManager());
	m_pMaterialGenerator->SetTaskHost(GetTaskHost());
	m_pMaterialGenerator->sigMaterialGenerated.Connect(this, &CDialogCAF::AssignMaterial);  // Update subsets.

	m_pSkeletonProperties.reset(new SProperties());

	m_pSerializationContextList.reset(new Serialization::CContextList());
	m_pSerializationContextList->Update<CDialogCAF>(this);

	SetupUI();

	m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport()->AddConsumer(this);

	// Register RC callbacks.
	m_pAnimationRcCaller.reset(new CRcInOrderCaller());
	m_pAnimationRcCaller->SetAssignCallback(std::bind(&CDialogCAF::RcCaller_Assign, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	m_pAnimationRcCaller->SetDiscardCallback(&DeletePayload<SRcPayload> );

	m_pSkeletonRcCaller.reset(new CRcInOrderCaller());
	m_pSkeletonRcCaller->SetAssignCallback(std::bind(&CDialogCAF::RcCaller_ChrAssign, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	m_pSkeletonRcCaller->SetDiscardCallback(&DeletePayload<SRcPayload> );

	m_pSkinRcCaller.reset(new CRcInOrderCaller());
	m_pSkinRcCaller->SetAssignCallback(std::bind(&CDialogCAF::RcCaller_SkinAssign, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	m_pSkinRcCaller->SetDiscardCallback(&DeletePayload<SRcPayload> );
}

CDialogCAF::~CDialogCAF()
{
	m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport()->RemoveConsumer(this);
}

void CDialogCAF::MakeNameUnique(Private_DialogCAF::SAnimationClip& clip) const
{
	using namespace Private_DialogCAF;

	auto findOther = [&clip](const std::vector<SAnimationClip>& clips)
	{
		return std::any_of(clips.begin(), clips.end(), [&clip](const SAnimationClip& other)
			{
				return &other != &clip && other.outputName == clip.outputName;
		  });
	};

	int suffix = 1;
	const auto& clips = m_pSkeletonProperties->m_clips;
	const string savedName = clip.outputName;
	while (findOther(clips))
	{
		clip.outputName.Format("%s%d", savedName.c_str(), suffix++);
	}
}

void CDialogCAF::MakeNamesUnique()
{
	for (auto& clip : m_pSkeletonProperties->m_clips)
	{
		MakeNameUnique(clip);
	}
}

bool CDialogCAF::Serialize(Serialization::IArchive& ar, Private_DialogCAF::SAnimationClip& anim)
{
	using namespace Private_DialogCAF;

	CRY_ASSERT(ar.isEdit());

	const FbxTool::CScene* const pScene = GetScene();

	if (!pScene)
	{
		return false;
	}

	const bool bInitClip = anim.startFrame < 0 || anim.endFrame < 0;

	if (bInitClip)
	{
		// First time initialization. Set to first take.
		anim = CreateClip(pScene->GetAnimationTakeByIndex(0));
		MakeNameUnique(anim);
	}

	const string savedOutputName = anim.outputName;
	ar(anim.outputName, "outputName", "^Output name");

	const int clipCount = (int)m_pSkeletonProperties->m_clips.size();

	// Name changed by user.
	if (savedOutputName != anim.outputName && clipCount == anim.clipCount)
	{
		MakeNameUnique(anim);
	}

	anim.clipCount = clipCount;

	Serialization::StringList takes;
	for (int i = 0; i < pScene->GetAnimationTakeCount(); ++i)
	{
		const string name = pScene->GetAnimationTakeByIndex(i)->name;
		takes.push_back(name.empty() ? s_defaultTake : name.c_str());
	}
	Serialization::StringListValue dropDown(takes, anim.takeName.c_str());
	if (!ar(dropDown, "take", "Take"))
	{
		return false;
	}
	if (ar.isInput())
	{
		anim.takeName = dropDown.c_str();
	}

	ar(anim.startFrame, "startFrame", "Start frame");
	ar(anim.endFrame, "endFrame", "End frame");

	ar(Serialization::ActionButton([this, anim]()
		{
			PreviewAnimationClip(anim);
	  }), "previewThis", "Preview this");

	return true;
}

void CDialogCAF::AssignScene(const SImportScenePayload* pUserData)
{
	using namespace Private_DialogCAF;

	m_pScene.reset(new SScene());
	m_pScene->AddCallback([this](CDialogCAF::SScene*)
	{
		m_pRootMotionNodePanel->Reset();
	});

	// Create temporary directories.
	m_pScene->m_animation.m_pTempDir = CreateTemporaryDirectory();

	UpdateSkeleton();
	UpdateSkin();
	
	AutoAssignSubMaterialIDs(std::vector<std::pair<int, QString>>(), GetScene());

	m_pTextureManager->Init(GetSceneManager());

	UpdateMaterial();

	CreateDefaultClips(m_pSkeletonProperties->m_clips, GetScene());
	m_pPropertyTree->revert();
	m_pPropertyTree->setExpandLevels(1);

	{
		auto setRootNode = [this](const FbxTool::SNode* pNode)
		{
			m_pScene->SetRootMotionNode(pNode);
		};

		auto getRootNode = [this]()
		{
			return m_pScene->GetRootMotionNode();
		};

		m_pRootMotionNodePanel->Initialize(GetScene(), setRootNode, getRootNode);
	}
}

void CDialogCAF::UpdateMaterial()
{
	using namespace Private_DialogCAF;

	std::unique_ptr<SRcPayload> pPayload(new SRcPayload());
	pPayload->m_sceneId = GetSceneManager().GetTimestamp();
	m_pMaterialGenerator->GenerateTemporaryMaterial(pPayload.release());
}

void CDialogCAF::AssignMaterial(CMaterial* pMat, void* pUserData)
{
	using namespace Private_DialogCAF;
	std::unique_ptr<SRcPayload> pPayload((SRcPayload*)pUserData);

	if (IsLoadingSuspended())
	{
		return;
	}

	if (!IsCurrentScene(pPayload->m_sceneId))
	{
		return;
	}

	if (!pMat)
	{
		return;
	}

	m_pScene->m_pMaterial = pMat;

	UpdateSkin();
}

QString CreateCHPARAMS(const QString& path)
{
	const QString animPath = !path.isEmpty() ? path : QString(".");
	static const char* const szFormat =
	  "<Params>\n"
	  " <AnimationList>\n"
	  "  <Animation name=\"#filepath\" path=\"%1\"/>\n"
	  "  <Animation name=\"*\" path=\"*/*.caf\"/>\n"
	  "  <Animation name=\"*\" path=\"*/*.bspace\"/>\n"
	  "  <Animation name=\"*\" path=\"*/*.comb\"/>\n"
	  " </AnimationList>\n"
	  "</Params>\n";
	return QString(szFormat).arg(animPath);
}

void CDialogCAF::PreviewAnimationClip(const Private_DialogCAF::SAnimationClip& clip)
{
	using namespace Private_DialogCAF;

	if (!m_pScene)
	{
		return;
	}

	QString encodedClip = Encode(clip);

	m_pScene->m_animation.m_currentClip = encodedClip;
	m_pScene->m_animation.m_bIsPlaying = false;

	auto it = m_pScene->m_animation.m_clipFilenames.find(encodedClip);
	if (it == m_pScene->m_animation.m_clipFilenames.end())
	{
		UpdateAnimation(clip);
	}
	else
	{
		StartAnimation(it->second);

		m_pScene->m_animation.m_bIsPlaying = true;
	}
}

void CDialogCAF::UpdateAnimation(const Private_DialogCAF::SAnimationClip& clip)
{
	using namespace Private_DialogCAF;

	CRY_ASSERT(m_pScene && GetSceneManager().GetScene());

	FbxMetaData::SMetaData metaData;
	CreateMetaDataCaf(metaData, clip);

	std::unique_ptr<SRcPayload> pPayload(new SRcPayload());
	pPayload->m_pMetaDataFile = WriteTemporaryFile(m_pScene->m_animation.m_pTempDir->path(), metaData.ToJson());
	pPayload->m_sceneId = GetSceneManager().GetTimestamp();
	pPayload->m_clip = clip;

	// We keep the temporary animation files around, to prevent Qt from reusing the same filenames.
	// Reusing filenames might overwrite animation files.
	pPayload->m_pMetaDataFile->setAutoRemove(false);

	const QString metaDataFilename = pPayload->m_pMetaDataFile->fileName();
	m_pAnimationRcCaller->CallRc(metaDataFilename, pPayload.release(), GetTaskHost(), GetRcOptions());
}

void CDialogCAF::UpdateCurrentAnimation()
{	
	using namespace Private_DialogCAF;

	if (!m_pScene)
	{
		return;
	}

	m_pScene->m_animation.m_bIsPlaying = false;

	auto it = m_pScene->m_animation.m_clipFilenames.find(m_pScene->m_animation.m_currentClip);
	if (it != m_pScene->m_animation.m_clipFilenames.end())
	{
		StartAnimation(it->second);

		m_pScene->m_animation.m_bIsPlaying = true;
	}

}

void CDialogCAF::UpdateSkeleton()
{
	using namespace Private_DialogCAF;

	CRY_ASSERT(m_pScene && GetSceneManager().GetScene());

	m_pScene->m_defaultCharacter.m_pTempDir = CreateTemporaryDirectory();

	FbxMetaData::SMetaData metaData;
	CreateMetaDataChr(metaData);

	std::unique_ptr<SRcPayload> pPayload(new SRcPayload());
	pPayload->m_pMetaDataFile = WriteTemporaryFile(m_pScene->m_defaultCharacter.m_pTempDir->path(), metaData.ToJson());
	pPayload->m_sceneId = GetSceneManager().GetTimestamp();

	const QString metaDataFilename = pPayload->m_pMetaDataFile->fileName();
	m_pSkeletonRcCaller->CallRc(metaDataFilename, pPayload.release(), GetTaskHost(), GetRcOptions());
}

void CDialogCAF::UpdateSkin()
{
	using namespace Private_DialogCAF;

	CRY_ASSERT(m_pScene && GetSceneManager().GetScene());

	m_pScene->m_defaultSkin.m_pTempDir = CreateTemporaryDirectory();

	FbxMetaData::SMetaData metaData;
	CreateMetaDataSkin(metaData);

	std::unique_ptr<SRcPayload> pPayload(new SRcPayload());
	pPayload->m_pMetaDataFile = WriteTemporaryFile(m_pScene->m_defaultSkin.m_pTempDir->path(), metaData.ToJson());
	pPayload->m_sceneId = GetSceneManager().GetTimestamp();

	const QString metaDataFilename = pPayload->m_pMetaDataFile->fileName();
	m_pSkinRcCaller->CallRc(metaDataFilename, pPayload.release(), GetTaskHost(), GetRcOptions());
}

QString CDialogCAF::GetSkeletonFilePath() const
{
	return m_pScene->m_defaultCharacter.m_filePath;
}

QString CDialogCAF::GetSkinFilePath() const
{
	return m_pScene->m_defaultSkin.m_filePath;
}

void CDialogCAF::UpdateCharacter()
{
	const bool bComplete = m_pScene->m_bSkeleton && m_pScene->m_bAnimation;
	if (!bComplete)
	{
		return;
	}

	if (GetSkeletonFilePath().isEmpty())
	{
		return;
	}

	if (!m_pScene->m_pCharInstance)
	{
		m_pScene->m_pCharInstance = CreateTemporaryCharacter(
			GetSkeletonFilePath(),
			GetSkinFilePath(),
			QtUtil::ToQString(m_pScene->m_material));
	}

	ReloadAnimationSet();

	UpdateCurrentAnimation();
}

void CDialogCAF::OnIdleUpdate()
{
	using namespace Private_DialogCAF;

	m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport()->Update();
}

void CDialogCAF::OnReleaseResources()
{
	if (!m_pScene)
	{
		return;
	}

	m_pScene->m_animation.m_bIsPlaying = false;
}

void CDialogCAF::OnReloadResources()
{
	if (!m_pScene)
	{
		return;
	}

	UpdateCurrentAnimation();
}

void CDialogCAF::OnViewportRender(const SRenderContext& rc)
{
	GetIEditor()->GetSystem()->GetIAnimationSystem()->UpdateStreaming(-1, -1);

	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	if (!m_pScene || !m_pScene->m_pCharInstance)
	{
		return;
	}

	m_pScene->m_pCharInstance->StartAnimationProcessing(SAnimationProcessParams());
	m_pScene->m_pCharInstance->FinishAnimationComputations();

	SRendParams rp = *rc.renderParams;
	assert(rp.pMatrix);
	assert(rp.pPrevMatrix);

	Matrix34 m_LocalEntityMat(IDENTITY);
	rp.pMatrix = &m_LocalEntityMat;

	const SRenderingPassInfo& passInfo = *rc.passInfo;
	auto pInstanceBase = m_pScene->m_pCharInstance;
	gEnv->p3DEngine->PrecacheCharacter(NULL, 1.f, pInstanceBase, pInstanceBase->GetIMaterial(), m_LocalEntityMat, 0, 1.f, 4, true, passInfo);
	pInstanceBase->SetViewdir(rc.camera->GetViewdir());
	pInstanceBase->Render(rp, passInfo);

	DrawSkeleton(pAuxGeom, &m_pScene->m_pCharInstance->GetIDefaultSkeleton(), m_pScene->m_pCharInstance->GetISkeletonPose(),
	             QuatT(IDENTITY), "", rc.viewport->GetState().cameraTarget);

	// draw joint names
	const ISkeletonPose& skeletonPose = *m_pScene->m_pCharInstance->GetISkeletonPose();
	const IDefaultSkeleton& defaultSkeleton = m_pScene->m_pCharInstance->GetIDefaultSkeleton();
	uint32 numJoints = defaultSkeleton.GetJointCount();

	if (m_viewSettings.m_bShowJointNames)
	{
		for (int j = 0; j < numJoints; j++)
		{
			const char* pJointName = defaultSkeleton.GetJointNameByID(j);

			QuatT jointTM = QuatT(skeletonPose.GetAbsJointByID(j));
			IRenderAuxText::DrawLabel(jointTM.t, 2, pJointName);
		}
	}

	// DEBUG
	{
		string label;
		label.Format("Anim: current clip = %s, is playing = %d", m_pScene->m_animation.m_currentClip.toLocal8Bit().constData(), m_pScene->m_animation.m_bIsPlaying);
		static float black[4] = { 0.0f };
		pAuxGeom->Draw2dLabel(10, 40, 1.5, black, false, label.c_str());
	}
}

const char* CDialogCAF::GetDialogName() const
{
	return "Animation";
}

bool CDialogCAF::MayUnloadScene()
{
	return true;
}

int CDialogCAF::GetToolButtons() const
{
	return eToolButtonFlags_Import | eToolButtonFlags_Save;
}

QString CDialogCAF::ShowSaveAsDialog()
{
	QString filePath = ShowSaveToDirectoryPrompt();
	return QtUtil::ToQString(PathUtil::AddSlash(QtUtil::ToString(filePath)));
}

bool CDialogCAF::SaveAs(SSaveContext& ctx)
{
	using namespace Private_DialogCAF;

	std::shared_ptr<QTemporaryDir> pTempDir(std::move(ctx.pTempDir));

	struct SClipSaveState
	{
		SRcObjectSaveState rcSaveState;
		string clipName;
	};

	std::vector<SClipSaveState> clipSaveStates;
	clipSaveStates.reserve(m_pSkeletonProperties->m_clips.size());
	for (const SAnimationClip& clip : m_pSkeletonProperties->m_clips)
	{
		FbxMetaData::SMetaData metaData;
		CreateMetaDataCaf(metaData, clip);

		clipSaveStates.emplace_back();
		SClipSaveState& saveState = clipSaveStates.back();
		CaptureRcObjectSaveState(pTempDir, metaData, saveState.rcSaveState);
		saveState.clipName = clip.outputName;
	}

	const string targetDirPath = PathUtil::GetPathWithoutFilename(ctx.targetFilePath);

	const QString absOriginalFilePath = GetSceneManager().GetImportFile()->GetOriginalFilePath();
	ThreadingUtils::Async([clipSaveStates, absOriginalFilePath, targetDirPath]()
	{
		// Asset relative path to directory. targetFilePath is absolute path.
		const string dir = PathUtil::AbsolutePathToGamePath(targetDirPath);

		const std::pair<bool, string> ret = CopySourceFileToDirectoryAsync(QtUtil::ToString(absOriginalFilePath), dir).get();
		if (!ret.first)
		{
			const string& error = ret.second;
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Copying source file to '%s' failed: '%s'",
				dir.c_str(), error.c_str());
			return;
		}

		for (const SClipSaveState& clipSaveState : clipSaveStates)
		{
			const string targetFilePath = PathUtil::ReplaceExtension(PathUtil::Make(targetDirPath, clipSaveState.clipName), "caf");
			SaveRcObjectAsync(clipSaveState.rcSaveState, targetFilePath);
		}
	});

	return true;
}

void CDialogCAF::CreateMetaDataCaf(FbxMetaData::SMetaData& metaData, const Private_DialogCAF::SAnimationClip& clip) const
{
	metaData.Clear();

	metaData.sourceFilename = QtUtil::ToString(GetSceneManager().GetImportFile()->GetFilePath());

	metaData.animationClip.takeName = strcmp(clip.takeName, Private_DialogCAF::s_defaultTake) ? clip.takeName : "";
	metaData.animationClip.startFrame = clip.startFrame;
	metaData.animationClip.endFrame = clip.endFrame;
	metaData.outputFileExt = "caf";

	const FbxTool::SNode* const pRootMotionNode = m_pScene->GetRootMotionNode();
	if (pRootMotionNode)
	{
		metaData.animationClip.motionNodePath = FbxTool::GetPath(pRootMotionNode);
	}
}

void CDialogCAF::CreateMetaDataChr(FbxMetaData::SMetaData& metaData) const
{
	metaData.Clear();

	metaData.sourceFilename = QtUtil::ToString(GetSceneManager().GetImportFile()->GetFilePath());
	metaData.outputFileExt = "chr";
}

void CDialogCAF::CreateMetaDataSkin(FbxMetaData::SMetaData& metaData) const
{
	metaData.Clear();

	metaData.sourceFilename = QtUtil::ToString(GetSceneManager().GetImportFile()->GetFilePath());
	metaData.outputFileExt = "skin";

	WriteMaterialMetaData(GetScene(), metaData.materialData);

	if (m_pScene->m_pMaterial)
	{
		metaData.materialFilename = m_pScene->m_pMaterial->GetName();
	}
}

void CDialogCAF::RcCaller_Assign(CRcInOrderCaller* pCaller, const QString& filePath, void* pUserData)
{
	using namespace Private_DialogCAF;

	std::unique_ptr<SRcPayload> pPayload((SRcPayload*)pUserData);

	const QString cafFilePath = PathUtil::ReplaceExtension(filePath, "caf");

	if (!pPayload->m_compiledFilename.isEmpty())
	{
		const QString newFilePath = QFileInfo(filePath).dir().path() + "/" + PathUtil::ReplaceExtension(pPayload->m_compiledFilename, "caf");
		QFile file(cafFilePath);
		RenameAllowOverwrite(file, newFilePath);
	}

	const QString encodedClip = Encode(pPayload->m_clip);
	m_pScene->m_animation.m_clipFilenames[encodedClip] = QFileInfo(filePath).baseName();

	if (!pPayload->m_bSaveOnly && !IsLoadingSuspended() && IsCurrentScene(pPayload->m_sceneId))
	{
		m_pScene->m_bAnimation = true;

		UpdateCharacter();
		PreviewAnimationClip(pPayload->m_clip);
	}
}

void CDialogCAF::RcCaller_ChrAssign(CRcInOrderCaller* pCaller, const QString& filePath, void* pUserData)
{
	using namespace Private_DialogCAF;
	std::unique_ptr<SRcPayload> pPayload((SRcPayload*)pUserData);

	if (IsLoadingSuspended())
	{
		return;
	}

	if (!IsCurrentScene(pPayload->m_sceneId))
	{
		return;
	}

	const QString chrFilePath = PathUtil::ReplaceExtension(filePath, "chr");

	m_pScene->m_defaultCharacter.m_filePath = chrFilePath;
	m_pScene->m_bSkeleton = true;

	UpdateCharacter();
}

void CDialogCAF::RcCaller_SkinAssign(CRcInOrderCaller* pCaller, const QString& filePath, void* pUserData)
{
	using namespace Private_DialogCAF;
	std::unique_ptr<SRcPayload> pPayload((SRcPayload*)pUserData);

	if (IsLoadingSuspended())
	{
		return;
	}

	if (!IsCurrentScene(pPayload->m_sceneId))
	{
		return;
	}

	const QString skinFilePath = PathUtil::ReplaceExtension(filePath, "skin");

	m_pScene->m_defaultSkin.m_filePath = skinFilePath;

	if (m_pScene->m_pCharInstance)
	{
		m_pScene->m_pCharInstance.reset();
	}
	UpdateCharacter();
}

void CDialogCAF::SetupUI()
{
	m_pViewportContainer = new CSplitViewportContainer();

	m_pViewportContainer->GetDisplayOptionsWidget()->SetUserOptions(Serialization::SStruct(m_viewSettings), "viewSettings", "View");

	m_pPropertyTree = new QPropertyTree();
	m_pPropertyTree->setArchiveContext(m_pSerializationContextList->Tail());
	m_pPropertyTree->attach(Serialization::SStruct(*m_pSkeletonProperties.get()));
	connect(m_pPropertyTree, &QPropertyTree::signalChanged, this, &CDialogCAF::MakeNamesUnique);

	m_pRootMotionNodePanel = new Private_DialogCAF::CRootMotionNodePanel();

	QTabWidget* const pTabWidget = new QTabWidget;
	pTabWidget->addTab(m_pPropertyTree, "Clips");
	pTabWidget->addTab(m_pRootMotionNodePanel, "Root motion");

	QSplitter* const pCenterSplitter = new QSplitter(Qt::Horizontal);
	pCenterSplitter->addWidget(pTabWidget);
	pCenterSplitter->addWidget(m_pViewportContainer);

	pCenterSplitter->setStretchFactor(0, 0);
	pCenterSplitter->setStretchFactor(1, 1);

	QVBoxLayout* const pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->addWidget(pCenterSplitter);
	if (layout())
	{
		delete layout();
	}
	setLayout(pLayout);
}

QString CreateCDF(
  const QString& skeletonFilePath,
  const QString& skinFilePath,
  const QString& materialFilePath)
{
	static const char* const szCdfContent =
	  "<CharacterDefinition>\n"
	  " <Model File=\"%1\"/>\n"
	  " %2"
	  "</CharacterDefinition>\n";

	static const char* const szSkinContent =
	  " <AttachmentList>\n"
	  "  <Attachment Type=\"CA_SKIN\" AName=\"the_skin\" Binding=\"%1\" %2 Flags=\"0\"/>\n"
	  " </AttachmentList>\n";

	if (skeletonFilePath.isEmpty())
	{
		return QString();
	}

	QString attachments;

	QString material;
	if (!materialFilePath.isEmpty())
	{
		material = QString("Material=\"%1\"").arg(materialFilePath);
	}

	if (!skinFilePath.isEmpty())
	{
		attachments = QString(szSkinContent).arg(skinFilePath).arg(material);
	}

	return QString(szCdfContent).arg(skeletonFilePath).arg(attachments);
}

void CDialogCAF::ReloadAnimationSet()
{
	using namespace Private_DialogCAF;

	if (!m_pScene->m_pCharInstance)
	{
		return;
	}

	const QString chParams = CreateCHPARAMS(m_pScene->m_animation.m_pTempDir->path());

	ICharacterManager* const pCharManager = GetIEditor()->GetSystem()->GetIAnimationSystem();
	const char* const szFilenameCHRPARAMS = PathUtil::ReplaceExtension(GetSkeletonFilePath(), "chrparams").toLocal8Bit().constData();
	pCharManager->InjectCHRPARAMS(szFilenameCHRPARAMS, chParams.toLocal8Bit().constData(), chParams.length());
	m_pScene->m_pCharInstance->ReloadCHRPARAMS();
	pCharManager->ClearCHRPARAMSCache();
}

void CDialogCAF::StartAnimation(const QString& animFilename)
{
	if (!m_pScene->m_pCharInstance)
	{
		return;
	}

	ISkeletonAnim& skeletonAnim = *m_pScene->m_pCharInstance->GetISkeletonAnim();
	const int localAnimID = m_pScene->m_pCharInstance->GetIAnimationSet()->GetAnimIDByName(QtUtil::ToString(animFilename));
	if (localAnimID < 0)
	{
		return;
	}

	const float normalizedTime = 0.0f;

	CryCharAnimationParams params;
	params.m_fPlaybackSpeed = 1.0f;
	params.m_fTransTime = 0;
	params.m_fPlaybackWeight = 1.0f;
	params.m_nLayerID = 0;
	params.m_fKeyTime = normalizedTime;
	params.m_nFlags = CA_FORCE_SKELETON_UPDATE | CA_ALLOW_ANIM_RESTART;
	params.m_nFlags |= CA_LOOP_ANIMATION;
	skeletonAnim.StartAnimationById(localAnimID, params);

	m_pScene->m_pCharInstance->SetPlaybackScale(1.0f);
}

bool CDialogCAF::IsCurrentScene(int sceneId) const
{
	return GetScene() && GetSceneManager().GetTimestamp() == sceneId;
}

} // namespace MeshImporter

