// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Reference.h
//  Created:     11/02/2008 by Matthew
//  Description: References to AI objects
// -------------------------------------------------------------------------
//  History:
//
//  Notes:
//    - this-> is used when referring to members to compile on GCC - due to specialization rules compiler
//      cannot assume any inherited members exist in a templated base (Meyer, Item 43, Effective C++)
////////////////////////////////////////////////////////////////////////////

/**
 * References to AI objects.
 *
 * (More accurately: smartpointers)
 * These references can be preserved across frames and dereferenced to give an AIStub * or AIObject *, which should
 * always be discarded between frames.
 * There are four main types:
 * tAIObjectID -  A simple integer ID. This is the only type that should pass outside the AI system.
 * WeakRef -      Wraps an ID with templated type. For use within the AI system.
 * StrongRef -    Wraps an ID with templated type and auto_ptr semantics. Uniquely owns the object, controlling its validity. Within AI.
 * CountedRef -   Wraps a StrongRef for reference-counting semantics. Within AI.
 *
 * Notes:
 * Strong should be preferred but are not compatible (and will not compile) with STL containers. Counted can be used in this case.
 * Counted is rather bolted on and should be properly integrated!
 */

#ifndef __AI_REFERENCE_H_
#define __AI_REFERENCE_H_

#pragma once

//#define DEBUG_REFERENCES

/**
 * The simple integer AI object ID, for use outside the AI system.
 */

// (MATT) This should really live in a minimal AI include, which right now we don't have  {2009/04/08}
#ifndef INVALID_AIOBJECTID
typedef uint32 tAIObjectID;
	#define INVALID_AIOBJECTID ((tAIObjectID)(0))
#endif

template<class T> class CWeakRef;
template<class T> class CStrongRef;
template<class T> class CCountedRef;
class CAIObject;

/**
 * Enum whose single member, NILREF, represents an empty, untyped reference.
 * Only for syntactic sugar, to avoid the use of CWeakRef<CMyClass>() where, with pointers, NULL would have sufficed.
 * It is silently converted into a Nil weak reference of any type, hence very useful as a parameter when calling a function.
 */
enum type_nil_ref { NILREF };

/**
 * An abstract typeless base class for references.
 * Some functionality of the references is independent of type and so is defined here.
 */
class CAbstractUntypedRef
{
	friend class CObjectContainer;

public:
	/**
	 * Test if reference is currently unassigned.
	 * Being assigned does not imply validity. Test for this with IsValid().
	 * @return True if currently assigned to an object.
	 */
	ILINE bool IsNil() const;

	/**
	 * Test if reference is currently assigned.
	 * Being assigned does not imply validity. Test for this with IsValid().
	 * This method is simply the converse of IsNil - but avoids potential double-negation confusion in its usage.
	 * @return True if currently assigned to an object.
	 */
	ILINE bool IsSet() const;

	/**
	 * Return a simple AI object ID.
	 * @return the tAIObjectID wrapped, regardless of validity. INVALID_AIOBJECTID if unassigned;
	 */
	ILINE tAIObjectID GetObjectID() const;

	/**
	 * Return the AI object, if reference is valid.
	 * The object may have been removed since the reference was assigned.
	 * @return Pointer to a valid AI object or NULL.
	 */
	ILINE IAIObject* GetIAIObject() const;

	/**
	 * Tests whether a reference and a pointer refer to the same object.
	 * An invalid reference and a NULL pointer compare false for now, but that is debatable.
	 * @return true iff reference is Valid and refers to the same object as the pointer, or Nil and pointer is NULL
	 */
	ILINE bool operator==(const IAIObject* const pThatObject) const;

	/**
	 * Negation of operator== for a pointer
	 */
	ILINE bool operator!=(const IAIObject* const pThatObject) const;

	/**
	 * Tests whether two references refer to the same object.
	 * For now, if either reference is invalid we return false, even if both are invalid or even both refer to the same invalid object, which is debatable
	 * @return true iff references are to the same Valid object or are both Nil
	 */
	ILINE bool operator==(const CAbstractUntypedRef& that) const;

	/**
	 * Negation of operator== for another reference
	 */
	ILINE bool operator!=(const CAbstractUntypedRef& that) const;

	/**
	 * Comparison operator to allow use in sorted containers
	 */
	ILINE bool operator<(const CAbstractUntypedRef& that) const;

protected:
	// Allow just CObjectContainer to create these, and we trust it to have ensured the type is appropriate
	ILINE void Assign(tAIObjectID nID);

	tAIObjectID m_nID;
};

/**
 * An abstract typed base class for references.
 * Some functionality of the references is independent of type and so is defined here.
 */
template<class T> class CAbstractRef : public CAbstractUntypedRef
{
public:

	/**
	 * Get a typed weak reference instance from any existing reference.
	 * Weak references can be created from both strong and weak references.
	 * @return A weak reference to the same object, if any.
	 */
	ILINE CWeakRef<T> GetWeakRef() const;
};

/**
 * Typed strong reference for defining ownership, with auto_ptr semantics.
 * An AI object can only be owned by just one strong reference.
 * They are by definition always valid until Released.
 * Strong references can only be assigned centrally by the system when the object is created and only passes around by single-transferrable ownership.
 * If you need more - for instance STL-container compatability - see CCountedRef below.
 */
template<class T = CAIObject> class CStrongRef : public CAbstractRef<T>
{
public:
	/**
	 * Construct an unassigned (nil) reference.
	 */
	ILINE CStrongRef();

	/**
	 * Single transferable ownership constructor.
	 */
	ILINE CStrongRef(CStrongRef& ref);

	/**
	 * Convert a NilRef into an unassigned strong reference.
	 * Technically this is a copy-constructor but it's really just syntactic sugar
	 */
	ILINE CStrongRef(type_nil_ref);

	/**
	 * Destructor automatically releases any object owned.
	 */
	ILINE ~CStrongRef();

	/**
	 * Single transferable ownership assignment.
	 */
	ILINE CStrongRef<T>& operator=(CStrongRef& ref);

	// Don't perform any stub checking as this is strong
	T* GetAIObject() const;

	// Don't perform any stub checking as this is strong
	ILINE IAIObject* GetIAIObject() const;

	/**
	 * Release any object owned.
	 * Deliberately named differently to the Reset method in weak references, because Releasing
	 * a strong reference causes object deregistration - which is a much more significant event.
	 * @return true iff this reference owned an object
	 */
	ILINE bool Release();

	ILINE      operator bool() const;

	void       Serialize(TSerialize ser, const char* sName = NULL);

	// Could define deref here - semantics are same as pointer for strong
	// Conversely, removing this is helpful in finding dodgy places in the code!
	ILINE T* operator->() const;

protected:
	// In principle, we could cache the CAIObject pointer here - but whether it's worth the bloat is debatable, especially with non-trivial stub

	ILINE tAIObjectID GiveOwnership();
};

/**
 * Template function to convert a typed weak reference to another type.
 * Typed references can be cast implicitly to another type if the bare pointer could be cast implicitly.
 * Where that is not the case, StaticCast can explicitly cast to another type, if static_cast would accomplish this for a bare pointer.
 * This method, like static_cast, does not perform dynamic type checking.
 */
template<class S, class U>
ILINE CWeakRef<S> StaticCast(const CAbstractRef<U>& ref);

/**
 * Typed weak reference.
 * Weak references allow AI code to refer to an AI object that it does not own.
 * A weak reference may be invalidated by the object being Released at _any_ time.
 * However, any pointer obtained from is guaranteed valid until the end of the frame.
 * Weak references are easily obtained from other strong or weak references.
 * They may be passed around and stored freely.
 */
template<class T = CAIObject> class CWeakRef : public CAbstractRef<T>
{
	friend class CObjectContainer;
	friend class CAbstractRef<T>;
	friend class CCountedRef<T>;

	template<class S, class U>
	friend CWeakRef<S> StaticCast(const CAbstractRef<U>& ref);

public:
	/**
	 * Construct an unassigned weak reference.
	 */
	ILINE CWeakRef();

	/**
	 * Construct a weak reference from any typed reference.
	 */
	template<class S>
	ILINE CWeakRef(const CAbstractRef<S>& ref);

	/**
	 * Convert a NilRef into an unassigned weak reference.
	 * Technically this is a copy-constructor but it's really just syntactic sugar
	 */
	ILINE CWeakRef(type_nil_ref);

	/**
	 * Deassign this weak reference.
	 * This will not affect the object itself or any other references to it.
	 */
	ILINE void Reset();

	/**
	 * Deassign this weak reference.
	 * This will not affect the object itself or any other references to it.
	 */
	ILINE bool IsReset() const;

	/**
	 * If this reference is invalid, reset it to Nil.
	 * Returns whether it was valid or not.
	 * Often useful to resolve an argument, ensuring it is Valid or Nil before storing it.
	 */
	ILINE bool ValidateOrReset();

	/**
	 * Acts like GetAIObject(), but if invalid Reset to Nil.
	 * Often useful to check update status of stored references while checking their validity.
	 */
	ILINE T*   GetAIObjectOrReset();

	ILINE T*   GetAIObject() const;

	ILINE bool IsValid() const;

	/**
	 * Assign a weak weak reference from any typed reference.
	 */
	ILINE void Assign(const CAbstractRef<T>& ref);

	void       Serialize(TSerialize ser, const char* sName = NULL);

protected:
	// Allow just CObjectManager to create in this way and we trust it to have ensured the type is appropriate
	// This can probably go away eventually
	// Note that when converting over from pointers, use of 0 as an argument can cause complaints of protected member here
	ILINE CWeakRef(tAIObjectID nID);
};

/**
 * Get a weak reference to the given object, of the same type as the object pointer.
 * Convenience method, currently with incredibly slow implementation.
 */
template<class T>
CWeakRef<T> GetWeakRef(T* pObject);

template<class T>
void SerialisationHack(TSerialize ser, const char* sName, T** pObj);

/**
 * Typed counted reference for defining ownership in objects while being STL-compatible.
 * Can only be initialised from strong references or other counted references.
 * They are by definition always valid until Released.
 * Strong references can only be assigned centrally by the system when the object is created and cannot be passed around.
 */

// Implementation notes:
// Example reference counting classes often must be assigned and assume a valid counter object. Here we can be unassigned, like Strong.
// This _isn't_ an abstract ref! It shouldn't directly contain an ID. A bit of restructuring might be required here.
template<class T = CAIObject> class CCountedRef
{
public:

#ifdef DEBUG_REFERENCES
	CAIObject * pObj;
#endif

	/**
	 * Construct an unassigned (nil) reference.
	 */
	ILINE CCountedRef();

	/**
	 * Construction from a strong ref (which is loses its single-transferable ownership)
	 */
	ILINE CCountedRef(CStrongRef<T>& ref);

	/**
	 * Copy constructor.
	 */
	ILINE CCountedRef(const CCountedRef& ref);

	/**
	 * Convert a NilRef into an unassigned counted reference.
	 * Technically this is a copy-constructor but it's really just syntactic sugar
	 */
	ILINE CCountedRef(type_nil_ref);

	/**
	 * Counted reference assignment.
	 */
	ILINE CCountedRef& operator=(const CCountedRef& ref);

	/**
	 * Destructor, of course, decrements the reference count and possibly deregisters the object.
	 */
	ILINE ~CCountedRef();

	ILINE bool IsNil() const;

	/**
	 * Decrement the count on any object owned, possibly causing the object to be deregistered.
	 * Same name convention as Strong ref as it can have the same consequences.
	 * @return true iff this reference was counting an object.
	 */
	ILINE bool Release();

	ILINE      operator bool() const;

	void       Serialize(TSerialize ser, const char* sName = NULL);

	ILINE bool operator==(const IAIObject* const pThatObject) const;
	ILINE bool operator==(const CAbstractUntypedRef& that) const;

	/**
	 * Get a typed weak reference instance from any existing reference.
	 * Weak references can be created from both strong and weak references.
	 * @return A weak reference to the same object, if any.
	 */
	ILINE CWeakRef<T> GetWeakRef() const;

	ILINE T*          GetAIObject() const;

	// Could define deref here - semantics are same as pointer for strong
	// Conversely, removing this is helpful in finding dodgy places in the code!
	ILINE T* operator->() const;

	/**
	 * Return a simple AI object ID.
	 * @return the tAIObjectID wrapped, regardless of validity. INVALID_AIOBJECTID if unassigned;
	 */
	ILINE tAIObjectID GetObjectID() const;

protected:
	// In principle, we could cache the m_nID or CAIObject pointer here - but whether it's worth the bloat is debatable, especially with non-trivial stub

	struct SRefCounter
	{
		SRefCounter()
			: m_nRefs(1)
		{
		} // Start at 1
		~SRefCounter()
		{
		}

		// Strong ref released automatically
		CStrongRef<T> m_strongRef;
		int           m_nRefs;        // Since this isn't in the object itself, it needn't be mutable
	};

	SRefCounter* m_pCounter;

	// For now, we just use new and delete, but a pool seems sensible
	ILINE void ObtainCounter();
	ILINE void ReleaseCounter();

	// (MATT) Force a release to a strong ref. Only makes sense when count is 1 - or we will leave dangling pointers to a deleted counter {2009/03/30}
	ILINE void ForceReleaseCounter();
};

#endif // __AI_REFERENCE_H_
