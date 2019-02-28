// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _ZIP_DIR_TREE_HDR_
#define _ZIP_DIR_TREE_HDR_

namespace ZipDir
{

class FileEntryTree
{
public:
	FileEntryTree() : m_originalName(0) {}
	explicit FileEntryTree(const char* originalName) : m_originalName(originalName) {}
	~FileEntryTree () {Clear();}

	// adds a file to this directory
	// Function can modify szPath input
	ErrorEnum Add ( char* szPath, char* szUnifiedPath, const FileEntry& file);

	// Adds or finds the file. Returns non-initialized structure if it was added,
	// or an IsInitialized() structure if it was found
	// Function can modify szPath input
	FileEntry* Add ( char* szPath, char* szUnifiedPath );

	// returns the number of files in this tree, including this and sublevels
	unsigned NumFilesTotal()const;

	// returns the size required to serialize the tree
	size_t GetSizeSerialized()const;

	// serializes into the memory
	size_t Serialize (DirHeader* pDir)const;

	void Clear();

	void Swap (FileEntryTree& rThat)
	{
		m_mapDirs.swap (rThat.m_mapDirs);
		m_mapFiles.swap (rThat.m_mapFiles);
	}

	size_t GetSize() const;

	size_t GetCompressedFileSize() const;
	size_t GetUncompressedFileSize() const;

	bool IsOwnerOf (const FileEntry* pFileEntry)const;

	// subdirectories
	typedef std::map<const char*, FileEntryTree*,stl::less_strcmp<const char*> > SubdirMap;
	// file entries
	typedef std::map<const char*, FileEntry,stl::less_strcmp<const char*> > FileMap;

	FileEntryTree* FindDir(const char* szDirName);
	ErrorEnum RemoveDir (const char* szDirName);
	ErrorEnum RemoveAll (){Clear();return ZD_ERROR_SUCCESS;}
	FileEntry* FindFileEntry (const char* szFileName);
	FileMap::iterator FindFile (const char* szFileName);
	ErrorEnum RemoveFile (const char* szFileName);
	FileEntryTree* GetDirectory(){return this;} // the FileENtryTree is simultaneously an entry in the dir list AND the directory header
	
	FileMap::iterator GetFileBegin() {return m_mapFiles.begin();}
	FileMap::iterator GetFileEnd() {return m_mapFiles.end();}
	FileMap::const_iterator GetFileBegin() const {return m_mapFiles.begin();}
	FileMap::const_iterator GetFileEnd() const {return m_mapFiles.end();}
	unsigned NumFiles()const {return (unsigned)m_mapFiles.size();}

	SubdirMap::iterator GetDirBegin() {return m_mapDirs.begin();}
	SubdirMap::iterator GetDirEnd() {return m_mapDirs.end();}
	SubdirMap::const_iterator GetDirBegin() const {return m_mapDirs.begin();}
	SubdirMap::const_iterator GetDirEnd() const {return m_mapDirs.end();}
	size_t NumDirsTotal() const;

	const char* GetFileName(FileMap::iterator it) {return it->first;}
	const char* GetDirName(SubdirMap::iterator it) {return it->first;}
	const char* GetOriginalName() const{ return m_originalName; }

	FileEntry* GetFileEntry(FileMap::iterator it);
	FileEntryTree* GetDirEntry(SubdirMap::iterator it);
	const FileEntry* GetFileEntry(FileMap::const_iterator it) const;
	const FileEntryTree* GetDirEntry(SubdirMap::const_iterator it) const;

protected:
	SubdirMap m_mapDirs;
	FileMap m_mapFiles;
	const char* m_originalName;
};

}
#endif
