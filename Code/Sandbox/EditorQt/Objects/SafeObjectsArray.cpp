// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SafeObjectsArray.h"

//////////////////////////////////////////////////////////////////////////
CSafeObjectsArray::~CSafeObjectsArray()
{
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CSafeObjectsArray::Clear()
{
	size_t const num = m_objects.size();
	for (size_t i = 0; i < num; ++i)
	{
		m_objects[i]->RemoveEventListener(functor(*this, &CSafeObjectsArray::OnTargetEvent));
	}
	m_objects.clear();
}

//////////////////////////////////////////////////////////////////////////
void CSafeObjectsArray::RegisterEventCallback(CBaseObject::EventCallback const& cb)
{
	stl::push_back_unique(m_eventListeners, cb);
}

//////////////////////////////////////////////////////////////////////////
void CSafeObjectsArray::UnregisterEventCallback(CBaseObject::EventCallback const& cb)
{
	stl::find_and_erase(m_eventListeners, cb);
}

//////////////////////////////////////////////////////////////////////////
void CSafeObjectsArray::Add(CBaseObject* obj)
{
	if (obj != nullptr)
	{
		// Check if object is unique in array.
		if (!stl::find(m_objects, obj))
		{
			m_objects.push_back(obj);
			// Make reference on this object.
			obj->AddEventListener(functor(*this, &CSafeObjectsArray::OnTargetEvent));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSafeObjectsArray::Remove(CBaseObject* obj)
{
	// Find this object.
	if (stl::find_and_erase(m_objects, obj))
	{
		obj->RemoveEventListener(functor(*this, &CSafeObjectsArray::OnTargetEvent));
	}
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CSafeObjectsArray::Get(size_t const index) const
{
	CRY_ASSERT(index >= 0 && index < m_objects.size());
	return m_objects[index];
}

//////////////////////////////////////////////////////////////////////////
void CSafeObjectsArray::OnTargetEvent(CBaseObject* target, int event)
{
	if (event == OBJECT_ON_DELETE)
	{
		Remove(target);
	}

	if (!m_eventListeners.empty())
	{
		for (CBaseObject::EventCallback const eventCallback : m_eventListeners)
		{
			if (eventCallback.getFunc() != nullptr)
			{
				eventCallback(target, event);
			}
		}
	}
}

