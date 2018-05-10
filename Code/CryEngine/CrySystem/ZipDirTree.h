// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _ZIP_DIR_TREE_HDR_
#define _ZIP_DIR_TREE_HDR_

namespace ZipDir
{

class FileEntryTree
{
public:
	~FileEntryTree() { Clear(); }

	// adds a file to this directory
	// Function can modify szPath input
	ErrorEnum Add(char* szPath, const FileEntry& file);

	// Adds or finds the file. Returns non-initialized structure if it was added,
	// or an IsInitialized() structure if it was found
	// Function can modify szPath input
	FileEntry* Add(char* szPath);

	// returns the number of files in this tree, including this and sublevels
	unsigned int NumFilesTotal() const;

	// Total number of directories
	unsigned int NumDirsTotal() const;

	// returns the size required to serialize the tree
	size_t GetSizeSerialized() const;

	// serializes into the memory
	size_t Serialize(DirHeader* pDir) const;

	void   Clear();

	void   Swap(FileEntryTree& rThat)
	{
		m_mapDirs.swap(rThat.m_mapDirs);
		m_mapFiles.swap(rThat.m_mapFiles);
	}

	size_t GetSize() const;

	bool   IsOwnerOf(const FileEntry* pFileEntry) const;

	// subdirectories
	typedef std::map<const char*, FileEntryTree*, stl::less_strcmp<const char*>> SubdirMap;
	// file entries
	typedef std::map<const char*, FileEntry, stl::less_strcmp<const char*>>      FileMap;

	FileEntryTree*      FindDir(const char* szDirName);
	ErrorEnum           RemoveDir(const char* szDirName);
	ErrorEnum           RemoveAll() { Clear(); return ZD_ERROR_SUCCESS; }
	FileEntry*          FindFileEntry(const char* szFileName);
	FileMap::iterator   FindFile(const char* szFileName);
	ErrorEnum           RemoveFile(const char* szFileName);
	FileEntryTree*      GetDirectory()                     { return this; } // the FileENtryTree is simultaneously an entry in the dir list AND the directory header

	FileMap::iterator   GetFileBegin()                     { return m_mapFiles.begin(); }
	FileMap::iterator   GetFileEnd()                       { return m_mapFiles.end(); }
	unsigned            NumFiles() const                   { return (unsigned)m_mapFiles.size(); }
	SubdirMap::iterator GetDirBegin()                      { return m_mapDirs.begin(); }
	SubdirMap::iterator GetDirEnd()                        { return m_mapDirs.end(); }
	unsigned            NumDirs() const                    { return (unsigned)m_mapDirs.size(); }
	const char*         GetFileName(FileMap::iterator it)  { return it->first; }
	const char*         GetDirName(SubdirMap::iterator it) { return it->first; }

	FileEntry*          GetFileEntry(FileMap::iterator it);
	FileEntryTree*      GetDirEntry(SubdirMap::iterator it);

protected:
	SubdirMap m_mapDirs;
	FileMap   m_mapFiles;
};

}
#endif
