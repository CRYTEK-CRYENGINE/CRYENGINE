// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <vector>
#include <Serialization/Decorators/IGizmoSink.h>
#include <CrySerialization/Decorators/LocalFrame.h>
#include "ManipScene.h"
#include "Explorer/Explorer.h"

namespace CharacterTool
{
using std::vector;

using namespace Explorer;

class CharacterDocument;
class CharacterSpaceProvider : public Manip::ISpaceProvider
{
public:
	CharacterSpaceProvider(CharacterDocument* document) : m_document(document) {}
	Manip::SSpaceAndIndex FindSpaceIndexByName(int spaceType, const char* name, int parentsUp) const override;
	QuatT                 GetTransform(const Manip::SSpaceAndIndex& si) const override;
private:
	CharacterDocument* m_document;
};

enum GizmoLayer
{
	GIZMO_LAYER_NONE = -1,
	GIZMO_LAYER_SCENE,
	GIZMO_LAYER_CHARACTER,
	GIZMO_LAYER_ANIMATION,
	GIZMO_LAYER_COUNT
};

class GizmoSink : public Serialization::IGizmoSink
{
public:
	GizmoSink() : m_lastIndex(0), m_scene(0) {}
	void           SetScene(Manip::CScene* scene) { m_scene = scene; }
	ExplorerEntry* ActiveEntry()                  { return m_activeEntry.get(); }

	void           Clear(GizmoLayer layer);
	void           BeginWrite(ExplorerEntry* entry, GizmoLayer layer);
	void           BeginRead(GizmoLayer layer);
	void           EndRead();

	int            CurrentGizmoIndex() const override;
	int            Write(const Serialization::LocalPosition& position, const Serialization::GizmoFlags& gizmoFlags, const void* handle) override;
	int            Write(const Serialization::LocalOrientation& position, const Serialization::GizmoFlags& gizmoFlags, const void* handle) override;
	int            Write(const Serialization::LocalFrame& position, const Serialization::GizmoFlags& gizmoFlags, const void* handle) override;
	bool           Read(Serialization::LocalPosition* position, Serialization::GizmoFlags* gizmoFlags, const void* handle) override;
	bool           Read(Serialization::LocalOrientation* position, Serialization::GizmoFlags* gizmoFlags, const void* handle) override;
	bool           Read(Serialization::LocalFrame* position, Serialization::GizmoFlags* gizmoFlags, const void* handle) override;
	void           SkipRead() override;
	void           Reset(const void* handle);
private:
	int                       m_lastIndex;
	int                       m_currentLayer;
	Manip::CScene*            m_scene;
	_smart_ptr<ExplorerEntry> m_activeEntry;
};

}

