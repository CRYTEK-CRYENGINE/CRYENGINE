// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:CrySizerImpl.h
//  Implementation of the ICrySizer interface, which is used to
//  calculate the memory usage by the subsystems and components, to help
//  the artists keep the memory budged low.
//
//	History:
//
//////////////////////////////////////////////////////////////////////

#ifndef _CRY_SYSTEM_CRY_SIZER_CLASS_IMPLEMENTATION_HDR_
#define _CRY_SYSTEM_CRY_SIZER_CLASS_IMPLEMENTATION_HDR_

// prerequisities

//////////////////////////////////////////////////////////////////////////
// implementation of interface ICrySizer
// ICrySizer is passed to all subsystems and has a lot of helper functions that
// are compiled in the appropriate subsystems. CrySizerImpl is created in CrySystem
// and is passed to all the other subsystems
class CrySizerImpl : public ICrySizer
{
public:
	CrySizerImpl();
	~CrySizerImpl();

	virtual void   Release()      { delete this; }
	virtual size_t GetTotalSize() { return m_nTotalSize; };
	virtual size_t GetObjectCount();

	void           Reset();

	// adds an object identified by the unique pointer (it needs not be
	// the actual object position in the memory, though it would be nice,
	// but it must be unique throughout the system and unchanging for this object)
	// RETURNS: true if the object has actually been added (for the first time)
	//          and calculated
	virtual bool                AddObject(const void* pIdentifier, size_t nSizeBytes, int nCount = 1);

	virtual IResourceCollector* GetResourceCollector();

	// finalizes data collection, should be called after all objects have been added
	void End();

	void clear();

	// Arguments:
	//   pColl - can be 0
	void SetResourceCollector(IResourceCollector* pColl) { m_pResourceCollector = pColl; }

protected:

	IResourceCollector* m_pResourceCollector;               //

	// these functions must operate on the component name stack
	// they are to be only accessible from within class CrySizerComponentNameHelper
	// which should be used through macro SIZER_COMPONENT_NAME
	virtual void Push(const char* szComponentName);
	virtual void PushSubcomponent(const char* szSubcomponentName);
	virtual void Pop();

	// searches for the name in the name array; adds the name if it's not there and returns the index
	size_t getNameIndex(size_t nParent, const char* szComponentName);

	// returns the index of the current name on the top of the name stack
	size_t getCurrentName() const;

protected:
	friend class CrySizerStatsBuilder;

	// the stack of subsystem names; the indices in the name array are kept, not the names themselves
	typedef DynArray<size_t> NameStack;
	NameStack m_stackNames;

	// the array of names; each name ever pushed on the stack is present here
	struct ComponentName
	{
		ComponentName(){}
		ComponentName(const char* szName, size_t parent = 0) :
			strName(szName),
			nParent(parent),
			numObjects(0),
			sizeObjects(0),
			sizeObjectsTotal(0)
		{
		}

		void assign(const char* szName, size_t parent = 0)
		{
			strName = szName;
			nParent = parent;
			numObjects = 0;
			sizeObjects = 0;
			sizeObjectsTotal = 0;
			arrChildren.clear();
		}

		// the component name, not including the parents' names
		string           strName;
		// the index of the parent, 0 being the root
		size_t           nParent;
		// the number of objects within this component
		size_t           numObjects;
		// the size of the objects belonging to this component, in bytes
		size_t           sizeObjects;
		// the total size of all objects; gets filled by the end() method of the CrySizerImpl
		size_t           sizeObjectsTotal;
		// the children components
		DynArray<size_t> arrChildren;
	};
	typedef DynArray<ComponentName> NameArray;
	NameArray m_arrNames;

	// the set of objects and their sizes: the key is the object address/id,
	// the value is the size of the object and its name (the index of the name actually)
	struct Object
	{
		const void* pId;   // unique pointer identifying the object in memory
		size_t      nSize; // the size of the object in bytes
		size_t      nName; // the index of the name in the name array

		Object()
		{ clear(); }

		Object(const void* id, size_t size = 0, size_t name = 0) :
			pId(id), nSize(size), nName(name) {}

		// the objects are sorted by their Id
		bool operator<(const Object& right) const { return (UINT_PTR)pId < (UINT_PTR)right.pId; }
		bool operator<(const void* right) const   { return (UINT_PTR)pId < (UINT_PTR)right; }
		//friend bool operator < (const void* left, const Object& right);

		bool operator==(const Object& right) const { return pId == right.pId; }

		void clear()
		{
			pId = NULL;
			nSize = 0;
			nName = 0;
		}
	};
	typedef std::set<Object> ObjectSet;
	// 2^g_nHashPower == the number of subsets comprising the hash

	enum {g_nHashPower = 12};

	// hash size (number of subsets)
	enum {g_nHashSize = 1 << g_nHashPower};
	// hash function for an address; returns value 0..1<<g_nHashSize
	unsigned getHash(const void* pId);
	unsigned GetDepthLevel(unsigned nCurrent);

	ObjectSet m_setObjects[g_nHashSize];

	// the last object inserted; this is a small optimization for our template implementaiton
	// that often can add two times the same object
	Object m_LastObject;

	size_t m_nTotalSize;
};

#endif
