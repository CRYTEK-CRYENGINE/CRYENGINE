// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IBackgroundTaskManager.h>
#include <IAnimationCompressionManager.h>
#include <IEditor.h>
#include <CrySystem/File/IFileChangeMonitor.h>

class CBackgroundTaskUpdateLocalAnimations;
class CBackgroundTaskCompressCAF;
class CAnimationCompressionManager
	: public IAnimationCompressionManager
	  , public IFileChangeListener
	  , public IEditorNotifyListener
{
public:
	CAnimationCompressionManager();
	~CAnimationCompressionManager();

	bool IsEnabled() const override;
	void UpdateLocalAnimations() override;
	void QueueAnimationCompression(const char* animationName) override;

	void OnEditorNotifyEvent(EEditorNotifyEvent ev) override;
private:
	class CBackgroundTaskCompressCAF;
	class CBackgroundTaskUpdateLocalAnimations;
	class CBackgroundTaskImportAnimation;
	class CBackgroundTaskReloadCHRPARAMS;
	class CBackgroundTaskCleanUpAnimations;

	void Start();
	void OnFileChange(const char* filename, EChangeType eType) override;

	void OnCAFCompressed(CBackgroundTaskCompressCAF* pTask);
	void OnImportTriggered(const char* animationPath);
	void OnReloadCHRPARAMSComplete(CBackgroundTaskReloadCHRPARAMS* pTask);

	struct SAnimationInWorks
	{
		CBackgroundTaskCompressCAF* pTask;
	};

	CBackgroundTaskUpdateLocalAnimations* m_pRescanTask;
	CBackgroundTaskReloadCHRPARAMS*       m_pReloadCHRPARAMSTask;
	typedef std::map<string, SAnimationInWorks> TAnimationMap;
	TAnimationMap                         m_animationsInWork;
	std::vector<SAnimationInWorks>        m_canceledAnimations;
	bool m_enabled;
};

