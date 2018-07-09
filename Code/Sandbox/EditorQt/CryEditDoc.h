// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IEditorImpl.h"
#include "DocMultiArchive.h"
#include <CrySystem/TimeValue.h>

class CMission;
class CLevelShaderCache;
struct ICVar;
struct LightingSettings;
struct IVariable;

class SANDBOX_API CCryEditDoc : public _i_reference_target_t
{
public:
	CCryEditDoc();
	virtual ~CCryEditDoc();

	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);

	//TODO: this modified flag is only the level, not the entire data, should be moved to assets-based modified flags instead
	void         SetModifiedFlag(bool bModified);

	//Returns whether the level was modified. Not exposed anymore, please rely on asset-based modifier flags instead, should not be necessary to access from outside Sandbox
	bool         IsModified() const;

	//! Only true when closing the document
	bool		 IsClosing() const { return m_bIsClosing; }
	virtual void DeleteContents();
	virtual BOOL DoFileSave();

	struct TOpenDocContext
	{
		CTimeValue loading_start_time;
		string    relativeLevelName;
	};
	BOOL               BeforeOpenDocument(LPCTSTR lpszPathName, TOpenDocContext& context);
	BOOL               DoOpenDocument(LPCTSTR lpszPathName, TOpenDocContext& context);
	virtual BOOL       LoadXmlArchiveArray(TDocMultiArchive& arrXmlAr, const string& relativeLevelName, const string& levelPath);
	virtual void       ReleaseXmlArchiveArray(TDocMultiArchive& arrXmlAr);

	virtual void       Load(TDocMultiArchive& arrXmlAr, const string& szFilename);
	virtual void       StartStreamingLoad() {}
	virtual void       SyncCurrentMissionContent(bool bRetrieve);

	void               InitEmptyLevel(int resolution = 0, float unitSize = 0, bool bUseTerrain = false);
	bool               IsLevelExported() const;
	void               SetLevelExported(bool boExported = true);
	void               ChangeMission();
	bool               Save();
	void               Save(CXmlArchive& xmlAr);
	void               Load(CXmlArchive& xmlAr, const string& szFilename);
	void               Save(TDocMultiArchive& arrXmlAr);
	bool               SaveLevel(const string& filename);
	bool               LoadLevel(TDocMultiArchive& arrXmlAr, const string& filename);
	//! Get the path of the last level that was loaded last (relative to project root)
	QString            GetLastLoadedLevelName();
	//! Set the relative path of the last level that was loaded last (relative to project root)
	void               SetLastLoadedLevelName(const char* lastLoadedFileName);

	void               SaveAutoBackup(bool bForce = false);
	void               SerializeFogSettings(CXmlArchive& xmlAr);
	virtual void       SerializeViewSettings(CXmlArchive& xmlAr);
	void               SerializeMissions(TDocMultiArchive& arrXmlAr, string& currentMission, bool bPartsInXml);
	void               SerializeShaderCache(CXmlArchive& xmlAr);
	LightingSettings*  GetLighting();
	void               SetWaterColor(COLORREF col) { m_waterColor = col; };
	COLORREF           GetWaterColor()             { return m_waterColor; };
	void               ForceSkyUpdate();
	BOOL               CanCloseFrame(CFrameWnd* pFrame);
	void               GetMemoryUsage(ICrySizer* pSizer);
	XmlNodeRef&        GetFogTemplate()         { return m_fogTemplate; };
	XmlNodeRef&        GetEnvironmentTemplate() { return m_environmentTemplate; };
	//! Return currently active Mission.
	CMission*          GetCurrentMission(bool bSkipLoadingAIWhenSyncingContent = false);
	//! Get number of missions on Map.
	int                GetMissionCount() const     { return m_missions.size(); };
	//! Get Mission by index.
	CMission*          GetMission(int index) const { return m_missions[index]; };
	//! Find Mission by name.
	CMission*          FindMission(const string& name) const;
	//! Makes specified mission current.
	void               SetCurrentMission(CMission* mission);
	//! Add new mission to map.
	void               AddMission(CMission* mission);
	//! Remove existing mission from map.
	void               RemoveMission(CMission* mission);
	void               LogLoadTime(int time);
	bool               IsDocumentReady() const { return m_bDocumentReady; }
	bool               IsLevelBeingLoaded() const { return m_bLevelBeingLoaded; }
	void               SetDocumentReady(bool bReady);
	//! For saving binary data (voxel object)
	CXmlArchive*       GetTmpXmlArch()  { return m_pTmpXmlArchHack; }
	CLevelShaderCache* GetShaderCache() { return m_pLevelShaderCache; }

	void               OnEnvironmentPropertyChanged(IVariable* pVar);

	struct TSaveDocContext
	{
		bool bSaved;
	};
	BOOL         BeforeSaveDocument(LPCTSTR lpszPathName, TSaveDocContext& context);
	BOOL         DoSaveDocument(LPCTSTR lpszPathName, TSaveDocContext& context);
	BOOL         AfterSaveDocument(LPCTSTR lpszPathName, TSaveDocContext& context, bool bShowPrompt = true);

	virtual void SetPathName(LPCTSTR lpszPathName, BOOL bAddToMRU = TRUE);
	string      GetPathName() const { return m_pathName; };
	virtual BOOL DoSave(LPCTSTR lpszPathName, BOOL bReplace);//TODO : change the signature of this!
	string      GetTitle() const;

	// Check if document can be closed.
	bool CanClose();

protected:
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	void         LoadTemplates();
	//! called immediately after saving the level.
	void         ClearMissions();
	void         RegisterConsoleVariables();
	void         OnStartLevelResourceList();
	static void  OnValidateSurfaceTypesChanged(ICVar*);

	string      GetCryIndexPath(const LPCTSTR levelFilePath);

private:
	void   LoadShaderCache(TDocMultiArchive& arrXmlAr);
	void   ActivateMission(const string &currentMissionName);
	string GetCurrentMissionName(TDocMultiArchive& arrXmlAr);

protected:
	string                  m_strMasterCDFolder;
	string                  m_pathName;
	bool                     m_bLoadFailed;
	bool                     m_bModified;
	bool					 m_bIsClosing;
	COLORREF                 m_waterColor;
	XmlNodeRef               m_fogTemplate;
	XmlNodeRef               m_environmentTemplate;
	CMission*                m_mission;
	std::vector<CMission*>   m_missions;
	bool                     m_bDocumentReady;
	CXmlArchive*             m_pTmpXmlArchHack;
	CLevelShaderCache*       m_pLevelShaderCache;
	ICVar*                   doc_validate_surface_types;
	bool                     m_boLevelExported;
	bool                     m_bLevelBeingLoaded;
};

class CAutoDocNotReady
{
public:
	CAutoDocNotReady()
	{
		m_prevState = GetIEditorImpl()->GetDocument()->IsDocumentReady();
		GetIEditorImpl()->GetDocument()->SetDocumentReady(false);
	}

	~CAutoDocNotReady()
	{
		GetIEditorImpl()->GetDocument()->SetDocumentReady(m_prevState);
	}

private:
	bool m_prevState;
};

