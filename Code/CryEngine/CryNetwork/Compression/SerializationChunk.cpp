// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/CryCustomTypes.h>
#include "SerializationChunk.h"
#include "Streams/ByteStream.h"
#include "Protocol/Serialize.h"
#include "NetProfile.h"
#if ENABLE_DEBUG_KIT
	#include <CryEntitySystem/IEntitySystem.h>
#endif // ENABLE_DEBUG_KIT

#if ENABLE_SERIALIZATION_LOGGING
	#include <CryEntitySystem/IEntitySystem.h>
	#include <CryEntitySystem/IEntity.h>
	#include <CryEntitySystem/IEntityClass.h>
#endif
/*
 * helper routines
 */

#if TRACK_ENCODING
	#define NAME_FROM_OP(pOp) (pOp)->name.c_str()
#else
	#define NAME_FROM_OP(pOp) ""
#endif

#if ENABLE_CORRUPT_PACKET_DUMP
static char* eOpNames[] =
{
	"eO_NoOp",
	#define SERIALIZATION_TYPE(T) "eO_" # T,
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE
	"eO_String",
	"eO_OptionalGroup"
};
#endif

namespace
{
const int NUM_BITS_FOR_OPS_SIZE = 16;
const int16 INVALID_SKIP_FALSE = 0x7FFF;

// template helpers to copy a value to or from a buffer
template<class T>
ILINE void CopyToBuffer(const T& value, CByteOutputStream& s)
{
	s.PutTyped<T>() = value;
}

ILINE void CopyToBuffer(const SSerializeString& value, CByteOutputStream& s)
{
	uint32 len = value.size();
	s.PutTyped<uint32>() = len;
	s.Put(value.c_str(), value.length());
}

template<class T>
ILINE void CopyFromBuffer(T& value, CByteInputStream& s)
{
	value = s.GetTyped<T>();
}

ILINE void CopyFromBuffer(SSerializeString& value, CByteInputStream& s)
{
	uint32 len = s.GetTyped<uint32>();
	const char* pBuffer = (const char*) s.Get(len);
	value = SSerializeString(pBuffer, pBuffer + len);
}

// generator template to create compress/decompress to byte buffer functions
template<class T>
struct GenImpl
{
	static void DecodeFromStream(const char* name, CNetInputSerializeImpl& input, CByteOutputStream& output, ICompressionPolicyPtr pPolicy)
	{
		T temp;
		input.ValueImpl(name, temp, pPolicy);
		CopyToBuffer(temp, output);
	}
	static void EncodeToStream(const char* name, CByteInputStream& input, CNetOutputSerializeImpl& output, ICompressionPolicyPtr pPolicy)
	{
		T temp;
		CopyFromBuffer(temp, input);
		output.ValueImpl(name, temp, pPolicy);
	}
};

typedef void (* DecodeFromStreamFunction)(const char* name, CNetInputSerializeImpl& input, CByteOutputStream& output, ICompressionPolicyPtr pPolicy);
typedef void (* EncodeToStreamFunction)(const char* name, CByteInputStream& input, CNetOutputSerializeImpl& output, ICompressionPolicyPtr pPolicy);

// build tables of serialization functions for primitive types
DecodeFromStreamFunction DecodeFromStreamImpl[] = {
#define SERIALIZATION_TYPE(T) GenImpl<T>::DecodeFromStream,
#include <CryNetwork/SerializationTypes.h>
#undef SERIALIZATION_TYPE
};
EncodeToStreamFunction EncodeToStreamImpl[] = {
#define SERIALIZATION_TYPE(T) GenImpl<T>::EncodeToStream,
#include <CryNetwork/SerializationTypes.h>
#undef SERIALIZATION_TYPE
};

#if ENABLE_DEBUG_KIT
const char* EntryNames[] = {
	#define SERIALIZATION_TYPE(T) # T,
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE
};
#endif

}

/*
 * CToBufferImpl - collect data from the game, place it in a byte buffer
 */

class CSerializationChunk::CToBufferImpl : public CSimpleSerializeImpl<false, eST_Network>
{
public:
	CToBufferImpl(CByteOutputStream& out, const CSerializationChunk* pChunk) : m_out(out), m_op(0), m_pChunk(pChunk)
	{
		NET_ASSERT(m_pChunk);
	}
	~CToBufferImpl()
	{
	}
	bool Complete()
	{
		if (!m_pChunk)
			return false;
		if (m_op != m_pChunk->m_ops.size())
			return false;
		return true;
	}

	template<class T>
	void Value(const char* name, T& value, uint32 policy)
	{
		NET_ASSERT(m_op < m_pChunk->m_ops.size());
		SOp op = m_pChunk->m_ops[m_op];
#if TRACK_ENCODING
		NET_ASSERT(0 == strcmp(name, op.name.c_str()));
#endif
		NET_ASSERT(TypeToId<T>::type == op.type);
		NET_ASSERT(m_pChunk->m_overrideCompression || (CNetwork::Get()->GetCompressionManager().GetCompressionPolicy(policy) == op.pPolicy));
#if NET_PROFILE_ENABLE
		if (op.pPolicy)
		{
			NET_PROFILE_ADD_WRITE_BITS(op.pPolicy->GetBitCount(value));
		}
#endif
#if ENABLE_SERIALIZATION_LOGGING
		if (GetSerializationTarget() == eST_Network)
		{
			LogSerialize("Game", name, value, op.pPolicy, op.pPolicy->GetBitCount(value));
		}
#endif // ENABLE_SERIALIZATION_LOGGING
		CopyToBuffer(value, m_out);
		m_op++;
	}

	template<class T>
	void Value(const char* name, T& value)
	{
		Value(name, value, 0);
	}

	void Value(const char* name, SSerializeString& value, uint32 policy)
	{
		NET_ASSERT(m_op < m_pChunk->m_ops.size());
		SOp op = m_pChunk->m_ops[m_op];
#if TRACK_ENCODING
		NET_ASSERT(0 == strcmp(name, op.name.c_str()));
#endif
		NET_ASSERT(eO_String == op.type);
#if NET_PROFILE_ENABLE
		NET_PROFILE_ADD_WRITE_BITS(op.pPolicy->GetBitCount(value));
#endif
		CopyToBuffer(value, m_out);
		m_op++;
	}

	bool BeginGroup(const char* szName)
	{
		return true;
	}

	bool BeginOptionalGroup(const char* szName, bool cond)
	{
		SOp op = m_pChunk->m_ops[m_op];
#if TRACK_ENCODING
		NET_ASSERT(0 == strcmp(szName, op.name.c_str()));
#endif
		NET_ASSERT(op.skipFalse != INVALID_SKIP_FALSE && op.skipFalse >= 0);
		if (op.skipFalse) // optimization - skip empty groups always
		{
#if NET_PROFILE_ENABLE
			m_out.ConditionalPrelude();
#endif
			m_out.PutByte(cond);
			if (!cond)
				m_op += op.skipFalse;
			m_op++;
#if NET_PROFILE_ENABLE
			m_out.ConditionalPostlude();
#endif
			return cond;
		}
		else
		{
			return false;
		}
	}

	void EndGroup()
	{
	}

private:
	CByteOutputStream&         m_out;
	size_t                     m_op;
	const CSerializationChunk* m_pChunk;
};

/*
 * CFromBufferImpl - take a byte buffer and feed it into the game
 */

class CSerializationChunk::CFromBufferImpl : public CSimpleSerializeImpl<true, eST_Network>
{
public:
	CFromBufferImpl(CByteInputStream& in, const CSerializationChunk* pChunk) : m_in(in), m_op(0), m_pChunk(pChunk), m_partial(false) {}
	~CFromBufferImpl()
	{
	}

	bool Complete(bool mustHaveFinished)
	{
		if (mustHaveFinished)
		{
			if (!m_pChunk)
				return false;
			if (m_op != m_pChunk->m_ops.size())
				return false;
		}
		return true;
	}

	void FlagPartialRead()
	{
		m_partial = true;
	}

	bool WasPartial()
	{
		return m_partial;
	}

	template<class T>
	void Value(const char* name, T& value, uint32 policy)
	{
		SOp op = m_pChunk->m_ops[m_op];
#if TRACK_ENCODING
		NET_ASSERT(0 == strcmp(name, op.name.c_str()));
#endif
		NET_ASSERT(TypeToId<T>::type == op.type);
		NET_ASSERT(m_pChunk->m_overrideCompression || (CNetwork::Get()->GetCompressionManager().GetCompressionPolicy(policy) == op.pPolicy));
#if NET_PROFILE_ENABLE
		if (op.pPolicy)
		{
			NET_PROFILE_ADD_READ_BITS(op.pPolicy->GetBitCount(value));
		}
#endif
		CopyFromBuffer(value, m_in);
		m_op++;
	}

	template<class T>
	void Value(const char* name, T& value)
	{
		Value(name, value, 0);
	}

	void Value(const char* name, SSerializeString& value, uint32 policy)
	{
		SOp op = m_pChunk->m_ops[m_op];
#if TRACK_ENCODING
		NET_ASSERT(0 == strcmp(name, op.name.c_str()));
#endif
		NET_ASSERT(eO_String == op.type);
#if NET_PROFILE_ENABLE
		NET_PROFILE_ADD_READ_BITS(op.pPolicy->GetBitCount(value));
#endif
		CopyFromBuffer(value, m_in);
		m_op++;
	}

	bool BeginGroup(const char* szName)
	{
		return true;
	}

	bool BeginOptionalGroup(const char* szName, bool cond)
	{
#if TRACK_ENCODING
		NET_ASSERT(0 == strcmp(szName, m_pChunk->m_ops[m_op].name.c_str()));
#endif
		const uint16 skipFalse = m_pChunk->m_ops[m_op].skipFalse;
		NET_ASSERT(m_pChunk->m_ops[m_op].skipFalse >= 0);
		NET_ASSERT(skipFalse != INVALID_SKIP_FALSE);
		if (skipFalse) // optimization: skip empty chunks always
		{
#if NET_PROFILE_ENABLE
			m_in.ConditionalPrelude();
#endif
			cond = m_in.GetByte() != 0;
			if (!cond)
				m_op += skipFalse;
			m_op++;
#if NET_PROFILE_ENABLE
			m_in.ConditionalPostlude();
#endif
			return cond;
		}
		else
		{
			return false;
		}
	}

	void EndGroup()
	{
	}

private:
	CByteInputStream&          m_in;
	size_t                     m_op;
	const CSerializationChunk* m_pChunk;
	bool                       m_partial;
};

/*
 * CBuildImpl - doesn't serialize anything, but remembers the operations performed to be replayed later
 */

class CSerializationChunk::CBuildImpl : public CSimpleSerializeImpl<false, eST_Network>
{
public:
	CBuildImpl(CSerializationChunk* pChunk) : m_pChunk(pChunk) {}

	template<class T>
	void Value(const char* name, T& value, uint32 policy)
	{
		AddValue(name, TypeToId<T>::type, policy);
	}

	void Value(const char* name, SSerializeString& value, uint32 policy)
	{
		AddValue(name, eO_String, 0);
	}

	template<class T>
	void Value(const char* name, T& value)
	{
		Value(name, value, 0);
	}

	bool BeginGroup(const char* szName)
	{
		m_inOptionalGroup.push_back(0);
		return true;
	}

	bool BeginOptionalGroup(const char* szName, bool cond)
	{
		m_inOptionalGroup.push_back(1);
		m_resolveConditions.push_back(static_cast<uint16>(m_pChunk->m_ops.size()));
		AddValue(szName, eO_OptionalGroup, 0);
		return true;
	}

	void EndGroup()
	{
		NET_ASSERT(!m_inOptionalGroup.empty());
		if (m_inOptionalGroup.back())
		{
			NET_ASSERT(!m_resolveConditions.empty());
			SOp& op = m_pChunk->m_ops[m_resolveConditions.back()];
			NET_ASSERT(op.type == eO_OptionalGroup);

			op.skipFalse = static_cast<int16>(m_pChunk->m_ops.size() - m_resolveConditions.back());
			op.skipFalse--;  // we always increment after skipping
			NET_ASSERT(op.skipFalse >= 0);
			m_resolveConditions.pop_back();
		}
		m_inOptionalGroup.pop_back();
	}

	bool Complete()
	{
#if CRC8_ASPECT_FORMAT
		m_pChunk->m_crc = m_crc.Result();
#endif
		return true;
	}

private:
	void AddValue(const char* name, EOps type, uint32 policy)
	{
		SOp op;
		op.type = type;
		op.skipFalse = INVALID_SKIP_FALSE; // should never be used, but somewhat guarantees a crash if it is (which is a good thing!)
		if (m_pChunk->m_overrideCompression)
		{
			policy = 0;
		}
		op.pPolicy = CNetwork::Get()->GetCompressionManager().GetCompressionPolicy(policy);
#if TRACK_ENCODING
		op.name = name;
#endif
#if CRC8_ASPECT_FORMAT
		m_crc.Add((uint8)type);
		m_crc.Add32(policy);
#endif
		m_pChunk->m_ops.push_back(op);

		NET_ASSERT(m_pChunk->m_ops.size() < (1 << NUM_BITS_FOR_OPS_SIZE));
	}

	CSerializationChunk* m_pChunk;
	// a stack of offsets for each optional group we've entered indicating the start point of that optional group
	std::vector<uint16>  m_resolveConditions;
	// a stack of 0 or 1 indicating whether the group we're currently in is optional or not
	std::vector<uint8>   m_inOptionalGroup;
#if CRC8_ASPECT_FORMAT
	CCRC8                m_crc;
#endif
};

/*
 * CSerializationChunk main
 */

CSerializationChunk::CSerializationChunk()
{
	m_overrideCompression = false;
	++g_objcnt.serializationChunk;
}

CSerializationChunk::~CSerializationChunk()
{
	--g_objcnt.serializationChunk;
}

void CSerializationChunk::Reset()
{
	m_overrideCompression = false;
	m_ops.clear();
#if CRC8_ASPECT_FORMAT
	m_crc = 0;
#endif
}

void CSerializationChunk::DecodeFromStream(CNetInputSerializeImpl& input, CByteOutputStream& output, ChunkID chunkID, uint8 nProfile) const
{
	// when should we call EndGroup()
	// TODO: this probably isn't necessary under the current implementation... maybe it could be removed and save a few cycles on serialization
	MiniQueue<TOps::const_iterator, 128> endGroupOps;

	NetLogPacketDebug("CSerializationChunk::DecodeFromStream chunkID %d nProfile %d (%f)", chunkID, nProfile, input.GetInput().GetBitSize());

	output.PutTyped<ChunkID>() = chunkID;
	output.PutTyped<uint8>() = nProfile;

#if CRC8_ASPECT_FORMAT
	uint8 recCrc = input.GetInput().ReadBits(8);
	NET_ASSERT(recCrc == m_crc);
	if (recCrc != m_crc)
		input.Failed();

	static_assert((NUM_BITS_FOR_OPS_SIZE % 8) == 0, "Invalid bit count, has to be a multiple of 8!");
	static_assert((NUM_BITS_FOR_OPS_SIZE / 8) <= sizeof(uint16), "Invalid type size!");
	uint16 recLen = input.GetInput().ReadBits(NUM_BITS_FOR_OPS_SIZE);
	NET_ASSERT(recLen == m_ops.size());
	if (recLen != m_ops.size())
		input.Failed();
#endif

	// interpret the little serialization program we've built
	for (TOps::const_iterator pOp = m_ops.begin(); pOp != m_ops.end(); ++pOp)
	{
		if (!endGroupOps.Empty() && endGroupOps.Front() == pOp)
		{
			input.EndGroup();
			endGroupOps.Pop();
		}

		// TODO: should be able to turn this into a table lookup and a function call (no switch needed)
		switch (pOp->type)
		{
		default:
			assert(pOp->type < CRY_ARRAY_COUNT(DecodeFromStreamImpl));
			DecodeFromStreamImpl[pOp->type](NAME_FROM_OP(pOp), input, output, pOp->pPolicy);
			break;

		case eO_String:
			GenImpl<SSerializeString>::DecodeFromStream(NAME_FROM_OP(pOp), input, output, pOp->pPolicy);
			break;

		case eO_OptionalGroup:
			{
				if (pOp->skipFalse)
				{
					NET_ASSERT(pOp->skipFalse != INVALID_SKIP_FALSE && pOp->skipFalse >= 0);
					bool cond = input.BeginOptionalGroup(NAME_FROM_OP(pOp), false);
					output.PutByte(cond);
					if (!cond)
						pOp += pOp->skipFalse;
					else
						endGroupOps.Push(pOp + pOp->skipFalse); // remember when to call EndGroup()
				}
				//				else
				//					int k = 0; // make this break-pointable
			}
			break;
		}
	}

#if CRC8_ASPECT_FORMAT
	if (!input.Ok())
	{
		NetWarning("Readback failed: magic number is %.2x/%.2x:%.2x/%.2x", recCrc, recLen, m_crc, (unsigned int)m_ops.size());
	}
#endif
}

void CSerializationChunk::EncodeToStream(CByteInputStream& input, CNetOutputSerializeImpl& output, ChunkID chunkID, uint8 nProfile) const
{
	// when should we call EndGroup()
	// TODO: this probably isn't necessary under the current implementation... maybe it could be removed and save a few cycles on serialization
	MiniQueue<TOps::const_iterator, 128> endGroupOps;

	ChunkID savedChunkID = input.GetTyped<ChunkID>();
	uint8 savedProfile = input.GetTyped<uint8>();
	NET_ASSERT(savedChunkID == chunkID);
	NET_ASSERT(savedProfile == nProfile);

#if CRC8_ASPECT_FORMAT
	output.GetOutput().WriteBits(m_crc, 8);
	output.GetOutput().WriteBits(m_ops.size(), NUM_BITS_FOR_OPS_SIZE);
#endif

	// interpret the little serialization program we've built
	for (TOps::const_iterator pOp = m_ops.begin(); pOp != m_ops.end(); ++pOp)
	{
		if (!endGroupOps.Empty() && endGroupOps.Front() == pOp)
		{
			output.EndGroup();
			endGroupOps.Pop();
		}

		switch (pOp->type)
		{
		default:
			assert(pOp->type < CRY_ARRAY_COUNT(EncodeToStreamImpl));
			EncodeToStreamImpl[pOp->type](NAME_FROM_OP(pOp), input, output, pOp->pPolicy);
			break;

		case eO_String:
			GenImpl<SSerializeString>::EncodeToStream(NAME_FROM_OP(pOp), input, output, pOp->pPolicy);
			break;

		case eO_OptionalGroup:
			{
				NET_ASSERT(pOp->skipFalse != INVALID_SKIP_FALSE && pOp->skipFalse >= 0);
				if (pOp->skipFalse)
				{
					bool cond = input.GetByte() != 0;
					output.BeginOptionalGroup(NAME_FROM_OP(pOp), cond);
					NET_ASSERT(((int)(pOp - m_ops.begin())) + pOp->skipFalse < (int)m_ops.size());
					if (!cond)
						pOp += pOp->skipFalse;
					else
						endGroupOps.Push(pOp + pOp->skipFalse); // remember when to call EndGroup()
				}
				//				else
				//					int k = 0; // make this break-pointable
			}
			break;
		}
	}
}

ESynchObjectResult CSerializationChunk::UpdateGame(IGameContext* pCtx, CByteInputStream& input, EntityId id, NetworkAspectType nAspect, ChunkID chunkID, uint8 nProfile, bool& wasPartialUpdate) const
{
	ChunkID savedChunkID = input.GetTyped<ChunkID>();
	uint8 savedProfile = input.GetTyped<uint8>();

	if (savedChunkID != chunkID || savedProfile != nProfile)
	{
		if (CNetCVars::Get().LogLevel > 0)
			NetWarning("Chunk/profile mismatch: chunk is %d and was %d; profile is %d and was %d; skipping update", int(chunkID), int(savedChunkID), int(nProfile), int(savedProfile));
		return eSOR_Skip;
	}

	CFromBufferImpl impl(input, this);
	CSimpleSerialize<CFromBufferImpl> ser(impl);
	ESynchObjectResult result = pCtx->SynchObject(id, nAspect, nProfile, &ser, true);
	wasPartialUpdate = impl.WasPartial();
	if (result == eSOR_Failed)
	{
		NetWarning("CSerializationChunk::UpdateGame: game failed to sync object");
	}
	if (!ser.Ok())
	{
		NetWarning("CSerializationChunk::UpdateGame: serialization failed");
		result = eSOR_Failed;
	}
	if (result != eSOR_Failed)
	{
		// TODO: figure out why we're not error checking here...
		//       or calling Complete in case of failure
		if (!impl.Complete(false && result == eSOR_Ok))
		{
			NetWarning("CSerializationChunk::UpdateGame: failed to complete serialization");
			return eSOR_Failed;
		}
	}
	return result;
}

#if ENABLE_SERIALIZATION_LOGGING
struct SSerializationLogger
{
	SSerializationLogger(IEntity* pEntity, NetworkAspectType aspectType, uint8 profile)
		: m_pEntity(pEntity)
	{
		if (m_pEntity)
		{
			NetLog("[GameSerialization]: BEGIN Entity '%s' (class '%s', eid %u (0x%08X)) (NetworkAspectType %u, profile %u)", m_pEntity->GetName(), m_pEntity->GetClass()->GetName(), m_pEntity->GetId(), m_pEntity->GetId(), aspectType, profile);
		}
	}

	~SSerializationLogger()
	{
		if (m_pEntity)
		{
			NetLog("[GameSerialization]: END   Entity '%s' (class '%s', eid %u (0x%08X))", m_pEntity->GetName(), m_pEntity->GetClass()->GetName(), m_pEntity->GetId(), m_pEntity->GetId());
		}
	}

	IEntity* m_pEntity;
};
#endif // ENABLE_SERIALIZATION_LOGGING

bool CSerializationChunk::FetchFromGame(IGameContext* pCtx, CByteOutputStream& output, EntityId id, NetworkAspectType nAspect, ChunkID chunkID, uint8 nProfile) const
{
	output.PutTyped<ChunkID>() = chunkID;
	output.PutTyped<uint8>() = nProfile;

	CToBufferImpl impl(output, this);
	CSimpleSerialize<CToBufferImpl> ser(impl);

#if ENABLE_SERIALIZATION_LOGGING
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	SSerializationLogger serializationLogger(pEntity, nAspect, nProfile);
#endif // ENABLE_SERIALIZATION_LOGGING

	return pCtx->SynchObject(id, nAspect, nProfile, &ser, true) == eSOR_Ok && impl.Complete() && ser.Ok();
}

bool CSerializationChunk::Init(IGameContext* pCtx, EntityId id, NetworkAspectType nAspect, uint8 nProfile, NetworkAspectType noCompression)
{
	CBuildImpl impl(this);
	CSimpleSerialize<CBuildImpl> ser(impl);
	if (noCompression)
	{
		m_overrideCompression = true;
	}
	return pCtx->SynchObject(id, nAspect, nProfile, &ser, bool(ENABLE_SERIALIZATION_LOGGING)) == eSOR_Ok && impl.Complete() && ser.Ok();
}

bool CSerializationChunk::IsEmpty() const
{
	return m_ops.empty();
}

bool CSerializationChunk::operator<(const CSerializationChunk& rhs) const
{
	if (m_ops.size() < rhs.m_ops.size())
		return true;
	else if (m_ops.size() > rhs.m_ops.size())
		return false;
#if CRC8_ASPECT_FORMAT
	if (m_crc < rhs.m_crc)
		return true;
	else if (m_crc > rhs.m_crc)
		return false;
#endif
	for (size_t i = 0; i < m_ops.size(); i++)
	{
		const SOp& lop = m_ops[i];
		const SOp& rop = rhs.m_ops[i];

		if (lop.type < rop.type)
			return true;
		else if (lop.type > rop.type)
			return false;
		else if (lop.skipFalse < rop.skipFalse)
			return true;
		else if (lop.skipFalse > rop.skipFalse)
			return false;
		else if (lop.pPolicy < rop.pPolicy)
			return true;
		else if (lop.pPolicy > rop.pPolicy)
			return false;
#if TRACK_ENCODING
		else if (lop.name < rop.name)
			return true;
		else if (lop.name > rop.name)
			return false;
#endif
	}
	return false;
}

#if ENABLE_DEBUG_KIT
void CSerializationChunk::Dump(EntityId eid, uint32 id)
{
	NetLog("----------------------------------------------");
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(eid);
	NetLog("-------- BindToNetwork() %s (eid 0x%08X)", pEntity->GetName(), eid);

	#if CRC8_ASPECT_FORMAT
	NetLog("-------- SERIALIZATION CHUNK %.8x magicnumber=%.2x/%.2x --------", id, int(m_crc), m_ops.size());
	#else
	NetLog("-------- SERIALIZATION CHUNK %.8x --------", id);
	#endif

	for (TOps::const_iterator pOp = m_ops.begin(); pOp != m_ops.end(); ++pOp)
	{
		string out;

	#if TRACK_ENCODING
		out += string().Format(" name='%s'", pOp->name);
	#endif

		switch (pOp->type)
		{
		default:
			out = "VALUE" + out + string().Format(" type='%s' policy='%s'", EntryNames[pOp->type], KeyToString(pOp->pPolicy->key).c_str());
			break;

		case eO_String:
			out = "VALUE" + out + string().Format(" type='string' policy='%s'", KeyToString(pOp->pPolicy->key).c_str());
			break;

		case eO_OptionalGroup:
			out = "OPGRP" + out + string().Format(" jump-if-false=%d", pOp->skipFalse);
			break;
		}

		NetLog("%s", out.c_str());
	}
	NetLog("----------------------------------------------");
}
#endif
