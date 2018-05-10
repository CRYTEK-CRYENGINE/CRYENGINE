// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QObject>
#include <CrySystem/File/IFileChangeMonitor.h>
#include <CrySerialization/StringList.h>
#include <CrySerialization/Forward.h>

namespace CharacterTool
{

class SkeletonList : public QObject, public IFileChangeListener
{
	Q_OBJECT
public:
	SkeletonList();
	~SkeletonList();

	void                             Reset();
	bool                             Load();
	bool                             Save();
	bool                             HasSkeletonName(const char* skeletonName) const;
	string                           FindSkeletonNameByPath(const char* path) const;
	string                           FindSkeletonPathByName(const char* name) const;

	const Serialization::StringList& GetSkeletonNames() const { return m_skeletonNames; }

	void                             Serialize(Serialization::IArchive& ar);

signals:
	void SignalSkeletonListModified();
private:
	void OnFileChange(const char* sFilename, EChangeType eType) override;

	Serialization::StringList m_skeletonNames;
	struct SEntry
	{
		string alias;
		string path;

		void Serialize(Serialization::IArchive& ar);

		bool operator<(const SEntry& rhs) const { return alias == rhs.alias ? path < rhs.path : alias < rhs.alias; }
	};
	typedef std::vector<SEntry> SEntries;
	SEntries m_skeletonList;
};

}

