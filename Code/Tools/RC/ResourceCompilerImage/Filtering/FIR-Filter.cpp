// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2013-2014.
// -------------------------------------------------------------------------
//  File name:   FIR-Windows.h
//  Version:     v1.00
//  Created:     24/05/2013 by nielsf.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//   This is a rip-out of a much larger project, which has a MMX/SSE/SSE2
//   implementation and supports multi-threading. The code has received
//   as little changes as possible to allow the integration of any of those
//   features in the future.
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "..\ImageObject.h"

#include <math.h>
#include "FIR-Weights.h"

/* ####################################################################################################################
 */
#define mallocAligned(sze)	_aligned_malloc(sze, 16)
#define freeAligned(ptr)	  _aligned_free(ptr)

/* ####################################################################################################################
 */
class Rect2D {

public:
  /* ==================================================================================================================
   * Rect2D = ???
   */
  Rect2D()                         { }
  Rect2D(const int x, const int y) { visual[0] = x; visual[1] = y; }

private:
  int visual[2];

public:
  /* ==================================================================================================================
   * Access operators
   * M(i, j) == [row i][col j]
   */
  inline        int&    operator ()  (int i)       { return visual[i]; }
  inline        int     operator ()  (int i) const { return visual[i]; }

  inline        int&    operator []  (int i)       { return visual[i]; }
  inline        int     operator []  (int i) const { return visual[i]; }

  /* ==================================================================================================================
   * Logical operators
   */
  inline        bool    operator ==  (const Rect2D& tht) { return (visual[0] == tht.visual[0]) &&
                                                                  (visual[1] == tht.visual[1]); }
};

/* ####################################################################################################################
 */
template<class DataType, int excess = 128>
class Plane2D {

public:
  /* ==================================================================================================================
   * Plane2D = ???
   */
  Plane2D(const int x, const int y, const int p)
    : allocatedC(x, y)
  {
    planes = p; used = allocatedC; aligned[0] = (allocatedC[0] + 15) & (~15); aligned[1] = allocatedC[1]; allocate();
  }
  
  Plane2D(const Rect2D& i, const int p) 
    : allocatedC(i)
  {
    planes = p; used = allocatedC; aligned[0] = (allocatedC[0] + 15) & (~15); aligned[1] = allocatedC[1]; allocate();
  }

  ~Plane2D() { deallocate(); }

  /* ==================================================================================================================
	 * ??? = Plane2D
   */
  inline operator       Rect2D() const { return used; }
  inline operator       int     () const { return planes; }

  inline operator       DataType *  () const { return buffers; }
  inline operator       DataType ***() const { return rows; }
  inline operator const DataType *  () const { return (const DataType *)buffers; }
  inline operator const DataType ***() const { return (const DataType ***)rows; }

public:
  inline void locate(Rect2D take) {
    taken = take;

    if ((taken[0] >     aligned[0]) ||
        (taken[1] > abs(aligned[1])))
    {
      abort();
    }

    /* align column#length */
    used[0] = (taken[0] + 15) & (~15);
    used[1] =  taken[1]              ;

    for (int p = 0; p < planes; p++)
		{
      /* we may need to replicate the row-unsigned __int64 over all rows only (y < 0) */
      const unsigned long int rowsize = (aligned[1] <= 0 ? 0 : aligned[0] * sizeof(DataType));
      int r; char *buffer;
      assert((rowsize % 16) == 0);

      /* first block contains the row*-pointers */
      rows[p] = (DataType **)((char *)(rows + planes) + rowblocksize * p) + excess;

      /* "excess" empty pointers on negative offsets ----------------------------- */
      buffer = (char *)NULL;
      assert(((unsigned __int64)buffer % 16) == 0);

      for (r =         -excess; r <                         0; r++                  )
        rows[p][r] = (DataType*)(void*)buffer;

      /* regular row-pointers ---------------------------------------------------- */
      buffer = (char *)buffers + (planesize * p);
      assert(((unsigned __int64)buffer % 16) == 0);

      for (r =               0; r < abs(aligned[1])         ; r++, buffer += rowsize)
        rows[p][r] = (DataType*)(void*)buffer;

      /* overhanging row-pointers show on the first valid row (loop) ------------- */
      buffer = (char *)buffers + (planesize * p);
      assert(((unsigned __int64)buffer % 16) == 0);

      for (r = abs(aligned[1]); r < abs(aligned[1]) + excess; r++, buffer += rowsize)
        rows[p][r] = (DataType*)(void*)buffer;
    }
  }

  inline void clear() {
    memset(buffers, 0,                                planesize    * planes);
  }

  inline void delocate() {
    memset(buffers, 0,                                planesize    * planes);
    memset(rows   , 0, sizeof(DataType **) * planes + rowblocksize * planes);
  }

  inline void relocate(Rect2D take) {
    delocate();
    locate(take);
  }

protected:
  inline void allocate  () {
    /* adjust rows */
    allocatedC[1] = maximum<int>(1, allocatedC[1]);

    /* if "(y < 0)", we allocate exactly 1 row and replicate it over "abs(y)" */
    planesize    = sizeof(DataType) * aligned[0] * maximum<int>(1, aligned[1]);
    rowblocksize = sizeof(DataType *) * (abs<int>(aligned[1]) + (2 * excess) + 16);

    /* all planes after each other:
     *
     *  start -> plane0 -> plane1 -> ...
     */
    buffers = (DataType *  )mallocAligned(                               planesize    * planes);
	if (!buffers)
	{
		throw std::bad_alloc();
	}

    /* all pointers after each other:
     *
     *  start -> unsigned __int64 to planes -> unsigned __int64 to rows of planes
     */
    rows    = (DataType ***)mallocAligned(sizeof(DataType **) * planes + rowblocksize * planes);
	if (!rows)
	{
		freeAligned(buffers);
		throw std::bad_alloc();
	}

    /* ensure the blocks are aligned */
    assert(((unsigned __int64)buffers % 16) == 0);
    /* ensure the planes concat aligned */
    assert((planesize % 16) == 0);

    locate(allocatedC);
  }

  inline void deallocate() {
    freeAligned(buffers);
    freeAligned(rows);
  }

  Rect2D allocatedC, aligned; // real buffer sizes and its aligned counterpart
  Rect2D taken     , used;    // actually taken sizes and its aligned counterpart

  int planes;
  long int planesize, rowblocksize;
  DataType *buffers;
  DataType ***rows;
};

/* ####################################################################################################################	\
 */
#define	filterTVariables(filterVxNNum, dtyp, wtyp, reps)								                                                \
  /* addition of c-pointers already takes care of datatype-sizes */																											\
  const signed long int dy = /*parm->mirror ? -1 :*/ 1;								                                                  \
  const unsigned int stridei = parm->incols  * 1 * 1;								                                                    \
  const unsigned int stridet = parm->subcols * 1 * 1;								                                                    \
  const unsigned int strideo = parm->outcols * 1 * 1;								                                                    \
  /* offset and shift calculations still require the unmodified values */						                                    \
  const unsigned int strideiraw = parm->incols; 									                                                      \
  const unsigned int stridetraw = parm->subcols;									                                                      \
  const unsigned int strideoraw = parm->outcols;									                                                      \
  														                                                                                          \
  class Plane2D<dtyp> tmp(tmpcols, tmprows, 4);								                                                          \
  dtyp ***t = (dtyp ***)tmp;											                                                                      \
  int srcPos, dstPos;												                                                                            \
  bool plusminush = false; const bool of = true;									                                                      \
  bool plusminusv = false; const bool nc = false;									                                                      \
  FilterWeights<wtyp> *fwh = calculateFilterWeights<wtyp>(parm->resample.colrem, parm->caged ? 0 : 0 - parm->region.subtop, parm->caged ? srccols : parm->subrows - parm->region.subtop,	                                                                                                  \
                                                          parm->resample.colquo,               0                          ,               dstcols                                      , reps, parm->resample.colblur, parm->resample.wf, parm->resample.operation != eWindowEvaluation_Sum, plusminush); \
  FilterWeights<wtyp> *fwv = calculateFilterWeights<wtyp>(parm->resample.rowrem, parm->caged ? 0 : 0 - parm->region.intop , parm->caged ? srcrows : parm->inrows  - parm->region.intop ,	                                                                                                  \
                                                          parm->resample.rowquo,               0                          ,               dstrows                                      , reps, parm->resample.rowblur, parm->resample.wf, parm->resample.operation != eWindowEvaluation_Sum, plusminusv); \

#define	filterFTVariables(filterVxNNum)         											                                                  \
	filterTVariables(filterVxNNum, float , signed short, 1)

/* ####################################################################################################################	\
 */
#define	filterTCleanUp(filterVxNNum)											                                                              \
	delete[] fwh;													                                                                                \
	delete[] fwv;

/* ####################################################################################################################	\
 */
#define	filterTInitLoop()

/* ********************************************************************************************************************	\
 */
#define	filterTExitLoop()

/* ####################################################################################################################	\
 */
#define	filter4xNf(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip, init, next, fetch, store, exit, op, pm, hv, dtyp, atyp)	\
  init(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip);								                                                                                          \
														        \
  dstPos = 0; do {													\
    FilterWeights<signed short> &fw = *(hv + dstPos);									\
    const signed short *w = fw.weights;										        \
														        \
    next(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip);							        \
														        \
    atyp res0 = (op != eWindowEvaluation_Min ? 0 : 32768);								\
    atyp res1 = (op != eWindowEvaluation_Min ? 0 : 32768);								\
    atyp res2 = (op != eWindowEvaluation_Min ? 0 : 32768);								\
    atyp res3 = (op != eWindowEvaluation_Min ? 0 : 32768);								\
														        \
    srcPos = fw.first; do {												\
      /* get value */													\
      fetch(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip);							\
        													        \
      /* build result using sign inverted weights [32767,-32768] */							\
      if (op == eWindowEvaluation_Sum) {										\
        res0 -= ((atyp)_0 * *w  );											\
        res1 -= ((atyp)_1 * *w  );											\
        res2 -= ((atyp)_2 * *w  );											\
        res3 -= ((atyp)_3 * *w++);											\
      }                   										                \
      else if (op == eWindowEvaluation_Max) {										\
        res0 = maximum(res0, -(atyp)_0 * *w  );										\
        res1 = maximum(res1, -(atyp)_1 * *w  );										\
        res2 = maximum(res2, -(atyp)_2 * *w  );										\
        res3 = maximum(res3, -(atyp)_3 * *w++);										\
      }                   										                \
      else if (op == eWindowEvaluation_Min) {										\
        res0 = (atyp)32768.0 - maximum((atyp)32768.0 - res0, -(atyp)(1.0f - _0) * *w  );				\
        res1 = (atyp)32768.0 - maximum((atyp)32768.0 - res1, -(atyp)(1.0f - _1) * *w  );				\
        res2 = (atyp)32768.0 - maximum((atyp)32768.0 - res2, -(atyp)(1.0f - _2) * *w  );				\
        res3 = (atyp)32768.0 - maximum((atyp)32768.0 - res3, -(atyp)(1.0f - _3) * *w++);				\
      }                   										                \
    } while (++srcPos < fw.last);											\
														        \
/*  dtyp _0 = ldexp((dtyp)res0, -15);	*/										\
/*  dtyp _1 = ldexp((dtyp)res1, -15);	*/										\
/*  dtyp _2 = ldexp((dtyp)res2, -15);	*/										\
/*  dtyp _3 = ldexp((dtyp)res3, -15);	*/										\
														        \
    dtyp _0 = (dtyp)res0 * (dtyp)(1.0 / 32768.0);									\
    dtyp _1 = (dtyp)res1 * (dtyp)(1.0 / 32768.0);									\
    dtyp _2 = (dtyp)res2 * (dtyp)(1.0 / 32768.0);									\
    dtyp _3 = (dtyp)res3 * (dtyp)(1.0 / 32768.0);									\
														        \
    /* put value */													\
    store(srcOffs, srcSize, srcSkip, dstOffs,	dstSize, dstSkip);							\
  } while (++dstPos < (signed)dstSize);										        \
														        \
  exit(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip);

#define	filterF4xNHor(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip, init, next, fetch, store, exit, op, pm)	    \
	filter4xNf(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip, init, next, fetch, store, exit, op, 0, fwh, float, float /*double*/)

#define	filterF4xNVer(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip, init, next, fetch, store, exit, op, pm)		  \
	filter4xNf(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip, init, next, fetch, store, exit, op, 0, fwv, float, float /*double*/)

/* ####################################################################################################################	\
 */
#define	resampleF4xNFromPlane(stride)											                                                              \
	float _0, _1, _2, _3;												                                                                          \
															                                                                                          \
	_0 = (*i0)[ix], i0 += (stride) * dy;										                                                              \
	_1 = (*i1)[ix], i1 += (stride) * dy;										                                                              \
	_2 = (*i2)[ix], i2 += (stride) * dy;										                                                              \
	_3 = (*i3)[ix], i3 += (stride) * dy;

/* ********************************************************************************************************************	\
 */
#define	resampleF4xNFromStream(stride, instream)									                                                      \
	float _0, _1, _2, _3;												                                                                          \
															                                                                                          \
	_0 = instream[0];												                                                                              \
	_1 = instream[1];												                                                                              \
	_2 = instream[2];												                                                                              \
	_3 = instream[3], instream += (stride) * 4;

/* ********************************************************************************************************************	\
 */
#define	resampleF4xNFromStreamSwapped(stride, instream)									                                                \
	float _0, _1, _2, _3;												                                                                          \
															                                                                                          \
	_3 = instream[0];												                                                                              \
	_2 = instream[1];												                                                                              \
	_1 = instream[2];												                                                                              \
	_0 = instream[3], instream += (stride) * 4;

/* ####################################################################################################################	\
 */
#define	resampleF4xNToPlane(stride)											                                                                \
	(*o0)[ox] = _0;													                                                                              \
	(*o1)[ox] = _1;													                                                                              \
	(*o2)[ox] = _2;													                                                                              \
	(*o3)[ox] = _3, ox += (1) * 1;

/* ********************************************************************************************************************	\
 */
#define	resampleF4xNToStream(stride, outstream)										                                                      \
	outstream[0] = _0;												                                                                            \
	outstream[1] = _1;												                                                                            \
	outstream[2] = _2;												                                                                            \
	outstream[3] = _3, outstream += (1) * 4;

/* ********************************************************************************************************************	\
 */
#define	resampleF4xNToStreamSwapped(stride, outstream)									                                                \
	outstream[0] = _3;												                                                                            \
	outstream[1] = _2;												                                                                            \
	outstream[2] = _1;												                                                                            \
	outstream[3] = _0, outstream += (1) * 4;

/* ####################################################################################################################	\
 */
#undef	filterF4xNFromPlane
#define	filterF4xNFromPlane(stride)											                                                                \
	resampleF4xNFromPlane(stride)

/* ********************************************************************************************************************	\
 */
#undef	filterF4xNFromStream
#define	filterF4xNFromStream(stride, instream)										                                                      \
	resampleF4xNFromStream(stride, instream)

/* ********************************************************************************************************************	\
 */
#undef	filterF4xNFromStreamSwapped
#define	filterF4xNFromStreamSwapped(stride, instream)									                                                  \
	resampleF4xNFromStreamSwapped(stride, instream)

/* ####################################################################################################################	\
 */
#undef	filterF4xNToPlane
#define	filterF4xNToPlane(stride)											                                                                  \
	resampleF4xNToPlane(stride)

/* ********************************************************************************************************************	\
 */
#undef	filterF4xNToStream
#define	filterF4xNToStream(stride, outstream)										                                                        \
	resampleF4xNToStream(stride, outstream)

/* ********************************************************************************************************************	\
 */
#undef	filterF4xNToStreamSwapped
#define	filterF4xNToStreamSwapped(stride, outstream)									                                                  \
	resampleF4xNToStreamSwapped(stride, outstream)
			
/* ####################################################################################################################	\
 */
#define	loopEnter(id, untill, advance)											                                                            \
	unsigned int id; for (id = 0; id < untill; id += advance) {

#define	loopLeave(id, untill, advance)											                                                            \
	}

/* ####################################################################################################################	\
 */
#define	all4InitSwappablePlanePointers(left, top, row, rows, pp, swap, dtyp)						                                \
  dtyp *pp##0, *pp##1, *pp##2, *pp##3;											                                                            \
															                                                                                          \
  pp##0 = pp[0][/*parm->mirror ? (rows - 1)	- (row + top) :*/ (row + top)] + left;	/* r */				                      \
  pp##1 = pp[1][/*parm->mirror ? (rows - 1)	- (row + top) :*/ (row + top)] + left;	/* g */				                      \
  pp##2 = pp[2][/*parm->mirror ? (rows - 1)	- (row + top) :*/ (row + top)] + left;	/* b */				                      \
  pp##3 = pp[3][/*parm->mirror ? (rows - 1)	- (row + top) :*/ (row + top)] + left;	/* a */

#define	allF4InitSwappablePlanePointers(left, top, row, rows, pp, swap)							                                    \
	all4InitSwappablePlanePointers(left, top, row, rows, pp, swap, float)

/* ####################################################################################################################	\
 */
#define	all4InitFixedPlanePointers(left, top, row, rows, pp, swap, dtyp)						                                    \
  dtyp *pp##0, *pp##1, *pp##2, *pp##3;											                                                            \
															                                                                                          \
  pp##0 = pp[0][(row + top)] + left;	/* r */										                                                        \
  pp##1 = pp[1][(row + top)] + left;	/* g */										                                                        \
  pp##2 = pp[2][(row + top)] + left;	/* b */										                                                        \
  pp##3 = pp[3][(row + top)] + left;	/* a */

#define	allF4InitFixedPlanePointers(left, top, row, rows, pp, swap)							                                        \
	all4InitFixedPlanePointers(left, top, row, rows, pp, swap, float)

/* ####################################################################################################################	\
 */
#define	all4InitSwappablePlaneReferences(left, offs, top, row, rows, pp, swap, dtyp)					                          \
  unsigned long int pp##x = left + offs;										                                                            \
  dtyp **pp##0;														                                                                              \
  dtyp **pp##1;														                                                                              \
  dtyp **pp##2;														                                                                              \
  dtyp **pp##3;														                                                                              \
															                                                                                          \
  pp##0 = pp[0] + (/*parm->mirror ? (rows - 1) - (row + top) :*/ (row + top));	/* r */					                        \
  pp##1 = pp[1] + (/*parm->mirror ? (rows - 1) - (row + top) :*/ (row + top));	/* g */					                        \
  pp##2 = pp[2] + (/*parm->mirror ? (rows - 1) - (row + top) :*/ (row + top));	/* b */					                        \
  pp##3 = pp[3] + (/*parm->mirror ? (rows - 1) - (row + top) :*/ (row + top));	/* a */

#define	allF4InitSwappablePlaneReferences(left, offs, top, row, rows, pp, swap)						                              \
	all4InitSwappablePlaneReferences(left, offs, top, row, rows, pp, swap, float)

/* ####################################################################################################################	\
 */
#define	all4InitFixedPlaneReferences(left, row, rows, pp, ps, dtyp)							                                        \
  unsigned long int pp##x = left;											                                                                  \
  dtyp **pp##0;														                                                                              \
  dtyp **pp##1;														                                                                              \
  dtyp **pp##2;														                                                                              \
  dtyp **pp##3;														                                                                              \
															                                                                                          \
  pp##0 = ps[0] + (row);	/* r */											                                                                  \
  pp##1 = ps[1] + (row);	/* g */											                                                                  \
  pp##2 = ps[2] + (row);	/* b */											                                                                  \
  pp##3 = ps[3] + (row);	/* a */

#define	allF4InitFixedPlaneReferences(left, row, rows, pp, ps)								                                          \
	all4InitFixedPlaneReferences(left, row, rows, pp, ps, float)

/* ####################################################################################################################	\
 */
#define	allF4AdvanceSwappablePlaneReferences(row, rows, pp)								                                              \
  pp##0 += (rows - row) * dy;					/* r */							                                                              \
  pp##1 += (rows - row) * dy;					/* g */							                                                              \
  pp##2 += (rows - row) * dy;					/* b */							                                                              \
  pp##3 += (rows - row) * dy;					/* a */

/* ####################################################################################################################	\
 */
#define	allF4AdvanceFixedPlaneReferences(row, rows, pp)									                                                \
	allF4AdvanceSwappablePlaneReferences(row, rows, pp)

/* ####################################################################################################################	\
 */
#define	allF4AdvanceStreamPointer(left, col, cols, sp)									                                                \
  sp += ((cols) - (left + col)) * 4;

/* ********************************************************************************************************************	\
 */
#define	allF4AdvNMULStreamPointer(top, stride, sp)									                                                    \
  sp -= ((stride) * (top)) * 4;

/* ********************************************************************************************************************	\
 */
#define	allF4AdvPMULStreamPointer(top, stride, sp)									                                                    \
  sp += ((stride) * (top)) * 4;

/* ********************************************************************************************************************	\
 */
#define	allF4AdvSUBMStreamPointer(left, top, stride, sp)								                                                \
  sp -= (((stride) * (top)) - (left)) * 4;

/* ********************************************************************************************************************	\
 */
#define	allF4AdvADDMStreamPointer(left, top, stride, sp)								                                                \
  sp += ((left) + ((stride) * (top))) * 4;

/* ********************************************************************************************************************	\
 */
#define	allF4AdvPADDStreamPointer(offs, sp)										                                                          \
  sp += (offs) * 4;

/* ********************************************************************************************************************	\
 */
#define	allF4AdvSSUBStreamPointer(top, shift, offs, sp)									                                                \
  sp += (((top) << (shift)) - (offs)) * 4;

/* ********************************************************************************************************************	\
 */
#define	allF4AdvNAMAStreamPointer(left, top, offs, stride, sp)								                                          \
  sp -= ((left) + (((top) + (offs)) * (stride))) * 4;

 /* ################################################################################################################### \
 */
#undef	comcpyFNum
#define	comcpyFNum					1
#undef	orderedFNum
#define	orderedFNum					1
#undef	orderedFShift
#define	orderedFShift				0
#undef	interleavedFNum
#define	interleavedFNum			1

/* ####################################################################################################################	\
 */
#      define allTInitFixedOutPlaneReferences		allF4InitFixedPlaneReferences
#      define allTInitFixedInPlaneReferences		allF4InitFixedPlaneReferences
#      define allCInitSwappableInPlaneReferences		/*allF4InitSwappablePlaneReferences*/
#      define allCInitSwappableOutPlaneReferences		/*allF4InitSwappablePlaneReferences*/

#      define allCAdvADDMInStreamPointer		allF4AdvADDMStreamPointer
#      define allCAdvPADDInStreamPointer		allF4AdvPADDStreamPointer
#      define allCAdvPMULInStreamPointer		allF4AdvPMULStreamPointer
#      define allCAdvNMULInStreamPointer		allF4AdvNMULStreamPointer
#      define allCAdvADDMOutStreamPointer		allF4AdvADDMStreamPointer
#      define allCAdvSSUBOutStreamPointer		allF4AdvSSUBStreamPointer
 
#      define	getCxNFromStreamSwapped		/*filterF4xNFromStreamSwapped*/
#      define	getCxNFromStream		filterF4xNFromStream
#      define	getCxNFromPlane			/*filterF4xNFromPlane*/
#      define	getTxNFromPlane			filterF4xNFromPlane

#      define	filterHor			filterF4xNHor
#      define	filterVer			filterF4xNVer

#      define	comcpyCCheckHiLo()		/*orderedF4CheckHiLo*/
#      define	comcpyCCoVar()			/*orderedF4CoVar*/
#      define	comcpyCHistogram()		/*orderedF4Histogram*/

#      define	putCxNToStreamSwapped		/*filterF4xNToStreamSwapped*/
#      define	putCxNToStream			filterF4xNToStream
#      define	putCxNToPlane			/*filterF4xNToPlane*/
#      define	putTxNToPlane			filterF4xNToPlane

#    define	orderedNum			orderedFNum
#    define	orderedShift			orderedFShift

#    define	comcpyCMergeHiLo(orderedNum)		/*comcpyFTMergeHiLo*/
#    define	comcpyCCompleteCoVar(orderedNum)		/*comcpyFTCompleteCoVar*/
#    define	comcpyCCompleteHistogram(orderedNum)	/*comcpyFTCompleteHistogram*/

#    define	hiloCVariables(n)			/*hiloFTVariables*/
#    define	covarCVariables(n)			/*covarFTVariables*/
#    define	histoCVariables(n)			/*histoFTVariables*/

#    define	filterCVariables		filterFTVariables

#  define	orderedTInitLoop()			/*orderedTInitLoop*/
#  define	hiloTInitLoop()			    /*hiloTInitLoop*/
#  define	covarTInitLoop()			  /*covarTInitLoop*/
#  define	histoTInitLoop()			  /*histoTInitLoop*/

#  define	orderedTExitLoop()			/*orderedTExitLoop*/
#  define	hiloTExitLoop()			    /*hiloTExitLoop*/
#  define	covarTExitLoop()			  /*covarTExitLoop*/
#  define	histoTExitLoop()			  /*histoTExitLoop*/

#  define	filterCCleanUp			filterTCleanUp

/* ####################################################################################################################	\
 */

struct prcparm {
  /* configuration -------------------------------------------------------------------------------- */

  /* dimensions of the source/destination image */
  int inrows, outrows, subrows;
  int incols, outcols, subcols;

  /* region to process the stuff in */
  bool regional;
  /* don't fetch data from outside the region */
  bool caged;
  struct {
    /* offsets to the source/destination image-region */
    int intop , outtop , subtop ;
    int inleft, outleft, subleft;

    /* dimensions of the source/destination image-region */
    int inrows, outrows, subrows;
    int incols, outcols, subcols;
  } region;

  /* parameters ----------------------------------------------------------------------------------- */

  /* parameters for resampling */
  struct {
    /* we don't give floating-point x/y-factor, it's not exact enough */
    unsigned int rowquo, rowrem;
    unsigned int colquo, colrem;
    
    /* over/under-blurring (window minification/magnification) */
    float rowblur, colblur;
    
    /* the windowing-function for the filtering */
    IWindowFunction<double> *wf;
    
    /* operation to perform the filter with */
    int operation;
  } resample;

  /* private stuff -------------------------------------------------------------------------------- */

  /* what really has do be done after choosing/recalculation */
  int dorows, docols;
};

static void CheckBoundaries(const void *i, void *o, struct prcparm *parm)
{
  /* the parameter evaluation ----------------------------------------------- */
  const bool scaler = true;

  /* this is fairly straightforward */
  if (!parm->regional)
  {
    int rgrows = parm->inrows;
    int rgcols = parm->incols;

    /* compare out-region against available out-size */
    if (o)
    {
      int dorows = parm->outrows;
      int docols = parm->outcols;

      /* scale up */
      if (scaler)
      {
        dorows = dorows * parm->resample.rowrem / parm->resample.rowquo;
        docols = docols * parm->resample.colrem / parm->resample.colquo;
      }

      /* compare in scaled space */
      rgrows = maximum(rgrows, dorows);
      rgcols = maximum(rgcols, docols);

      /* scale down */
      if (scaler)
      {
        rgrows = rgrows * parm->resample.rowquo / parm->resample.rowrem;
        rgcols = rgcols * parm->resample.colquo / parm->resample.colrem;
      }
    }

    parm->dorows = rgrows;
    parm->docols = rgcols;
  }
  /* this may need some adjustment */
  else
  {
    int rgrows = parm->region.inrows;
    int rgcols = parm->region.incols;

    /* compare in/out-region against available out-size */
    if (o)
    {
      int dorows = parm->region.outrows;
      int docols = parm->region.outcols;

      /* compare out-region against available in-size */
      if (dorows > parm->outrows - parm->region.outtop)
        dorows = parm->outrows - parm->region.outtop;
      if (docols > parm->outcols - parm->region.outleft)
        docols = parm->outcols - parm->region.outleft;

      /* scale to in */
      if (scaler) 
      {
        dorows = dorows * parm->resample.rowrem / parm->resample.rowquo;
        docols = docols * parm->resample.colrem / parm->resample.colquo;
      }

      /* compare in scaled space */
      rgrows = maximum(rgrows, dorows);
      rgcols = maximum(rgcols, docols);

      /* compare in-region against available in-size */
      if (i)
      {
        if (rgrows > parm->inrows - parm->region.intop)
          rgrows = parm->inrows - parm->region.intop;
        if (rgcols > parm->incols - parm->region.inleft)
          rgcols = parm->incols - parm->region.inleft;
      }

      /* scale to out */
      if (scaler)
      {
        rgrows = rgrows * parm->resample.rowquo / parm->resample.rowrem;
        rgcols = rgcols * parm->resample.colquo / parm->resample.colrem;
      }
    }
    else
    {
      /* compare in-region against available in-size */
      if (i)
      {
        if (rgrows > parm->inrows - parm->region.intop)
          rgrows = parm->inrows - parm->region.intop;
        if (rgcols > parm->incols - parm->region.inleft)
          rgcols = parm->incols - parm->region.inleft;
      }
    }

    parm->dorows = rgrows;
    parm->docols = rgcols;
  }

  assert(parm->dorows > 0);
  assert(parm->docols > 0);
}

/* ####################################################################################################################	\
 * multi-threadable if needed
 */
static void RunAlgorithm(const float *i, float *o, struct prcparm *parm)
{
	/* make	these local, so	no indirect access is needed */
	const unsigned int srcrows = parm->dorows * parm->resample.rowrem / parm->resample.rowquo;
	const unsigned int srccols = parm->docols * parm->resample.colrem / parm->resample.colquo;
	const unsigned int dstrows = parm->dorows;
	const unsigned int dstcols = parm->docols;
	const unsigned int cstZero = 0;

	/* temporary buffer region */
	parm->subrows        = srccols;
	parm->subcols        = dstrows;
	parm->region.subleft = 0;
	parm->region.subtop  = 0;
	parm->region.subcols = parm->subcols;
	parm->region.subrows = parm->subrows;

	if (!parm->caged)
	{
		int oleft;
		int oright;

		/* check for out-of-region access rectangle */
		calculateFilterRange(srccols, oleft, oright, dstcols, 0, dstcols, parm->resample.colblur, parm->resample.wf);

		/* round down left, round up right */
		oleft  =  oleft                      & (~(orderedNum - 1));
		oright = (oright + (orderedNum - 1)) & (~(orderedNum - 1));

		/* clamp to available image-rectangle */
		if ((oleft  < (signed)parm->region.subtop) ||
		    (oright > (signed)parm->subrows))
		{
		  oleft  = maximum<int>(oleft , -(signed)parm->region.inleft);
		  oright = minimum<int>(oright,  (signed)parm->incols);
		}

		/* readjust temporary buffer region to include out-of-region accesses */
		parm->region.inleft += oleft; //rm->docols  -= oleft; //rm->docols  += (oright - srccols);
		parm->region.subtop -= oleft; parm->subrows -= oleft; parm->subrows += (oright - srccols);
	}

	const unsigned int tmprows = parm->subrows;
	const unsigned int tmpcols = parm->subcols;

	/* --------------------------------------------------------------------------------------------
	 * common resampling
	 */

	/* init t */
	filterCVariables(orderedNum);

	filterTInitLoop();

	/* --------------------------------------------------------------------------------------------
	 * reading rows, writing cols (xy-flip)
	 *
	 * make	srccol x	inrow -> dstrow x srccol
	 *
	 * we are reading vertical, and writing horizontal
	 * in effect we can use fast parallel-reads, but need
	 * slow interleaved-writes
	 * as reads are slower (ask+receive) than writes (send)
	 * this should even be gracefully fast
	 */
	allCAdvADDMInStreamPointer(parm->region.inleft, parm->region.intop, parm->incols, i);

#define	filterRowInit(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip)							                                  \
  allTInitFixedOutPlaneReferences(cstZero, srcOffs, -, o, t);

#define	filterRowNext(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip)							                                  \
  /* every in/out-put may swap */												                                                                  \
  allCInitSwappableInPlaneReferences(parm->region.inleft, srcOffs, parm->region.intop, fw.first, parm->inrows, i, false);	\
  /* because the filter moves back and forth, we always have to reposition from 0 */						                          \
  allCAdvPMULInStreamPointer(srcSkip##raw, fw.first, i);

#define	filterRowFetch(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip)							                                \
  /* vertical stride, horizontal fetch */											                                                            \
  getCxNFromStreamSwapped(srcSkip, i);											                                                              \
  getCxNFromStream(srcSkip, i);												                                                                    \
  getCxNFromPlane(1);														                                                                          \
																                                                                                          \
  /*srcPos++;*/

#define	filterRowStore(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip)							                                \
  /* because the filter moves back and forth, we always have to reposition to 0 */						                            \
  allCAdvNMULInStreamPointer(srcSkip##raw, fw.last, i);									                                                  \
															                                                                                            \
  /* horizontal stride, vertical store */											                                                            \
  putTxNToPlane(1);														                                                                            \
																                                                                                          \
  /*dstPos++;*/

#define	filterRowExit(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip)							                                  \
  allCAdvPADDInStreamPointer(orderedNum, i);

	if (parm->resample.operation == eWindowEvaluation_Sum)
	{
		loopEnter(tmprow, tmprows, orderedNum);

		{
			filterVer(tmprow, srcrows, stridei,
				tmprow, dstrows, stridet, filterRowInit, filterRowNext, filterRowFetch, filterRowStore, filterRowExit, eWindowEvaluation_Sum, of);
		}

		loopLeave(tmprow, tmprows, orderedNum);
	}
	else if (parm->resample.operation == eWindowEvaluation_Max)
	{
		loopEnter(tmprow, tmprows, orderedNum);

		{
			filterVer(tmprow, srcrows, stridei,
				tmprow, dstrows, stridet, filterRowInit, filterRowNext, filterRowFetch, filterRowStore, filterRowExit, eWindowEvaluation_Max, of);
		}

		loopLeave(tmprow, tmprows, orderedNum);
	}
	else if (parm->resample.operation == eWindowEvaluation_Min)
	{
		loopEnter(tmprow, tmprows, orderedNum);

		{
			filterVer(tmprow, srcrows, stridei,
				tmprow, dstrows, stridet, filterRowInit, filterRowNext, filterRowFetch, filterRowStore, filterRowExit, eWindowEvaluation_Min, of);
		}

		loopLeave(tmprow, tmprows, orderedNum);
	}

	/* 1st resampling end
	 * --------------------------------------------------------------------------------------------
	 */
	filterTExitLoop();

	/* return collected min/max */
	hiloCVariables(orderedNum);
	covarCVariables(orderedNum);
	histoCVariables(orderedNum);

	/* --------------------------------------------------------------- */
	orderedTInitLoop();
	filterTInitLoop();

	hiloTInitLoop();
	covarTInitLoop();
	histoTInitLoop();

	/* --------------------------------------------------------------------------------------------
	 * reading rows, writing cols (xy-flip)
	 *
	 * make	dstrow x srccol -> outcol x dstrow
	 *
	 * we are reading vertical, and writing horizontal
	 * in effect we can use fast parallel-reads, but need
	 * slow interleaved-writes
	 * as reads are slower (ask+receive) than writes (send)
	 * this should even be gracefully fast
	 */
	allCAdvADDMOutStreamPointer(parm->region.outleft, parm->region.outtop, parm->outcols, o);

#define	filterColInit(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip)							                                      \
  allCInitSwappableOutPlaneReferences(parm->region.outleft, cstZero, parm->region.outtop, srcOffs, parm->outrows, o, false);

#define	filterColNext(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip)							                                      \
  /* every in/out-put may swap */												                                                                      \
  allTInitFixedInPlaneReferences(srcOffs, parm->region.subtop + fw.first, -, i, t);

#define	filterColFetch(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip)							                                    \
  /* vertical stride, horizontal fetch */											                                                                \
  getTxNFromPlane(1);														                                                                              \
																                                                                                              \
  /*srcPos++;*/

#define	filterColStore(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip)							                                    \
  comcpyCCheckHiLo();														                                                                              \
  comcpyCCoVar();														                                                                                  \
  comcpyCHistogram();														                                                                              \
															                                                                                                \
  /* horizontal stride, vertical store */											                                                                \
  putCxNToStreamSwapped(dstSkip, o);												                                                                  \
  putCxNToStream(dstSkip, o);													                                                                        \
  putCxNToPlane(1);														                                                                                \
															                                                                                                \
  /*dstPos++;*/

#define	filterColExit(srcOffs, srcSize, srcSkip, dstOffs, dstSize, dstSkip)							                                      \
  allCAdvSSUBOutStreamPointer(dstSkip##raw, orderedShift, dstPos, o);

	if (parm->resample.operation == eWindowEvaluation_Sum)
	{
		loopEnter(dstrow, dstrows, orderedNum);

		{
			filterHor(dstrow, srccols, stridet,
				dstrow, dstcols, strideo, filterColInit, filterColNext, filterColFetch, filterColStore, filterColExit, eWindowEvaluation_Sum, of);
		}

		loopLeave(dstrow, dstrows, orderedNum);
	}
	else if (parm->resample.operation == eWindowEvaluation_Max)
	{
		loopEnter(dstrow, dstrows, orderedNum);

		{
			filterHor(dstrow, srccols, stridet,
				dstrow, dstcols, strideo, filterColInit, filterColNext, filterColFetch, filterColStore, filterColExit, eWindowEvaluation_Max, of);
		}

		loopLeave(dstrow, dstrows, orderedNum);
	}
	else if (parm->resample.operation == eWindowEvaluation_Min)
	{
		loopEnter(dstrow, dstrows, orderedNum);

		{
			filterHor(dstrow, srccols, stridet,
				dstrow, dstcols, strideo, filterColInit, filterColNext, filterColFetch, filterColStore, filterColExit, eWindowEvaluation_Min, of);
		}

		loopLeave(dstrow, dstrows, orderedNum);
	}

	/* 2nd resampling end
	 * --------------------------------------------------------------------------------------------
	 */
	histoTExitLoop();
	covarTExitLoop();
	hiloTExitLoop();

	filterTExitLoop();
	orderedTExitLoop();
	/* --------------------------------------------------------------- */

	/* return collected min/max */
	comcpyCMergeHiLo(orderedNum);
	comcpyCCompleteCoVar(orderedNum);
	comcpyCCompleteHistogram(orderedNum);

	/* exit	t */
	filterCCleanUp(orderedNum);
}

static void SplitAlgorithm(const void *i, void *o, struct prcparm *templ, int threads = 8)
{
	struct prcparm fraction[32];
	int t, istart = 0, sstart = 0, ostart = 0;
	const bool scaler = true;

	int theight = 0;

	/* prepare data to be emitted to the threads */
	for (t = 0; t < threads; t++) {
		fraction[t] = *templ;

		/* adjust the processing-region according to the available threads */
		{
#undef	split		/* only prefix-threads need aligned transpose (for not trashing suffix-thread data) */
#define	split(rows)	!scaler							\
	? ((rows * (t + 1)) / threads) & (~(t != threads - 1 ? 15 : 0))		\
	: ((rows * (t + 1)) / threads) & (~                         0 )

			/* area covered */
			const int  inrows = (fraction[t].regional ? fraction[t].region.inrows  : fraction[t].inrows );
			const int  incols = (fraction[t].regional ? fraction[t].region.incols  : fraction[t].incols );
			const int subrows = (fraction[t].regional ? fraction[t].region.subrows : fraction[t].subrows);
			const int subcols = (fraction[t].regional ? fraction[t].region.subcols : fraction[t].subcols);
			const int outrows = (fraction[t].regional ? fraction[t].region.outrows : fraction[t].outrows);
			const int outcols = (fraction[t].regional ? fraction[t].region.outcols : fraction[t].outcols);

			/* splitting blocks */
			const int istop = split(inrows) , sstop = split(subrows), ostop = split(outrows);
			const int irows = istop - istart, srows = sstop - sstart, orows = ostop - ostart;
			const int icols = incols        , scols = subcols       , ocols = outcols       ;

			assert(irows > 0); assert(orows > 0);
			assert(icols > 0); assert(ocols > 0);

			/* now we are regional */
			fraction[t].regional       = true;

			/* take previous regionality into account */
			fraction[t].region.intop += istart; fraction[t].region.subtop += sstart; fraction[t].region.outtop += ostart;
			fraction[t].region.inrows = irows ; fraction[t].region.subrows = srows ; fraction[t].region.outrows = orows ;

			/* take previous regionality into account */
			fraction[t].region.inleft += 0;     fraction[t].region.subleft += 0;     fraction[t].region.outleft += 0;
			fraction[t].region.incols  = icols; fraction[t].region.subcols  = scols; fraction[t].region.outcols  = ocols;

			/* advance block */
			istart = istop; sstart = sstop; ostart = ostop;

			/* check */
			theight += irows;
		}

		// the algorithm supports "i" and "o" pointing to the same memory
		CheckBoundaries((float*)i, (float*)o, &fraction[t]);
		RunAlgorithm((float*)i, (float*)o, &fraction[t]);
	}

	assert(theight >= (templ->regional ? templ->region.inrows : templ->inrows));
}

/* ####################################################################################################################	\
 */
void ImageObject::FilterImage(int filterIndex, int filterOp, float blurH, float blurV, const ImageObject* srcImg, int srcMip, int dstMip, RECT* srcRect, RECT* dstRect)
{
	uint32 srcWidth, srcHeight;
	char* pSrcMem;
	uint32 dwSrcPitch;

	srcImg->GetImagePointer(srcMip, pSrcMem, dwSrcPitch);

	srcWidth = srcImg->GetWidth(srcMip);
	srcHeight = srcImg->GetHeight(srcMip);

	uint32 dstWidth, dstHeight;
	char* pDestMem;
	uint32 dwDestPitch;

	GetImagePointer(dstMip, pDestMem, dwDestPitch);

	dstWidth = GetWidth(dstMip);
	dstHeight = GetHeight(dstMip);

	{
		struct prcparm parm;

		parm.incols  = srcWidth;
		parm.inrows  = srcHeight;
		parm.outcols = dstWidth;
		parm.outrows = dstHeight;
		parm.subcols = 0;
		parm.subrows = 0;
		parm.regional = false;
		parm.caged = false;

		memset(&parm.region, 0, sizeof(parm.region));

		if (srcRect || dstRect)
		{
			parm.regional = true;
			parm.caged = true;

			parm.region.inleft  = (!srcRect ? 0            :                  srcRect->left);
			parm.region.intop   = (!srcRect ? 0            :                   srcRect->top);
			parm.region.incols  = (!srcRect ? parm.incols  : srcRect->right - srcRect->left);
			parm.region.inrows  = (!srcRect ? parm.inrows  : srcRect->bottom - srcRect->top);

			parm.region.outleft = (!dstRect ? 0            :                  dstRect->left);
			parm.region.outtop  = (!dstRect ? 0            :                   dstRect->top);
			parm.region.outcols = (!dstRect ? parm.outcols : dstRect->right - dstRect->left);
			parm.region.outrows = (!dstRect ? parm.outrows : dstRect->bottom - dstRect->top);

			if (!srcRect)
			{
				parm.region.inleft  = parm.region.outleft * srcHeight / dstHeight;
				parm.region.intop   = parm.region.outtop  * srcWidth  / dstWidth;
			}

			if (!dstRect)
			{
				parm.region.outleft = parm.region.inleft * dstHeight / srcHeight;
				parm.region.outtop  = parm.region.intop  * dstWidth  / srcWidth;
			}
		}

		parm.resample.colquo = dstWidth;
		parm.resample.colrem = srcWidth;
		parm.resample.rowquo = dstHeight;
		parm.resample.rowrem = srcHeight;
		parm.resample.rowblur = blurH;
		parm.resample.colblur = blurV;
		parm.resample.operation = filterOp;
		parm.resample.wf = 0;

		switch (filterIndex)
		{
//			case eWindowFunction_COMBINER        : parm.resample.wf = new CombinerWindowFunction<double>(..., ...); break;
			case eWindowFunction_Point           : parm.resample.wf = new PointWindowFunction<double>(); break;
			case eWindowFunction_Box             : parm.resample.wf = new BoxWindowFunction<double>(); break;
			case eWindowFunction_Triangle        : parm.resample.wf = new TriangleWindowFunction<double>(); break;
			case eWindowFunction_Quadric         : parm.resample.wf = new QuadricWindowFunction<double>(); break;
			case eWindowFunction_Cubic           : parm.resample.wf = new CubicWindowFunction<double>(); break;
			case eWindowFunction_Hermite         : parm.resample.wf = new HermiteWindowFunction<double>(); break;
			case eWindowFunction_Catrom          : parm.resample.wf = new CatromWindowFunction<double>(); break;
			case eWindowFunction_Sine            : parm.resample.wf = new SineWindowFunction<double>(); break;
			case eWindowFunction_Sinc            : parm.resample.wf = new SincWindowFunction<double>(); break;
			case eWindowFunction_Bessel          : parm.resample.wf = new BesselWindowFunction<double>(); break;
			case eWindowFunction_Lanczos         : parm.resample.wf = new LanczosWindowFunction<double>(); break;
			case eWindowFunction_Gaussian        : parm.resample.wf = new GaussianWindowFunction<double>(); break;
			case eWindowFunction_Normal          : parm.resample.wf = new NormalWindowFunction<double>(); break;
			case eWindowFunction_Mitchell        : parm.resample.wf = new MitchellWindowFunction<double>(); break;
			case eWindowFunction_Hann            : parm.resample.wf = new HannWindowFunction<double>(); break;
			case eWindowFunction_BartlettHann    : parm.resample.wf = new BartlettHannWindowFunction<double>(); break;
			case eWindowFunction_Hamming         : parm.resample.wf = new HammingWindowFunction<double>(); break;
			case eWindowFunction_Blackman        : parm.resample.wf = new BlackmanWindowFunction<double>(); break;
			case eWindowFunction_BlackmanHarris  : parm.resample.wf = new BlackmanHarrisWindowFunction<double>(); break;
			case eWindowFunction_BlackmanNuttall : parm.resample.wf = new BlackmanNuttallWindowFunction<double>(); break;
			case eWindowFunction_Flattop         : parm.resample.wf = new FlatTopWindowFunction<double>(); break;
			case eWindowFunction_Kaiser          : parm.resample.wf = new KaiserWindowFunction<double>(); break;

			case eWindowFunction_SigmaSix        : parm.resample.wf = new SigmaSixWindowFunction<double>(); break;
			case eWindowFunction_KaiserSinc      : parm.resample.wf = new CombinerWindowFunction<double>(new SincWindowFunction<double>(), new KaiserWindowFunction<double>()); break;

			default: abort();  break;
		}

		parm.dorows = 0;
		parm.docols = 0;

		// TODO: not working yet, debug and enable
//		SplitAlgorithm(pSrcMem, pDestMem, &parm);

		// the algorithm supports "pSrcMem" and "pDestMem" pointing to the same memory
		CheckBoundaries((float*)(void*)pSrcMem, (float*)(void*)pDestMem, &parm);
		RunAlgorithm((float*)(void*)pSrcMem, (float*)(void*)pDestMem, &parm);

		delete parm.resample.wf;
	}
}

void ImageObject::FilterImage(const CImageProperties* pProps, const ImageObject* srcImg, int srcMip, int dstMip, RECT* srcRect, RECT* dstRect)
{
	if (pProps)
	{
		FilterImage(pProps->GetMipGenerationMethodIndex(), pProps->GetMipGenerationEvalIndex(), pProps->GetMipHorizontalBlurring(), pProps->GetMipVerticalBlurring(), srcImg, srcMip, dstMip, srcRect, dstRect);
	}
	else
	{
		FilterImage(pProps->GetMipGenerationMethodDefault(), pProps->GetMipGenerationEvalDefault(), 0.0f, 0.0f, srcImg, srcMip, dstMip, srcRect, dstRect);
	}
}

void ImageToProcess::FilterImage(int dstWidth, int dstHeight)
{
	if (get()->GetPixelFormat() != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		set(0);
		return;
	}

	ImageObject* const pNewImage = new ImageObject(dstWidth, dstHeight, get()->GetMipCount(), get()->GetPixelFormat(), get()->GetCubemap());

	pNewImage->CopyPropertiesFrom(*get());
	pNewImage->FilterImage(NULL, get(), 0, 0, NULL, NULL);

	set(pNewImage);
}
