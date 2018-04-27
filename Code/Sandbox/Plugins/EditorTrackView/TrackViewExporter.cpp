// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TrackViewExporter.h"

#include "TrackViewPlugin.h"
#include "AnimationContext.h"
#include "TrackViewUndo.h"

#include "Nodes/TrackViewCameraNode.h"
#include "Nodes/TrackViewSequence.h"
#include "CurveEditorContent.h"

#include "IObjectManager.h"
#include "ViewManager.h"

#include "Exporter/FBXImporterDlg.h"
#include "Exporter/FBXExporterDlg.h"
#include "Exporter/XMLExporterDlg.h"

#include <FileDialogs/SystemFileDialog.h>
#include "Controls/QuestionDialog.h"
#include <QCheckBox>

inline f32 Sandbox2MayaFOVDeg(const f32 fov, const f32 ratio)     { return RAD2DEG(2.0f * atan_tpl(tan(DEG2RAD(fov) / 2.0f) * ratio)); };
inline f32 Sandbox2MayaFOVRad2Deg(const f32 fov, const f32 ratio) { return RAD2DEG(2.0f * atan_tpl(tan(fov / 2.0f) * ratio)); };

CTrackViewExporter::CTrackViewExporter()
	: m_bBakedKeysSequenceExport(true)
	, m_pivotEntityObject(nullptr)
	, m_bExportOnlyMasterCamera(false)
	, m_bExportLocalCoords(false)
	, m_numberOfExportFrames(30)
	, m_bAnimKeyTimeExport(false)
	, m_bSoundKeyTimeExport(false)
	, m_FBXBakedExportFramerate(30)
	, m_animTimeExportMasterSequenceCurrentTime(0.0f)
	, m_pBaseObj(nullptr)
	, m_fScale(1.0f)
{
}

CTrackViewExporter::~CTrackViewExporter()
{
}

enum EFormat
{
	eFormat_Unknown = 0,
	eFormat_FBX,
	eFormat_XML,
};

struct SExportFormat
{
	EFormat     format;
	const char* szExt;
	const char* szFilter;
};

SExportFormat supportedExportFormats[] =
{
	{ eFormat_FBX, "fbx", "FBX Files (*.fbx)" },
	{ eFormat_XML, "xml", "XML Files (*.xml)" }
};

SExportFormat supportedImportFormats[] =
{
	{ eFormat_FBX, "fbx", "FBX Files (*.fbx)" }
};

bool CTrackViewExporter::ImportFromFile()
{
	m_data.Clear();
	bool bResult = false;
	ExtensionFilterVector extensionFilters;
	const size_t formatCount = sizeof(supportedImportFormats) / sizeof(supportedImportFormats[0]);
	for (size_t i = 0; i < formatCount; ++i)
	{
		extensionFilters << CExtensionFilter(supportedImportFormats[i].szFilter, supportedImportFormats[i].szExt);
	}

	CSystemFileDialog::RunParams runParams;
	runParams.title = QWidget::tr("Import From File");
	runParams.extensionFilters = extensionFilters;

	string fileName = CSystemFileDialog::RunImportFile(runParams, nullptr).toLocal8Bit().data();

	if (fileName.GetLength() > 0)
	{
		EFormat selFormat = eFormat_Unknown;
		const char* szExt = PathUtil::GetExt(fileName);

		for (size_t i = 0; i < formatCount; ++i)
		{
			if (!stricmp(szExt, supportedImportFormats[i].szExt))
			{
				selFormat = supportedImportFormats[i].format;
				break;
			}
		}

		switch (selFormat)
		{
		case eFormat_FBX:
			if (ReadFileIntoMemory(fileName))
			{
				bResult = ImportFBXFile();
			}
			break;

		case eFormat_XML:
			// TODO: bResult = ImportXMLFile();
			break;

		default:
			CryLog("Invalid format");
			bResult = false;
			break;
		}
	}

	return bResult;
}

bool CTrackViewExporter::ReadFileIntoMemory(const char* fileName)
{
	const char* szExt = PathUtil::GetExt(fileName);
	IExportManager* pExportManager = GetIEditor()->GetExportManager();
	if (pExportManager)
	{
		IExporter* pExporter = pExportManager->GetExporterForExtension(szExt);
		if (pExporter)
		{
			return pExporter->ImportFromFile(fileName, &m_data);
		}
	}
	return false;
}

bool CTrackViewExporter::ImportFBXFile()
{
	CTrackViewSequence* pSequence = CTrackViewPlugin::GetAnimationContext()->GetSequence();
	if (pSequence)
	{
		CUndo undo("Replace Keys");
		CTrackViewTrackBundle tracks = pSequence->GetAllTracks();
		const uint numTracks = tracks.GetCount();

		for (uint trackID = 0; trackID < numTracks; ++trackID)
		{
			CTrackViewTrack* pTrack = tracks.GetTrack(trackID);
			CUndo::Record(new CUndoTrackObject(pTrack, false));
		}

		CTrackViewTrackBundle trackBundle = pSequence->GetAllTracks();
		const int objectCount = m_data.GetObjectCount();

		uint objectsToImport = 0;
		QFBXImporterDlg fbxImporterDlg;
		for (int objectId = 0; objectId < objectCount; ++objectId)
		{
			SExportObject* pObject = m_data.m_objects[objectId];
			string objectName = pObject->name;

			if (objectName.MakeLower().Find("group") != -1)
			{
				continue;
			}

			fbxImporterDlg.AddImportObject(objectName);
			++objectsToImport;
		}

		if (objectsToImport == 0 || fbxImporterDlg.exec() == QDialog::Rejected)
		{
			return false;
		}

		// Remove all keys from the affected tracks
		bool bApplyToAll = false;
		std::vector<int> decisions;
		int result = QDialogButtonBox::No;
		for (int objectId = 0; objectId < objectCount; ++objectId)
		{
			SExportObject* pObject = m_data.GetExportObject(objectId);
			if (pObject)
			{
				// Clear only the selected tracks
				if (!fbxImporterDlg.IsObjectSelected(pObject->name))
				{
					continue;
				}

				const char* szUpdatedNodeName = pObject->name;
				if (pSequence->GetAnimNodesByName(szUpdatedNodeName).GetCount() > 0)
				{
					if (!bApplyToAll)
					{
						QString titleText = QWidget::tr("Import Keys");
						QString messageText = QWidget::tr("%1 already contains an object named %2! All keys will be overwritten!").arg(QWidget::tr(pSequence->GetName()), QWidget::tr(szUpdatedNodeName));
						CQuestionDialog question;

						if (objectCount > 1)
						{
							question.AddCheckBox(QObject::tr("Apply my decision to other objects"), &bApplyToAll);
						}

						question.SetupQuestion(titleText, messageText);
						result = question.Execute();
					}

					decisions.push_back(result);
					if (result == QDialogButtonBox::No)
					{
						continue;
					}
				}

				int animationDataCount = pObject->GetEntityAnimationDataCount();
				for (int animDataId = 0; animDataId < animationDataCount; ++animDataId)
				{
					const Export::EntityAnimData* pAnimData = pObject->GetEntityAnimationData(animDataId);
					CTrackViewTrack* pTrack = GetTrackViewTrack(pAnimData, trackBundle, szUpdatedNodeName);
					if (pTrack)
					{
						CTrackViewKeyBundle keys = pTrack->GetAllKeys();
						uint keyCount = keys.GetKeyCount();

						for (uint deleteKeyID = keyCount - 1; deleteKeyID >= 0 && deleteKeyID < keyCount; --deleteKeyID)
						{
							CTrackViewKeyHandle key = keys.GetKey(deleteKeyID);
							key.Delete();
						}
					}
				}
			}
		}

		for (uint objectId = 0; objectId < objectCount; ++objectId)
		{
			const SExportObject* pObject = m_data.GetExportObject(objectId);
			if (pObject)
			{
				if (!fbxImporterDlg.IsObjectSelected(pObject->name))
				{
					continue;
				}

				const char* szUpdatedNodeName = pObject->name;
				if (pSequence->GetAnimNodesByName(szUpdatedNodeName).GetCount() > 0)
				{
					if (decisions[objectId] == QDialogButtonBox::No)
					{
						continue;
					}
				}
				else
				{
					// TODO: create a new node?
					CryLog("No matching node could be found for object %s. skipping...", szUpdatedNodeName);
					continue;
				}

				const int animatonDataCount = pObject->GetEntityAnimationDataCount();

				// Add keys from the imported file to the selected tracks
				for (int animDataID = 0; animDataID < animatonDataCount; ++animDataID)
				{
					const Export::EntityAnimData* pAnimData = pObject->GetEntityAnimationData(animDataID);
					CTrackViewTrack* pTrack = GetTrackViewTrack(pAnimData, trackBundle, (string)szUpdatedNodeName);
					if (pTrack)
					{
						SAnimTime keyTime(pAnimData->keyTime);
						CTrackViewKeyHandle key = pTrack->CreateKey(keyTime);

						S2DBezierKey bezierKey;
						key.GetKey(&bezierKey);
						bezierKey.m_time = keyTime;
						bezierKey.m_controlPoint.m_value = pAnimData->keyValue;
						key.SetKey(&bezierKey);
					}
				}

				// TODO:
				// After all keys are added, we are able to add the left and right tangents to the imported keys
				return true;
			}
		}
	}
	return false;
}

bool CTrackViewExporter::ExportToFile(bool bCompleteSequence)
{
	m_data.Clear();
	bool bResult = false;
	ExtensionFilterVector extensionFilters;
	const size_t formatCount = sizeof(supportedImportFormats) / sizeof(supportedImportFormats[0]);
	for (size_t i = 0; i < formatCount; ++i)
	{
		extensionFilters << CExtensionFilter(supportedImportFormats[i].szFilter, supportedImportFormats[i].szExt);
	}

	CSystemFileDialog::RunParams runParams;
	runParams.title = QWidget::tr("Export Selected Nodes To File");
	runParams.extensionFilters = extensionFilters;

	string fileName = CSystemFileDialog::RunExportFile(runParams, nullptr).toLocal8Bit().data();
	;
	if (fileName.GetLength() > 0)
	{
		EFormat selFormat = eFormat_Unknown;
		const char* szExt = PathUtil::GetExt(fileName);

		for (size_t i = 0; i < formatCount; ++i)
		{
			if (!stricmp(szExt, supportedExportFormats[i].szExt))
			{
				selFormat = supportedExportFormats[i].format;
				break;
			}
		}

		switch (selFormat)
		{
		case eFormat_FBX:
			{
				if (bCompleteSequence)
				{
					if (ShowFBXExportDialog())
					{
						CTrackViewSequence* pSequence = CTrackViewPlugin::GetAnimationContext()->GetSequence();
						m_numberOfExportFrames = (pSequence->GetTimeRange().end.GetTicks() / SAnimTime::numTicksPerSecond) * m_FBXBakedExportFramerate;

						if (!m_bExportOnlyMasterCamera)
						{
							AddObjectsFromSequence(pSequence);
						}

						bResult = ProcessObjectsForExport();
						SolveHierarchy();
					}
				}
				else
				{
					bResult = AddSelectedEntityObjects();
				}
				break;
			}

		case eFormat_XML:
			{
				if (bCompleteSequence)
				{
					if (ShowXMLExportDialog())
					{
						CTrackViewSequence* pSequence = CTrackViewPlugin::GetAnimationContext()->GetSequence();
						if (pSequence)
						{
							XmlNodeRef xmlOutput = XmlHelpers::CreateXmlNode(pSequence->GetName());
							AddObjectsFromSequence(pSequence, xmlOutput);
							return xmlOutput->saveToFile(fileName);
						}
					}
				}
				else
				{
					// TODO: single node xml export
				}
				break;
			}

		default:
			{
				CryLog("Invalid format");
				return false;
			}
			break;
		}

		if (bResult)
		{
			IExportManager* pExportManager = GetIEditor()->GetExportManager();
			if (pExportManager)
			{
				IExporter* pExporter = pExportManager->GetExporterForExtension(szExt);
				if (pExporter)
				{
					return pExporter->ExportToFile(fileName, &m_data);
				}
				else
				{
					CryLog("Couldn't find suitable exporter for %s file format!", szExt);
				}
			}
		}
	}
	return false;
}

bool CTrackViewExporter::ShowFBXExportDialog()
{
	CTrackViewSequence* pSequence = CTrackViewPlugin::GetAnimationContext()->GetSequence();
	if (!pSequence)
	{
		return false;
	}

	CTrackViewNode* pivotObjectNode = pSequence->GetFirstSelectedNode();

	if (pivotObjectNode && !pivotObjectNode->IsGroupNode())
	{
		m_pivotEntityObject = static_cast<CEntityObject*>(GetIEditor()->GetObjectManager()->FindObject(pivotObjectNode->GetName()));
	}

	bool bExportMasterCamOnly = false;
	bool bExportLocalToSelected = false;

	QFBXExporterDlg fbxExporterDlg(false, false, (m_pivotEntityObject != nullptr));
	fbxExporterDlg.exec();

	if (!fbxExporterDlg.GetResult(m_FBXBakedExportFramerate, m_bExportOnlyMasterCamera, m_bExportLocalCoords))
	{
		return false;
	}

	return true;
}

bool CTrackViewExporter::ShowXMLExportDialog()
{
	QXMLExporterDlg xmlExporterDlg;
	xmlExporterDlg.exec();
	return xmlExporterDlg.GetResult(m_bAnimKeyTimeExport, m_bSoundKeyTimeExport);
}

bool CTrackViewExporter::AddSelectedEntityObjects()
{
	CTrackViewSequence* pSequence = CTrackViewPlugin::GetAnimationContext()->GetSequence();

	if (!pSequence)
	{
		return false;
	}

	CTrackViewAnimNodeBundle selectedNodes = pSequence->GetSelectedAnimNodes();
	const unsigned int numSelectedNodes = selectedNodes.GetCount();

	for (unsigned int nodeNumber = 0; nodeNumber < numSelectedNodes; ++nodeNumber)
	{
		CTrackViewAnimNode* pAnimNode = selectedNodes.GetNode(nodeNumber);
		CEntityObject* pEntityObject = pAnimNode->GetNodeEntity();

		if (pEntityObject)
		{
			// Don't add selected cameraTarget nodes directly. Add them through GetLookAt, before you add camera object.
			if (pEntityObject->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget)))
			{
				if (numSelectedNodes == 1)
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Please select camera object, to export its camera target.");
					return false;
				}

				continue;
			}

			CBaseObjectPtr pLookAt = pEntityObject->GetLookAt();

			if (pLookAt && pLookAt->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget)))
			{
				AddObject(pLookAt);
			}

			AddObject(pEntityObject);
		}
	}

	return true;
}

bool CTrackViewExporter::AddObject(CBaseObject* pBaseObj)
{
	if (pBaseObj->GetType() == OBJTYPE_GROUP)
	{
		for (int i = 0; i < pBaseObj->GetChildCount(); i++)
		{
			AddObject(pBaseObj->GetChild(i));
		}
	}

	/*
	   if(m_isOccluder)
	   {
	    const ObjectType OT	=	pBaseObj->GetType();
	    IRenderNode* pRenderNode = pBaseObj->GetEngineNode();
	    if( (OT==OBJTYPE_BRUSH || OT==OBJTYPE_SOLID) && pRenderNode )
	    {
	      if((pRenderNode->GetRndFlags()&ERF_GOOD_OCCLUDER)==0)
	        return false;
	    }
	    else
	    {
	      return false;
	    }
	   }
	 */
	m_pBaseObj = pBaseObj;

	if (pBaseObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		AddEntityAnimationData(pBaseObj);
		return true;
	}

	/*
	   if (m_isPrecaching)
	   {
	    AddMeshes(0);
	    return true;
	   }

	   Export::CObject* pObj = new Export::CObject(m_pBaseObj->GetName());

	   AddPosRotScale(pObj,pBaseObj);
	   m_data.m_objects.push_back(pObj);

	   m_objectMap[pBaseObj] = int(m_data.m_objects.size()-1);

	   AddMeshes(pObj);
	   m_pBaseObj = 0;
	 */
	return true;
}

void CTrackViewExporter::AddEntityAnimationData(CBaseObject* pBaseObj)
{
	SExportObject* pObj = new SExportObject(m_pBaseObj->GetName());

	if (pBaseObj->IsKindOf(RUNTIME_CLASS(CCameraObject)))
	{
		pObj->SetObjectEntityType(Export::eCamera);

		CBaseObjectPtr pLookAt = pBaseObj->GetLookAt();
		if (pLookAt)
		{
			if (pLookAt->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget)))
			{
				cry_strcpy(pObj->cameraTargetNodeName, pLookAt->GetName());
			}
		}

		ProcessEntityAnimationTrack(pBaseObj, pObj, eAnimParamType_FOV);
	}
	else if (pBaseObj->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget)))
	{
		pObj->SetObjectEntityType(Export::eCameraTarget);
	}

	ProcessEntityAnimationTrack(pBaseObj, pObj, eAnimParamType_Position);
	ProcessEntityAnimationTrack(pBaseObj, pObj, eAnimParamType_Rotation);

	m_data.AddObject(pObj);
	m_objectMap[pBaseObj] = int(m_data.GetObjectCount() - 1);
}

void CTrackViewExporter::AddEntityAnimationData(const CTrackViewTrack* pTrack, SExportObject* pObj, EAnimParamType entityTrackParamType)
{
	int keyCount = pTrack->GetKeyCount();
	pObj->m_entityAnimData.reserve(keyCount * Export::kReserveCount);

	SCurveEditorCurve currentCurve;
	CreateCurveFromTrack(pTrack, currentCurve);

	for (int keyNumber = 0; keyNumber < keyCount; keyNumber++)
	{
		const CTrackViewKeyConstHandle currentKeyHandle = pTrack->GetKey(keyNumber);
		const SAnimTime currentKeyTime = currentKeyHandle.GetTime();

		float trackValue = stl::get<float>(pTrack->GetValue(currentKeyTime));
		if (currentCurve.GetKeyCount() > 0)
		{
			Export::EntityAnimData entityData;
			entityData.dataType = (EAnimParamType)pTrack->GetParameterType().GetType();
			entityData.keyValue = trackValue;
			entityData.keyTime = currentKeyTime.ToFloat();

			if (entityTrackParamType == eAnimParamType_Position)
			{
				entityData.keyValue *= 100.0f;
			}
			else if (entityTrackParamType == eAnimParamType_FOV)
			{
				entityData.keyValue = Sandbox2MayaFOVDeg(entityData.keyValue, Export::kAspectRatio);
			}

			float fInTantentX = currentCurve.m_keys[keyNumber].m_controlPoint.m_inTangent.x;
			float fInTantentY = currentCurve.m_keys[keyNumber].m_controlPoint.m_inTangent.y;

			float fOutTantentX = currentCurve.m_keys[keyNumber].m_controlPoint.m_outTangent.x;
			float fOutTantentY = currentCurve.m_keys[keyNumber].m_controlPoint.m_outTangent.y;

			float fTangentDelta = 0.01f;

			if (fInTantentX == 0.0f)
			{
				fInTantentX = Export::kTangentDelta;
			}

			if (fOutTantentX == 0.0f)
			{
				fOutTantentX = Export::kTangentDelta;
			}

			entityData.leftTangent = fInTantentY / fInTantentX;
			entityData.rightTangent = fOutTantentY / fOutTantentX;

			if (entityTrackParamType == eAnimParamType_Position)
			{
				entityData.leftTangent *= 100.0f;
				entityData.rightTangent *= 100.0f;
			}

			SAnimTime prevKeyTime(0);
			SAnimTime nextKeyTime(0);

			bool bIsFirstKey = false;
			bool bIsMiddleKey = false;
			bool bIsLastKey = false;

			if (keyNumber == 0 && keyNumber < (keyCount - 1))
			{
				const CTrackViewKeyConstHandle nextKeyHandle = pTrack->GetKey(keyNumber + 1);
				nextKeyTime = nextKeyHandle.GetTime();

				if (nextKeyTime != SAnimTime(0))
				{
					bIsFirstKey = true;
				}
			}
			else if (keyNumber > 0)
			{
				const CTrackViewKeyConstHandle prevKeyHandle = pTrack->GetKey(keyNumber - 1);
				prevKeyTime = prevKeyHandle.GetTime();

				if (keyNumber < (keyCount - 1))
				{
					const CTrackViewKeyConstHandle nextKeyHandle = pTrack->GetKey(keyNumber + 1);
					nextKeyTime = nextKeyHandle.GetTime();

					if (nextKeyTime != SAnimTime(0))
					{
						bIsMiddleKey = true;
					}
				}
				else
				{
					bIsLastKey = true;
				}
			}

			float fLeftTangentWeightValue = 0.0f;
			float fRightTangentWeightValue = 0.0f;

			if (bIsFirstKey)
			{
				fRightTangentWeightValue = fOutTantentX / nextKeyTime;
			}
			else if (bIsMiddleKey)
			{
				fLeftTangentWeightValue = fInTantentX / (currentKeyTime - prevKeyTime);
				fRightTangentWeightValue = fOutTantentX / (nextKeyTime - currentKeyTime);
			}
			else if (bIsLastKey)
			{
				fLeftTangentWeightValue = fInTantentX / (currentKeyTime - prevKeyTime);
			}

			entityData.leftTangentWeight = fLeftTangentWeightValue;
			entityData.rightTangentWeight = fRightTangentWeightValue;
			pObj->AddEntityAnimationData(entityData);
		}
	}
}

void CTrackViewExporter::ProcessEntityAnimationTrack(const CBaseObject* pBaseObj, SExportObject* pObj, EAnimParamType entityTrackParamType)
{
	CTrackViewAnimNode* pEntityNode = nullptr;

	if (pBaseObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		pEntityNode = CTrackViewPlugin::GetSequenceManager()->GetActiveAnimNode(static_cast<const CEntityObject*>(pBaseObj));
	}

	CTrackViewTrack* pEntityTrack = (pEntityNode ? pEntityNode->GetTrackForParameter(entityTrackParamType) : 0);

	bool bIsCameraTargetObject = pBaseObj->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget));

	if (!pEntityTrack)
	{
		// In case its CameraTarget which does not have track
		if (bIsCameraTargetObject && entityTrackParamType != eAnimParamType_FOV)
		{
			AddPosRotScale(pObj, pBaseObj);
		}

		return;
	}

	if (pEntityTrack->GetParameterType() == eAnimParamType_FOV)
	{
		AddEntityAnimationData(pEntityTrack, pObj, entityTrackParamType);
		return;
	}

	bool bIsCameraObject = pBaseObj->IsKindOf(RUNTIME_CLASS(CCameraObject));

	if (pEntityTrack->GetParameterType() == eAnimParamType_Rotation && bIsCameraObject)
	{
		bool bShakeTrackPresent = false;
		CTrackViewTrack* pParentTrack = static_cast<CTrackViewTrack*>(pEntityTrack->GetParentNode());

		if (pParentTrack)
		{
			for (int trackID = 0; trackID < pParentTrack->GetChildCount(); ++trackID)
			{
				CTrackViewTrack* pSubTrack = static_cast<CTrackViewTrack*>(pParentTrack->GetChild(trackID));

				if (pSubTrack->GetParameterType() == eAnimParamType_ShakeMultiplier)
				{
					QString titleText = QWidget::tr("Camera shake track");
					QString messageText = QWidget::tr("%1 contains a shake track. Do you want this data to be baked into the rotation data?").arg(QWidget::tr(pBaseObj->GetName()));
					bShakeTrackPresent = (QDialogButtonBox::Yes == CQuestionDialog::SQuestion(titleText, messageText));
					break;
				}

			}
		}

		if (bShakeTrackPresent)
		{
			CTrackViewSequence* pSequence = CTrackViewPlugin::GetAnimationContext()->GetSequence();

			if (!pSequence)
			{
				return;
			}

			CTrackViewAnimNode* pCameraNode = nullptr;

			if (pBaseObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			{
				pCameraNode = CTrackViewPlugin::GetSequenceManager()->GetActiveAnimNode(static_cast<const CEntityObject*>(pBaseObj));
			}

			SAnimTime ticks(0);
			const size_t sizeInfo = pObj->GetEntityAnimationDataCount();
			const int32 fpsTimeInterval = SAnimTime::numTicksPerSecond / m_FBXBakedExportFramerate;

			pObj->m_entityAnimData.reserve(m_numberOfExportFrames * Export::kReserveCount);

			for (int frameID = 0; frameID < m_numberOfExportFrames; ++frameID)
			{
				Quat rotation(Ang3(0.0f, 0.0f, 0.0f));
				static_cast<CTrackViewCameraNode*>(pCameraNode)->GetShakeRotation(ticks, rotation);
				Ang3 worldAngles = RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(rotation)));

				Export::EntityAnimData entityData;
				entityData.keyTime = ticks.ToFloat();
				entityData.leftTangentWeight = 0.0f;
				entityData.rightTangentWeight = 0.0f;
				entityData.leftTangent = 0.0f;
				entityData.rightTangent = 0.0f;

				entityData.keyValue = worldAngles.x;
				entityData.dataType = (EAnimParamType)eAnimParamType_RotationX;
				pObj->AddEntityAnimationData(entityData);

				entityData.keyValue = worldAngles.y;
				entityData.dataType = (EAnimParamType)eAnimParamType_RotationY;
				pObj->AddEntityAnimationData(entityData);

				entityData.keyValue = worldAngles.z;
				entityData.dataType = (EAnimParamType)eAnimParamType_RotationZ;
				pObj->AddEntityAnimationData(entityData);

				ticks += SAnimTime(fpsTimeInterval);
			}

			return;
		}
	}

	for (int trackNumber = 0; trackNumber < pEntityTrack->GetChildCount(); ++trackNumber)
	{
		CTrackViewTrack* pSubTrack = static_cast<CTrackViewTrack*>(pEntityTrack->GetChild(trackNumber));

		if (pSubTrack)
		{
			AddEntityAnimationData(pSubTrack, pObj, entityTrackParamType);
		}
	}
}

void CTrackViewExporter::AddPosRotScale(SExportObject* pObj, const CBaseObject* pBaseObj)
{
	Vec3 pos = pBaseObj->GetPos();
	pObj->pos.x = pos.x * m_fScale;
	pObj->pos.y = pos.y * m_fScale;
	pObj->pos.z = pos.z * m_fScale;

	Quat rot = pBaseObj->GetRotation();
	pObj->rot.v.x = rot.v.x;
	pObj->rot.v.y = rot.v.y;
	pObj->rot.v.z = rot.v.z;
	pObj->rot.w = rot.w;

	Vec3 scale = pBaseObj->GetScale();
	pObj->scale.x = scale.x;
	pObj->scale.y = scale.y;
	pObj->scale.z = scale.z;

	if (pBaseObj->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget)))
	{
		AddEntityData(pObj, eAnimParamType_PositionX, pBaseObj->GetPos().x, 0.0f);
		AddEntityData(pObj, eAnimParamType_PositionY, pBaseObj->GetPos().y, 0.0f);
		AddEntityData(pObj, eAnimParamType_PositionZ, pBaseObj->GetPos().z, 0.0f);
		AddEntityData(pObj, eAnimParamType_RotationX, pBaseObj->GetRotation().v.x, 0.0f);
		AddEntityData(pObj, eAnimParamType_RotationY, pBaseObj->GetRotation().v.y, 0.0f);
		AddEntityData(pObj, eAnimParamType_RotationZ, pBaseObj->GetRotation().v.z, 0.0f);
	}
}

void CTrackViewExporter::AddEntityData(SExportObject* pObj, EAnimParamType dataType, const float fValue, const float fTime)
{
	Export::EntityAnimData entityData;
	entityData.dataType = dataType;
	entityData.leftTangent = Export::kTangentDelta;
	entityData.rightTangent = Export::kTangentDelta;
	entityData.rightTangentWeight = 0.0f;
	entityData.leftTangentWeight = 0.0f;
	entityData.keyValue = fValue;
	entityData.keyTime = fTime;
	pObj->AddEntityAnimationData(entityData);
}

bool CTrackViewExporter::AddObjectsFromSequence(CTrackViewSequence* pSequence, XmlNodeRef seqNode)
{
	CTrackViewAnimNodeBundle allNodes = pSequence->GetAllAnimNodes();
	const unsigned int numAllNodes = allNodes.GetCount();

	for (unsigned int nodeID = 0; nodeID < numAllNodes; ++nodeID)
	{
		CTrackViewAnimNode* pAnimNode = allNodes.GetNode(nodeID);
		CEntityObject* pEntityObject = pAnimNode->GetNodeEntity();

		if (seqNode && pAnimNode)
		{
			FillAnimTimeNode(seqNode, pAnimNode, pSequence);
		}

		if (pEntityObject)
		{
			string addObjectName = pEntityObject->GetName();

			if (IsDuplicateObjectBeingAdded(addObjectName))
			{
				continue;
			}

			SExportObject* pObj = new SExportObject(pEntityObject->GetName());

			if (pEntityObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
			{
				pObj->SetObjectEntityType(Export::eCamera);
				CBaseObjectPtr pLookAt = pEntityObject->GetLookAt();

				if (pLookAt)
				{
					if (AddCameraTargetObject(pLookAt))
					{
						cry_strcpy(pObj->cameraTargetNodeName, pLookAt->GetName());
					}
				}
			}
			else if (pEntityObject->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget)))
			{
				// Add camera target through and before its camera object.
				continue;
			}

			pObj->m_entityAnimData.reserve(m_numberOfExportFrames * Export::kReserveCount);
			pObj->SetLastPtr(pEntityObject);
			m_data.AddObject(pObj);
			m_objectMap[pEntityObject] = int(m_data.GetObjectCount() - 1);
		}
	}

	CTrackViewTrackBundle trackBundle = pSequence->GetTracksByParam(eAnimParamType_Sequence);

	const uint numSequenceTracks = trackBundle.GetCount();

	for (uint i = 0; i < numSequenceTracks; ++i)
	{
		CTrackViewTrack* pSequenceTrack = trackBundle.GetTrack(i);

		if (pSequenceTrack->IsDisabled())
		{
			continue;
		}

		const uint numKeys = pSequenceTrack->GetKeyCount();

		for (int keyIndex = 0; keyIndex < numKeys; ++keyIndex)
		{
			CTrackViewKeyHandle& keyHandle = pSequenceTrack->GetKey(keyIndex);
			SSequenceKey sequenceKey;
			keyHandle.GetKey(&sequenceKey);

			if (sequenceKey.m_sequenceGUID != CryGUID::Null())
			{
				CTrackViewSequence* pSubSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(sequenceKey.m_sequenceGUID);

				if (pSubSequence && !pSubSequence->IsDisabled())
				{
					XmlNodeRef subSeqNode = 0;

					if (!seqNode)
					{
						AddObjectsFromSequence(pSubSequence);
					}
					else
					{
						// In case of exporting animation/sound times data
						const string sequenceName = pSubSequence->GetName();
						XmlNodeRef subSeqNode = seqNode->createNode(sequenceName);

						if (sequenceName == m_animTimeExportMasterSequenceName)
						{
							m_animTimeExportMasterSequenceCurrentTime = sequenceKey.m_time;
						}
						else
						{
							m_animTimeExportMasterSequenceCurrentTime += sequenceKey.m_time;
						}

						AddObjectsFromSequence(pSubSequence, subSeqNode);
						seqNode->addChild(subSeqNode);
					}
				}
			}
		}
	}

	return false;
}

void CTrackViewExporter::FillAnimTimeNode(XmlNodeRef writeNode, CTrackViewAnimNode* pObjectNode, CTrackViewSequence* currentSequence)
{
	if (!writeNode)
	{
		return;
	}

	CTrackViewTrackBundle allTracks = pObjectNode->GetAllTracks();
	const unsigned int numAllTracks = allTracks.GetCount();
	bool bCreatedSubNodes = false;

	if (numAllTracks > 0)
	{
		XmlNodeRef objNode = writeNode->createNode(CleanXMLText(pObjectNode->GetName()));
		writeNode->setAttr("time", m_animTimeExportMasterSequenceCurrentTime.ToFloat());

		for (unsigned int trackID = 0; trackID < numAllTracks; ++trackID)
		{
			CTrackViewTrack* childTrack = allTracks.GetTrack(trackID);

			EAnimParamType trackType = childTrack->GetParameterType().GetType();

			if (trackType == eAnimParamType_Animation || trackType == eAnimParamType_AudioTrigger)
			{
				string childName = CleanXMLText(childTrack->GetName());

				if (childName.IsEmpty())
				{
					continue;
				}

				XmlNodeRef subNode = objNode->createNode(childName);
				CTrackViewKeyBundle keyBundle = childTrack->GetAllKeys();
				uint keysNumber = keyBundle.GetKeyCount();

				for (uint keyID = 0; keyID < keysNumber; ++keyID)
				{
					CTrackViewKeyHandle& keyHandle = keyBundle.GetKey(keyID);

					string keyContentName;
					SAnimTime keyTime(0);
					float keyStartTime = 0.0f;
					float keyEndTime = 0.0f;
					float keyDuration = 0.0f;

					if (trackType == eAnimParamType_Animation)
					{
						if (!m_bAnimKeyTimeExport)
						{
							continue;
						}

						SCharacterKey animationKey;
						keyHandle.GetKey(&animationKey);
						keyStartTime = animationKey.m_startTime;
						keyEndTime = animationKey.m_endTime;
						keyTime = animationKey.m_time;
						keyContentName = CleanXMLText(animationKey.m_animation);
						keyDuration = animationKey.GetAnimDuration();
					}
					else if (trackType == eAnimParamType_AudioTrigger)
					{
						if (!m_bSoundKeyTimeExport)
						{
							continue;
						}

						SAudioTriggerKey audioTriggerKey;
						keyHandle.GetKey(&audioTriggerKey);
						keyTime = audioTriggerKey.m_time;
						keyContentName = CleanXMLText(audioTriggerKey.m_startTriggerName.c_str());
						keyDuration = audioTriggerKey.m_duration.ToFloat();
					}

					if (keyContentName.IsEmpty())
					{
						continue;
					}

					XmlNodeRef keyNode = subNode->createNode(keyContentName);

					SAnimTime keyGlobalTime = m_animTimeExportMasterSequenceCurrentTime + keyTime;
					keyNode->setAttr("keyTime", keyGlobalTime.ToFloat());

					if (keyStartTime > 0)
					{
						keyNode->setAttr("startTime", keyStartTime);
					}

					if (keyEndTime > 0)
					{
						keyNode->setAttr("endTime", keyEndTime);
					}

					if (keyDuration > 0)
					{
						keyNode->setAttr("duration", keyDuration);
					}

					subNode->addChild(keyNode);
					objNode->addChild(subNode);
					bCreatedSubNodes = true;
				}
			}
		}

		if (bCreatedSubNodes)
		{
			writeNode->addChild(objNode);
		}
	}
}

bool CTrackViewExporter::IsDuplicateObjectBeingAdded(const string& newObjectName)
{
	for (int objectID = 0; objectID < m_data.GetObjectCount(); ++objectID)
	{
		if (newObjectName.CompareNoCase(m_data.m_objects[objectID]->name) == 0)
		{
			return true;
		}
	}

	return false;
}

bool CTrackViewExporter::AddCameraTargetObject(CBaseObjectPtr pLookAt)
{
	if (pLookAt && pLookAt->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget)))
	{
		if (IsDuplicateObjectBeingAdded(pLookAt->GetName()))
		{
			return false;
		}

		SExportObject* pCameraTargetObject = new SExportObject(pLookAt->GetName());
		pCameraTargetObject->SetObjectEntityType(Export::eCameraTarget);
		pCameraTargetObject->m_entityAnimData.reserve(m_numberOfExportFrames * Export::kReserveCount);
		pCameraTargetObject->SetLastPtr(pLookAt);

		m_data.AddObject(pCameraTargetObject);
		m_objectMap[pLookAt] = int(m_data.GetObjectCount() - 1);

		return true;
	}

	return false;
}

string CTrackViewExporter::CleanXMLText(const string& text)
{
	string outText(text);
	outText.Replace("\\", "_");
	outText.Replace("/", "_");
	outText.Replace(" ", "_");
	outText.Replace(":", "-");
	outText.Replace(";", "-");
	return outText;
}

bool CTrackViewExporter::ProcessObjectsForExport()
{
	SExportObject* pObj = new SExportObject(Export::kMasterCameraName);
	pObj->SetObjectEntityType(Export::eCamera);
	m_data.AddObject(pObj);

	const int32 fpsTimeInterval = SAnimTime::numTicksPerSecond / m_FBXBakedExportFramerate;
	IObjectManager* pObjMgr = GetIEditor()->GetObjectManager();

	CTrackViewPlugin::GetAnimationContext()->SetRecording(false);
	CTrackViewPlugin::GetAnimationContext()->PlayPause(false);

	CViewport* vp = GetIEditor()->GetViewManager()->GetSelectedViewport();
	/*
	   if (vp && vp->IsRenderViewport())
	    ((CRenderViewport*)vp)->SetSequenceCamera();
	 */
	const int32 startFrame = 0;
	SAnimTime ticks(startFrame * fpsTimeInterval);

	for (int32 frameID = startFrame; frameID <= m_numberOfExportFrames; ++frameID)
	{
		CTrackViewPlugin::GetAnimationContext()->SetTime(ticks);

		for (size_t objectID = 0; objectID < m_data.GetObjectCount(); ++objectID)
		{
			SExportObject* pObj = m_data.GetExportObject(objectID);
			CBaseObject* pObject = 0;

			if (strcmp(pObj->name, Export::kMasterCameraName) == 0)
			{
				CryGUID camGUID = GetIEditor()->GetViewManager()->GetCameraObjectId();
				pObject = GetIEditor()->GetObjectManager()->FindObject(GetIEditor()->GetViewManager()->GetCameraObjectId());
			}
			else
			{
				if (m_bExportOnlyMasterCamera && pObj->GetObjectEntityType() != Export::eCameraTarget)
				{
					continue;
				}

				pObject = pObj->GetLastObjectPtr();
			}

			if (!pObject)
			{
				continue;
			}

			Quat rotation(pObject->GetRotation());

			if (pObject->GetParent())
			{
				CBaseObject* pParentObject = pObject->GetParent();
				Quat parentWorldRotation;

				const Vec3& parentScale = pParentObject->GetScale();
				float threshold = 0.0003f;

				bool bParentScaled = false;

				if ((fabsf(parentScale.x - 1.0f) + fabsf(parentScale.y - 1.0f) + fabsf(parentScale.z - 1.0f)) >= threshold)
				{
					bParentScaled = true;
				}

				if (bParentScaled)
				{
					Matrix34 tm = pParentObject->GetWorldTM();
					tm.OrthonormalizeFast();
					parentWorldRotation = Quat(tm);
				}
				else
				{
					parentWorldRotation = Quat(pParentObject->GetWorldTM());
				}

				rotation = parentWorldRotation * rotation;
			}

			Vec3 objectPos = pObject->GetWorldPos();

			if (m_bExportLocalCoords && m_pivotEntityObject && m_pivotEntityObject != pObject)
			{
				Matrix34 currentObjectTM = pObject->GetWorldTM();
				Matrix34 invParentTM = m_pivotEntityObject->GetWorldTM();
				invParentTM.Invert();
				currentObjectTM = invParentTM * currentObjectTM;

				objectPos = currentObjectTM.GetTranslation();
				rotation = Quat(currentObjectTM);
			}

			Ang3 worldAngles = RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(rotation)));

			Export::EntityAnimData entityData;
			entityData.keyTime = ticks.ToFloat();
			entityData.leftTangentWeight = 0.0f;
			entityData.rightTangentWeight = 0.0f;
			entityData.leftTangent = 0.0f;
			entityData.rightTangent = 0.0f;

			entityData.keyValue = objectPos.x;
			entityData.keyValue *= 100.0f;
			entityData.dataType = (EAnimParamType)eAnimParamType_PositionX;
			pObj->AddEntityAnimationData(entityData);

			entityData.keyValue = objectPos.y;
			entityData.keyValue *= 100.0f;
			entityData.dataType = (EAnimParamType)eAnimParamType_PositionY;
			pObj->AddEntityAnimationData(entityData);

			entityData.keyValue = objectPos.z;
			entityData.keyValue *= 100.0f;
			entityData.dataType = (EAnimParamType)eAnimParamType_PositionZ;
			pObj->AddEntityAnimationData(entityData);

			entityData.keyValue = worldAngles.x;
			entityData.dataType = (EAnimParamType)eAnimParamType_RotationX;
			pObj->AddEntityAnimationData(entityData);

			entityData.keyValue = worldAngles.y;
			entityData.dataType = (EAnimParamType)eAnimParamType_RotationY;
			pObj->AddEntityAnimationData(entityData);

			entityData.keyValue = worldAngles.z;
			entityData.dataType = (EAnimParamType)eAnimParamType_RotationZ;
			pObj->AddEntityAnimationData(entityData);

			if (pObject->IsKindOf(RUNTIME_CLASS(CCameraObject)) || pObj->GetObjectEntityType() == Export::eCamera)
			{
				entityData.dataType = (EAnimParamType)eAnimParamType_FOV;
				CCameraObject* pCameraObject = (CCameraObject*)pObject;
				IEntity* entity = pCameraObject->GetIEntity();
				IEntityCameraComponent* pCameraProxy = (IEntityCameraComponent*)entity->GetProxy(ENTITY_PROXY_CAMERA);

				if (pCameraProxy)
				{
					CCamera& cam = pCameraProxy->GetCamera();
					entityData.keyValue = Sandbox2MayaFOVRad2Deg(cam.GetFov(), Export::kAspectRatio);
					pObj->AddEntityAnimationData(entityData);
				}
			}
		}

		ticks += SAnimTime(fpsTimeInterval);
	}

	return true;
}

void CTrackViewExporter::SolveHierarchy()
{
	for (TObjectMap::iterator it = m_objectMap.begin(); it != m_objectMap.end(); ++it)
	{
		CBaseObject* pObj = it->first;
		int index = it->second;

		if (pObj && pObj->GetParent())
		{
			CBaseObject* pParent = pObj->GetParent();
			TObjectMap::iterator itFind = m_objectMap.find(pParent);

			if (itFind != m_objectMap.end())
			{
				int indexOfParent = itFind->second;

				if (indexOfParent >= 0 && index >= 0)
				{
					m_data.GetExportObject(index)->nParent = indexOfParent;
				}
			}
		}
	}

	m_objectMap.clear();
}

CTrackViewTrack* CTrackViewExporter::GetTrackViewTrack(const Export::EntityAnimData* pAnimData, CTrackViewTrackBundle trackBundle, const string& nodeName)
{
	for (unsigned int trackId = 0; trackId < trackBundle.GetCount(); ++trackId)
	{
		CTrackViewTrack* pTrack = trackBundle.GetTrack(trackId);
		const string bundleTrackName = pTrack->GetAnimNode()->GetName();

		if (bundleTrackName.CompareNoCase(nodeName) != 0)
		{
			continue;
		}

		// Position, Rotation
		if (pTrack->IsCompoundTrack())
		{
			for (uint childTrackId = 0; childTrackId < pTrack->GetChildCount(); ++childTrackId)
			{
				CTrackViewTrack* childTrack = static_cast<CTrackViewTrack*>(pTrack->GetChild(childTrackId));
				if (childTrack->GetParameterType().GetType() == pAnimData->dataType)
				{
					return childTrack;
				}
			}
		}

		// FOV
		if (pTrack->GetParameterType().GetType() == pAnimData->dataType)
		{
			return pTrack;
		}
	}

	return nullptr;
}

void CTrackViewExporter::CreateCurveFromTrack(const CTrackViewTrack* pTrack, SCurveEditorCurve& curve)
{
	if (pTrack)
	{
		const CryGUID trackGUID = pTrack->GetGUID();
		curve.userSideLoad.resize(sizeof(CryGUID));
		memcpy(&curve.userSideLoad[0], &trackGUID, sizeof(CryGUID));

		const uint numKeys = pTrack->GetKeyCount();

		for (uint i = 0; i < numKeys; ++i)
		{
			CTrackViewKeyConstHandle& keyHandle = pTrack->GetKey(i);

			S2DBezierKey bezierKey;
			keyHandle.GetKey(&bezierKey);

			SCurveEditorKey curveEditorKey;
			curveEditorKey.m_time = bezierKey.m_time;
			curveEditorKey.m_controlPoint = bezierKey.m_controlPoint;
			curveEditorKey.m_bSelected = true;
			curve.m_keys.push_back(curveEditorKey);
		}
	}
}

