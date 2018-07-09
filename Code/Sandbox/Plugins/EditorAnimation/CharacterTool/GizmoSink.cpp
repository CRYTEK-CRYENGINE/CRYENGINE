// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "GizmoSink.h"
#include "CharacterDocument.h"
#include <CryAnimation/ICryAnimation.h>

namespace CharacterTool
{

static const Manip::SElement* FindElementByHandle(int lastIndex, const void* handle, int layer, const Manip::CScene* scene)
{
	if (!scene)
		return 0;

	int numElements = scene->Elements().size();
	int last = min(numElements, lastIndex + 1);
	const Manip::SElements& elements = scene->Elements();

	for (int i = last; i < numElements; ++i)
		if (elements[i].layer == layer && elements[i].originalHandle == handle)
		{
			return &elements[i];
		}

	for (int i = 0; i < last; ++i)
		if (elements[i].layer == layer && elements[i].originalHandle == handle)
		{
			return &elements[i];
		}
	return 0;
}

static Manip::SElement* FindElementByHandle(int lastIndex, const void* handle, int layer, Manip::CScene* scene)
{
	return const_cast<Manip::SElement*>(FindElementByHandle(lastIndex, handle, layer, const_cast<const Manip::CScene*>(scene)));
}

Manip::SSpaceAndIndex CharacterSpaceProvider::FindSpaceIndexByName(int spaceType, const char* name, int parentsUp) const
{
	Manip::SSpaceAndIndex si;

	if (!m_document)
		return si;
	if (!m_document->CompressedCharacter())
		return si;

	si.m_space = spaceType;

	ICharacterInstance* pICharacterInstance = m_document->CompressedCharacter();
	if (pICharacterInstance == 0)
		return si;

	if (spaceType == Serialization::SPACE_SOCKET_RELATIVE_TO_BINDPOSE || spaceType == Serialization::SPACE_SOCKET_RELATIVE_TO_JOINT)
	{
		IAttachmentManager* pIAttachmentManager = pICharacterInstance->GetIAttachmentManager();
		if (pIAttachmentManager)
		{
			IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByName(name);
			if (pIAttachment)
			{
				if (pIAttachment->GetType() == CA_BONE || pIAttachment->GetType() == CA_FACE)
				{
					si.m_attachmentCRC32 = CCrc32::ComputeLowercase(name);
					return si;
				}
			}

			IProxy* pIProxy = pIAttachmentManager->GetProxyInterfaceByName(name);
			if (pIProxy)
			{
				si.m_attachmentCRC32 = CCrc32::ComputeLowercase(name);
				return si;
			}
		}
	}

	if (spaceType == Serialization::SPACE_JOINT ||
	    spaceType == Serialization::SPACE_JOINT_WITH_PARENT_ROTATION ||
	    spaceType == Serialization::SPACE_JOINT_WITH_CHARACTER_ROTATION)
	{
		IDefaultSkeleton& defaultSkeleton = pICharacterInstance->GetIDefaultSkeleton();
		si.m_jointCRC32 = CCrc32::ComputeLowercase(name + (*name == '$'));
		return si;
	}

	if (spaceType == Serialization::SPACE_ENTITY)
	{
		return si;
	}
	return si;
}

QuatT CharacterSpaceProvider::GetTransform(const Manip::SSpaceAndIndex& si) const
{
	if (!m_document)
		return IDENTITY;
	QuatT characterLocation(m_document->PhysicalLocation());
	ICharacterInstance* pICharacterInstance = m_document->CompressedCharacter();
	if (pICharacterInstance == 0)
		return characterLocation;

	IAttachmentManager* pIAttachmentManager = pICharacterInstance->GetIAttachmentManager();
	ISkeletonPose& skeletonPose = *pICharacterInstance->GetISkeletonPose();
	IDefaultSkeleton& defaultSkeleton = m_document->CompressedCharacter()->GetIDefaultSkeleton();

	int spaceType = si.m_space;

	if (pIAttachmentManager)
	{
		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByNameCRC(si.m_attachmentCRC32);
		if (pIAttachment)
		{
			if (pIAttachment->GetType() == CA_FACE)
			{
				if (spaceType == Serialization::SPACE_SOCKET_RELATIVE_TO_BINDPOSE)
				{
					const QuatT& defaultParentSpace = pIAttachment->GetAttAbsoluteDefault();
					const QuatT& parentSpace = pIAttachment->GetAttModelRelative();
					return characterLocation * parentSpace * defaultParentSpace.GetInverted();
				}
			}

			if (pIAttachment->GetType() == CA_BONE)
			{
				if (spaceType == Serialization::SPACE_SOCKET_RELATIVE_TO_BINDPOSE)
				{
					const QuatT& defaultParentSpace = pIAttachment->GetAttAbsoluteDefault();
					const QuatT& parentSpace = pIAttachment->GetAttModelRelative();
					return characterLocation * parentSpace * defaultParentSpace.GetInverted();
				}
				if (spaceType == Serialization::SPACE_SOCKET_RELATIVE_TO_JOINT)
				{
					const QuatT& defaultParentSpace = pIAttachment->GetAttRelativeDefault();
					const QuatT& parentSpace = pIAttachment->GetAttModelRelative();
					return characterLocation * parentSpace * defaultParentSpace.GetInverted();
				}
			}
		}

		IProxy* pIProxy = pIAttachmentManager->GetProxyInterfaceByCRC(si.m_attachmentCRC32);
		if (pIProxy)
		{
			if (spaceType == Serialization::SPACE_SOCKET_RELATIVE_TO_BINDPOSE)
			{
				const QuatT& defaultParentSpace = pIProxy->GetProxyAbsoluteDefault();
				const QuatT& parentSpace = pIProxy->GetProxyModelRelative();
				return characterLocation * parentSpace * defaultParentSpace.GetInverted();
			}
			if (spaceType == Serialization::SPACE_SOCKET_RELATIVE_TO_JOINT)
			{
				const QuatT& defaultParentSpace = pIProxy->GetProxyRelativeDefault();
				const QuatT& parentSpace = pIProxy->GetProxyModelRelative();
				return characterLocation * parentSpace * defaultParentSpace.GetInverted();
			}
		}
	}

	int nJointID = defaultSkeleton.GetJointIDByCRC32(si.m_jointCRC32);
	if (nJointID < 0)
		return characterLocation;

	switch (spaceType)
	{
	case Serialization::SPACE_JOINT_WITH_PARENT_ROTATION:
		{
			int parent = defaultSkeleton.GetJointParentIDByID(nJointID);
			QuatT parentSpace = skeletonPose.GetAbsJointByID(parent);
			QuatT jointSpace = skeletonPose.GetRelJointByID(nJointID);
			return characterLocation * parentSpace * QuatT(IDENTITY, jointSpace.t);
		}

	case Serialization::SPACE_JOINT_WITH_CHARACTER_ROTATION:
		{
			QuatT jointSpace = skeletonPose.GetAbsJointByID(nJointID);
			return characterLocation * QuatT(IDENTITY, jointSpace.t);
		}
	case Serialization::SPACE_JOINT:
		{
			return characterLocation * skeletonPose.GetAbsJointByID(nJointID); //return the joint in word space
		}

	default:
		return characterLocation;   //root in world-space
	}
	;
}

// ---------------------------------------------------------------------------

void GizmoSink::BeginWrite(ExplorerEntry* activeEntry, GizmoLayer layer)
{
	m_lastIndex = -1;
	m_currentLayer = int(layer);

	if (!m_scene)
		return;
	m_scene->ClearLayer(int(layer));
	m_activeEntry = activeEntry;
}

void GizmoSink::BeginRead(GizmoLayer layer)
{
	m_lastIndex = -1;
	m_currentLayer = int(layer);
}

void GizmoSink::Clear(GizmoLayer layer)
{
	if (!m_scene)
		return;

	m_scene->ClearLayer(layer);
}

void GizmoSink::EndRead()
{
	if (!m_scene)
		return;

	Manip::SElements& elements = m_scene->Elements();
	size_t numElements = elements.size();
	for (size_t i = 0; i < numElements; ++i)
	{
		Manip::SElement& element = elements[i];
		if (element.layer == m_currentLayer && element.changed)
			element.changed = false;
	}
}

bool GizmoSink::Read(Serialization::LocalFrame* decorator, Serialization::GizmoFlags* gizmoFlags, const void* handle)
{
	const Manip::SElement* element = FindElementByHandle(m_lastIndex, handle, m_currentLayer, m_scene);
	++m_lastIndex;
	if (!element || !element->changed)
		return false;
	*decorator->position = element->placement.transform.t;
	*decorator->rotation = element->placement.transform.q;
	return true;
}

int GizmoSink::CurrentGizmoIndex() const
{
	return m_lastIndex + 1;
}

int GizmoSink::Write(const Serialization::LocalFrame& decorator, const Serialization::GizmoFlags& gizmoFlags, const void* handle)
{
	if (!m_scene)
		return -1;
	Manip::SElement e;
	e.placement.transform = QuatT(*decorator.rotation, *decorator.position);
	e.placement.size = Vec3(0.02f, 0.02f, 0.02f);
	e.originalHandle = handle;
	if (m_scene->SpaceProvider())
	{
		e.parentSpaceIndex = m_scene->SpaceProvider()->FindSpaceIndexByName(decorator.positionSpace, decorator.parentName, 0);
		if (decorator.positionSpace != decorator.rotationSpace)
			e.parentOrientationSpaceIndex = m_scene->SpaceProvider()->FindSpaceIndexByName(decorator.rotationSpace, decorator.parentName, 0);
	}
	e.alwaysXRay = true;
	e.caps = Manip::CAP_SELECT | Manip::CAP_MOVE | Manip::CAP_ROTATE;
	e.shape = Manip::SHAPE_AXES;
	e.layer = m_currentLayer;
	m_scene->AddElement(e, (Manip::ElementId)handle);
	++m_lastIndex;
	return m_lastIndex;
}

bool GizmoSink::Read(Serialization::LocalPosition* decorator, Serialization::GizmoFlags* gizmoFlags, const void* handle)
{
	const Manip::SElement* element = FindElementByHandle(m_lastIndex, handle, m_currentLayer, m_scene);
	++m_lastIndex;
	if (!element || !element->changed)
		return false;
	*decorator->value = element->placement.transform.t;
	return true;
}

int GizmoSink::Write(const Serialization::LocalPosition& decorator, const Serialization::GizmoFlags& gizmoFlags, const void* handle)
{
	if (!m_scene)
		return -1;
	Manip::SElement e;
	e.placement.transform = QuatT(*decorator.value, IDENTITY);
	e.placement.size = Vec3(0.02f, 0.02f, 0.02f);
	e.originalHandle = handle;
	if (m_scene->SpaceProvider())
		e.parentSpaceIndex = m_scene->SpaceProvider()->FindSpaceIndexByName(decorator.space, decorator.parentName, 0);
	e.alwaysXRay = true;
	e.caps = Manip::CAP_SELECT | Manip::CAP_MOVE;
	e.shape = Manip::SHAPE_AXES;
	e.layer = m_currentLayer;
	e.poseModifier = true;
	m_scene->AddElement(e, (Manip::ElementId)handle);
	++m_lastIndex;
	return m_lastIndex;
}

int GizmoSink::Write(const Serialization::LocalOrientation& decorator, const Serialization::GizmoFlags& gizmoFlags, const void* handle)
{
	if (!m_scene)
		return -1;
	Manip::SElement e;
	e.placement.transform = QuatT(ZERO, *decorator.value);
	e.placement.size = Vec3(0.02f, 0.02f, 0.02f);
	e.originalHandle = handle;
	if (m_scene->SpaceProvider())
		e.parentSpaceIndex = m_scene->SpaceProvider()->FindSpaceIndexByName(decorator.space, decorator.parentName, 0);
	e.alwaysXRay = true;
	e.caps = Manip::CAP_SELECT | Manip::CAP_ROTATE;
	e.shape = Manip::SHAPE_AXES;
	e.layer = m_currentLayer;
	m_scene->AddElement(e, (Manip::ElementId)handle);
	++m_lastIndex;
	return m_lastIndex;
}

bool GizmoSink::Read(Serialization::LocalOrientation* decorator, Serialization::GizmoFlags* gizmoFlags, const void* handle)
{
	const Manip::SElement* element = FindElementByHandle(m_lastIndex, handle, m_currentLayer, m_scene);
	++m_lastIndex;
	if (!element || !element->changed)
		return false;
	*decorator->value = element->placement.transform.q;
	return true;
}

void GizmoSink::Reset(const void* handle)
{
	int lastIndex = 0;
	Manip::SElement* element = FindElementByHandle(lastIndex, handle, m_currentLayer, m_scene);
	if (!element)
		return;

	element->placement.transform = IDENTITY;
	element->changed = true;
	m_scene->SignalElementsChanged(1 << element->layer);
}

void GizmoSink::SkipRead()
{
	++m_lastIndex;
}

}

