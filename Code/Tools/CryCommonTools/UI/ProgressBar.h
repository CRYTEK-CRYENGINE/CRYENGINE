// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PROGRESSBAR_H__
#define __PROGRESSBAR_H__

#pragma once

#include "IUIComponent.h"

class ProgressBar : public IUIComponent
{
public:
	ProgressBar();

	void SetProgress(float progress);

	// IUIComponent
	virtual void CreateUI(void* window, int left, int top, int width, int height);
	virtual void Resize(void* window, int left, int top, int width, int height);
	virtual void DestroyUI(void* window);
	virtual void GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight);

private:
	void* m_progressBar;
};

#endif //__PROGRESSBAR_H__
