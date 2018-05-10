// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SPACER_H__
#define __SPACER_H__

#include "IUIComponent.h"

class Spacer : public IUIComponent
{
public:
	Spacer(int minWidth, int minHeight, int maxWidth, int maxHeight);

	// IUIComponent
	virtual void CreateUI(void* window, int left, int top, int width, int height);
	virtual void Resize(void* window, int left, int top, int width, int height);
	virtual void DestroyUI(void* window);
	virtual void GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight);

private:
	int m_minWidth;
	int m_minHeight;
	int m_maxWidth;
	int m_maxHeight;
};

#endif //__SPACER_H__
