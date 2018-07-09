// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __LAYOUT_H__
#define __LAYOUT_H__

#include "IUIComponent.h"
#include <vector>

class Layout : public IUIComponent
{
public:
	enum Direction
	{
		DirectionHorizontal,
		DirectionVertical
	};
	explicit Layout(Direction direction);
	void AddComponent(IUIComponent* component);

	// IUIComponent
	virtual void CreateUI(void* window, int left, int top, int width, int height);
	virtual void Resize(void* window, int left, int top, int width, int height);
	virtual void DestroyUI(void* window);
	virtual void GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight);

private:
	void UpdateLayout(void* window, int left, int top, int width, int height);

	struct ComponentEntry
	{
		explicit ComponentEntry(IUIComponent* component): component(component), left(0), top(0), width(0), height(0) {}
		IUIComponent* component;
		int left;
		int top;
		int width;
		int height;
	};
	std::vector<ComponentEntry> m_components;
	Direction m_direction;
};

#endif //__LAYOUT_H__
