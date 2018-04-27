// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Layout.h"
#include <cassert>
#include <Windows.h>

Layout::Layout(Direction direction)
:	m_direction(direction)
{
}

void Layout::AddComponent(IUIComponent* component)
{
	m_components.push_back(ComponentEntry(component));
}

void Layout::CreateUI(void* window, int left, int top, int width, int height)
{
	UpdateLayout(window, left, top, width, height);

	for (int componentIndex = 0, componentCount = int(m_components.size()); componentIndex < componentCount; ++componentIndex)
	{
		IUIComponent* component = m_components[componentIndex].component;
		component->CreateUI(window, m_components[componentIndex].left, m_components[componentIndex].top, m_components[componentIndex].width, m_components[componentIndex].height);
	}
}

void Layout::Resize(void* window, int left, int top, int width, int height)
{
	UpdateLayout(window, left, top, width, height);

	for (int componentIndex = 0, componentCount = int(m_components.size()); componentIndex < componentCount; ++componentIndex)
	{
		IUIComponent* component = m_components[componentIndex].component;
		component->Resize(window, m_components[componentIndex].left, m_components[componentIndex].top, m_components[componentIndex].width, m_components[componentIndex].height);
	}
}

void Layout::DestroyUI(void* window)
{
	for (int componentIndex = 0, componentCount = int(m_components.size()); componentIndex < componentCount; ++componentIndex)
	{
		IUIComponent* component = m_components[componentIndex].component;
		component->DestroyUI(window);
	}
}

void Layout::GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight)
{
	int minW = 0;
	int maxW = 0;
	int minH = 0;
	int maxH = 0;
	for (int componentIndex = 0, componentCount = int(m_components.size()); componentIndex < componentCount; ++componentIndex)
	{
		IUIComponent* component = m_components[componentIndex].component;
		int compMinW, compMaxW, compMinH, compMaxH;
		component->GetExtremeDimensions(window, compMinW, compMaxW, compMinH, compMaxH);
		switch (m_direction)
		{
		case DirectionVertical:
			minW = (minW > compMinW ? minW : compMinW);
			maxW = (maxW > compMaxW ? maxW : compMaxW); // Deliberately take the larger maximum.
			minH += compMinH;
			maxH += compMaxH;
			break;
		case DirectionHorizontal:
			minW += compMinW;
			maxW += compMaxW;
			minH = (minH > compMinH ? minH : compMinH);
			maxH = (maxH > compMaxH ? maxH : compMaxH); // Deliberately take the larger maximum.
			break;
		}
	}

	// Make sure the window is at least a certain size;
	minW = (minW >= 10 ? minW : 10);
	maxW = (maxW >= minW ? maxW : minW);
	minH = (minH >= 10 ? minH : 10);
	maxH = (maxH >= minH ? maxH : minH);

	minWidth = minW;
	maxWidth = maxW;
	minHeight = minH;
	maxHeight = maxH;
}

void Layout::UpdateLayout(void* window, int left, int top, int width, int height)
{
	assert(window);

	int remainingToAllocate;
	switch (m_direction)
	{
	case DirectionVertical: remainingToAllocate = height; break;
	case DirectionHorizontal: remainingToAllocate = width; break;
	}

	int smallestAllocationAmount = INT_MAX;
	int canBeExtendedCount = 0;
	for (int componentIndex = 0, componentCount = int(m_components.size()); componentIndex < componentCount; ++componentIndex)
	{
		IUIComponent* component = m_components[componentIndex].component;
		int compMinW, compMaxW, compMinH, compMaxH;
		component->GetExtremeDimensions(window, compMinW, compMaxW, compMinH, compMaxH);

		switch (m_direction)
		{
		case DirectionVertical:
			{
				int allocationAmount = compMaxH - compMinH;
				if (allocationAmount > 0)
				{
					++canBeExtendedCount;
					smallestAllocationAmount = (smallestAllocationAmount < allocationAmount ? smallestAllocationAmount : allocationAmount);
				}
				m_components[componentIndex].height = compMinH;
				m_components[componentIndex].width = (width > compMaxW ? compMaxW : width);
				remainingToAllocate -= m_components[componentIndex].height;
			}
			break;

		case DirectionHorizontal:
			{
				int allocationAmount = compMaxW - compMinW;
				if (allocationAmount > 0)
				{
					++canBeExtendedCount;
					smallestAllocationAmount = (smallestAllocationAmount < allocationAmount ? smallestAllocationAmount : allocationAmount);
				}
				m_components[componentIndex].width = compMinW;
				m_components[componentIndex].height = (height > compMaxH ? compMaxH : height);
				remainingToAllocate -= m_components[componentIndex].width;
			}
			break;
		}
	}

	while (remainingToAllocate > 0 && canBeExtendedCount > 0)
	{
		int equitablePerCompAllocation = remainingToAllocate / canBeExtendedCount;
		int compAllocation = (equitablePerCompAllocation < smallestAllocationAmount ? equitablePerCompAllocation : smallestAllocationAmount);
		compAllocation = (compAllocation > 0 ? compAllocation : 1);
		canBeExtendedCount = 0;
		smallestAllocationAmount = INT_MAX;
		for (int componentIndex = 0, componentCount = int(m_components.size()); componentIndex < componentCount; ++componentIndex)
		{
			IUIComponent* component = m_components[componentIndex].component;
			int compMinW, compMaxW, compMinH, compMaxH;
			component->GetExtremeDimensions(window, compMinW, compMaxW, compMinH, compMaxH);
			switch (m_direction)
			{
			case DirectionVertical:
				{
					int componentExpandAmount = compMaxH - m_components[componentIndex].height;
					if (componentExpandAmount > 0)
					{
						m_components[componentIndex].height += compAllocation;
						assert(m_components[componentIndex].height <= compMaxH);
						componentExpandAmount -= compAllocation;
						remainingToAllocate -= compAllocation;
						if (componentExpandAmount > 0)
						{
							smallestAllocationAmount = (smallestAllocationAmount < componentExpandAmount ? smallestAllocationAmount : componentExpandAmount);
							++canBeExtendedCount;
						}
					}
				}
				break;

			case DirectionHorizontal:
				{
					int componentExpandAmount = compMaxW - m_components[componentIndex].width;
					if (componentExpandAmount > 0)
					{
						m_components[componentIndex].width += compAllocation;
						assert(m_components[componentIndex].width <= compMaxW);
						componentExpandAmount -= compAllocation;
						remainingToAllocate -= compAllocation;
						if (componentExpandAmount > 0)
						{
							smallestAllocationAmount = (smallestAllocationAmount < componentExpandAmount ? smallestAllocationAmount : componentExpandAmount);
							++canBeExtendedCount;
						}
					}
				}
				break;
			}
		}
	}

	switch (m_direction)
	{
	case DirectionVertical:
		{
			int posY = top;
			for (int componentIndex = 0, componentCount = int(m_components.size()); componentIndex < componentCount; ++componentIndex)
			{
				m_components[componentIndex].left = left;
				m_components[componentIndex].top = posY;
				posY += m_components[componentIndex].height;
			}
		}
		break;
	case DirectionHorizontal:
		{
			int posX = left;
			for (int componentIndex = 0, componentCount = int(m_components.size()); componentIndex < componentCount; ++componentIndex)
			{
				m_components[componentIndex].top = top;
				m_components[componentIndex].left = posX;
				posX += m_components[componentIndex].width;
			}
		}
		break;
	}
}
