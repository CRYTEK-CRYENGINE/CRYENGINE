// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Can we use shallow copy and reference counting to reduce cost of copying containers?
// #SchematycTODO : Move declaration of CVariantContainer after declaration of CVariant?
// #SchematycTODO : Refactor to make use of new C++11 enumeration and union syntax.
// #SchematycTODO : Rename CVariantContainer to CContainer?
// #SchematycTODO : Can we ditch pool string now? It might be enough to just store a reference counted string/char array.
// #SchematycTODO : Replace COMPILE_TIME_ASSERT with static_assert.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_ArrayView.h>

#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/Deprecated/IStringPool.h"
#include "CrySchematyc2/Utils/StringUtils.h"

namespace Schematyc2
{
	class CVariant;

	typedef DynArray<CVariant>												VariantDynArray;
	typedef TemplateUtils::CArrayView<CVariant>				TVariantArray;
	typedef TemplateUtils::CArrayView<const CVariant>	TVariantConstArray;
	typedef std::vector<CVariant>											TVariantVector;

	bool operator == (const VariantDynArray& lhs, const VariantDynArray& rhs);
	bool operator != (const VariantDynArray& lhs, const VariantDynArray& rhs);

	class CVariantContainer
	{
	public:

		static const size_t npos = s_invalidIdx;

		inline CVariantContainer() {}

		template <size_t SIZE> inline CVariantContainer(const CVariant (&values)[SIZE])
		{
			m_data.reserve(SIZE);
			for(size_t pos = 0; pos < SIZE; ++ pos)
			{
				m_data.push_back(values[pos]);
			}
		}

		inline void Reserve(size_t size)
		{
			m_data.reserve(size);
		}

		inline void PushBack(const CVariant& value)
		{
			m_data.push_back(value);
		}

		inline void PopBack()
		{
			m_data.pop_back();
		}

		inline void Add(const TVariantConstArray& value)
		{
			for(size_t pos = 0, stride = value.size(); pos < stride; ++ pos)
			{
				m_data.push_back(value[pos]);
			}
		}

		bool RemoveByIndex(const size_t index);
		bool RemoveByValue(const TVariantConstArray& value);

		size_t FindByValue(const CVariant& value) const;

		void Clear();

		inline bool Empty() const
		{
			return m_data.empty();
		}

		inline size_t Size() const
		{
			return m_data.size();
		}

		inline CVariant& Front()
		{
			return m_data.front();
		}

		inline const CVariant& Front() const
		{
			return m_data.front();
		}

		inline CVariant& Back()
		{
			return m_data.back();
		}

		inline const CVariant& Back() const
		{
			return m_data.back();
		}

		inline CVariant& operator [] (size_t i)
		{
			return m_data[i];
		}

		inline const CVariant& operator [] (size_t i) const
		{
			return m_data[i];
		}

		inline bool operator == (const CVariantContainer& rhs) const
		{
			return m_data == rhs.m_data;
		}

		inline bool operator != (const CVariantContainer& rhs) const
		{
			return m_data != rhs.m_data;
		}

	private:

		VariantDynArray m_data;
	};

	/*struct SVariantBlockHeader
	{
		inline SVariantBlockHeader()
			: size(0)
		{}
		
		inline SVariantBlockHeader(const uint32 _size)
			: size(_size)
		{}

		inline bool operator == (const SVariantBlockHeader& rhs) const
		{
			return size == rhs.size;
		}

		inline bool operator != (const SVariantBlockHeader& rhs) const
		{
			return size != rhs.size;
		}

		uint32	size;
	};*/

	/*struct SVariantArrayHeader
	{
		inline SVariantArrayHeader()
			: length(0)
		{}

		inline SVariantArrayHeader(const uint32 _length)
			: length(_length)
		{}

		inline bool operator == (const SVariantArrayHeader& rhs) const
		{
			return length == rhs.length;
		}

		inline bool operator != (const SVariantArrayHeader& rhs) const
		{
			return length != rhs.length;
		}

		uint32	length;
	};*/

	class CVariant
	{
	public:

		enum ETypeId
		{
			NIL = 0,
			BOOL,
			INT8,
			UINT8,
			INT16,
			UINT16,
			INT32,
			UINT32,
			INT64,
			UINT64,
			FLOAT,
			CONTAINER,
			//BLOCK_HEADER,
			//ARRAY_HEADER,
			STRING,
			POOL_STRING
		};

		inline CVariant()
			: m_typeId(NIL)
		{}

		inline CVariant(ETypeId typeId)
			: m_typeId(NIL)
		{
			switch(typeId)
			{
			case BOOL:
				{
					static_assert(sizeof(bool) <= sizeof(m_data), "Type doesn't fit into the buffer");
					*reinterpret_cast<bool*>(m_data) = bool();
					m_typeId = BOOL;
					break;
				}
			case INT8:
				{
					static_assert(sizeof(int8) <= sizeof(m_data), "Type doesn't fit into the buffer");
					*reinterpret_cast<int8*>(m_data) = int8();
					m_typeId = INT8;
					break;
				}
			case UINT8:
				{
					static_assert(sizeof(uint8) <= sizeof(m_data), "Type doesn't fit into the buffer");
					*reinterpret_cast<uint8*>(m_data) = uint8();
					m_typeId = UINT8;
					break;
				}
			case INT16:
				{
					static_assert(sizeof(int16) <= sizeof(m_data), "Type doesn't fit into the buffer");
					*reinterpret_cast<int16*>(m_data) = int16();
					m_typeId = INT16;
					break;
				}
			case UINT16:
				{
					static_assert(sizeof(uint16) <= sizeof(m_data), "Type doesn't fit into the buffer");
					*reinterpret_cast<uint16*>(m_data) = uint16();
					m_typeId = UINT16;
					break;
				}
			case INT32:
				{
					static_assert(sizeof(int32) <= sizeof(m_data), "Type doesn't fit into the buffer");
					*reinterpret_cast<int32*>(m_data) = int32();
					m_typeId = INT32;
					break;
				}
			case UINT32:
				{
					static_assert(sizeof(uint32) <= sizeof(m_data), "Type doesn't fit into the buffer");
					*reinterpret_cast<uint32*>(m_data) = uint32();
					m_typeId = UINT32;
					break;
				}
			case INT64:
				{
					static_assert(sizeof(int64) <= sizeof(m_data), "Type doesn't fit into the buffer");
					*reinterpret_cast<int64*>(m_data) = int64();
					m_typeId = INT64;
					break;
				}
			case UINT64:
				{
					static_assert(sizeof(uint64) <= sizeof(m_data), "Type doesn't fit into the buffer");
					*reinterpret_cast<uint64*>(m_data) = uint64();
					m_typeId = UINT64;
					break;
				}
			case FLOAT:
				{
					static_assert(sizeof(float) <= sizeof(m_data), "Type doesn't fit into the buffer");
					*reinterpret_cast<float*>(m_data) = float();
					m_typeId = FLOAT;
					break;
				}
			case CONTAINER:
				{
					static_assert(sizeof(CVariantContainer) <= sizeof(m_data), "Type doesn't fit into the buffer");
					new (m_data) CVariantContainer();
					m_typeId = CONTAINER;
					break;
				}
			/*case BLOCK_HEADER:
				{
					COMPILE_TIME_ASSERT(sizeof(SVariantBlockHeader) <= sizeof(m_data));
					new (m_data) SVariantBlockHeader();
					m_typeId = BLOCK_HEADER;
					break;
				}*/
			/*case ARRAY_HEADER:
				{
					COMPILE_TIME_ASSERT(sizeof(SVariantArrayHeader) <= sizeof(m_data));
					new (m_data) SVariantArrayHeader();
					m_typeId = ARRAY_HEADER;
					break;
				}*/
			case STRING:
				{
					m_data[0] = '\0';
					m_typeId  = STRING;
					break;
				}
			case POOL_STRING:
				{
					static_assert(sizeof(CPoolString) <= sizeof(m_data), "Type doesn't fit into the buffer");
					new (m_data) CPoolString();
					m_typeId = POOL_STRING;
					break;
				}
			}
		}

		inline CVariant(bool value)
			: m_typeId(BOOL)
		{
			static_assert(sizeof(bool) <= sizeof(m_data), "Type doesn't fit into the buffer");
			*reinterpret_cast<bool*>(m_data) = value;
		}

		inline CVariant(int8 value)
			: m_typeId(INT8)
		{
			static_assert(sizeof(int8) <= sizeof(m_data), "Type doesn't fit into the buffer");
			*reinterpret_cast<int8*>(m_data) = value;
		}

		inline CVariant(uint8 value)
			: m_typeId(UINT8)
		{
			static_assert(sizeof(uint8) <= sizeof(m_data), "Type doesn't fit into the buffer");
			*reinterpret_cast<uint8*>(m_data) = value;
		}

		inline CVariant(int16 value)
			: m_typeId(INT16)
		{
			static_assert(sizeof(int16) <= sizeof(m_data), "Type doesn't fit into the buffer");
			*reinterpret_cast<int16*>(m_data) = value;
		}

		inline CVariant(uint16 value)
			: m_typeId(UINT16)
		{
			static_assert(sizeof(uint16) <= sizeof(m_data), "Type doesn't fit into the buffer");
			*reinterpret_cast<uint16*>(m_data) = value;
		}

		inline CVariant(int32 value)
			: m_typeId(INT32)
		{
			static_assert(sizeof(int32) <= sizeof(m_data), "Type doesn't fit into the buffer");
			*reinterpret_cast<int32*>(m_data) = value;
		}

		inline CVariant(uint32 value)
			: m_typeId(UINT32)
		{
			static_assert(sizeof(uint32) <= sizeof(m_data), "Type doesn't fit into the buffer");
			*reinterpret_cast<uint32*>(m_data) = value;
		}

		inline CVariant(int64 value)
			: m_typeId(INT64)
		{
			static_assert(sizeof(int64) <= sizeof(m_data), "Type doesn't fit into the buffer");
			*reinterpret_cast<int64*>(m_data) = value;
		}

		inline CVariant(uint64 value)
			: m_typeId(UINT64)
		{
			static_assert(sizeof(uint64) <= sizeof(m_data), "Type doesn't fit into the buffer");
			*reinterpret_cast<uint64*>(m_data) = value;
		}

		inline CVariant(float value)
			: m_typeId(FLOAT)
		{
			static_assert(sizeof(float) <= sizeof(m_data), "Type doesn't fit into the buffer");
			*reinterpret_cast<float*>(m_data) = value;
		}

		/*inline CVariant(const SVariantBlockHeader& value)
			: m_typeId(BLOCK_HEADER)
		{
			COMPILE_TIME_ASSERT(sizeof(SVariantBlockHeader) <= sizeof(m_data));
			*reinterpret_cast<SVariantBlockHeader*>(m_data) = value;
		}*/

		/*inline CVariant(const SVariantArrayHeader& value)
			: m_typeId(ARRAY_HEADER)
		{
			COMPILE_TIME_ASSERT(sizeof(SVariantArrayHeader) <= sizeof(m_data));
			*reinterpret_cast<SVariantArrayHeader*>(m_data) = value;
		}*/

		inline CVariant(const char* value)
			: m_typeId(POOL_STRING)
		{
			StoreString(value);
		}

		inline CVariant(const CVariant& rhs)
		{
			Copy(rhs);
		}

		inline ~CVariant()
		{
			Release();
		}

		inline ETypeId GetTypeId() const
		{
			return static_cast<ETypeId>(m_typeId);
		}

		inline bool& AsBool()
		{
			CRY_ASSERT(m_typeId == BOOL);
			return *reinterpret_cast<bool*>(m_data);
		}

		inline const bool& AsBool() const
		{
			CRY_ASSERT(m_typeId == BOOL);
			return *reinterpret_cast<const bool*>(m_data);
		}

		inline int8& AsInt8()
		{
			CRY_ASSERT(m_typeId == INT8);
			return *reinterpret_cast<int8*>(m_data);
		}

		inline const int8& AsInt8() const
		{
			CRY_ASSERT(m_typeId == INT8);
			return *reinterpret_cast<const int8*>(m_data);
		}

		inline uint8& AsUInt8()
		{
			CRY_ASSERT(m_typeId == UINT8);
			return *reinterpret_cast<uint8*>(m_data);
		}

		inline const uint8& AsUInt8() const
		{
			CRY_ASSERT(m_typeId == UINT8);
			return *reinterpret_cast<const uint8*>(m_data);
		}

		inline int16& AsInt16()
		{
			CRY_ASSERT(m_typeId == INT16);
			return *reinterpret_cast<int16*>(m_data);
		}

		inline const int16& AsInt16() const
		{
			CRY_ASSERT(m_typeId == INT16);
			return *reinterpret_cast<const int16*>(m_data);
		}

		inline uint16& AsUInt16()
		{
			CRY_ASSERT(m_typeId == UINT16);
			return *reinterpret_cast<uint16*>(m_data);
		}

		inline const uint16& AsUInt16() const
		{
			CRY_ASSERT(m_typeId == UINT16);
			return *reinterpret_cast<const uint16*>(m_data);
		}

		inline int32& AsInt32()
		{
			CRY_ASSERT(m_typeId == INT32);
			return *reinterpret_cast<int32*>(m_data);
		}

		inline const int32& AsInt32() const
		{
			CRY_ASSERT(m_typeId == INT32);
			return *reinterpret_cast<const int32*>(m_data);
		}

		inline uint32& AsUInt32()
		{
			CRY_ASSERT(m_typeId == UINT32);
			return *reinterpret_cast<uint32*>(m_data);
		}

		inline const uint32& AsUInt32() const
		{
			CRY_ASSERT(m_typeId == UINT32);
			return *reinterpret_cast<const uint32*>(m_data);
		}

		inline int64& AsInt64()
		{
			CRY_ASSERT(m_typeId == INT64);
			return *reinterpret_cast<int64*>(m_data);
		}

		inline const int64& AsInt64() const
		{
			CRY_ASSERT(m_typeId == INT64);
			return *reinterpret_cast<const int64*>(m_data);
		}

		inline uint64& AsUInt64()
		{
			CRY_ASSERT(m_typeId == UINT64);
			return *reinterpret_cast<uint64*>(m_data);
		}

		inline const uint64& AsUInt64() const
		{
			CRY_ASSERT(m_typeId == UINT64);
			return *reinterpret_cast<const uint64*>(m_data);
		}

		inline float& AsFloat()
		{
			CRY_ASSERT(m_typeId == FLOAT);
			return *reinterpret_cast<float*>(m_data);
		}

		inline const float& AsFloat() const
		{
			CRY_ASSERT(m_typeId == FLOAT);
			return *reinterpret_cast<const float*>(m_data);
		}

		inline CVariantContainer& AsContainer()
		{
			CRY_ASSERT(m_typeId == CONTAINER);
			return *reinterpret_cast<CVariantContainer*>(m_data);
		}

		inline const CVariantContainer& AsContainer() const
		{
			CRY_ASSERT(m_typeId == CONTAINER);
			return *reinterpret_cast<const CVariantContainer*>(m_data);
		}

		/*inline SVariantBlockHeader& AsBlockHeader()
		{
			CRY_ASSERT(m_typeId == BLOCK_HEADER);
			return *reinterpret_cast<SVariantBlockHeader*>(m_data);
		}*/

		/*inline const SVariantBlockHeader& AsBlockHeader() const
		{
			CRY_ASSERT(m_typeId == BLOCK_HEADER);
			return *reinterpret_cast<const SVariantBlockHeader*>(m_data);
		}*/

		/*inline SVariantArrayHeader& AsArrayHeader()
		{
			CRY_ASSERT(m_typeId == ARRAY_HEADER);
			return *reinterpret_cast<SVariantArrayHeader*>(m_data);
		}*/

		/*inline const SVariantArrayHeader& AsArrayHeader() const
		{
			CRY_ASSERT(m_typeId == ARRAY_HEADER);
			return *reinterpret_cast<const SVariantArrayHeader*>(m_data);
		}*/

		inline const char* c_str() const
		{
			CRY_ASSERT((m_typeId == STRING) || (m_typeId == POOL_STRING));
			switch(m_typeId)
			{
			case Schematyc2::CVariant::STRING:
				{
					return m_data;
				}
			case Schematyc2::CVariant::POOL_STRING:
				{
					return reinterpret_cast<const CPoolString*>(m_data)->c_str();
				}
			default:
				{
					return "";
				}
			}
		}

		template <typename VISITOR> void Visit(VISITOR& visitor) const
		{
			switch(m_typeId)
			{
			case BOOL:
				{
					visitor(*reinterpret_cast<const bool*>(m_data));
					break;
				}
			case INT8:
				{
					visitor(*reinterpret_cast<const int8*>(m_data));
					break;
				}
			case UINT8:
				{
					visitor(*reinterpret_cast<const uint8*>(m_data));
					break;
				}
			case INT16:
				{
					visitor(*reinterpret_cast<const int16*>(m_data));
					break;
				}
			case UINT16:
				{
					visitor(*reinterpret_cast<const uint16*>(m_data));
					break;
				}
			case INT32:
				{
					visitor(*reinterpret_cast<const int32*>(m_data));
					break;
				}
			case UINT32:
				{
					visitor(*reinterpret_cast<const uint32*>(m_data));
					break;
				}
			case INT64:
				{
					visitor(*reinterpret_cast<const int64*>(m_data));
					break;
				}
			case UINT64:
				{
					visitor(*reinterpret_cast<const uint64*>(m_data));
					break;
				}
			case FLOAT:
				{
					visitor(*reinterpret_cast<const float*>(m_data));
					break;
				}
			case CONTAINER:
				{
					visitor(*reinterpret_cast<const CVariantContainer*>(m_data));
					break;
				}
			/*case BLOCK_HEADER:
				{
					visitor(*reinterpret_cast<const SVariantBlockHeader*>(m_data));
					break;
				}*/
			/*case ARRAY_HEADER:
				{
					visitor(*reinterpret_cast<const SVariantArrayHeader*>(m_data));
					break;
				}*/
			case STRING:
				{
					visitor(m_data);
					break;
				}
			case POOL_STRING:
				{
					visitor(reinterpret_cast<const CPoolString*>(m_data)->c_str());
					break;
				}
			}
		}

		inline CVariant& operator = (bool value)
		{
			Release();
			*reinterpret_cast<bool*>(m_data) = value;
			m_typeId                         = BOOL;
			return *this;
		}

		inline CVariant& operator = (int8 value)
		{
			Release();
			*reinterpret_cast<int8*>(m_data) = value;
			m_typeId                         = INT8;
			return *this;
		}

		inline CVariant& operator = (uint8 value)
		{
			Release();
			*reinterpret_cast<uint8*>(m_data) = value;
			m_typeId                          = UINT8;
			return *this;
		}

		inline CVariant& operator = (int16 value)
		{
			Release();
			*reinterpret_cast<int16*>(m_data) = value;
			m_typeId                          = INT16;
			return *this;
		}

		inline CVariant& operator = (uint16 value)
		{
			Release();
			*reinterpret_cast<uint16*>(m_data) = value;
			m_typeId                           = UINT16;
			return *this;
		}

		inline CVariant& operator = (int32 value)
		{
			Release();
			*reinterpret_cast<int32*>(m_data) = value;
			m_typeId                          = INT32;
			return *this;
		}

		inline CVariant& operator = (uint32 value)
		{
			Release();
			*reinterpret_cast<uint32*>(m_data) = value;
			m_typeId                           = UINT32;
			return *this;
		}

		inline CVariant& operator = (int64 value)
		{
			Release();
			*reinterpret_cast<int64*>(m_data) = value;
			m_typeId                          = INT64;
			return *this;
		}

		inline CVariant& operator = (uint64 value)
		{
			Release();
			*reinterpret_cast<uint64*>(m_data) = value;
			m_typeId                           = UINT64;
			return *this;
		}

		inline CVariant& operator = (float value)
		{
			Release();
			*reinterpret_cast<float*>(m_data) = value;
			m_typeId                          = FLOAT;
			return *this;
		}

		inline CVariant& operator = (const char* value)
		{
			Release();
			StoreString(value);
			return *this;
		}

		inline CVariant& operator = (const CVariant& rhs)
		{
			Release();
			Copy(rhs);
			return *this;
		}

		inline bool operator == (const CVariant& rhs) const
		{
			if(m_typeId == rhs.m_typeId)
			{
				switch(m_typeId)
				{
				case NIL:
					{
						return true;
					}
				case BOOL:
					{
						return *reinterpret_cast<const bool*>(m_data) == *reinterpret_cast<const bool*>(rhs.m_data);
					}
				case INT8:
					{
						return *reinterpret_cast<const int8*>(m_data) == *reinterpret_cast<const int8*>(rhs.m_data);
					}
				case UINT8:
					{
						return *reinterpret_cast<const uint8*>(m_data) == *reinterpret_cast<const uint8*>(rhs.m_data);
					}
				case INT16:
					{
						return *reinterpret_cast<const int16*>(m_data) == *reinterpret_cast<const int16*>(rhs.m_data);
					}
				case UINT16:
					{
						return *reinterpret_cast<const uint16*>(m_data) == *reinterpret_cast<const uint16*>(rhs.m_data);
					}
				case INT32:
					{
						return *reinterpret_cast<const int32*>(m_data) == *reinterpret_cast<const int32*>(rhs.m_data);
					}
				case UINT32:
					{
						return *reinterpret_cast<const uint32*>(m_data) == *reinterpret_cast<const uint32*>(rhs.m_data);
					}
				case INT64:
					{
						return *reinterpret_cast<const int64*>(m_data) == *reinterpret_cast<const int64*>(rhs.m_data);
					}
				case UINT64:
					{
						return *reinterpret_cast<const uint64*>(m_data) == *reinterpret_cast<const uint64*>(rhs.m_data);
					}
				case FLOAT:
					{
						return *reinterpret_cast<const float*>(m_data) == *reinterpret_cast<const float*>(rhs.m_data);
					}
				case CONTAINER:
					{
						return *reinterpret_cast<const CVariantContainer*>(m_data) == *reinterpret_cast<const CVariantContainer*>(rhs.m_data);
					}
				/*case BLOCK_HEADER:
					{
						return *reinterpret_cast<const SVariantBlockHeader*>(m_data) == *reinterpret_cast<const SVariantBlockHeader*>(rhs.m_data);
					}*/
				/*case ARRAY_HEADER:
					{
						return *reinterpret_cast<const SVariantArrayHeader*>(m_data) == *reinterpret_cast<const SVariantArrayHeader*>(rhs.m_data);
					}*/
				case STRING:
					{
						return strcmp(m_data, rhs.m_data) == 0;
					}
				case POOL_STRING:
					{
						return *reinterpret_cast<const CPoolString*>(m_data) == *reinterpret_cast<const CPoolString*>(rhs.m_data);
					}
				}
			}
			return false;
		}

		inline bool operator != (const CVariant& rhs) const
		{
			if(m_typeId == rhs.m_typeId)
			{
				switch(m_typeId)
				{
				case NIL:
					{
						return false;
					}
				case BOOL:
					{
						return *reinterpret_cast<const bool*>(m_data) != *reinterpret_cast<const bool*>(rhs.m_data);
					}
				case INT8:
					{
						return *reinterpret_cast<const int8*>(m_data) != *reinterpret_cast<const int8*>(rhs.m_data);
					}
				case UINT8:
					{
						return *reinterpret_cast<const uint8*>(m_data) != *reinterpret_cast<const uint8*>(rhs.m_data);
					}
				case INT16:
					{
						return *reinterpret_cast<const int16*>(m_data) != *reinterpret_cast<const int16*>(rhs.m_data);
					}
				case UINT16:
					{
						return *reinterpret_cast<const uint16*>(m_data) != *reinterpret_cast<const uint16*>(rhs.m_data);
					}
				case INT32:
					{
						return *reinterpret_cast<const int32*>(m_data) != *reinterpret_cast<const int32*>(rhs.m_data);
					}
				case UINT32:
					{
						return *reinterpret_cast<const uint32*>(m_data) != *reinterpret_cast<const uint32*>(rhs.m_data);
					}
				case INT64:
					{
						return *reinterpret_cast<const int64*>(m_data) != *reinterpret_cast<const int64*>(rhs.m_data);
					}
				case UINT64:
					{
						return *reinterpret_cast<const uint64*>(m_data) != *reinterpret_cast<const uint64*>(rhs.m_data);
					}
				case FLOAT:
					{
						return *reinterpret_cast<const float*>(m_data) != *reinterpret_cast<const float*>(rhs.m_data);
					}
				case CONTAINER:
					{
						return *reinterpret_cast<const CVariantContainer*>(m_data) != *reinterpret_cast<const CVariantContainer*>(rhs.m_data);
					}
				/*case BLOCK_HEADER:
					{
						return *reinterpret_cast<const SVariantBlockHeader*>(m_data) != *reinterpret_cast<const SVariantBlockHeader*>(rhs.m_data);
					}*/
				/*case ARRAY_HEADER:
					{
						return *reinterpret_cast<const SVariantArrayHeader*>(m_data) != *reinterpret_cast<const SVariantArrayHeader*>(rhs.m_data);
					}*/
				case STRING:
					{
						return strcmp(m_data, rhs.m_data) != 0;
					}
				case POOL_STRING:
					{
						return *reinterpret_cast<const CPoolString*>(m_data) != *reinterpret_cast<const CPoolString*>(rhs.m_data);
					}
				}
			}
			return true;
		}

	private:

		inline void StoreString(const char* szString)
		{
			const size_t length = szString ? strlen(szString) : 0;
			if(length < sizeof(m_data))
			{
				if(length > 0)
				{
					cry_strcpy(m_data, szString);
				}
				else
				{
					m_data[0] = '\0';
				}
				m_typeId = STRING;
			}
			else
			{
				static_assert(sizeof(CPoolString) <= sizeof(m_data), "Type doesn't fit into the buffer");
				new (m_data) CPoolString(szString);
				m_typeId = POOL_STRING;
			}
		}

		inline void Copy(const CVariant& rhs)
		{
			switch(rhs.m_typeId)
			{
			case BOOL:
				{
					*reinterpret_cast<bool*>(m_data) = *reinterpret_cast<const bool*>(rhs.m_data);
					break;
				}
			case INT8:
				{
					*reinterpret_cast<int8*>(m_data) = *reinterpret_cast<const int8*>(rhs.m_data);
					break;
				}
			case UINT8:
				{
					*reinterpret_cast<uint8*>(m_data) = *reinterpret_cast<const uint8*>(rhs.m_data);
					break;
				}
			case INT16:
				{
					*reinterpret_cast<int16*>(m_data) = *reinterpret_cast<const int16*>(rhs.m_data);
					break;
				}
			case UINT16:
				{
					*reinterpret_cast<uint16*>(m_data) = *reinterpret_cast<const uint16*>(rhs.m_data);
					break;
				}
			case INT32:
				{
					*reinterpret_cast<int32*>(m_data) = *reinterpret_cast<const int32*>(rhs.m_data);
					break;
				}
			case UINT32:
				{
					*reinterpret_cast<uint32*>(m_data) = *reinterpret_cast<const uint32*>(rhs.m_data);
					break;
				}
			case INT64:
				{
					*reinterpret_cast<int64*>(m_data) = *reinterpret_cast<const int64*>(rhs.m_data);
					break;
				}
			case UINT64:
				{
					*reinterpret_cast<uint64*>(m_data) = *reinterpret_cast<const uint64*>(rhs.m_data);
					break;
				}
			case FLOAT:
				{
					*reinterpret_cast<float*>(m_data) = *reinterpret_cast<const float*>(rhs.m_data);
					break;
				}
			case CONTAINER:
				{
					new (m_data) CVariantContainer(*reinterpret_cast<const CVariantContainer*>(rhs.m_data));
					break;
				}
			/*case BLOCK_HEADER:
				{
					new (m_data) SVariantBlockHeader(*reinterpret_cast<const SVariantBlockHeader*>(rhs.m_data));
					break;
				}*/
			/*case ARRAY_HEADER:
				{
					new (m_data) SVariantArrayHeader(*reinterpret_cast<const SVariantArrayHeader*>(rhs.m_data));
					break;
				}*/
			case STRING:
				{
					cry_strcpy(m_data, rhs.m_data);
					break;
				}
			case POOL_STRING:
				{
					new (m_data) CPoolString(*reinterpret_cast<const CPoolString*>(rhs.m_data));
					break;
				}
			}
			m_typeId = rhs.m_typeId;
		}

		inline void Release()
		{
			switch(m_typeId)
			{
			case CONTAINER:
				{
					reinterpret_cast<CVariantContainer*>(m_data)->~CVariantContainer();
					break;
				}
			case POOL_STRING:
				{
					reinterpret_cast<CPoolString*>(m_data)->~CPoolString();
					break;
				}
			}
		}

		char  m_data[11];
		uint8 m_typeId;
	};

	inline bool operator == (const VariantDynArray& lhs, const VariantDynArray& rhs)
	{
		const size_t	size = lhs.size();
		if(size != rhs.size())
		{
			return false;
		}
		for(size_t i = 0; i < size; ++ i)
		{
			if(lhs[i] != rhs[i])
			{
				return false;
			}
		}
		return true;
	}

	inline bool operator != (const VariantDynArray& lhs, const VariantDynArray& rhs)
	{
		return !(lhs == rhs);
	}

	inline bool CVariantContainer::RemoveByIndex(const size_t index)
	{
		const size_t size = m_data.size();
		const size_t stride = 1;
		if (index < size)
		{
			m_data.erase(m_data.begin() + index, m_data.begin() + index + stride);
			return true;
		}
		return false;
	}

	inline bool CVariantContainer::RemoveByValue(const TVariantConstArray& value)
	{
		size_t       size = m_data.size();
		const size_t stride = value.size();
		size_t       count = 0;
		//CRY_ASSERT((size >= stride) && ((size % stride) == 0));
		if((size >= stride) && ((size % stride) == 0))
		{
			for(size_t begin = 0; begin < size; )
			{
				size_t pos = 0;
				for( ; pos < stride; ++ pos)
				{
					if(m_data[begin + pos] != value[pos])
					{
						break;
					}
				}
				if(pos == stride)
				{
					m_data.erase(m_data.begin() + begin, m_data.begin() + begin + stride);
					size -= stride;
					++ count;
				}
				else
				{
					begin += stride;
				}
			}
		}
		return count > 0;
	}

	inline size_t CVariantContainer::FindByValue(const CVariant& value) const
	{
		VariantDynArray::const_iterator iter = std::find_if(m_data.begin(), m_data.end(), [&value](const CVariant& item) {return value == item; });
		if (iter == m_data.end())
		{
			return npos;
		}
		else
		{
			return (iter - m_data.begin());
		}
	}

	inline void CVariantContainer::Clear()
	{
		m_data.clear();
	}

	class CVariantArrayInputArchive : public Serialization::IArchive
	{
	public:

		inline CVariantArrayInputArchive(const TVariantConstArray& inputs)
			: Serialization::IArchive(Serialization::IArchive::INPUT | Serialization::IArchive::INPLACE)
			, m_inputs(inputs)
			, m_pos(0)
		{}

		// Serialization::IArchive

		virtual bool operator () (bool& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if(m_pos < m_inputs.size())
			{
				value = m_inputs[m_pos].AsBool();
				++ m_pos;
			}
			return true;
		}

		// note: not implemented
		//virtual bool operator () (char& value, const char* szName = "", const char* szLabel = nullptr) override;

		virtual bool operator () (int8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if(m_pos < m_inputs.size())
			{
				value = m_inputs[m_pos].AsInt8();
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (uint8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if(m_pos < m_inputs.size())
			{
				value = m_inputs[m_pos].AsUInt8();
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (int16& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if (m_pos < m_inputs.size())
			{
				value = m_inputs[m_pos].AsInt16();
				++m_pos;
			}
			return true;
		}

		virtual bool operator () (uint16& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if (m_pos < m_inputs.size())
			{
				value = m_inputs[m_pos].AsUInt16();
				++m_pos;
			}
			return true;
		}

		virtual bool operator () (int32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if(m_pos < m_inputs.size())
			{
				value = m_inputs[m_pos].AsInt32();
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (uint32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if(m_pos < m_inputs.size())
			{
				value = m_inputs[m_pos].AsUInt32();
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (int64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if(m_pos < m_inputs.size())
			{
				value = m_inputs[m_pos].AsInt64();
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (uint64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if(m_pos < m_inputs.size())
			{
				value = m_inputs[m_pos].AsUInt64();
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (float& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if(m_pos < m_inputs.size())
			{
				value = m_inputs[m_pos].AsFloat();
				++ m_pos;
			}
			return true;
		}

		// note: not implemented
		//virtual bool operator () (double& value, const char* szName = "", const char* szLabel = nullptr) override;

		virtual bool operator () (Serialization::IString& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_inputs.size());
			if(m_pos < m_inputs.size())
			{
				value.set(m_inputs[m_pos].c_str());
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (const Serialization::SStruct& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			value(*this);
			return true;
		}

		virtual bool operator() (Serialization::IContainer& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			if (value.isFixedSize())
			{
				if (value.size())
				{
					do
					{
						value(*this, "", nullptr);
					} while (value.next());
				}
				return true;
			}
			else
			{
				CryFatalError("Serialization of dynamic containers is not yet supported!");
				return false;
			}
		}

		using Serialization::IArchive::operator ();

		// ~Serialization::IArchive

		inline void operator () (const char*& value)
		{
			value = m_inputs[m_pos].c_str();
			++ m_pos;
		}

	private:

		TVariantConstArray m_inputs;
		size_t             m_pos;
	};

	class CVariantArrayOutputArchive : public Serialization::IArchive
	{
	public:

		inline CVariantArrayOutputArchive(const TVariantArray& outputs)
			: Serialization::IArchive(Serialization::IArchive::OUTPUT | Serialization::IArchive::INPLACE)
			, m_outputs(outputs)
			, m_pos(0)
		{}

		// Serialization::IArchive

		virtual bool operator () (bool& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if(m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value;
				++ m_pos;
			}
			return true;
		}

		// note: not implemented
		//virtual bool operator () (char& value, const char* szName = "", const char* szLabel = nullptr) override;

		virtual bool operator () (int8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if(m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value;
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (uint8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if(m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value;
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (int16& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if (m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value;
				++m_pos;
			}
			return true;
		}

		virtual bool operator () (uint16& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if (m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value;
				++m_pos;
			}
			return true;
		}

		virtual bool operator () (int32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if(m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value;
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (uint32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if(m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value;
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (int64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if(m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value;
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (uint64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if(m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value;
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (float& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if(m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value;
				++ m_pos;
			}
			return true;
		}

		// note: not implemented
		//virtual bool operator () (double& value, const char* szName = "", const char* szLabel = nullptr) override;

		virtual bool operator () (Serialization::IString& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			CRY_ASSERT(m_pos < m_outputs.size());
			if(m_pos < m_outputs.size())
			{
				m_outputs[m_pos] = value.get();
				++ m_pos;
			}
			return true;
		}

		virtual bool operator () (const Serialization::SStruct& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			value(*this);
			return true;
		}

		virtual bool operator () (Serialization::IContainer& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			if(value.isFixedSize())
			{
				if(value.size())
				{
					do
					{
						value(*this, "", nullptr);
					} while(value.next());
				}
				return true;
			}
			else
			{
				CryFatalError("Serialization of dynamic containers is not yet supported!");
				return false;
			}
		}

		using Serialization::IArchive::operator ();

		// ~Serialization::IArchive

		inline void operator () (const char*& value)
		{
			m_outputs[m_pos] = value;
			++ m_pos;
		}

		inline size_t GetPos() const
		{
			return m_pos;
		}

	private:

		TVariantArray m_outputs;
		size_t        m_pos;
	};

	class CVariantVectorOutputArchive : public Serialization::IArchive
	{
	public:

		inline CVariantVectorOutputArchive(TVariantVector& outputs)
			: Serialization::IArchive(Serialization::IArchive::OUTPUT | Serialization::IArchive::INPLACE)
			, m_outputs(outputs)
		{}

		// Serialization::IArchive

		virtual bool operator () (bool& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value);
			return true;
		}

		// note: not implemented
		//virtual bool operator () (char& value, const char* szName = "", const char* szLabel = nullptr) override;

		virtual bool operator () (int8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value);
			return true;
		}

		virtual bool operator () (uint8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value);
			return true;
		}

		virtual bool operator () (int16& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value);
			return true;
		}

		virtual bool operator () (uint16& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value);
			return true;
		}

		virtual bool operator () (int32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value);
			return true;
		}

		virtual bool operator () (uint32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value);
			return true;
		}

		virtual bool operator () (int64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value);
			return true;
		}

		virtual bool operator () (uint64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value);
			return true;
		}

		virtual bool operator () (float& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value);
			return true;
		}

		// note: not implemented
		//virtual bool operator () (double& value, const char* szName = "", const char* szLabel = nullptr) override;

		virtual bool operator () (Serialization::IString& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			m_outputs.push_back(value.get());
			return true;
		}

		virtual bool operator () (const Serialization::SStruct& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			value(*this);
			return true;
		}

		virtual bool operator() (Serialization::IContainer& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			if(value.isFixedSize())
			{
				if(value.size())
				{
					do
					{
						value(*this, "", nullptr);
					} while(value.next());
				}
				return true;
			}
			else
			{
				CryFatalError("Serialization of dynamic containers is not yet supported!");
				return false;
			}
		}

		using Serialization::IArchive::operator ();

		// ~Serialization::IArchive

		inline void operator () (const char*& value)
		{
			m_outputs.push_back(value);
		}

	private:

		TVariantVector& m_outputs;
	};

	namespace StringUtils
	{
		// Helper structure for reading variant from string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		struct SVariantFromString
		{
			inline SVariantFromString(const char* _szInput)
				: szInput(_szInput)
			{}

			inline void operator () (bool &output)
			{
				output = BoolFromString(szInput, false);
			}

			inline void operator () (int32 &output)
			{
				output = Int32FromString(szInput, 0);
			}

			inline void operator () (uint32 &output)
			{
				output = UInt32FromString(szInput, 0);
			}

			inline void operator () (float &output)
			{
				output = FloatFromString(szInput, 0.0f);
			}

			inline void operator () (CPoolString& output)
			{
				output = szInput;
			}

			inline void operator () (ExplicitEntityId& output)
			{
				output = ExplicitEntityId(UInt32FromString(szInput, 0));
			}

			inline void operator () (SGUID& output)
			{
				output.sysGUID = SysGUIDFromString(szInput);
			}

			const char* szInput;
		};

		// Helper structure for writing variant to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		struct SVariantToString_CharArray
		{
			inline SVariantToString_CharArray(const CharArrayView& _output)
				: output(_output)
			{}

			inline void operator () (bool input)
			{
				BoolToString(input, output);
			}

			inline void operator () (int8 input)
			{
				Int8ToString(input, output);
			}

			inline void operator () (uint8 input)
			{
				UInt8ToString(input, output);
			}

			inline void operator () (int16 input)
			{
				Int16ToString(input, output);
			}

			inline void operator () (uint16 input)
			{
				UInt16ToString(input, output);
			}

			inline void operator () (int32 input)
			{
				Int32ToString(input, output);
			}

			inline void operator () (uint32 input)
			{
				UInt32ToString(input, output);
			}

			inline void operator () (int64 input)
			{
				Int64ToString(input, output);
			}

			inline void operator () (uint64 input)
			{
				UInt64ToString(input, output);
			}

			inline void operator () (float input)
			{
				FloatToString(input, output);
			}

			inline void operator () (const CVariantContainer& input) {}

			inline void operator () (const char* input)
			{
				Copy(input, output);
			}

			const CharArrayView&	output;
		};

		// Write variant to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline char* VariantToString(const CVariant& input, const CharArrayView& output)
		{
			SVariantToString_CharArray visitor(output);
			input.Visit(visitor);
			return output.begin();
		}

		// Helper structure for writing variant to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		struct SVariantToString_StackString
		{
			SVariantToString_StackString(stack_string &_output)
				: output(_output)
			{}

			inline void operator () (bool input)
			{
				BoolToString(input, output);
			}

			inline void operator () (int8 input)
			{
				Int8ToString(input, output);
			}

			inline void operator () (uint8 input)
			{
				UInt8ToString(input, output);
			}

			inline void operator () (int16 input)
			{
				Int16ToString(input, output);
			}

			inline void operator () (uint16 input)
			{
				UInt16ToString(input, output);
			}

			inline void operator () (int32 input)
			{
				Int32ToString(input, output);
			}

			inline void operator () (uint32 input)
			{
				UInt32ToString(input, output);
			}

			inline void operator () (int64 input)
			{
				Int64ToString(input, output);
			}

			inline void operator () (uint64 input)
			{
				UInt64ToString(input, output);
			}

			inline void operator () (float input)
			{
				FloatToString(input, output);
			}

			inline void operator () (const CVariantContainer& input) {}

			inline void operator () (const char* input)
			{
				output = input;
			}

			stack_string& output;
		};

		// Write variant to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* VariantToString(const CVariant& input, stack_string& output)
		{
			output.clear();
			SVariantToString_StackString visitor(output);
			input.Visit(visitor);
			return output.c_str();
		}

		// Helper structure for writing variant to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		struct SVariantToString_String
		{
			SVariantToString_String(string &_output)
				: output(_output)
			{}

			inline void operator () (bool input)
			{
				BoolToString(input, output);
			}

			inline void operator () (int8 input)
			{
				Int8ToString(input, output);
			}

			inline void operator () (uint8 input)
			{
				UInt8ToString(input, output);
			}

			inline void operator () (int16 input)
			{
				Int16ToString(input, output);
			}

			inline void operator () (uint16 input)
			{
				UInt16ToString(input, output);
			}

			inline void operator () (int32 input)
			{
				Int32ToString(input, output);
			}

			inline void operator () (uint32 input)
			{
				UInt32ToString(input, output);
			}

			inline void operator () (int64 input)
			{
				Int64ToString(input, output);
			}

			inline void operator () (uint64 input)
			{
				UInt64ToString(input, output);
			}

			inline void operator () (float input)
			{
				FloatToString(input, output);
			}

			inline void operator () (const CVariantContainer& input) {}

			inline void operator () (const char* input)
			{
				output = input;
			}

			string& output;
		};

		// Write variant to string.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		inline const char* VariantToString(const CVariant& input, string& output)
		{
			output.clear();
			SVariantToString_String visitor(output);
			input.Visit(visitor);
			return output.c_str();
		}
	}
}
