// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompressionManager.h"
#include <CryNetwork/ISerialize.h>
#include "Utils.h"
#include "ICompressionPolicy.h"
#include <CryNetwork/SimpleSerialize.h>
#include "SerializationChunk.h"
#include "RangedIntPolicy.h"
#include "OwnChannelCompressionPolicy.h"
#include <queue>

bool CCompressionManager::CCompareChunks::operator()(CSerializationChunk* p0, CSerializationChunk* p1) const
{
	return *p0 < *p1;
}

/*
 * CCompressionRegistry
 */

CCompressionRegistry* CCompressionRegistry::m_pSelf = 0;

void CCompressionRegistry::RegisterPolicy(const string& name, CompressionPolicyCreator create)
{
	VectorMap<string, CompressionPolicyCreator>::iterator iter = m_policyFactories.find(name);
	if (iter != m_policyFactories.end())
		CryFatalError("Duplicated policy implementation name %s", name.c_str());
	m_policyFactories.insert(std::make_pair(name, create));
}

ICompressionPolicyPtr CCompressionRegistry::CreatePolicyOfType(const string& type, uint32 key)
{
	VectorMap<string, CompressionPolicyCreator>::iterator iter = m_policyFactories.find(type);
	if (iter == m_policyFactories.end())
		return 0;
	else
		return iter->second(key);
}

void CCompressionRegistry::Create()
{
	m_pSelf = new CCompressionRegistry;
}

/*
 * CCompressionManager
 */

CCompressionManager::CCompressionManager()
{
	m_pDefaultPolicy = 0;
	m_pTemporaryChunk = new CSerializationChunk;
}

CCompressionManager::~CCompressionManager()
{
	ClearCompressionPolicies(true);
}

void CCompressionManager::ClearCompressionPolicies(bool includingDefault)
{
	stl::free_container(m_chunks);
	m_chunkToId.clear();
	m_compressionPolicies.clear();
	if (includingDefault)
		m_pDefaultPolicy = NULL;
}

void CCompressionManager::Reset(bool useCompression, bool unloading)
{
	if (m_pDefaultPolicy == NULL)
	{
		m_pDefaultPolicy = CCompressionRegistry::Get()->CreatePolicyOfType("Default", 0);
	}
	NET_ASSERT(m_pDefaultPolicy != NULL);

	ClearCompressionPolicies(false);

	if (!unloading)
	{
		uint32 defaultNameKey;
		StringToKey("dflt", defaultNameKey);

		m_compressionPolicies.insert(std::make_pair(defaultNameKey, m_pDefaultPolicy));
		m_compressionPolicies.insert(std::make_pair(0, m_pDefaultPolicy));

		string filename = "Scripts/Network/CompressionPolicy.xml";
		XmlNodeRef config = gEnv->pSystem->LoadXmlFromFile(filename);
		if (config)
		{
			std::queue<XmlNodeRef> waitingToLoad;
			for (int i = 0; i < config->getChildCount(); i++)
				waitingToLoad.push(config->getChild(i));

			uint32 skipCount = 0;
			while (!waitingToLoad.empty() && skipCount < waitingToLoad.size())
			{
				XmlNodeRef loading = waitingToLoad.front();
				waitingToLoad.pop();

				string name = loading->getAttr("name");
				bool processed = true;
				if (name.empty())
				{
					NetWarning("Policy with no name at %s:%d", filename.c_str(), loading->getLine());
				}
				else
				{
					uint32 nameKey;
					if (!StringToKey(name.c_str(), nameKey))
					{
						NetWarning("Unable to convert policy name '%s' to a four character code; ignoring it (found at %s:%d)", name.c_str(), filename.c_str(), loading->getLine());
					}
					else
					{
						if (m_compressionPolicies.find(nameKey) != m_compressionPolicies.end())
						{
							NetWarning("Duplicate policy %s found at %s:%d; skipping", name.c_str(), filename.c_str(), loading->getLine());
						}
						if (0 == strcmp("Alias", loading->getTag()))
						{
							string is = loading->getAttr("is");
							if (is.empty())
							{
								NetWarning("Alias with no basis found at %s:%d", filename.c_str(), loading->getLine());
							}
							else
							{
								uint32 isKey;
								if (!StringToKey(is.c_str(), isKey))
								{
									NetWarning("Unable to convert alias basis '%s' to a four character code; ignoring it (found at %s:%d)", is.c_str(), filename.c_str(), loading->getLine());
								}
								else
								{
									TCompressionPoliciesMap::iterator iter = m_compressionPolicies.find(isKey);
									if (iter == m_compressionPolicies.end())
									{
										processed = false;
									}
									else
									{
										m_compressionPolicies.insert(std::make_pair(nameKey, iter->second));
									}
								}
							}
						}
						else if (0 == strcmp("Policy", loading->getTag()))
						{
							ICompressionPolicyPtr pPolicy;
							if (useCompression)
								pPolicy = CreatePolicy(loading, filename, nameKey);
							else if (nameKey == 'eid')
								pPolicy = CCompressionRegistry::Get()->CreatePolicyOfType("SimpleEntityId", 'eid');
							else
								pPolicy = m_pDefaultPolicy;
							if (!pPolicy)
							{
								NetWarning("Failed to create policy %s at %s:%d; reverting to default", name.c_str(), filename.c_str(), loading->getLine());
								pPolicy = m_pDefaultPolicy;
							}
							m_compressionPolicies.insert(std::make_pair(nameKey, pPolicy));
						}
					}
				}

				if (processed)
				{
					skipCount = 0;
				}
				else
				{
					waitingToLoad.push(loading);
					skipCount++;
				}
			}
		}
	}
}

ICompressionPolicyPtr CCompressionManager::CreatePolicy(XmlNodeRef node, const string& filename, uint32 key)
{
	if (!node)
		return ICompressionPolicyPtr();

	ICompressionPolicyPtr pPolicy;
	string impl = node->getAttr("impl");
	if (impl.empty())
	{
		ICompressionPolicyPtr pOwn(CreatePolicy(node->findChild("Own"), filename, key));
		ICompressionPolicyPtr pOther(CreatePolicy(node->findChild("Other"), filename, key));

		if (!pOwn || !pOther)
			return ICompressionPolicyPtr();

		pPolicy = new COwnChannelCompressionPolicy(key, pOwn, pOther);
	}
	else
	{
		pPolicy = CCompressionRegistry::Get()->CreatePolicyOfType(impl, key);
		if (!pPolicy)
		{
			NetWarning("Implementation '%s' could not be created for policy in %s:%d", impl.c_str(), filename.c_str(), node->getLine());
			return 0;
		}

		if (!pPolicy->Load(node, filename))
		{
			NetWarning("Couldn't load parameters for policy implemented by '%s' in %s:%d", impl.c_str(), filename.c_str(), node->getLine());
			return 0;
		}
	}

	return pPolicy;
}

ICompressionPolicyPtr CCompressionManager::GetCompressionPolicy(uint32 key)
{
	TCompressionPoliciesMap::iterator iter = m_compressionPolicies.find(key);
	if (iter == m_compressionPolicies.end())
		return GetCompressionPolicyFallback(key);
	return iter->second;
}

ICompressionPolicyPtr CCompressionManager::GetCompressionPolicyFallback(uint32 key)
{
	if ((key & 0xff000000) == (uint32)ISerialize::ENUM_POLICY_TAG)
	{
		ICompressionPolicyPtr pPolicy = CreateRangedInt(key & 0xffffff, key);
		m_compressionPolicies.insert(std::make_pair(key, pPolicy));
		return pPolicy;
	}
	NetWarning("Compression policy %s not found", KeyToString(key).c_str());
	return m_pDefaultPolicy;
}

ChunkID CCompressionManager::GameContextGetChunkID(IGameContext* pCtx, EntityId id, NetworkAspectType nAspect, uint8 nProfile, NetworkAspectType skipCompression)
{
	ASSERT_PRIMARY_THREAD;

	m_pTemporaryChunk->Reset();
	if (!m_pTemporaryChunk->Init(pCtx, id, nAspect, nProfile, skipCompression))
		return InvalidChunkID;

	std::map<CSerializationChunkPtr, ChunkID, CCompareChunks>::iterator iter = m_chunkToId.find(m_pTemporaryChunk);
	if (iter == m_chunkToId.end())
	{
		iter = m_chunkToId.insert(std::make_pair(m_pTemporaryChunk, static_cast<ChunkID>(m_chunks.size()))).first;
		m_chunks.push_back(m_pTemporaryChunk);
#if ENABLE_DEBUG_KIT
		m_pTemporaryChunk->Dump(id, iter->second);
#endif
		m_pTemporaryChunk = new CSerializationChunk;
	}
	return iter->second;
}

bool CCompressionManager::IsChunkEmpty(ChunkID chunkID)
{
	if (chunkID == InvalidChunkID)
		return true;
	CSerializationChunkPtr pChunk = GetChunk(chunkID);
	if (pChunk)
		return pChunk->IsEmpty();
	else
		return true;
}

void CCompressionManager::BufferToStream(ChunkID chunk, uint8 profile, CByteInputStream& in, CNetOutputSerializeImpl& out)
{
	if (CSerializationChunkPtr pChunk = GetChunk(chunk))
		pChunk->EncodeToStream(in, out, chunk, profile);
}

void CCompressionManager::StreamToBuffer(ChunkID chunk, uint8 profile, CNetInputSerializeImpl& in, CByteOutputStream& out)
{
	if (CSerializationChunkPtr pChunk = GetChunk(chunk))
		pChunk->DecodeFromStream(in, out, chunk, profile);
}

ICompressionPolicyPtr CCompressionManager::CreateRangedInt(int nMax, uint32 key)
{
	CRangedIntPolicy pol;
	pol.SetValues(0, nMax);
	return new CCompressionPolicy<CRangedIntPolicy>(key, pol);

	//	return m_pDefaultPolicy;
}

void CCompressionManager::GetMemoryStatistics(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "CCompressionManager");

	pSizer->Add(*this);
	pSizer->AddHashMap(m_compressionPolicies);
	pSizer->AddContainer(m_chunks);
	pSizer->AddContainer(m_chunkToId);

	for (size_t i = 0; i < m_chunks.size(); ++i)
		m_chunks[i]->GetMemoryStatistics(pSizer);

	for (TCompressionPoliciesMap::const_iterator it = m_compressionPolicies.begin(); it != m_compressionPolicies.end(); ++it)
		it->second->GetMemoryStatistics(pSizer);
}
