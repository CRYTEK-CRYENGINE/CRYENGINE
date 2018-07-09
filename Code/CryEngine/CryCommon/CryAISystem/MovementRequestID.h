// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

//! When you queue a movement request you will get a request id back.
//! This ID is used when you want to cancel a queued request.
//! (Wrapper around 'unsigned int' but provides type safety.)
//! An ID of 0 represents an invalid/uninitialized ID.
struct MovementRequestID
{
	unsigned int id;

	MovementRequestID() : id(0) {}
	MovementRequestID(unsigned int _id) : id(_id) {}
	MovementRequestID& operator++()                           { ++id; return *this; }
	bool               operator==(unsigned int otherID) const { return id == otherID; }
	operator unsigned int() const { return id; }
	bool IsInvalid() const { return id == 0; }

	static MovementRequestID Invalid() { return MovementRequestID(); }
};

//! \endcond