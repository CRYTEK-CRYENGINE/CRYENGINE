// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// EditorCommon
#include "EditorFramework/Editor.h"
#include "IEditor.h"

// Qt
#include <QWidget>

class QFile;
class QTemporaryDir;
class QTemporaryFile;

void LogPrintf(const char* szFormat, ...);

namespace FbxTool
{

class CScene;

} // namespace FbxTool

namespace FbxMetaData
{

struct SMetaData;

} // namespace FbxMetaData

class CAsyncImportSceneTask;
struct ITaskHost;
class CAsyncTaskHostForDialog;

class CTempRcObject;

class CAsset;

QString                         WriteTemporaryMetaData(const FbxMetaData::SMetaData& metaData);
std::unique_ptr<QTemporaryFile> WriteTemporaryFile(const QString& dir, const string& content, QString templateName = "");

// Reads meta data from either .cgf or .json file (in that order).
bool ReadMetaDataFromFile(const QString& filePath, FbxMetaData::SMetaData& metaData);

struct SJointsViewSettings
{
	bool m_bShowJointNames;

	SJointsViewSettings()
		: m_bShowJointNames(false)
	{}

	void Serialize(Serialization::IArchive& ar);
};

namespace MeshImporter
{

template<typename T>
class CSceneModel
{
public: 
	typedef std::function<void (T*)> Callback;

	void AddCallback(const Callback& callback)
	{
		m_callbacks.push_back(callback);
	}

	class CBatchSignals
	{
	public:
		CBatchSignals(CSceneModel& sceneModel) 
			: m_sceneModel(sceneModel)
			, m_callbacks(std::move(sceneModel.m_callbacks))
			, m_bSignalNeeded(false)
		{
			sceneModel.AddCallback([this](T*)
			{
				m_bSignalNeeded = true;
			});
		}

		~CBatchSignals()
		{
			m_sceneModel.m_callbacks = std::move(m_callbacks);
			if (m_bSignalNeeded)
			{
				m_sceneModel.Notify();
			}
		}
	private:
		CSceneModel m_sceneModel;
		std::vector<Callback> m_callbacks;
		bool m_bSignalNeeded;
	};

protected:
	void Notify()
	{
		for (auto& c : m_callbacks)
		{
			if (c)
			{
				c(static_cast<T*>(this));
			}
		}
	}
private:
	std::vector<Callback> m_callbacks;
};

struct IDialogHost
{
	virtual ~IDialogHost() {}

	virtual void Host_AddMenu(const char* menu) = 0;
	virtual void Host_AddToMenu(const char* menu, const char* command) = 0;
};

struct IImportFile
{
	virtual ~IImportFile() {}

	// When the source file selected by the user is located inside the game directory, 'filePath'
	// and 'originalFilePath' are the same. Otherwise, 'originalFilePath' is the file selected by
	// the user, and 'filePath' is the copy in the game directory we actually imported.
	// Both GetFilePath and GetOriginalFilePath return an absolute path.
	virtual QString GetFilePath() const = 0;
	virtual QString GetOriginalFilePath() const = 0;

	virtual QString GetFilename() const = 0;
};

class CImportFileManager
{
private:
	struct CImportFile : IImportFile
	{
		~CImportFile();
		virtual QString GetFilePath() const override;
		virtual QString GetOriginalFilePath() const override;
		virtual QString GetFilename() const override;

		QString                        m_filePath;
		QString                        m_originalFilePath;
		std::unique_ptr<QTemporaryDir> m_pTemporaryDir;
	};
public:
	// If "filePath" refers to a location outside the game directory, a temporary directory is created and the file is copied.
	static std::unique_ptr<IImportFile> ImportFile(const QString& filePath);
};

struct SDisplayScene
{
	SDisplayScene()
		: m_timestamp(0)
	{}

	std::unique_ptr<IImportFile>     m_pImportFile;
	std::unique_ptr<FbxTool::CScene> m_pScene;
	int                              m_timestamp;
};

struct SImportScenePayload
{
	QString targetFilename;
	CAsset* pAsset = nullptr;
	std::unique_ptr<FbxMetaData::SMetaData> pMetaData;
};

//! Manages importing and lifetime of FBX scenes.
class CSceneManager
{
public:
	typedef std::function<void (const SImportScenePayload* pUserData)> AssignSceneCallback;
	typedef std::function<void ()>                                     UnloadSceneCallback;

	~CSceneManager();

	void AddAssignSceneCallback(const AssignSceneCallback& callback)
	{
		m_assignSceneCallbacks.push_back(callback);
	}

	void AddUnloadSceneCallback(const UnloadSceneCallback& callback)
	{
		m_unloadSceneCallbacks.push_back(callback);
	}

	void                            ImportFile(const QString& filePath, const SImportScenePayload* pUserData, ITaskHost* pTaskHost);
	void							UnloadScene();

	const FbxTool::CScene*          GetScene() const;
	FbxTool::CScene*                GetScene();

	const IImportFile*              GetImportFile() const;

	FbxMetaData::SSceneUserSettings GetUserSettings() const;

	int GetTimestamp() const
	{
		return m_pDisplayScene ? m_pDisplayScene->m_timestamp : -1;
	}

	std::shared_ptr<SDisplayScene>  GetDisplayScene()
	{
		return m_pDisplayScene;
	}
private:
	void OnSceneImported(CAsyncImportSceneTask* pTask);

	std::shared_ptr<SDisplayScene>   m_pDisplayScene;
	std::vector<AssignSceneCallback> m_assignSceneCallbacks;
	std::vector<UnloadSceneCallback> m_unloadSceneCallbacks;
};

//! Baseclass for all importers.
class CBaseDialog
	: public QWidget
	  , public IEditorNotifyListener
{
public:
	enum EFileFormats
	{
		eFileFormat_CGF   = BIT(0),
		eFileFormat_JSON  = BIT(1),
		eFileFormat_CHR   = BIT(2),
		eFileFormat_I_CAF = BIT(3),
		eFileFormat_SKIN  = BIT(4),
	};

	enum EToolButtonFlags
	{
		eToolButtonFlags_Save   = BIT(0),
		eToolButtonFlags_Open   = BIT(1),
		eToolButtonFlags_Import = BIT(2),
		eToolButtonFlags_Reimport = BIT(3),
	};

protected:
	QString ShowImportFilePrompt();
	QString ShowOpenFilePrompt(int fileFormatFlags);
public:
	CBaseDialog(QWidget* pParent = nullptr);
	virtual ~CBaseDialog();

	bool                OnImportFile();
	void                OnReimportFile();
	void                OnDropFile(const QString& filePath);
	void        OnOpen();
	void                OnSaveAs();
	void                OnSave();
	void				OnCloseAsset();

	// Tries to opens a file for editing.
	// \p filePath Asset-relative file path to either chunk-file that stores meta data (e.g., .cgf), or asset meta-data (.cryasset).
	bool Open(const string& filePath);

	virtual void        CreateMenu(IDialogHost* pDialogHost);

	virtual const char* GetDialogName() const = 0;

	virtual int              GetOpenFileFormatFlags() { return 0; }

	// IEditorNotifyListener implementation.
	virtual void           OnEditorNotifyEvent(EEditorNotifyEvent evt) override;

	virtual int GetToolButtons() const { return 0; }  // See EToolButtonFlags.

	// Implement this in derived dialogs.
	virtual bool MayUnloadScene() = 0;

protected:
	const FbxTool::CScene* GetScene() const;
	FbxTool::CScene*       GetScene();

	void                   ImportFile(const QString& filePath, const SImportScenePayload* pUserData = nullptr);

	bool                   IsLoadingSuspended() const;

	QString                ShowSaveAsFilePrompt(int fileFormat);
	QString                ShowSaveToDirectoryPrompt();

	virtual void           OnIdleUpdate()                                    {}
	virtual void           OnReleaseResources()                              {}
	virtual void           OnReloadResources() {}

	virtual void           AssignScene(const SImportScenePayload* pUserData) {}
	virtual void           UnloadScene() {}

	string GetTargetFilePath() const;

	// Implement this in derived dialogs.
	virtual void ReimportFile() {}

	//! Returns a game relative path. If path refers to a directory, it must end with /. An empty string aborts saving.
	virtual QString          ShowSaveAsDialog() = 0;

	struct SSaveContext
	{
		std::unique_ptr<QTemporaryDir> pTempDir;
		string targetFilePath;
	};

	//! \p targetFilePath Asset-relative file path.
	bool SaveAs(const QString& targetFilePath);

	virtual bool             SaveAs(SSaveContext& saveContext) = 0;

	CAsyncTaskHostForDialog* GetTaskHost();
	const CSceneManager& GetSceneManager() const;
	CSceneManager&       GetSceneManager();

	std::unique_ptr<CTempRcObject> CreateTempRcObject();

	// Drag&Drop.
	virtual void dragEnterEvent(QDragEnterEvent* pEvent) override;
	virtual void dropEvent(QDropEvent* pEvent) override;

private:
	struct SSceneData;

	bool MayUnloadSceneInternal();

	void BaseAssignScene(const SImportScenePayload* pUserData);

	CSceneManager                            m_sceneManager;
	std::unique_ptr<SSceneData>              m_sceneData;

	std::unique_ptr<CAsyncTaskHostForDialog> m_pTaskHost;

	bool m_bLoadingSuspended;
};

void RenameAllowOverwrite(QFile& file, const QString& newFilePath);
void RenameAllowOverwrite(const string& oldFilePath, const string& newFilePath);

QString ReplaceExtension(const QString& str, const char* ext);

std::vector<string> GetAssetTypesFromFileFormatFlags(int flags);

} // namespace MeshImporter

std::unique_ptr<QTemporaryDir> CreateTemporaryDirectory();

