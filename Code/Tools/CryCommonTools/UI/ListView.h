// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __LISTVIEW_H__
#define __LISTVIEW_H__

#include "IUIComponent.h"

class ListView : public IUIComponent
{
public:
	ListView();

	void Add(int imageIndex, const TCHAR* message);
	void Clear();

	// IUIComponent
	virtual void CreateUI(void* window, int left, int top, int width, int height);
	virtual void Resize(void* window, int left, int top, int width, int height);
	virtual void DestroyUI(void* window);
	virtual void GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight);

private:
	void* CreateImageList();

	void* m_list;
};

#endif //__LISTVIEW_H__
