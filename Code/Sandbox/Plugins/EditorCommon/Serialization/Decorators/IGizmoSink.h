// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Serialization {

struct LocalPosition;
struct LocalFrame;
struct LocalOrientation;

struct GizmoFlags
{
	bool visible;
	bool selected;

	GizmoFlags() : visible(true), selected(false) {}
};

struct IGizmoSink
{
	virtual int  CurrentGizmoIndex() const = 0;
	virtual int  Write(const LocalPosition&, const GizmoFlags& flags, const void* handle) = 0;
	virtual int  Write(const LocalOrientation&, const GizmoFlags& flags, const void* handle) = 0;
	virtual int  Write(const LocalFrame&, const GizmoFlags& flags, const void* handle) = 0;
	virtual void SkipRead() = 0;
	virtual bool Read(LocalPosition* position, GizmoFlags* flags, const void* handle) = 0;
	virtual bool Read(LocalOrientation* position, GizmoFlags* flags, const void* handle) = 0;
	virtual bool Read(LocalFrame* position, GizmoFlags* flags, const void* handle) = 0;
	virtual void Reset(const void* handle) = 0;
};

}

