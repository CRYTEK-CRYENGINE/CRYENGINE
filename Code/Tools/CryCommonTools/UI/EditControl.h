// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EDITCONTROL_H__
#define __EDITCONTROL_H__

#include "IUIComponent.h"

class EditControl : public IUIComponent
{
public:
	EditControl();

	// IUIComponent
	virtual void CreateUI(void* window, int left, int top, int width, int height);
	virtual void Resize(void* window, int left, int top, int width, int height);
	virtual void DestroyUI(void* window);
	virtual void GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight);

	void* m_edit;
};

#endif //__EDITCONTROL_H__
