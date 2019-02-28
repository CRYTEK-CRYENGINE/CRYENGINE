// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		struct IItemMonitor;  // below

		//===================================================================================
		//
		// ItemMonitorUniquePtr
		//
		//===================================================================================

		typedef std::unique_ptr<IItemMonitor> ItemMonitorUniquePtr;

		//===================================================================================
		//
		// IItemMonitor
		//
		// - can be optionally installed by a generator to monitor changes in the generated items that might yield to a corruption of the reasoning space
		// - item monitors persist throughout the lifetime of a query
		// - depending on whether it's a hierarchical query, the item monitor may get carried over to the next child query until the parent query decides that monitoring is no longer needed
		// - multiple monitors can be installed for a single query
		// - this would be the case if the items are really complex and cover different domains whereas each of them might become corrupt in its own way
		//
		//===================================================================================

		struct IItemMonitor
		{
			enum EHealthState
			{
				StillHealthy,
				CorruptionOccurred,
			};

			virtual               ~IItemMonitor() {}
			virtual EHealthState  UpdateAndCheckForCorruption(Shared::IUqsString& outExplanationInCaseOfCorruption) = 0;

			// All these innocently looking operator-new/delete functions do is call their global counterparts, but there's a specific reason for why we have them defined in this class:
			// When the caller on the *client*-side instantiates an IItemMonitor, he will do so using the new-operator. Nothing fancy about that yet. Then, he will
			// hand over that object to an ItemMonitorUniquePtr (which is a std::unique_ptr) and pass that to the *core*-side of things. From then on, the item-monitor
			// lives on the *core*-side. Eventually, the ItemMonitorUniquePtr will go out of scope, and ask the std::unique_ptr's deleter to destroy the object. Things become
			// interesting now: std::unique_ptr will invoke the C++ language's delete-operator, which in turn calls an operator-delete-function and that particular function is
			// the delete-operator of this class. Now comes the important part: since this delete-operator-function is declared *inline* here, it's code will actually reside
			// on the *client*-side again, which is exactly what we want :-)
			// The operator-new-function is just here for completeness.
			static void*          operator new(size_t numBytes);
			static void           operator delete(void* p);
		};

		inline void* IItemMonitor::operator new(size_t numBytes)
		{
			return ::operator new(numBytes);
		}

		inline void IItemMonitor::operator delete(void* p)
		{
			::operator delete(p);
		}

	}
}
