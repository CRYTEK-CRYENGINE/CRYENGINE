// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IExportManager.h>

class CEntityObject;
class CTrackViewTrack;
class CTrackViewAnimNode;
class CTrackViewSequence;
class CTrackViewTrackBundle;
struct SCurveEditorCurve;

class CTrackViewExporter
{
public:
	CTrackViewExporter();
	~CTrackViewExporter();

	bool ImportFromFile();
	bool ExportToFile(bool bCompleteSequence = false);

private:
	bool             ImportFBXFile();
	bool             ReadFileIntoMemory(const char* fileName);

	bool             ShowFBXExportDialog();
	bool             ShowXMLExportDialog();

	bool             AddSelectedEntityObjects();
	bool             AddObject(CBaseObject* pBaseObj);
	void             AddEntityAnimationData(CBaseObject* pBaseObj);
	void             AddEntityAnimationData(const CTrackViewTrack* pTrack, SExportObject* pObj, EAnimParamType entityTrackParamType);
	void             ProcessEntityAnimationTrack(const CBaseObject* pBaseObj, SExportObject* pObj, EAnimParamType entityTrackParamType);

	void             AddPosRotScale(SExportObject* pObj, const CBaseObject* pBaseObj);
	void             AddEntityData(SExportObject* pObj, EAnimParamType dataType, const float fValue, const float fTime);

	bool             AddObjectsFromSequence(CTrackViewSequence* pSequence, XmlNodeRef seqNode = 0);
	void             FillAnimTimeNode(XmlNodeRef writeNode, CTrackViewAnimNode* pObjectNode, CTrackViewSequence* currentSequence);
	bool             IsDuplicateObjectBeingAdded(const string& newObjectName);
	bool             AddCameraTargetObject(CBaseObjectPtr pLookAt);

	string          CleanXMLText(const string& text);
	bool             ProcessObjectsForExport();
	void             SolveHierarchy();

	CTrackViewTrack* GetTrackViewTrack(const Export::EntityAnimData* pAnimData, CTrackViewTrackBundle trackBundle, const string& nodeName);
	void             CreateCurveFromTrack(const CTrackViewTrack* pTrack, SCurveEditorCurve& curve);

	bool           m_bBakedKeysSequenceExport;

	uint           m_FBXBakedExportFramerate;
	bool           m_bExportLocalCoords;
	bool           m_bExportOnlyMasterCamera;
	int            m_numberOfExportFrames;
	CEntityObject* m_pivotEntityObject;

	string        m_animTimeExportMasterSequenceName;
	SAnimTime      m_animTimeExportMasterSequenceCurrentTime;
	XmlNodeRef     m_animTimeNode;

	bool           m_bAnimKeyTimeExport;
	bool           m_bSoundKeyTimeExport;

	SExportData    m_data;
	float          m_fScale;
	typedef std::map<CBaseObject*, int> TObjectMap;
	TObjectMap     m_objectMap;
	CBaseObject*   m_pBaseObj;
};

