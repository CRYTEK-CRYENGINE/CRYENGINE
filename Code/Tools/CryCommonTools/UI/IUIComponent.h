// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IUICOMPONENT_H__
#define __IUICOMPONENT_H__

class IUIComponent
{
public:
	virtual void CreateUI(void* window, int left, int top, int width, int height) = 0;
	virtual void Resize(void* window, int left, int top, int width, int height) = 0;
	virtual void DestroyUI(void* window) = 0;
	virtual void GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight) = 0;
};

#endif //__IUICOMPONENT_H__
