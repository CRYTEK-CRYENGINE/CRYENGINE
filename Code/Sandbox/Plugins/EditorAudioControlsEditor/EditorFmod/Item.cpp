// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Item.h"

namespace ACE
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
void CItem::AddChild(CItem* const pChild)
{
	m_children.push_back(pChild);
	pChild->SetParent(this);
}

//////////////////////////////////////////////////////////////////////////
void CItem::RemoveChild(CItem* const pChild)
{
	stl::find_and_erase(m_children, pChild);
	pChild->SetParent(nullptr);
}

//////////////////////////////////////////////////////////////////////////
void CItem::Clear()
{
	m_children.clear();
}
} // namespace Fmod
} // namespace Impl
} // namespace ACE

