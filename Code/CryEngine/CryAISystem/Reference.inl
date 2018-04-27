// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "AIObject.h"
#include "AIObjectManager.h"

bool CAbstractUntypedRef::IsNil() const
{
	return (m_nID == INVALID_AIOBJECTID);
}

bool CAbstractUntypedRef::IsSet() const
{
	return (m_nID != INVALID_AIOBJECTID);
}

tAIObjectID CAbstractUntypedRef::GetObjectID() const
{
	return m_nID;
}

IAIObject* CAbstractUntypedRef::GetIAIObject() const
{
	CRY_ASSERT(gAIEnv.pObjectContainer);

	CAIObject* object = gAIEnv.pObjectContainer->GetAIObject(*this);
	if (!object)
		return NULL;

	return reinterpret_cast<IAIObject*>(object);
}

bool CAbstractUntypedRef::operator==(const IAIObject* const pThatObject) const
{
	return GetIAIObject() == pThatObject;
}

bool CAbstractUntypedRef::operator!=(const IAIObject* const pThatObject) const
{
	return !(*this == pThatObject);
}

bool CAbstractUntypedRef::operator==(const CAbstractUntypedRef& that) const
{
	return (m_nID == that.m_nID);
}

bool CAbstractUntypedRef::operator!=(const CAbstractUntypedRef& that) const
{
	return !(*this == that);
}

bool CAbstractUntypedRef::operator<(const CAbstractUntypedRef& that) const
{
	return this->m_nID < that.m_nID;
}

void CAbstractUntypedRef::Assign(tAIObjectID nID)
{
	m_nID = nID;
}

template<class T>
CWeakRef<T> CAbstractRef<T >::GetWeakRef() const
{
	return CWeakRef<T>(m_nID);
}

template<class T>
CStrongRef<T>::CStrongRef()
{
	this->m_nID = INVALID_AIOBJECTID;
}

template<class T>
CStrongRef<T>::CStrongRef(CStrongRef<T>& ref)
{
	this->m_nID = ref.GiveOwnership();
}

template<class T>
CStrongRef<T>::CStrongRef(type_nil_ref)
{
	this->m_nID = INVALID_AIOBJECTID;
}

template<class T>
CStrongRef<T>::~CStrongRef()
{
	Release();
}

template<class T>
CStrongRef<T>& CStrongRef<T >::operator=(CStrongRef<T>& ref)
{
	// Do nothing if assigned to self
	if (this != &ref)
	{
		// Release any object currently owned
		Release();

		// Take ownership of object
		this->m_nID = ref.GiveOwnership();
	}

	return *this;
}

template<class T>
T* CStrongRef<T >::GetAIObject() const
{
	return static_cast<T*>(this->GetIAIObject());
}

template<class T>
IAIObject* CStrongRef<T >::GetIAIObject() const
{
	return CAbstractUntypedRef::GetIAIObject();
}

template<class T>
bool CStrongRef<T >::Release()
{
	if (this->m_nID != INVALID_AIOBJECTID)
	{
		CRY_ASSERT(gAIEnv.pObjectContainer);
		gAIEnv.pObjectContainer->DeregisterObject(*this);
		this->m_nID = INVALID_AIOBJECTID;
		return true;
	}
	return false;
}

template<class T>
CStrongRef<T >::operator bool() const
{
	return !this->IsNil();
}

template<class T>
void CStrongRef<T >::Serialize(TSerialize ser, const char* sName)
{
#if _DEBUG
	if (ser.IsWriting() && this->m_nID != INVALID_AIOBJECTID)
	{
		if (!GetAIObject())
		{
			assert(false);
			CryLogAlways("Saving an AI strong ref to an object that doesn't exist - code bug");
		}
	}
#endif

	// [AlexMcC|19.03.10] Make sure we release this object before overwriting it.
	// Otherwise we'll leak any objects that were created as things were initialized
	// that are about to be replaced with things we load from save games.
	if (ser.IsReading())
		Release();

	// Will the compiler coalesce all these? Or, would #T be useful?
	ser.Value((sName ? sName : "strongRef"), this->m_nID);
}

template<class T>
T* CStrongRef<T >::operator->() const
{
	return this->GetAIObject();
}

template<class T>
tAIObjectID CStrongRef<T >::GiveOwnership()
{
	// Here, obviously, we quietly revert to an empty reference without destroying the object we own.
	// It is not a problem if we are Nil. Callers should be responsible new owners
	tAIObjectID nID = this->m_nID;
	this->m_nID = INVALID_AIOBJECTID;
	return nID;
}

template<class S, class U> CWeakRef<S> StaticCast(const CAbstractRef<U>& ref)
{
	// Allow any cast that static_cast would allow on a bare pointer
	S* pDummy = static_cast<S*>((U*)0);
	(void)pDummy;
	return CWeakRef<S>(ref.GetObjectID());
}

template<class T>
CWeakRef<T>::CWeakRef()
{
	this->m_nID = INVALID_AIOBJECTID;
}

template<class T> template<class S>
CWeakRef<T>::CWeakRef(const CAbstractRef<S>& ref)
{
	// Allow this for anywhere an implicit pointer cast would succeed.
	T* pDummy = (S*)0;
	(void)pDummy;
	this->m_nID = ref.GetObjectID();
}

template<class T>
CWeakRef<T>::CWeakRef(type_nil_ref)
{
	this->m_nID = INVALID_AIOBJECTID;
}

template<class T>
void CWeakRef<T >::Reset()
{
	this->m_nID = INVALID_AIOBJECTID;
}

template<class T>
bool CWeakRef<T >::IsReset() const
{
	return(this->m_nID == INVALID_AIOBJECTID);
}

template<class T>
bool CWeakRef<T >::ValidateOrReset()
{
	if (IsValid())
		return true;

	Reset();

	return false;
}

template<class T>
T* CWeakRef<T >::GetAIObjectOrReset()
{
	T* pObject = this->GetAIObject();
	if (!pObject)
		Reset();

	return pObject;
}

template<class T>
T* CWeakRef<T >::GetAIObject() const
{
	T* pObject = static_cast<T*>(this->GetIAIObject());
	return pObject;
}

template<class T>
bool CWeakRef<T >::IsValid() const
{
	return gAIEnv.pObjectContainer->IsValid(*this);
}

template<class T>
void CWeakRef<T >::Assign(const CAbstractRef<T>& ref)
{
	this->m_nID = ref.GetObjectID();
}

template<class T>
void CWeakRef<T >::Serialize(TSerialize ser, const char* sName)
{
	if (ser.IsWriting())
		this->ValidateOrReset();
	ser.Value((sName ? sName : "weakRef"), this->m_nID);  // Will the compiler coalesce all these? Or, would #T be useful?
}

template<class T>
CWeakRef<T>::CWeakRef(tAIObjectID nID)
{
	this->m_nID = nID;
}

template<class T>
CWeakRef<T> GetWeakRef(T* pObject)
{
	if (!pObject)
		return NILREF;

	return StaticCast<T>(pObject->GetSelfReference());
}

template<class T>
void SerialisationHack(TSerialize ser, const char* sName, T** pObj)
{
	CWeakRef<T> ref;
	if (ser.IsReading())
	{
		ref.Serialize(ser, sName);
		*pObj = ref.GetAIObject();
	}

	if (ser.IsWriting())
	{
		if (*pObj)
			ref = GetWeakRef(*pObj);
		ref.Serialize(ser, sName);
	}
}

#ifdef DEBUG_REFERENCES
	#define SET_DEBUG_OBJ(x) pObj = x
#else
	#define SET_DEBUG_OBJ(x)
#endif

template<class T>
CCountedRef<T>::CCountedRef()
{
	this->m_pCounter = NULL;
	SET_DEBUG_OBJ(NULL);
}

template<class T>
CCountedRef<T>::CCountedRef(CStrongRef<T>& ref)
{
	this->m_pCounter = NULL;
	// Remember there's no point counting Nil references. We also don't check validity, just like Strong.
	if (!ref.IsNil())
	{
		ObtainCounter();                     // Get a reference counting object (starts at 1)
		this->m_pCounter->m_strongRef = ref; // Acquire the ownership - note we don't need to be friends
		SET_DEBUG_OBJ(ref.GetAIObject());
	}
}

template<class T>
CCountedRef<T>::CCountedRef(const CCountedRef& ref)
{
	this->m_pCounter = NULL;
	// If the other instance is empty, we don't need to do anything.
	if (ref.m_pCounter)
	{
		this->m_pCounter = ref.m_pCounter; // Share the counter
		++(this->m_pCounter->m_nRefs);     // Increment the count
		SET_DEBUG_OBJ(ref.GetAIObject());
	}
}

template<class T>
CCountedRef<T>::CCountedRef(type_nil_ref)
{
	this->m_pCounter = NULL;
#ifdef DEBUG_REFERENCES
	this->pObj = NULL;
#endif
}

template<class T>
CCountedRef<T>& CCountedRef<T >::operator=(const CCountedRef& ref)
{
	// Do nothing if assigned to same counter
	if (m_pCounter == ref.m_pCounter)
		return *this;

	// Release one count on any object currently owned and possibly release object itself
	Release();

	if (ref.m_pCounter != NULL)
	{
		this->m_pCounter = ref.m_pCounter;
		++(this->m_pCounter->m_nRefs);
		SET_DEBUG_OBJ(ref.GetAIObject());
	}

	return *this;
}

template<class T>
CCountedRef<T>::~CCountedRef()
{
	Release();
}

template<class T>
bool CCountedRef<T >::IsNil() const
{
	return (this->m_pCounter == NULL);
}

template<class T>
bool CCountedRef<T >::Release()
{
	if (!this->m_pCounter)
		return false;

	--(this->m_pCounter->m_nRefs);
	if (this->m_pCounter->m_nRefs == 0)
	{
		ReleaseCounter();
	}
	else
	{
		this->m_pCounter = NULL;
		SET_DEBUG_OBJ(NULL);
	}
	return true;
}

template<class T>
CCountedRef<T >::operator bool() const
{
	return this->m_pCounter != NULL;
}

template<class T>
void CCountedRef<T >::Serialize(TSerialize ser, const char* sName)
{
	// (MATT) Perhaps this could be tidied up {2009/04/02}

	// Here, for now, I have to hack it and just handle the case of a count of 1 when serialisation occurs.
	// Given that the multiple references are really only needed for STL compatability, that's safe for now.
	// (MATT) No it's not, when counted refs are being copied _during_ serisation to help implement the process {2009/04/01}
	//assert(!this->m_pCounter || this->m_pCounter->m_nRefs <= 1);

	// I think that when reading, an initial count greater than one is a very bad thing
	if (ser.IsReading())
		assert(!this->m_pCounter || this->m_pCounter->m_nRefs == 1);

	// Easiest efficient way to do this: a local copy of our Strong ref
	// We temporarily transfer any ownership to this strong ref
	CStrongRef<T> strongRef;
	if (this->m_pCounter)
	{
		strongRef = this->m_pCounter->m_strongRef; // Note this takes ownership
	}

	// Serialise that, which of course will write it out or read it in, which will replace ownership if need be
	strongRef.Serialize(ser, (sName ? sName : "countedRef"));

	// Now we must translate back into a counter object, if any is needed
	// This is true whether we wrote out, and need our counter working again to continue play,
	// or whether we read in and so need to make a counter object to start working with the new value.

	// If we need a counter now and we don't already have one, create one
	if (strongRef.IsSet() && !this->m_pCounter)
		ObtainCounter();

	if (this->m_pCounter)
	{
		// If we had a counter but don't need it anymore, because the object we owned has been replaced by Nil, we must delete the counter
		if (strongRef.IsNil())
			ForceReleaseCounter();
		else
			this->m_pCounter->m_strongRef = strongRef; // Note this passes any ownership back
	}

	// We shouldn't have local ownership when we finish
	assert(strongRef.IsNil());
}

template<class T>
bool CCountedRef<T >::operator==(const IAIObject* const pThatObject) const
{
	// No reference is equivalent to Nil is equivalent to a NULL pointer
	if (!m_pCounter)
		return (pThatObject == NULL);

	return m_pCounter->m_strongRef == pThatObject;
}

template<class T>
bool CCountedRef<T >::operator==(const CAbstractUntypedRef& that) const
{
	// No reference is equivalent to Nil
	if (!m_pCounter)
		return (that.IsNil());

	return m_pCounter->m_strongRef == that;
}

template<class T>
CWeakRef<T> CCountedRef<T >::GetWeakRef() const
{
	return (m_pCounter ? CWeakRef<T>(m_pCounter->m_strongRef.GetObjectID()) : NILREF);
}

template<class T>
T* CCountedRef<T >::GetAIObject() const
{
	return (m_pCounter ? m_pCounter->m_strongRef.GetAIObject() : NULL);
}

template<class T>
T* CCountedRef<T >::operator->() const
{
	return this->m_pCounter->m_strongRef.GetAIObject();
}

template<class T>
tAIObjectID CCountedRef<T >::GetObjectID() const
{
	return (m_pCounter ? m_pCounter->m_strongRef.GetObjectID() : INVALID_AIOBJECTID);
}

template<class T>
void CCountedRef<T >::ObtainCounter()
{
	assert(!m_pCounter);
	m_pCounter = new SRefCounter();
}

template<class T>
void CCountedRef<T >::ReleaseCounter()
{
	assert(m_pCounter && m_pCounter->m_nRefs == 0);
	SAFE_DELETE(m_pCounter);
	SET_DEBUG_OBJ(NULL);
}

template<class T>
void CCountedRef<T >::ForceReleaseCounter()
{
	assert(m_pCounter && m_pCounter->m_nRefs == 1);
	SAFE_DELETE(m_pCounter);
	SET_DEBUG_OBJ(NULL);
}
