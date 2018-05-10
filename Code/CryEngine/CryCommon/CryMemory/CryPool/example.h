// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Kopietz
// Modified: -
//
//---------------------------------------------------------------------------

#ifndef __CCRYPOOLDOC__
#define __CCRYPOOLDOC__

//The documentation is split up into 3 main parts, so strg+f for
// -Theory
// -Building blocks
// -Usage
// -FAQ
// -Realloc/Resize

/////////////////////////////////////////////////////////////////////////
// -Theory
/////////////////////////////////////////////////////////////////////////

//this includes the 3 major parts of the allocate suite
//1. the memory location templates
//2. container types
//3. some allocator version
//addtional you get
//4. a simple stack based defragmentation template
//5. helper

//1. memory location templates
//   There are two types of them, static and dynamic
//1.1 CMemoryStatic<size> allows you do define on compile time what size
//    it should have, suitable for pool you know that they won't grow or
//    shrink
//1.2 CMemoryDynamic, this one has no template parameter, it has just one
//    indirection via ptr to the memory location and size, that you will
//    set during initialization.

//2. Container types
//   We have also two container types, one so called "In Place"
//   and one "Referenced".
//2.1 "In Place" means that a header is placed above every allocation,
//    this is the usual way most allocators work.
//2.2 "Referenced", has an extra pool of headers that point to the actual
//    memory. This is suitable for
//    - external memory locations that are not directly accessable by the
//      cpu. E.g. pools on disk, networks, rsx memory..
//    - defragmentation, because you don't save a ptr to the real memory
//      location, just a "handle" of the referencing item.
//    - big alignments, having 4kb of alignment would waste also
//    - 4kb for ever "In Place" header, you might not want that.

//3. Allocators
//   This time we have 3 of them, "BestFit", "WorstFit" and "FirstFit"
//3.1 FirstFit just seeks for any location big enought to fit your
//    requested size of memory. Internally it also saves the last used
//    free memory area to speed up allocations.
//		Use this also if you have just one particular allocation size.
//3.2 WorstFit, although it might sound illogical, WorstFit can reduce
//    memory fragmentation in a cases with very random allocation sizes,
//    because it gives smaller free blocks the chance to concatenate to
//    bigger free blocks again while filling up those previously
//    generated big blocks. The bad side is that it takes quite some time
//    to find the biggest block as this needs to be done every time you
//    allocate, so use this just when having a low amount of allocations
//    or you're really desperately looking for mem.
//3.3 BestFit, it's best used if you don't have just one allocation size,
//    but still very few varying sizes. Previously released blocks of
//    the currently allocating sizes will be seeked and reused, this
//    strongly helps to reduce fragmentation. While this might be slow
//    in some cases, it can save you from doing any defragmentation.

//4. Defragmentation
//   At the moment just one defragmentation algorithm is implemented:
//   "Stack defragmentator"
//   If you don't want some block to be moved, "Lock" it using your
//   memory handle.
//4.1 Stack based
//    To reduce fragmentation, holes are filled up with the next used,
//    memory area. This defragmentation sheme is useful when you have
//    some long living locations as well as very short living ones.
//    At some point all long live memory will end up at the bottom of
//    the stack, while leaving empty memory areas at the top for short
//    living allocations.

//5. Helper
//   this should be filled up with some handy helper tools for this
//   pool suite.
//   The first tool is a wrapper for the usage with stl
//5.1 Wrapper for STL
//    As you know, you can pass your own allocator as the last
//    parameter of stl containers, with this helper you can use a pool
//    created with this suite and wrap it for the stl.

/////////////////////////////////////////////////////////////////////////
// -Building blocks
/////////////////////////////////////////////////////////////////////////

//That's the theory, so how does it work?
//It's pretty simple, you compose the pool of your dreams by cascading
//templates.
//Lets start with an exmaple
//Per level you want to allocate a fixed amount of memory for your
//textures.
CMemoryDynamic
//- They are placed in some memory you can access directly with the cpu:
CInPlace
//- and you don't want to defragmentate, so you prefer an allocation
//  sheme that reduces fragmentation.
  CBestFit
//now you combine them
typedef CBestFit<CInPlace<CMemoryDynamic>, CListItemInPlace> TMyOwnPool;

//Yes, it's that simple.
//ok, ok, texture memory is usually nothing you want to access directly
//with your cpu, so let's create a referencing pool. Therefor you need
//to also specify how many nodes that can reference your pool will have.
//We won't have more than 4000 textures, so let's start with
{
	enum TEXTURE_NODE_COUNT = 4096
};
//and now our referencing pool
typedef CBestFit < CReferenced<CMemoryDynamic, TEXTURE_NODE_COUNT> TMyOwnPool;

//But yeah, you're right, texture memory has also a fixed size, lets
//assume it's 128MB.
{
	enum TEXTURE_MEMORY_SIZE = 128 * 1024 * 1024
};
//and our fixed sized memory pool
typedef CBestFit < CReferenced<CMemoryStatic<TEXTURE_MEMORY_SIZE>, TEXTURE_NODE_COUNT> TMyOwnPool;

//ok, but you don't trust the best fit allocator in all cases, you prefer
//a fast one and you accept the slow down for defragmentation incase the
//allocation fails.
//So lets created a straight First Fit allocator with defragmentation:
typedef CDefragStacked < CFirstFit<CReferenced<CMemoryStatic<TEXTURE_MEMORY_SIZE>, TEXTURE_NODE_COUNT>> TMyOwnPool;

//here you see how simple you can add defragmentation, but be careful, it
//works of course just on Reference based memory containers, if you have
//Direct pointers to In Place allocation, we cannot shuffle them around.

/////////////////////////////////////////////////////////////////////////
// -Usage
/////////////////////////////////////////////////////////////////////////

//it all starts by including the meain header
#include "PoolAlloc.h"

//Define your dream allocator, preferably using a typedef (or macro)
typedef CBestFit<CInPlace<CMemoryDynamic>, CListItemInPlace> TMyOwnPool;
//also typedef (or macro) your handle
typedef uint8*                                               TMyHandle; //in case of "In Place" allocations
typedef uint32                                               TMyHandle; //in case of "Referenced"

//Instantiate it
TMyOwnPool g_MyMemory;

//now you need to initialize it,
g_MyMemory.InitMem(pMemoryArea, MemorySize); //in case you use "CMemoryDynamic"
g_MyMemory.InitMem();                        //in case you use "CmemoryStatic,
                                             //altough you could pass the same
                                             //parameters, they'd be ignored.
                                             //Use this also to flush the pool
                                             //quickly

//now allocate
TMyHandle MemID = g_MyMemory.Allocate<TMyHandle>(Size);
//optionally alignment can be passed as 2nd parameter
TMyHandle MemID = g_MyMemory.Allocate<TMyHandle>(Size, Align);

//free it simply by calling
g_Memory.Free(MemID);

//you might want to call the beat function to defragment the memory
//on regular base
g_Memory.Beat();
//you might also want to call it just when an allocation failed to
//defragmentate the memory as good as possible
if (!(MemID = g_Memoery.Allocate<TMyHandle>(Size)))
{
	while (g_Memory.Beat())
		;
	MemID = g_Memoery.Allocate<TMyHandle>(Size);
}

//To acquire the pointer to your data, you need to resolve the handle
MyObject* pObject = g_Memory.Resolve<MyOBject*>(MemID);

/////////////////////////////////////////////////////////////////////////
// -Realloc/Resize
/////////////////////////////////////////////////////////////////////////

// The Containers provide a "resize" function. This one does nothing else
// than the name suggest, it is freeing some memory at the end of your
// allocation or, if free memory is available, allocates some memory to
// the end of your buffer. But it may also fail, if not enough memory
// available to allocate.
// "Realloc" on the other side requires an extra template that you wrap
// around your existing one like:
typedef CReallocator<TMyOwnPool> TMyOwnPoolWithReallocation;
// This one will first try to use resize, but in case it fails, it will
// allocate a seperate memory area, copy the data and free the old one.
//
// But this may fail as well, therefor the result is not a pointer to the
// allocation, but true/false.
// There for you need to pass a pointer to your pointer to the memory area
// or handle you deal with.
Handle = rMemory.Allocate<TPtr>(10, 1);
if (!rMemory.Reallocate<TPtr>(&Handles, 11, 1))
{
	//handle realloc failure
}

/////////////////////////////////////////////////////////////////////////
// -FAQ
/////////////////////////////////////////////////////////////////////////

//"DO I HAVE TO ALWAYS RESOLVE?"
//if you use "In Place" memory, not at all, all resolve does is to
//cast your handle to your object ptr and returns it.
//if you use "Referenced" memory and you don't defragmentate, you
//can do it once and keep the ptr, but you also need to keep the
//handle to free the memory later on.

//"any reason I should resolve?"
//Yes, first of all, it makes it very easy to switch between various
//pool configuration for testing, you simply change some params of
//your typedef (or macro) and it should work out of the box.
//second, for defragmentation it's the only way to go and for future
//things it might be needed as well

//"but isn't resolving just overhead?"
//in case of "In Place": no, the resolve function just returns the
//pointer, casting to your wanted type
//in case of "Referenced": it cost you one indirection.

//"How do I flush the whole pool without freeing all items?"
g_Memory.InitMem()
//yes, you can call "InitMem" once again, you need to pass the mem
//ptr and size if using CMemoryDynamic e.g.
g_Memory.Init(g_Memory.Size(), g_Memory.Data());

//"How do I lock the allocated memory to avoid any reallocation"
g_Memory.Item(ptr)->Lock();

//"How do I get the size of a memory block?"
g_Memory.Item(ptr)->MemSize();

//"Is there any example?"
//for a real life example check PAUnitTest.cpp used to validate all
//functions of this pool.

//bug reports? questions? support?
//just ask me :) (michael kopietz)

#endif
