// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

// ---- basic generic info about the XMLCPB system ----

// XMLCPB -> XML ComPressed Binary
// The XMLCPB system is used as a substitute for standard XML when intermediate memory usage and final size are a problem. It is also easily streamable at writting time.
// However its not a true generic XML format, there are many limitations. Basically:
//   - There are some hardcoded size limits: (numbers may change, check the defines in code for the actual value)
//     max amount of different tags(1024), different attr names(1024), different data strings(64k), simultaneously active read(64) or write(256) nodes, num children on a node (16383), num of attrs on a node (255), and some others.
//   - read and write are separated. Can generate an XMLCPB or can read from a generated XMLCPB, but can not do both at once. Also, can not append to or modify an already finished XMLCPB.
//   - the writting process is specially constrained: can not add childs or attrs to a node after it is "done" (which happens as soon as another node of the same parent is created ).
//   - the reading process is not as limited. But extremely random access can cause performance issues.

// in general, XMLCPB is tailored towards sequentially or near sequentially generation and reading of the data, like what happens in the savegame/loadgame processes (its main use and original purpose).

#pragma once
#ifndef XMLCPB_COMMON_H
	#define XMLCPB_COMMON_H

//#define XMLCPB_COLLECT_STATS  // remove comment for some manual profiling

	#ifndef _RELEASE
		#define XMLCPB_CHECK_HARDCODED_LIMITS // comment out this define for accurate performance profiling in profile build (it adds many runtime checks)
	#endif

	#if CRY_PLATFORM_WINDOWS // consoles dont need it because they already have a built-in integrity check. Also, in the way it is implemented atm, it could have a noticeable performance hit on consoles.
		#define XMLCPB_CHECK_FILE_INTEGRITY
	#endif

	#ifndef _RELEASE
		#define XMLCPB_DEBUGUTILS
	#endif

namespace XMLCPB {

typedef uint32 AttrSetID; // an "AttrSet" defines the datatype + tagId of all the attributes of a node.
typedef uint32 StringID;
typedef uint32 NodeLiveID;
typedef uint32 NodeGlobalID; // identifies each "real" node, as opposed to "live" nodes. The actual value is just the index of the node in the buffer that stores all of them
typedef uint32 FlatAddr;     // is just a 32 bits offset into data buffer.
typedef uint16 FlatAddr16;   // is just a 16 bits offset into data buffer.

static uint32 const XMLCPB_INVALID_ID = 0xffffffff;
static uint32 const XMLCPB_ROOTNODE_ID = 0;
static uint32 const XMLCPB_INVALID_FLATADDR = 0xffffffff;
static uint32 const XMLCPB_INVALID_SAFECHECK_ID = 0xffffffff;
static uint32 const XMLCPB_ZLIB_BUFFER_SIZE = 32 * 1024;        // size for the buffer used in the zlib compression

//////////////////////////////////////////////////////////////////////////
struct SFileHeader
{
	static const uint FILETYPECHECK = 'PBX0';

	SFileHeader()
	{
		memset(this, 0, sizeof(*this));
		m_fileTypeCheck = FILETYPECHECK;
	}

	struct SStringTable
	{
		uint32 m_numStrings;
		uint32 m_sizeStringData;
	};

	uint32 m_fileTypeCheck;

	#ifdef XMLCPB_CHECK_FILE_INTEGRITY
	enum { MD5_SIGNATURE_SIZE = 16 };
	char m_MD5Signature[MD5_SIGNATURE_SIZE];  // it uses the MD5 algorithm for the integrity check.
	#endif

	SStringTable m_tags;
	SStringTable m_attrNames;
	SStringTable m_strData;

	uint32       m_numAttrSets;
	uint32       m_sizeAttrSets;

	uint32       m_sizeNodes;
	uint32       m_numNodes;

	bool         m_hasInternalError;
};

// saved in the file before every compressed block
struct SZLibBlockHeader
{
	enum { NO_ZLIB_USED = 0xffffffff };
	uint32 m_compressedSize;  // when it is = NO_ZLIB_USED, the data is raw, without zlib compression (this should happen only very rarely)
	uint32 m_uncompressedSize;
};

//////////////////////////////////////////////////////////////////////////

enum eAttrDataType
{
	//...basic types......
	DT_STR,
	DT_INT32,
	DT_F1,
	DT_F3,   // Vec2 is not used at all, so removed it and will just store as F3 in case it is ever used
	DT_F4,
	DT_INT64,
	DT_RAWDATA,

	DT_NUM_BASIC_TYPES,

	//....compressed types...
	DT_POS16 = DT_NUM_BASIC_TYPES,   // the next type after DT_NUM_BASIC_TYPES need to be assigned like this to keep the correct enumeration
	DT_NEG16,
	DT_POS8,
	DT_NEG8,
	DT_F1_1DEC,         // stored as an small integer

	// float semi constant formats. explained in CAttrWriter::PackFloatInSemiConstType()
	DT_F3_1CONST,       // 1 const, 2 float vals
	DT_F3_2CONST,       // 2 const, 1 float vals
	DT_F3_3CONST,       // 3 const, 0 float vals
	DT_F4_1CONST,       // etc
	DT_F4_2CONST,
	DT_F4_3CONST,
	DT_F4_4CONST,

	//........numeric constants......
	DT_0,
	DT_1,
	DT_2,
	DT_3,
	DT_4,
	DT_5,
	DT_6,
	DT_7,
	DT_8,
	DT_9,
	DT_10,

	DT_255,

	// f3 specific constants.....
	DT_F3_100,
	DT_F3_010,
	DT_F3_001,

	//........string constants............
	DT_FIRST_CONST_STR,
	DT_NUM_CONST_STR  = 30, // how many string constants there are defined. it has to be the same number than the size of s_constStrings
	DT_LAST_CONST_STR = DT_FIRST_CONST_STR + DT_NUM_CONST_STR - 1,

	DT_NUMTYPES,
	DT_INVALID = 0xffffffff
};

struct STypeInfo
{
	eAttrDataType type; // used only to check that the array is correctly defined
	uint32        size;
	eAttrDataType basicType;
	const char*   pName; // used only for profiling / debuging / error visualization
};

eAttrDataType                 GetBasicDataType(eAttrDataType type);
uint32                        GetDataTypeSize(eAttrDataType type);
const char*                   GetDataTypeName(eAttrDataType type);
void                          InitializeDataTypeInfo();
const char*                   GetConstantString(uint32 ind);
inline bool                   IsTypeStrConstant(eAttrDataType type)   { return type >= DT_FIRST_CONST_STR && type <= DT_LAST_CONST_STR; }
inline StringID               GetStringConstantID(eAttrDataType type) { assert(IsTypeStrConstant(type)); return type - DT_FIRST_CONST_STR; }
template<class T> inline bool IsFlagActive(T val, uint32 flag)
{
	return (val & flag) != 0;
}

enum eNodeConstants
{
	// CNodeLiveWriter::Compact() has an easier to understand description of the data that all those mad constants do define

	// node header composition
	FLN_HASATTRS                           = BIT(15),
	FLN_CHILDREN_ARE_RIGHT_BEFORE          = BIT(14),
	LOWEST_BIT_USED_AS_FLAG_IN_NODE_HEADER = 14,
	CHILDREN_HEADER_BITS                   = 2,
	CHILDREN_HEADER_BIT_POSITION           = 12,
	HIGHPART_ATTRID_NUM_BITS               = 2,
	HIGHPART_ATTRID_BIT_POSITION           = 10,
	BITS_TAGID                             = 10,

	// the AttrSetId is stored, when possible, as a 8 bit value + some extra bits (2 atm) that are in the header of the node. When not possible, it is stored as a 16bits value.
	HIGHPART_ATTRID_MASK   = ((BIT(HIGHPART_ATTRID_NUM_BITS) - 1) << HIGHPART_ATTRID_BIT_POSITION),
	ATRID_USING_16_BITS    = BIT(8 + HIGHPART_ATTRID_NUM_BITS) - 1, // when the value stored in the 8 bits+highbits is this, it meants that the actual value is stored separately in 16 bits
	ATTRID_MAX_VALUE_8BITS = BIT(8 + HIGHPART_ATTRID_NUM_BITS) - 2, // -2 instead -1 because the actual max number is used to mark when the value cant be stored in the 8 bits + high bits (ATRID_USING_16_BITS)

	// constants for the "number of children" value stored in the header
	CHILDREN_HEADER_MASK    = ((BIT(CHILDREN_HEADER_BITS) - 1) << CHILDREN_HEADER_BIT_POSITION),
	CHILDREN_HEADER_CANTFIT = BIT(CHILDREN_HEADER_BITS) - 1,  // when the children number cant fit in the header, the header part gets this value
	CHILDREN_HEADER_MAXNUM  = BIT(CHILDREN_HEADER_BITS) - 2,

	// constants for the "number of children" value stored outside the header (used when the header is not enough)
	FL_CHILDREN_NUMBER_IS_16BITS                 = BIT(7),      // flag to mark when the number of children is stored in 16 bits
	FL_CHILDREN_NUMBER_AND_BLOCKDIST_IN_ONE_BYTE = BIT(6),      // flag to mark when the number of children and the distance to them will both be stored in the children byte
	MAX_NUMBER_OF_CHILDREN_IN_8BITS              = BIT(6) - 1,  // need to leave room for the flags
	MAX_NUMBER_OF_CHILDREN_IN_16BITS             = BIT(14) - 1, // still need to leave room for the flags

	// some constants for when is possible to store both the distance to the children and the number of children in the single byte
	CHILDANDDISTINONEBYTE_CHILD_BITS          = 3,
	CHILDANDDISTINONEBYTE_DIST_BITS           = (6 - CHILDANDDISTINONEBYTE_CHILD_BITS), // need to leave room for the 2 flags
	CHILDANDDISTINONEBYTE_MAX_AMOUNT_CHILDREN = BIT(CHILDANDDISTINONEBYTE_CHILD_BITS) - 1,
	CHILDANDDISTINONEBYTE_MAX_DIST            = BIT(CHILDANDDISTINONEBYTE_DIST_BITS) - 1,

	// constants for child blocks
	CHILDREN_PER_BLOCK                = 64,
	CHILDBLOCKS_USING_MORE_THAN_8BITS = BIT(7),
	CHILDBLOCKS_USING_24BITS          = CHILDBLOCKS_USING_MORE_THAN_8BITS | BIT(6), // only matters when CHILDBLOCKS_USING_MORE_THAN_8BITS is already true
	CHILDBLOCKS_MAX_DIST_FOR_8BITS    = BIT(7) - 1,
	CHILDBLOCKS_MASK_TO_REMOVE_FLAGS  = BIT(6) - 1,
	CHILDBLOCKS_MAX_DIST_FOR_16BITS   = BIT(6) - 1,

	// tag work values derived from BITS definitions
	MAX_TAGID    = BIT(BITS_TAGID) - 1,
	MASK_TAGID   = MAX_TAGID,
	MAX_NUM_TAGS = MAX_TAGID + 1,
};

enum eAttrConstants
{
	MAX_NUM_ATTRS  = 255,

	BITS_TYPEID    = 6,    // bits used for the Data Type ID on each attr
	BITS_NAMEID    = 10,   // for the strID that defines the name of each attr
	BITS_STRDATAID = 16,   // for the strID of the data on each attr that is of "string" type.

	// work values derived from BITS definitions
	MAX_TYPEID                = BIT(BITS_TYPEID) - 1,
	MAX_NUM_TYPES             = MAX_TYPEID + 1,
	MASK_TYPEID               = MAX_TYPEID,

	MAX_NAMEID                = BIT(BITS_NAMEID) - 1,
	MAX_NUM_NAMES             = MAX_NAMEID + 1,

	MAX_NUM_STRDATA           = BIT(BITS_STRDATAID),
	MAX_STRDATAID             = MAX_NUM_STRDATA - 1,
	VARIABLE_DATA_SIZE        = 0xffffffff,

	MAX_SIZE_FOR_A_DATASTRING = 1024,

	// for the 2-bits values in the bytemask, for float packed semiconstant types
	PFSC_0   = 0, // when is 0
	PFSC_1   = 1, // when is 1
	PFSC_N1  = 2, // when is -1
	PFSC_VAL = 3, // when is no constant
};

// the only reason for all the swappings is because zlib is more efficient when trying to compress some values in PC format (little endian) than in console format. Yea, rly.
	#if 1

// Ian[24:11:10] Since zlib compression now happens on a parallel thread time is not so important so I'll crank up the compression rate to compensate
		#define SwapIntegerValue(val)

	#else

template<class T>
inline void SwapIntegerValue(T& val)
{
		#if defined(NEED_ENDIAN_SWAP)
	SwapEndianBase(&val, 1);
		#endif
}

template<>
inline void SwapIntegerValue(FlatAddr& _val)
{
		#if defined(NEED_ENDIAN_SWAP)
	uint32 val = _val;
	SwapEndianBase(&val, 1);
	_val = val;
		#endif
}

	#endif

} // end namespace

#endif
