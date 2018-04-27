// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Spacer.h"

Spacer::Spacer(int minWidth, int minHeight, int maxWidth, int maxHeight)
: m_minWidth(minWidth),
  m_minHeight(minHeight),
	m_maxWidth(maxWidth),
	m_maxHeight(maxHeight)
{
}

void Spacer::CreateUI(void* window, int left, int top, int width, int height)
{
}

void Spacer::Resize(void* window, int left, int top, int width, int height)
{
}

void Spacer::DestroyUI(void* window)
{
}

void Spacer::GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight)
{
	minWidth = m_minWidth;
	maxWidth = m_maxWidth;
	minHeight = m_minHeight;
	maxHeight = m_maxHeight;
}
