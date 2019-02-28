// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DialogCHR.h"
#include "DialogCHR_SceneUserData.h"
#include "SceneModelSkeleton.h"
#include "SceneView.h"
#include "SceneContextMenu.h"
#include "Viewport.h"
#include "AsyncHelper.h"
#include "AnimationHelpers/AnimationHelpers.h"
#include "RcLoader.h"
#include "FbxMetaData.h"
#include "RenderHelpers.h"
#include "Scene/SceneElementSourceNode.h"
#include "Scene/SceneElementTypes.h"
#include "Scene/Scene.h"
#include "Scene/SourceSceneHelper.h"
#include "QtCommon.h"
#include "MaterialGenerator/MaterialGenerator.h"
#include "MaterialHelpers.h"
#include "SkeletonPoser/SkeletonPoser.h"
#include "TempRcObject.h"
#include "SaveRcObject.h"
#include "DisplayOptions.h"
#include "Serialization/Serialization.h"
#include "SkeletonHelpers/SkeletonHelpers.h"
#include "ImporterUtil.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryPhysics/IPhysicsDebugRenderer.h>

#include <FilePathUtil.h>
#include <QtViewPane.h>
#include <Material/Material.h>
#include <Material/MaterialManager.h>
#include <ThreadingUtils.h>

// EditorQt
#include "QViewportSettings.h"

#include <Controls/QMenuComboBox.h>
#include <Serialization/Decorators/EditorActionButton.h>

#include <Serialization/QPropertyTree/QPropertyTree.h>

#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QSplitter>
#include <QLabel>

namespace MeshImporter
{

namespace Private_DialogCHR
{

void MarkSubtreeWithoutRoot(const FbxTool::SNode* pSubtreeRoot, std::vector<bool>& mark)
{
	std::vector<const FbxTool::SNode*> elementStack;

	for (int i = 0; i < pSubtreeRoot->numChildren; ++i)
	{
		elementStack.push_back(pSubtreeRoot->ppChildren[i]);
	}

	while (!elementStack.empty())
	{
		const FbxTool::SNode* const pElement = elementStack.back();
		elementStack.pop_back();

		mark[pElement->id] = true;

		for (int i = 0; i < pElement->numChildren; ++i)
		{
			elementStack.push_back(pElement->ppChildren[i]);
		}
	}
}

class CPreviewModeWidget : public QWidget
{
public:
	enum EPreviewMode
	{
		ePreviewMode_Character,
		ePreviewMode_StaticGeometry
	};

	explicit CPreviewModeWidget(QWidget* pParent = nullptr)
		: QWidget(pParent)
	{
		m_pPreviewMode = new QMenuComboBox();

		m_pPreviewMode->Clear();
		m_pPreviewMode->AddItem(tr("Show character"), ePreviewMode_Character);
		m_pPreviewMode->AddItem(tr("Show all meshes"), ePreviewMode_StaticGeometry);

		QVBoxLayout* const pLayout = new QVBoxLayout();
		pLayout->setContentsMargins(0, 0, 0, 0);
		pLayout->addWidget(m_pPreviewMode);
		setLayout(pLayout);
	}

	EPreviewMode GetPreviewMode() const
	{
		const int currentIndex = m_pPreviewMode->GetCheckedItem();
		CRY_ASSERT(currentIndex >= 0);  // Is single-selection?
		return (EPreviewMode)m_pPreviewMode->GetData(currentIndex).toInt();
	}
private:
	QMenuComboBox* m_pPreviewMode;
};

struct SViewSettings
{
	bool m_bShowSkin;
	bool m_bShowPhysicalProxies;
	bool m_bShowJointLimits;
	bool m_bShowJointNames;

	SViewSettings()
		: m_bShowSkin(true)
		, m_bShowPhysicalProxies(false)
		, m_bShowJointLimits(true)
		, m_bShowJointNames(false)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(m_bShowSkin, "showSkin", "Show Skin");
		ar(m_bShowPhysicalProxies, "showPhysicalProxies", "Physical Proxies");
		ar(m_bShowJointLimits, "showRagdollJointLimits", "Ragdoll Joint Limits");
		ar(m_bShowJointNames, "showJointNames", "Show joint names");
	}
};

} // namespace Private_DialogCHR

namespace Private_DialogCHR
{

bool IsMeshElement(const CSceneElementCommon* pSceneElement)
{
	return pSceneElement->GetType() == ESceneElementType::SourceNode && ((CSceneElementSourceNode*)pSceneElement)->GetNode()->numMeshes;
}

void GetListOfMeshElements(CScene* pScene, DynArray<string>& names, DynArray<CSceneElementCommon*>& elements)
{
	for (int i = 0; i < pScene->GetElementCount(); ++i)
	{
		CSceneElementCommon* const pSceneElement = pScene->GetElementByIndex(i);
		if (IsMeshElement(pSceneElement))
		{
			names.push_back(pSceneElement->GetName());
			elements.push_back(pSceneElement);
		}
	}
}

} // namespace Private_DialogCHR

CDialogCHR::SCharacter::~SCharacter()
{
	DestroyPhysics();
}

void CDialogCHR::SCharacter::DestroyPhysics()
{
	IPhysicalWorld* const pPhysWorld = gEnv->pPhysicalWorld;
	if (!pPhysWorld)
		return;

	if (m_pPhysicalEntity)
	{
		pPhysWorld->DestroyPhysicalEntity(m_pPhysicalEntity);
		if (m_pCharInstance)
		{
			if (m_pCharInstance->GetISkeletonPose()->GetCharacterPhysics(-1))
				pPhysWorld->DestroyPhysicalEntity(m_pCharInstance->GetISkeletonPose()->GetCharacterPhysics(-1));
			m_pCharInstance->GetISkeletonPose()->SetCharacterPhysics(NULL);
		}
		m_pPhysicalEntity = nullptr;
	}
}

std::unique_ptr<CModelProperties::SSerializer> CDialogCHR::CreateSerializer(CSceneElementCommon* pSceneElement)
{
	using namespace Private_DialogCHR;

	struct SSerializer
	{
		std::function<void()> m_revertPropertyTree;

		SSerializer(const FbxTool::SNode* pFbxNode, CScene* pScene, CSceneCHR* pSceneUserData, const std::function<void()>& revertPropertyTree)
			: m_pFbxNode(pFbxNode)
			, m_pScene(pScene)
			, m_pSceneUserData(pSceneUserData)
			, m_revertPropertyTree(revertPropertyTree)
		{
		}

		int GetIndexFromProxyMesh(const DynArray<CSceneElementCommon*>& proxyCandidateElements) const
		{
			if (!m_pSceneUserData->GetPhysicsProxyNode(m_pFbxNode))
			{
				return 0;  // By convention, first item is <no proxy>.
			}

			for (int i = 1; i < proxyCandidateElements.size(); ++i)
			{
				if (proxyCandidateElements[i]->GetType() == ESceneElementType::SourceNode &&
					((CSceneElementSourceNode*)proxyCandidateElements[i])->GetNode() == m_pSceneUserData->GetPhysicsProxyNode(m_pFbxNode))
				{
					return i;
				}
			}

			return 0;
		}

		void SetProxyMeshFromElement(const CSceneElementCommon* proxyElement)
		{
			if (!proxyElement)
			{
				m_pSceneUserData->SetPhysicsProxyNode(m_pFbxNode, nullptr);
				return;
			}

			if (proxyElement->GetType() == ESceneElementType::SourceNode)
			{
				CSceneElementSourceNode* const pSourceNodeElement = (CSceneElementSourceNode*)proxyElement;
				m_pSceneUserData->SetPhysicsProxyNode(m_pFbxNode, pSourceNodeElement->GetNode());
			}
		}

		struct SAxisConstraint
		{
			const int m_axis;
			CSceneCHR* m_pSceneUserData;
			const FbxTool::SNode* m_pFbxNode;
			std::function<void()> m_revertPropertyTree;
			float values[2];
			bool hasLimit[2];

			SAxisConstraint(SSerializer* pSerializer, int axis)
				: m_pSceneUserData(pSerializer->m_pSceneUserData)
				, m_pFbxNode(pSerializer->m_pFbxNode)
				, m_revertPropertyTree(pSerializer->m_revertPropertyTree)
				, m_axis(axis)
			{
			}

			void Serialize(Serialization::IArchive& ar)
			{
				static const char* limitLabels[] = { "^Min", "^Max" };
				static const char* limitNames[] = { "min", "max" };
				static const char* valueNames[] = { "minVal", "maxVal" };
				static const char* valueLabels[] = { "^", "^" };

				const FbxTool::SNode* const pFbxNode = m_pFbxNode;
				CSceneCHR* const pSceneUserData = m_pSceneUserData;
				const int axis = m_axis;
				auto revertPropertyTree = m_revertPropertyTree;

				for (int i = 0; i < 2; ++i)
				{
					auto copyFromPose = [pFbxNode, pSceneUserData, axis, revertPropertyTree, i]()
					{
						const auto axisLimit = (CSceneCHR::EAxisLimit)(3 * i + axis);
						const auto jointRot = pSceneUserData->GetJointRotationXYZ(pFbxNode)[axis];
						pSceneUserData->SetJointLimit(pFbxNode, axisLimit, jointRot);
						revertPropertyTree();
					};

					auto copyToPose = [pFbxNode, pSceneUserData, axis, revertPropertyTree, i]()
					{
						const auto axisLimit = (CSceneCHR::EAxisLimit)(3 * i + axis);
						auto jointRot = pSceneUserData->GetJointRotationXYZ(pFbxNode);
						if (pSceneUserData->GetJointLimit(pFbxNode, axisLimit, &jointRot[axis]))
						{
							pSceneUserData->SetJointRotationXYZ(pFbxNode, jointRot);
							revertPropertyTree();
						}
					};

					ar(hasLimit[i], limitNames[i], limitLabels[i]);
					ar(Serialization::ActionButton(copyFromPose, "icons:General/Picker.ico"), "button0", "^");
					ar.doc("Copy from pose");
					ar(values[i], valueNames[i], valueLabels[i]);
					ar(Serialization::ActionButton(copyToPose, "icons:General/Picker.ico"), "button1", "^");
					ar.doc("Copy to pose");
					ar(Serialization::SGap(), "gap", "^");
				}
			}
		};

		void SerializeIkConstraints(Serialization::IArchive& ar)
		{
			SAxisConstraint ac[3] =
			{
				SAxisConstraint(this, 0),  // x
				SAxisConstraint(this, 1),  // y
				SAxisConstraint(this, 2)   // z
			};

			// Initialize axis constraints.
			for (int axis = 0; axis < 3; ++axis)
			{
				for (int limit = 0; limit < 2; ++limit)
				{
					const int axisLimit = 3 * limit + axis;
					ac[axis].hasLimit[limit] = m_pSceneUserData->GetJointLimit(m_pFbxNode, (CSceneCHR::EAxisLimit)axisLimit, &ac[axis].values[limit]);
				}
			}

			ar (ac[0], "axisConstraintX", "X");
			ar (ac[1], "axisConstraintY", "Y");
			ar (ac[2], "axisConstraintZ", "Z");

			for (int axis = 0; axis < 3; ++axis)
			{
				for (int limit = 0; limit < 2; ++limit)
				{
					const int axisLimit = 3 * limit + axis;
					if (ac[axis].hasLimit[limit])
					{
						m_pSceneUserData->SetJointLimit(m_pFbxNode, (CSceneCHR::EAxisLimit)axisLimit, ac[axis].values[limit]);
					}
					else
					{
						m_pSceneUserData->ClearJointLimit(m_pFbxNode, (CSceneCHR::EAxisLimit)axisLimit);
					}
				}
			}
		}

		void Serialize(Serialization::IArchive& ar)
		{
			string name = m_pFbxNode->szName;
			ar(name, "name", "!Name");

			Serialization::StringList proxyCandidateNames;
			DynArray<CSceneElementCommon*> proxyCandidateElements;

			proxyCandidateNames.push_back("<no proxy>");
			proxyCandidateElements.push_back(nullptr);

			GetListOfMeshElements(m_pScene, proxyCandidateNames, proxyCandidateElements);

			Serialization::StringListValue proxyDropDown(proxyCandidateNames, GetIndexFromProxyMesh(proxyCandidateElements));
			ar(proxyDropDown, "proxyMesh", "Proxy Mesh");
			if (ar.isInput())
			{
				SetProxyMeshFromElement(proxyCandidateElements[proxyDropDown.index()]);
			}

			bool bSnapToJoint = m_pSceneUserData->GetPhysicsProxySnapToJoint(m_pFbxNode);
			ar(bSnapToJoint, "snapToJoint", "Snap to joint");
			ar.doc("Move proxy to its joint origin");
			m_pSceneUserData->SetPhysicsProxySnapToJoint(m_pFbxNode, bSnapToJoint);

			if (ar.openBlock("pose", "Pose"))
			{
				QuatT jointTransform(m_pSceneUserData->GetJointTransform(m_pFbxNode));
				ar(Serialization::SRotation(jointTransform.q), "jointTransform", "Joint rotation");
				m_pSceneUserData->SetJointTransform(m_pFbxNode, jointTransform);
			}

			if (ar.openBlock("ik_limits", "IK Limits"))
			{
				SerializeIkConstraints(ar);
			}

			SNodeInfo nodeInfo = { m_pFbxNode };
			ar(nodeInfo, "nodeInfo", "Node Info");
		}

		const FbxTool::SNode* m_pFbxNode;
		CScene* m_pScene;
		CSceneCHR* m_pSceneUserData;
	};

	if (pSceneElement->GetType() != ESceneElementType::SourceNode)
	{
		return std::make_unique<CModelProperties::SSerializer>();
	}

	CSceneElementSourceNode* const pSourceNodeElement = (CSceneElementSourceNode*)pSceneElement;

	auto revertPropertyTree = [this]() { m_pPropertyTree->revert(); };

	std::shared_ptr<SSerializer> pObject(new SSerializer(pSourceNodeElement->GetNode(), pSourceNodeElement->GetScene(), m_pSceneUserData.get(), revertPropertyTree));

	std::unique_ptr<CModelProperties::SSerializer> pSerializer(new CModelProperties::SSerializer());
	pSerializer->m_sstruct = Serialization::SStruct(*pObject);
	pSerializer->m_pObject = pObject;
	return pSerializer;
}

// ==================================================
// CDialogCHR
// ==================================================

void CDialogCHR::UpdateSkin()
{
	FbxMetaData::SMetaData metaData;
	CreateMetaDataSkin(metaData);
	m_skinObject->SetMetaData(metaData);
	m_skinObject->CreateAsync();
}

void CDialogCHR::UpdateSkeleton()
{
	FbxMetaData::SMetaData metaData;
	CreateMetaDataChr(metaData, QtUtil::ToString(GetSceneManager().GetImportFile()->GetFilePath()));
	m_skeletonObject->SetMetaData(metaData);
	m_skeletonObject->CreateAsync();
}

void CDialogCHR::UpdateStatGeom()
{
	FbxMetaData::SMetaData metaData;
	CreateMetaDataCgf(metaData);
	m_statGeomObject->SetMetaData(metaData);
	m_statGeomObject->CreateAsync();
}

void CDialogCHR::UpdateCharacter()
{
	if (m_skeletonObject->GetFilePath().empty())
	{
		return;
	}

	QString skeletonFilePath;
	if (!m_skeletonObject->GetFilePath().empty())
	{
		skeletonFilePath = QtUtil::ToQString(PathUtil::ReplaceExtension(m_skeletonObject->GetFilePath(), "chr"));
	}

	QString skinFilePath;
	if (!m_skinObject->GetFilePath().empty())
	{
		skinFilePath = QtUtil::ToQString(PathUtil::ReplaceExtension(m_skinObject->GetFilePath(), "skin"));
	}

	m_pCharacter->m_pCharInstance = CreateTemporaryCharacter(skeletonFilePath, skinFilePath, QString());
	m_pCharacter->m_pCharInstance->SetCharEditMode(m_pCharacter->m_pCharInstance->GetCharEditMode() | CA_CharacterTool);

	m_pCharacter->m_pPoseModifier = CSkeletonPoseModifier::CreateClassInstance();

	Physicalize();

	// Initialize bones.
	const uint32 jointCount = m_pCharacter->m_pCharInstance->GetIDefaultSkeleton().GetJointCount();
	m_jointHeat.resize(jointCount, 0.0f);
}

CDialogCHR::CDialogCHR(QWidget* pParent)
	: CBaseDialog(pParent)
	, m_pCharacter(new SCharacter())
	, m_hitJointId(kInvalidJointId)
{
	using namespace Private_DialogCHR;

	m_pSceneUserData.reset(new CSceneCHR());
	m_pSceneUserData->sigJointProxyChanged.Connect(std::function<void()>([this]()
	{
		UpdateSkeleton();
	}));
	m_pSceneUserData->sigJointLimitsChanged.Connect(std::function<void()>([this]()
	{
		UpdateSkeleton();
	}));

	m_pSceneModel.reset(new CSceneModelSkeleton(&m_pSceneUserData->m_skeleton));
	m_pSceneModel->SetPseudoRootVisible(false);

	m_pViewportContainer = new CSplitViewportContainer();

	m_pViewSettings.reset(new SViewSettings());
	auto pDisplayOptionsWidget = m_pViewportContainer->GetDisplayOptionsWidget();
	pDisplayOptionsWidget->SetUserOptions(Serialization::SStruct(*m_pViewSettings), "viewSettings", "View");

	m_pPreviewModeWidget = new CPreviewModeWidget();
	m_pViewportContainer->SetHeaderWidget(m_pPreviewModeWidget);

	m_pPropertyTree = new QPropertyTree();
	m_pPropertyTree->setSliderUpdateDelay(0);

	m_pModelProperties.reset(new CModelProperties(m_pPropertyTree));
	m_pModelProperties->AddCreateSerializerFunc([this](QAbstractItemModel* pModel, const QModelIndex& index)
	{
		const QModelIndex sourceIndex = GetSourceModelIndex(index);
		if (!sourceIndex.isValid() || m_pSceneModel.get() != sourceIndex.model())
		{
			return std::unique_ptr<CModelProperties::SSerializer>();
		}
		CSceneElementCommon* const pSceneElement = m_pSceneModel->GetSceneElementFromModelIndex(sourceIndex);
		return CreateSerializer(pSceneElement);
	});

	m_skinObject = CreateTempRcObject();
	m_skinObject->SetFinalize(std::function<void(const CTempRcObject*)>([this](const CTempRcObject*)
	{
		UpdateCharacter();
	}));

	m_skeletonObject = CreateTempRcObject();
	m_skeletonObject->SetFinalize(std::function<void(const CTempRcObject*)>([this](const CTempRcObject*)
	{
		UpdateCharacter();
	}));

	m_statGeomObject = CreateTempRcObject();
	m_statGeomObject->SetFinalize(std::function<void(const CTempRcObject*)>([this](const CTempRcObject* pRcObj)
	{
		const string filenameCgf = PathUtil::ReplaceExtension(pRcObj->GetFilePath(), "cgf");
		m_pStatObj = GetIEditor()->Get3DEngine()->LoadStatObj(filenameCgf.c_str(), NULL, NULL, false);
	}));

	m_pSceneUserData->AddCallback([this](CSceneCHR* pSceneUserData)
	{
		UpdateSkeleton();

		m_pSceneModel->Reset();
	});

	SetupUI();
}

void CDialogCHR::SetupUI()
{
	m_pSceneView = new CSceneViewCommon();
	CSceneViewContainer* const pSceneViewContainer = new CSceneViewContainer(m_pSceneModel.get(), m_pSceneView, nullptr);

	m_pSceneContextMenu.reset(new CSceneContextMenuCommon(m_pSceneView));
	m_pSceneContextMenu->Attach();

	m_pModelProperties->ConnectViewToPropertyObject(m_pSceneView);

	QSplitter* const pCenterSplitter = new QSplitter(Qt::Horizontal);

	// Scene view and label.
	{
		QLabel* const pLabel = new QLabel();
		auto setLabelText = [this, pLabel](CSceneCHR* pSceneUserData)
		{
			const FbxTool::SNode* pExportRoot = pSceneUserData->m_skeleton.GetExportRoot();

			QString label;
			
			if (pExportRoot)
			{
				label = QString("Selected root: %1").arg(pExportRoot->szName);
			}
			else
			{
				label = QString("Selected root: auto-detect");
			}

			pLabel->setText(label);
		};
		m_pSceneUserData->AddCallback(setLabelText);
		setLabelText(m_pSceneUserData.get());  // Set initial text.

		QVBoxLayout* const pLayout = new QVBoxLayout();
		pLayout->setContentsMargins(0, 0, 0, 0);
		pLayout->addWidget(pLabel);
		pLayout->addWidget(pSceneViewContainer);
		QWidget* const pDummy = new QWidget();
		pDummy->setLayout(pLayout);
		pCenterSplitter->addWidget(pDummy);
	}

	pCenterSplitter->addWidget(m_pViewportContainer);

	// Properties and material widget
	{
		QTabWidget* const pTabWidget = new QTabWidget();
		pTabWidget->addTab(m_pPropertyTree, tr("Properties"));

		pCenterSplitter->addWidget(pTabWidget);
	}

	pCenterSplitter->setStretchFactor(0, 0);
	pCenterSplitter->setStretchFactor(1, 1);
	pCenterSplitter->setStretchFactor(2, 0);

	QVBoxLayout* const pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->addWidget(pCenterSplitter);

	if (layout())
	{
		delete layout();
	}
	setLayout(pLayout);

	m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport()->AddConsumer(this);
}


CDialogCHR::~CDialogCHR()
{
	m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport()->RemoveConsumer(this);
}

int CDialogCHR::GetToolButtons() const
{
	return eToolButtonFlags_Import | eToolButtonFlags_Save | eToolButtonFlags_Open;
}

QString CDialogCHR::ShowSaveAsDialog()
{
	return ShowSaveAsFilePrompt(eFileFormat_CHR);
}

bool CDialogCHR::SaveAs(SSaveContext& ctx)
{
	using namespace Private_DialogCHR;

	const QString targetFilePath = QtUtil::ToQString(ctx.targetFilePath);

	FbxMetaData::SMetaData metaData;
	CreateMetaDataChr(metaData, QtUtil::ToString(GetSceneManager().GetImportFile()->GetFilePath()));

	std::shared_ptr<QTemporaryDir> pTempDir(std::move(ctx.pTempDir));

	// Write material.
	const QString targetFileDir = QFileInfo(targetFilePath).dir().path();
	const QString materialFilePath = targetFileDir + "/" + QFileInfo(targetFilePath).baseName() + "_hitmaterial.mtl";
	CMaterial* const pMaterial = CreateSkeletonMaterial(materialFilePath);

	m_material.pTempDir.release();
	m_material.materialName = pMaterial->GetName();

	SRcObjectSaveState saveState;
	CaptureRcObjectSaveState(pTempDir, metaData, saveState);

	const QString absOriginalFilePath = GetSceneManager().GetImportFile()->GetOriginalFilePath();
	ThreadingUtils::Async([this, saveState, absOriginalFilePath, targetFilePath, pTempDir]()
	{
		// Asset relative path to directory. targetFilePath is absolute path.
		const string dir = PathUtil::GetPathWithoutFilename(PathUtil::AbsolutePathToGamePath(QtUtil::ToString(targetFilePath)));

		const std::pair<bool, string> ret = CopySourceFileToDirectoryAsync(QtUtil::ToString(absOriginalFilePath), dir).get();
		if (!ret.first)
		{
			const string& error = ret.second;
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Copying source file to '%s' failed: '%s'",
				dir.c_str(), error.c_str());
			return;
		}

		SaveRcObjectAsync(saveState, QtUtil::ToString(targetFilePath));
	});

	return true;
}

CMaterial* CDialogCHR::CreateSkeletonMaterial(const QString& materialFilePath)
{
	string materialName = GetMaterialNameFromFilePath(QtUtil::ToString(materialFilePath));

	const int mtlFlags = GetScene()->GetMaterialCount() > 1 ? MTL_FLAG_MULTI_SUBMTL : 0;

	CMaterial* pMaterial = GetIEditor()->GetMaterialManager()->CreateMaterial(materialName, XmlNodeRef(), mtlFlags);

	CreateMaterial(pMaterial, GetScene());

	return pMaterial;
}

void CDialogCHR::Physicalize()
{
	IPhysicalWorld* pPhysWorld = gEnv->pPhysicalWorld;
	if (!pPhysWorld)
		return;

	ICharacterInstance* const pCharacter = m_pCharacter->m_pCharInstance;

	m_pCharacter->DestroyPhysics();

	if (pCharacter)
	{
		pCharacter->GetISkeletonPose()->DestroyCharacterPhysics();
		m_pCharacter->m_pPhysicalEntity = pPhysWorld->CreatePhysicalEntity(PE_LIVING);
		IPhysicalEntity* pCharPhys = pCharacter->GetISkeletonPose()->CreateCharacterPhysics(m_pCharacter->m_pPhysicalEntity, 80.0f, -1, 70.0f);
		pCharacter->GetISkeletonPose()->CreateAuxilaryPhysics(pCharPhys, Matrix34(IDENTITY));
		pe_player_dynamics pd;
		pd.bActive = 0;
		m_pCharacter->m_pPhysicalEntity->SetParams(&pd);
	}
}

void CDialogCHR::AssignScene(const SImportScenePayload* pUserData)
{
	m_pSceneModel->SetScene(GetScene());
	m_pSceneUserData->Initialize(*GetScene());

	AutoAssignSubMaterialIDs(std::vector<std::pair<int, QString>>(), GetScene());

	m_material.pTempDir.release();
	m_material.materialName.clear();

	if (pUserData && pUserData->pMetaData)
	{
		const SBlockSignals blockSignals(*m_pSceneUserData);
		ReadNodeMetaData(pUserData->pMetaData->nodeData);
		ReadJointPhysicsData(pUserData->pMetaData->jointPhysicsData);
		m_material.materialName = pUserData->pMetaData->materialFilename;
	}
	else
	{
		m_material.pTempDir = std::move(CreateTemporaryDirectory());

		QString materialFilePath = m_material.pTempDir->path() + "/mi_caf_material.mtl";

		CMaterial* pMaterial = CreateSkeletonMaterial(materialFilePath);

		m_material.materialName = pMaterial->GetName();
	}

	UpdateSkeleton();
	UpdateSkin();
	UpdateStatGeom();
}

void CDialogCHR::UnloadScene()
{
	CRY_ASSERT(m_pSceneModel);
	m_pSceneModel->ClearScene();
}

const char* CDialogCHR::GetDialogName() const
{
	return "Skeleton";
}

void CDialogCHR::OnIdleUpdate()
{
	using namespace Private_DialogCHR;

	m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport()->Update();
}

void CDialogCHR::WriteNodeMetaData(std::vector<FbxMetaData::SNodeMeta>& nodeMetaData) const
{
	// TODO: Does it make sense to preserve child information?

	if (!m_pSceneUserData->m_skeleton.GetExportRoot())
	{
		// Empty node meta data means auto-selection.
		return;
	}

	struct SStackItem
	{
		const FbxTool::SNode*                pNode;
		std::vector<FbxMetaData::SNodeMeta>* pParentMeta;
	};
	std::vector<SStackItem> stack;

	{
		SStackItem item;
		item.pNode = m_pSceneUserData->m_skeleton.GetExportRoot();
		item.pParentMeta = &nodeMetaData;
		stack.push_back(item);
	}

	const std::vector<bool>& nodesInclusion = m_pSceneUserData->m_skeleton.GetNodesInclusion();

	while (!stack.empty())
	{
		SStackItem item = stack.back();
		stack.pop_back();

		item.pParentMeta->emplace_back();
		FbxMetaData::SNodeMeta* const pMeta = &item.pParentMeta->back();
		pMeta->children.reserve(item.pNode->numChildren);

		pMeta->name = item.pNode->szName;
		pMeta->nodePath = FbxTool::GetPath(item.pNode);

		if (!nodesInclusion[item.pNode->id])
		{
			continue;
		}

		pMeta->children.reserve(item.pNode->numChildren);
		for (int i = 0; i < item.pNode->numChildren; ++i)
		{
			SStackItem childItem;
			childItem.pNode = item.pNode->ppChildren[i];
			childItem.pParentMeta = &pMeta->children;
			stack.push_back(childItem);
		}
	}
}

void CDialogCHR::WriteJointPhysicsData(std::vector<FbxMetaData::SJointPhysicsData>& jointPhysicsData) const
{
	const FbxTool::CScene* const pFbxScene = GetScene();
	CRY_ASSERT(pFbxScene);

	for (int i = 0; i < pFbxScene->GetNodeCount(); ++i)
	{
		bool bWriteToFile = false;
		FbxMetaData::SJointPhysicsData jpd;

		const FbxTool::SNode* const pFbxNode = pFbxScene->GetNodeByIndex(i);
		CRY_ASSERT(pFbxNode);

		jpd.jointNodePath = FbxTool::GetPath(pFbxNode);

		// Proxy node.
		const FbxTool::SNode* const pProxyNode = m_pSceneUserData->GetPhysicsProxyNode(pFbxNode);
		if (pProxyNode)
		{
			jpd.proxyNodePath = FbxTool::GetPath(pProxyNode);
			jpd.bSnapToJoint = m_pSceneUserData->GetPhysicsProxySnapToJoint(pFbxNode);
			bWriteToFile = true;
		}

		// IK constraints.
		{
			jpd.jointLimits.mask = 0;
			for (int i = 0; i < 6; ++i)
			{
				if (m_pSceneUserData->GetJointLimit(pFbxNode, (CSceneCHR::EAxisLimit)i, &jpd.jointLimits.values[i]))
				{
					jpd.jointLimits.mask |= (1 << i);
					bWriteToFile = true;
				}
			}
		}

		if (bWriteToFile)
		{
			jointPhysicsData.push_back(jpd);
		}
	}
}

void CDialogCHR::ReadNodeMetaData(const std::vector<FbxMetaData::SNodeMeta>& nodeMetaData)
{
	if (nodeMetaData.empty())
	{
		CRY_ASSERT(!m_pSceneUserData->m_skeleton.GetExportRoot());
		return;  // Auto-detect root. Nothing to do.
	}

	const FbxTool::CScene* pScene = GetScene();
	CRY_ASSERT(pScene);

	struct SStackItem
	{
		SStackItem(const FbxMetaData::SNodeMeta& metadata, const FbxTool::SNode* pNode) : pMetadata(&metadata), pNode(pNode), child(0) {}

		const FbxMetaData::SNodeMeta* pMetadata;
		const FbxTool::SNode* pNode;
		int child;
	};
	std::vector<SStackItem> stack;

	auto pushBack = [&stack, pScene](const FbxMetaData::SNodeMeta& meta)
	{
		const FbxTool::SNode* pNode = FindChildNode(pScene->GetRootNode(), meta.nodePath);
		if (!pNode)
		{
			LogPrintf("Skipping unknown node '%s' referenced by meta data", meta.name.c_str());
			return;
		}

		stack.emplace_back(meta, pNode);
	};

	for (const auto& meta : nodeMetaData)
	{
		pushBack(meta);
	}

	// CSceneCHR::CBatchSignals batchSignals(*m_pSceneUserData);

	const FbxTool::SNode* pRoot = nullptr;
	std::vector<bool> needsExclusion(pScene->GetNodeCount() + 1, false);

	while (!stack.empty())
	{
		SStackItem& item = stack.back();

		if (item.child == 0)  // We visit this item the first time.
		{
			if (!pRoot)
			{
				pRoot = item.pNode;
				Private_DialogCHR::MarkSubtreeWithoutRoot(pRoot, needsExclusion);
			}
			else
			{
				needsExclusion[item.pNode->id] = false;
			}
		}

		if (item.child == item.pMetadata->children.size())
		{
			stack.pop_back();
			continue;
		}

		pushBack(item.pMetadata->children[item.child]);
		++item.child;
	}

	m_pSceneUserData->m_skeleton.SetNodeInclusion(pRoot, true);

	for (int i = 0; i < pScene->GetNodeCount(); ++i)
	{
		const FbxTool::SNode* const pNode = pScene->GetNodeByIndex(i);
		if (needsExclusion[pNode->id])
		{
			m_pSceneUserData->m_skeleton.SetNodeInclusion(pNode, false);
		}
	}
}

void CDialogCHR::ReadJointPhysicsData(const std::vector<FbxMetaData::SJointPhysicsData>& jointPhysicsData)
{
	const FbxTool::CScene* const pFbxScene = GetScene();
	CRY_ASSERT(pFbxScene);

	for (const auto& jpd : jointPhysicsData)
	{
		const FbxTool::SNode* const pFbxNode = FbxTool::FindChildNode(pFbxScene->GetRootNode(), jpd.jointNodePath);
		const FbxTool::SNode* const pProxyNode = FbxTool::FindChildNode(pFbxScene->GetRootNode(), jpd.proxyNodePath);
		if (pFbxNode)
		{
			if (pProxyNode)
			{
				m_pSceneUserData->SetPhysicsProxyNode(pFbxNode, pProxyNode);
				m_pSceneUserData->SetPhysicsProxySnapToJoint(pFbxNode, jpd.bSnapToJoint);
			}

			for (int i = 0; i < 6; ++i)
			{
				if (jpd.jointLimits.mask & (1 << i))
				{
					m_pSceneUserData->SetJointLimit(pFbxNode, (CSceneCHR::EAxisLimit)i, jpd.jointLimits.values[i]);
				}
				else
				{
					m_pSceneUserData->ClearJointLimit(pFbxNode, (CSceneCHR::EAxisLimit)i);
				}
			}
		}
	}
}

void CDialogCHR::CreateMetaDataChr(FbxMetaData::SMetaData& metaData, const string& sourceFilename) const
{
	metaData.Clear();

	metaData.sourceFilename = sourceFilename;
	metaData.outputFileExt = "chr";

	if (!m_material.materialName.empty())
	{
		metaData.materialFilename = m_material.materialName;
	}
	else
	{
		metaData.materialFilename = GetIEditor()->GetSystem()->GetIConsole()->GetCVar("mi_defaultMaterial")->GetString();
	}

	WriteNodeMetaData(metaData.nodeData);
	WriteJointPhysicsData(metaData.jointPhysicsData);
}

void CDialogCHR::CreateMetaDataSkin(FbxMetaData::SMetaData& metaData) const
{
	metaData.Clear();

	metaData.sourceFilename = QtUtil::ToString(GetSceneManager().GetImportFile()->GetFilePath());
	metaData.outputFileExt = "skin";
	metaData.materialFilename = GetIEditor()->GetSystem()->GetIConsole()->GetCVar("mi_defaultMaterial")->GetString();
}

void CDialogCHR::CreateMetaDataCgf(FbxMetaData::SMetaData& metaData) const
{
	metaData.Clear();

	metaData.sourceFilename = QtUtil::ToString(GetSceneManager().GetImportFile()->GetFilePath());
	metaData.outputFileExt = "cgf";
	metaData.materialFilename = GetIEditor()->GetSystem()->GetIConsole()->GetCVar("mi_defaultMaterial")->GetString();
}

void CDialogCHR::RenderCharacter(const SRenderContext& rc, ICharacterInstance* pCharInstance)
{
	if (!pCharInstance || !m_pViewSettings->m_bShowSkin)
	{
		return;
	}

	SRendParams rp = *rc.renderParams;
	assert(rp.pMatrix);
	assert(rp.pPrevMatrix);

	Matrix34 localEntityMat(IDENTITY);
	rp.pMatrix = &localEntityMat;

	const SRenderingPassInfo& passInfo = *rc.passInfo;
	gEnv->p3DEngine->PrecacheCharacter(NULL, 1.f, pCharInstance, pCharInstance->GetIMaterial(), localEntityMat, 0, 1.f, 4, true, passInfo);
	pCharInstance->SetViewdir(rc.camera->GetViewdir());
	pCharInstance->Render(rp, passInfo);
}

void CDialogCHR::RenderPhysics(const SRenderContext& rc, ICharacterInstance* pCharacter)
{
	const bool bShowProxies = m_pViewSettings->m_bShowPhysicalProxies; 
	const bool bShowLimits = m_pViewSettings->m_bShowJointLimits;

	if (!bShowProxies && !bShowLimits)
	{
		return;
	}

	IRenderer* const renderer = gEnv->pRenderer;
	IRenderAuxGeom* const pAuxGeom = renderer->GetIRenderAuxGeom();
	const SAuxGeomRenderFlags savedFlags = pAuxGeom->GetRenderFlags();
	pAuxGeom->SetRenderFlags(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeNone | e_DepthWriteOff | e_DepthTestOn);

	IPhysicsDebugRenderer* const pPhysRender = GetIEditor()->GetSystem()->GetIPhysicsDebugRenderer();
	pPhysRender->UpdateCamera(*rc.camera);
	int drawHelpers = 0;
	if (bShowProxies)
	{
		drawHelpers |= 2;  // Enable rendering of proxies.
	}
	if (bShowLimits)
	{
		drawHelpers |= 0x10;  // Enable rendering of ragdoll joint limits.
	}

	ISkeletonPose& skeletonPose = *pCharacter->GetISkeletonPose();
	IPhysicalEntity* const pBaseEntity = skeletonPose.GetCharacterPhysics();
	if (pBaseEntity)
	{
		pPhysRender->DrawEntityHelpers(pBaseEntity, drawHelpers);
	}

	const f32 frameTime = GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();
	pPhysRender->Flush(frameTime);
	pAuxGeom->SetRenderFlags(savedFlags);
}

void CDialogCHR::RenderJoints(const SRenderContext& rc, ICharacterInstance* pCharInstance)
{
	if (!pCharInstance)
	{
		return;
	}

	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	const SAuxGeomRenderFlags savedFlags = pAuxGeom->GetRenderFlags();
	pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	DrawSkeleton(pAuxGeom, &pCharInstance->GetIDefaultSkeleton(), pCharInstance->GetISkeletonPose(),
	             QuatT(IDENTITY), "", rc.viewport->GetState().cameraTarget);

	const ISkeletonPose& skeletonPose = *pCharInstance->GetISkeletonPose();
	IDefaultSkeleton& defaultSkeleton = pCharInstance->GetIDefaultSkeleton();

	DrawHighlightedJoints(*pCharInstance, m_jointHeat, rc.viewport->Camera()->GetPosition());
	DrawJoints(*pCharInstance);

	if (m_pViewSettings->m_bShowJointNames)
	{
		// Draw all joint names
		uint32 numJoints = defaultSkeleton.GetJointCount();

		for (int j = 0; j < numJoints; j++)
		{
			const char* pJointName = defaultSkeleton.GetJointNameByID(j);

			QuatT jointTM = QuatT(skeletonPose.GetAbsJointByID(j));
			IRenderAuxText::DrawLabel(jointTM.t, 2, pJointName);
		}
	}
	else if (m_hitJointId != kInvalidJointId)
	{
		const QViewport* const vp = m_pViewportContainer->GetSplitViewport()->GetPrimaryViewport();
		const char* const jointName = defaultSkeleton.GetJointNameByID(m_hitJointId);
		ShowLabelNextToCursor(vp, jointName);
	}
	pAuxGeom->SetRenderFlags(savedFlags);
}

void CDialogCHR::RenderCgf(const SRenderContext& rc)
{
	if (!m_pStatObj)
	{
		return;
	}

	Matrix34 identity(IDENTITY);
	rc.renderParams->pMatrix = &identity;
	m_pStatObj->Render(*rc.renderParams, *rc.passInfo);
}

static void CoolDown(std::vector<float>& heat)
{
	for (float& h : heat)
	{
		h = std::max(0.0f, h - kSelectionCooldownPerSec * gEnv->pSystem->GetITimer()->GetFrameTime());
	}
}

void CDialogCHR::OnViewportRender(const SRenderContext& rc)
{
	using namespace Private_DialogCHR;

	if (m_pPreviewModeWidget->GetPreviewMode() == CPreviewModeWidget::ePreviewMode_StaticGeometry)
	{
		RenderCgf(rc);
	}
	else if (m_pCharacter->m_pCharInstance)
	{
		RenderCharacter(rc, m_pCharacter->m_pCharInstance);
		RenderPhysics(rc, m_pCharacter->m_pCharInstance);

		if (FbxTool::CScene* pFbxScene = GetScene())
		{
			for (int i = 0; i < pFbxScene->GetNodeCount(); ++i)
			{
				const FbxTool::SNode* const pFbxNode = pFbxScene->GetNodeByIndex(i);
				QuatT jointTransform = m_pSceneUserData->GetJointTransform(pFbxNode);
				m_pCharacter->m_pPoseModifier->PoseJoint(pFbxNode->szName, jointTransform);
			}
		}

		m_pCharacter->m_pCharInstance->GetISkeletonAnim()->PushPoseModifier(-1, m_pCharacter->m_pPoseModifier, "Skeleton pose modifier");
		m_pCharacter->m_pCharInstance->GetISkeletonPose()->SetForceSkeletonUpdate(1);

		GetIEditor()->GetSystem()->GetIAnimationSystem()->UpdateStreaming(-1, -1);
		m_pCharacter->m_pCharInstance->StartAnimationProcessing(SAnimationProcessParams());
		m_pCharacter->m_pCharInstance->FinishAnimationComputations();
	}

	RenderJoints(rc, m_pCharacter->m_pCharInstance);

	// Joint cool-down.
	if (m_hitJointId != kInvalidJointId)
	{
		const float oldHeat = m_jointHeat[m_hitJointId];
		CoolDown(m_jointHeat);
		m_jointHeat[m_hitJointId] = oldHeat;
	}
	else
	{
		CoolDown(m_jointHeat);
	}
}

void CDialogCHR::PickJoint(QViewport& viewport, float x, float y)
{
	m_hitJointId = ::PickJoint(*m_pCharacter->m_pCharInstance, viewport, x, y);

	if (m_hitJointId == -1)
	{
		return;
	}

	m_jointHeat[m_hitJointId] = 1.0f;
}

void CDialogCHR::SelectJoint(int32 jointId)
{
	const char* jointName = m_pCharacter->m_pCharInstance->GetIDefaultSkeleton().GetJointNameByID(jointId);
	const FbxTool::SNode* pHitNode = nullptr;
	const FbxTool::CScene* const pScene = GetScene();
	for (size_t i = 0, N = pScene->GetNodeCount(); i < N; ++i)
	{
		const FbxTool::SNode* const pNode = pScene->GetNodeByIndex(i);
		if (!strcmp(pNode->szName, jointName))
		{
			pHitNode = pNode;
		}
	}
	if (pHitNode)
	{
		SelectSceneElementWithNode(m_pSceneModel.get(), m_pSceneView, pHitNode);
	}
}

void CDialogCHR::OnViewportMouse(const SMouseEvent& ev)
{
	using namespace Private_DialogCHR;

	if (m_pPreviewModeWidget->GetPreviewMode() == CPreviewModeWidget::ePreviewMode_Character && m_pCharacter->m_pCharInstance)
	{
		PickJoint(*ev.viewport, ev.x, ev.y);

		if (SMouseEvent::BUTTON_LEFT == ev.button && m_hitJointId != -1)
		{
			SelectJoint(m_hitJointId);
		}
	}
}

} // namespace MeshImporter

