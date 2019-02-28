// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IMETADATARECORDER_H__
#define __IMETADATARECORDER_H__

#pragma once

struct IMetadata
{
	static IMetadata*        CreateInstance();
	void                     Delete();

	virtual void             SetTag(uint32 tag) = 0;
	virtual bool             SetValue(uint32 type, const uint8* data, uint8 size) = 0;
	virtual bool             AddField(const IMetadata* metadata) = 0;
	virtual bool             AddField(uint32 tag, uint32 type, const uint8* data, uint8 size) = 0;

	virtual uint32           GetTag() const = 0;
	virtual size_t           GetNumFields() const = 0; // 0 means this is a basic typed value
	virtual const IMetadata* GetFieldByIndex(size_t i) const = 0;
	virtual uint32           GetValueType() const = 0;
	virtual uint8            GetValueSize() const = 0;
	virtual bool             GetValue(uint8* data /*[out]*/, uint8* size /*[in|out]*/) const = 0;

	virtual IMetadata*       Clone() const = 0;

	virtual void             Reset() = 0;

protected:
	virtual ~IMetadata() {}
};

// This interface should be implemented by user of IMetadataRecorder.
struct IMetadataListener
{
	virtual ~IMetadataListener(){}
	virtual void OnData(const IMetadata* metadata) = 0;
};

// Records toplevel metadata - everything being recorded is added to the toplevel in a sequential manner.
struct IMetadataRecorder
{
	static IMetadataRecorder* CreateInstance();
	void                      Delete();

	virtual bool              InitSave(const char* filename) = 0;
	virtual bool              InitLoad(const char* filename) = 0;

	virtual void              RecordIt(const IMetadata* metadata) = 0;
	virtual void              Flush() = 0;

	virtual bool              Playback(IMetadataListener* pListener) = 0;

	virtual void              Reset() = 0;

protected:
	virtual ~IMetadataRecorder() {}
};

template<typename I>
class CSimpleAutoPtr
{
public:
	CSimpleAutoPtr() { m_pI = I::CreateInstance(); }
	~CSimpleAutoPtr() { m_pI->Delete(); }
	I*       operator->() const { return m_pI; }
	const I* get() const        { return m_pI; }
private:
	CSimpleAutoPtr(const CSimpleAutoPtr& rhs);
	CSimpleAutoPtr& operator=(const CSimpleAutoPtr& rhs);
	I* m_pI;
};

typedef CSimpleAutoPtr<IMetadata>         IMetadataPtr;
typedef CSimpleAutoPtr<IMetadataRecorder> IMetadataRecorderPtr;

#endif
