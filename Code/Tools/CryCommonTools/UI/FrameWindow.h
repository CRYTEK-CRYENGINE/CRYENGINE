// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FRAMEWINDOW_H__
#define __FRAMEWINDOW_H__

#include <vector>
#include "Layout.h"

class IUIComponent;

class FrameWindow
{
public:
	FrameWindow();
	~FrameWindow();
	void AddComponent(IUIComponent* component);
	void Show(bool show, int width, int height);
	void SetCaption(const TCHAR* caption);
	void* GetHWND();

private:
	void UpdateComponentUI(bool create);
	std::pair<int, int> InitializeSize();
	void CalculateExtremeDimensions(int& minWidth, int& maxWidth, int& minHeight, int& maxHeight);
	void OnSizeChanged(int width, int height);

	void* m_hwnd;
	Layout m_layout;
};

#endif //__FRAMEWINDOW_H__
