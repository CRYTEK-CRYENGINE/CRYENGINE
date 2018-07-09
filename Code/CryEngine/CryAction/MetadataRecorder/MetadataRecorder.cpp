// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IMetadataRecorder.h"

enum EBasicKeywords4CC
{
	eBK_void = 0, // end of meta data block, or invalid fcc
	eBK_meta = 'meta',

	eBK_tags = 'tags', // begin of list of tags
	eBK_typs = 'typs', // begin of list of typs
};

class CMetadata : public IMetadata
{
	friend class CMetadataRecorder;

public:
	CMetadata() : m_tag(0), m_type(eBK_void), m_size(0) {}

	~CMetadata() {}

	void SetTag(uint32 tag)
	{
		if (tag == eBK_meta)
			return;

		m_tag = tag;
	}

	bool SetValue(uint32 type, const uint8* data, uint8 size)
	{
		if (type == eBK_void || type == eBK_meta || data == NULL || size == 0)
			return false;

		if (m_tag == 0 || m_type != 0 && m_type != type)
			// tag not set or changing set type
			return false;

		m_type = type;
		memcpy(m_data, data, size);
		m_size = size;
		return true;
	}

	bool AddField(const IMetadata* metadata)
	{
		if (m_tag == eBK_void || m_type != eBK_void && m_type != eBK_meta || metadata == NULL)
			// tag not set or set type is not meta
			return false;

		m_type = eBK_meta;

		m_fields.push_back(*static_cast<const CMetadata*>(metadata));
		return true;
	}

	bool AddField(uint32 tag, uint32 type, const uint8* data, uint8 size)
	{
		if (tag == eBK_void || type == eBK_void || type == eBK_meta || data == NULL || size == 0)
			return false;

		if (m_tag == eBK_void || m_type != eBK_void && m_type != eBK_meta)
			// tag not set or set type is not meta
			return false;

		m_type = eBK_meta;

		CMetadata metadata;
		metadata.SetTag(tag);
		if (!metadata.SetValue(type, data, size))
			return false;
		m_fields.push_back(metadata);
		return true;
	}

	void Reset()
	{
		m_tag = eBK_void;
		m_type = eBK_void;
		m_size = 0;
		m_fields.resize(0);
	}

	IMetadata* Clone() const
	{
		CMetadata* pMetadata = new CMetadata();
		*pMetadata = *this;
		return pMetadata;
	}

	uint32 GetTag() const
	{
		return m_tag;
	}

	size_t GetNumFields() const
	{
		if (m_tag == eBK_void || m_type != eBK_meta)
			return 0;

		return m_fields.size();
	}

	const IMetadata* GetFieldByIndex(size_t i) const
	{
		if (m_tag == eBK_void || m_type != eBK_meta)
			return NULL;

		if (i >= m_fields.size())
			return NULL;

		return &m_fields[i];
	}

	uint32 GetValueType() const
	{
		if (m_tag == eBK_void)
			return eBK_void;

		return m_type;
	}

	uint8 GetValueSize() const
	{
		if (m_tag == eBK_void || m_type == eBK_void || m_type == eBK_meta)
			return 0;

		return m_size;
	}

	bool GetValue(uint8* data /*[out]*/, uint8* size /*[in|out]*/) const
	{
		if (size == NULL)
			return false;

		if (m_tag == eBK_void || m_type == eBK_void || m_type == eBK_meta)
		{
			*size = 0;
			return false;
		}

		if (data == NULL || *size < m_size)
		{
			*size = m_size;
			return false;
		}

		memcpy(data, m_data, m_size);
		*size = m_size;
		return true;
	}

	CMetadata& operator=(const CMetadata& rhs)
	{
		Reset();

		m_tag = rhs.m_tag;
		m_type = rhs.m_type;

		uint8 nsize = rhs.m_size;
		memcpy(m_data, rhs.m_data, nsize);
		m_size = rhs.m_size;
		for (size_t i = 0; i < rhs.m_fields.size(); ++i)
			m_fields.push_back(rhs.m_fields[i]);

		return *this;
	}

	bool operator==(const CMetadata& rhs) const
	{
		if (m_tag == rhs.m_tag && m_type == rhs.m_type && m_size == rhs.m_size && 0 == memcmp(m_data, rhs.m_data, m_size) && m_fields.size() == rhs.m_fields.size())
		{
			for (size_t i = 0; i < m_fields.size(); ++i)
				if (!(m_fields[i] == rhs.m_fields[i]))
					return false;
			return true;
		}

		return false;
	}

	// needed when MetadataRecorder is saving - it writes a list of tags and a list types used in this metadata structure
	void ExtractTagsAndTypes(std::set<uint32>& tags, std::set<uint32>& types) const // a stream of 4CC's
	{
		if (m_tag == eBK_void || m_type == eBK_void)
			// incomplete metadata definition
			return;

		tags.insert(m_tag);
		types.insert(m_type);
		if (m_type == eBK_meta)
			for (size_t i = 0; i < m_fields.size(); ++i)
				m_fields[i].ExtractTagsAndTypes(tags, types);
	}

	// serialize tag/type/data to the output stream
	// MetadataRecorder will write the 32bit size for each toplevel metadata recorded
	void SerializeTo(std::vector<uint8>& to) const
	{
		if (m_tag == eBK_void || m_type == eBK_void)
			// incomplete
			return;

		to.insert(to.end(), (uint8*)&m_tag, (uint8*)&m_tag + sizeof(m_tag));
		to.insert(to.end(), (uint8*)&m_type, (uint8*)&m_type + sizeof(m_type));

		if (m_type == eBK_meta)
		{
			for (size_t i = 0; i < m_fields.size(); ++i)
				m_fields[i].SerializeTo(to);
			uint32 mend = eBK_void;
			to.insert(to.end(), (uint8*)&mend, (uint8*)&mend + sizeof(mend));
		}
		else
		{
			to.push_back(m_size); // since we are trying to be very general here, so we have to prefix the size always (although not so memory friendly)
			to.insert(to.end(), m_data, m_data + m_size);
		}
	}

	// deserialize tag/type/data from the input stream
	// MetadataRecorder will first read the 32bit size for this toplevel metadata, so it can form the input stream
	bool SerializeFrom(const std::vector<uint8>& from, size_t& offset, const std::set<uint32>& tags, const std::set<uint32>& types)
	{
		Reset();

		if (offset + sizeof(m_tag) > from.size())
			goto L_error;
		memcpy(&m_tag, &from[offset], sizeof(m_tag));
		if (tags.find(m_tag) == tags.end())
			goto L_error;
		offset += sizeof(m_tag);
		if (offset + sizeof(m_type) > from.size())
			goto L_error;
		memcpy(&m_type, &from[offset], sizeof(m_type));
		if (types.find(m_type) == types.end())
			goto L_error;
		offset += sizeof(m_type);
		if (m_type == eBK_meta)
		{
			while (true)
			{
				CMetadata metadata;
				if (!metadata.SerializeFrom(from, offset, tags, types))
					goto L_error;
				m_fields.push_back(metadata);
				uint32 fcc = eBK_void;
				if (offset + sizeof(fcc) > from.size())
					goto L_error;
				memcpy(&fcc, &from[offset], sizeof(fcc));
				if (fcc == eBK_void)
				{
					offset += sizeof(fcc);
					break; // end of this metadata block
				}
			}
		}
		else
		{
			if (offset + 1 > from.size())
				goto L_error;
			m_size = from[offset++];
			if (offset + m_size > from.size())
				goto L_error;
			memcpy(m_data, &from[offset], m_size);
			offset += m_size;
		}

		return true;

L_error:
		Reset();
		return false;
	}

private:
	uint32                 m_tag;
	uint32                 m_type;

	uint8                  m_data[256]; // 255 big enough buffer to hold any basic type value
	uint8                  m_size;

	std::vector<CMetadata> m_fields;
};

IMetadata* IMetadata::CreateInstance()
{
	return new CMetadata();
}

void IMetadata::Delete()
{
	delete this;
}

static const char* TopLevelMetadataFolder = "%USER%/!Metadata/";

class CMetadataRecorder : public IMetadataRecorder
{
public:
	CMetadataRecorder() : m_mode(eM_None) {}
	~CMetadataRecorder() {}

	bool InitSave(const char* filename)
	{
		if (m_file.GetHandle())
			return false; // need to call Reset()
		Reset();        // safe to do it here (data/file/mode)
		if (!m_file.Open(string(TopLevelMetadataFolder) + filename, "wb"))
			return false;
		m_mode = eM_Saving;
		return true;
	}

	bool InitLoad(const char* filename)
	{
		if (m_file.GetHandle())
			return false;
		Reset(); // data/file/mode (just for safty)
		if (!m_file.Open(string(TopLevelMetadataFolder) + filename, "rb"))
			return false;
		m_mode = eM_Loading;
		return true;
	}

	void RecordIt(const IMetadata* metadata)
	{
		if (m_mode != eM_Saving || !metadata)
			return;

		const CMetadata* pMetadata = static_cast<const CMetadata*>(metadata);
		pMetadata->ExtractTagsAndTypes(m_tags, m_types);
		m_metadata.push_back(*pMetadata);
	}

	void Flush()
	{
		if (m_mode != eM_Saving)
			return;

		// write tags and types ending with 0
		WriteFccCodes(m_file, m_tags, eBK_tags);
		WriteFccCodes(m_file, m_types, eBK_typs);

		// write toplevel metadata prefixed with data size
		for (size_t i = 0; i < m_metadata.size(); ++i)
		{
			std::vector<uint8> stm;
			m_metadata[i].SerializeTo(stm);
			uint32 sz = stm.size();
			if (sz == 0)
				continue;
			m_file.Write(&sz, sizeof(sz));
			m_file.Write(&stm[0], sz);
		}

		// a seperator between flushes
		uint32 seperator = eBK_void;
		m_file.Write(&seperator, sizeof(seperator));

		ResetData();
	}

	bool Playback(IMetadataListener* pListener)
	{
		if (m_mode != eM_Loading || !pListener)
			return false;

		ResetData();

		// read tags and types
		if (!ReadFccCodes(m_file, m_tags, eBK_tags))
			return false;
		if (!ReadFccCodes(m_file, m_types, eBK_typs))
			return false;

		// read data
		while (true)
		{
			uint32 sz = 0;
			if (m_file.ReadRaw(&sz, sizeof(sz)) != sizeof(sz))
				return false;
			if (sz == 0)
				break; // flush seperator encountered
			std::vector<uint8> stm;
			stm.resize(sz);
			if (m_file.ReadRaw(&stm[0], sz) != sz)
				return false;
			CMetadata metadata;
			size_t offset = 0;
			if (!metadata.SerializeFrom(stm, offset, m_tags, m_types))
				return false;
			CRY_ASSERT(offset == stm.size());
			m_metadata.push_back(metadata);
		}

		for (size_t i = 0; i < m_metadata.size(); ++i)
			pListener->OnData(&m_metadata[i]);
		return true;
	}

	void Reset()
	{
		ResetData();
		m_file.Close();
		m_mode = eM_None;
	}

private:
	typedef std::vector<CMetadata> TTopLevelMetadataVector;
	TTopLevelMetadataVector m_metadata;

	typedef std::set<uint32> TFourCCSet;
	TFourCCSet m_tags, m_types;

	CCryFile   m_file;

	enum EMode { eM_None, eM_Saving, eM_Loading };
	EMode m_mode;

	void ResetData()
	{
		m_metadata.resize(0);
		m_tags.clear();
		m_types.clear();
	}

	static inline void WriteFccCodes(CCryFile& file, const TFourCCSet& fccs, uint32 keyword)
	{
		uint32 fcc = keyword;
		file.Write(&fcc, sizeof(fcc));
		for (TFourCCSet::const_iterator it = fccs.begin(); it != fccs.end(); ++it)
		{
			fcc = *it;
			file.Write(&fcc, sizeof(fcc));
		}
		fcc = eBK_void;
		file.Write(&fcc, sizeof(fcc));
	}

	static inline bool ReadFccCodes(CCryFile& file, TFourCCSet& fccs, uint32 keyword)
	{
		uint32 fcc = 0;
		if (file.ReadRaw(&fcc, sizeof(fcc)) != sizeof(fcc))
			return false;
		if (fcc != keyword)
			return false;
		while (true)
		{
			if (file.ReadRaw(&fcc, sizeof(fcc)) != sizeof(fcc))
				return false;
			if (fcc == eBK_tags || fcc == eBK_typs)
				return false;
			if (fcc == eBK_void)
				break;
			fccs.insert(fcc);
		}
		return true;
	}

};

IMetadataRecorder* IMetadataRecorder::CreateInstance()
{
	return new CMetadataRecorder();
}

void IMetadataRecorder::Delete()
{
	delete this;
}
