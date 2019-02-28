// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _INTROMOVIERENDERER_H_
#define _INTROMOVIERENDERER_H_

#include <CrySystem/Scaleform/IFlashPlayer.h>

class CIntroMovieRenderer : public ILoadtimeCallback, public IFSCommandHandler
{
protected:

	enum EVideoStatus
	{
		eVideoStatus_PrePlaying = 0,
		eVideoStatus_Playing    = 1,
		eVideoStatus_Stopped    = 2,
		eVideoStatus_Finished   = 3,
		eVideoStatus_Error      = 4,
	};

public:

	CIntroMovieRenderer() = default;
	virtual ~CIntroMovieRenderer() = default;

	bool Initialize();
	void WaitForCompletion();

	// ILoadtimeCallback
	virtual void LoadtimeUpdate(float deltaTime);
	virtual void LoadtimeRender();
	// ~ILoadtimeCallback

	// IFSCommandHandler
	virtual void HandleFSCommand(const char* pCommand, const char* pArgs, void* pUserData = 0) {}
	// ~IFSCommandHandler

protected:

	void         UpdateViewport();
	void         SetViewportIfChanged(const int x, const int y, const int width, const int height, const float pixelAR);
	int          GetSubtitleChannelForSystemLanguage();

	EVideoStatus GetCurrentStatus();

	//////////////////////////////////////////////////////////////////////////

	std::shared_ptr<IFlashPlayer> m_pFlashPlayer;

};

#endif // _INTROMOVIERENDERER_H_
