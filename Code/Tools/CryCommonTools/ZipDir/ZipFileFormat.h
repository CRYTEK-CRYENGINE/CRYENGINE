// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Declarations of control structures in Zip File.

#ifndef _ZIP_FILE_FORMAT_HDR_
#define _ZIP_FILE_FORMAT_HDR_

#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
#pragma pack(push)
#pragma pack(1)
#define PACK_GCC
#else
#define PACK_GCC __attribute__ ((packed))
#endif

namespace ZipFile
{
	typedef unsigned int ulong;
	typedef unsigned short ushort;

	enum
	{
		BLOCK_CIPHER_NUM_KEYS = 16
	};
	enum
	{
		BLOCK_CIPHER_KEY_LENGTH = 16
	};
	enum
	{
		RSA_KEY_MESSAGE_LENGTH = 128    // The modulus of our private/public key pair for signing, verification, encryption and decryption.
	};

	// General-purpose bit field flags
	enum {
		GPF_ENCRYPTED = 1 << 0, // If set, indicates that the file is encrypted.
		GPF_DATA_DESCRIPTOR = 1 << 3, // if set, the CRC32 and sizes aren't set in the file header, but only in the data descriptor following compressed data
		GPF_RESERVED_8_ENHANCED_DEFLATING = 1 << 4, // Reserved for use with method 8, for enhanced deflating.
		GPF_COMPRESSED_PATCHED = 1 << 5, // the file is compressed patched data
	};

	// compression methods
	enum {
		METHOD_STORE  = 0, // The file is stored (no compression)
		METHOD_SHRINK = 1, // The file is Shrunk
		METHOD_REDUCE_1 = 2, // The file is Reduced with compression factor 1
		METHOD_REDUCE_2 = 3, // The file is Reduced with compression factor 2
		METHOD_REDUCE_3 = 4, // The file is Reduced with compression factor 3
		METHOD_REDUCE_4 = 5, // The file is Reduced with compression factor 4
		METHOD_IMPLODE  = 6, // The file is Imploded
		METHOD_TOKENIZE = 7, // Reserved for Tokenizing compression algorithm
		METHOD_DEFLATE  = 8, // The file is Deflated
		METHOD_DEFLATE64 = 9, // Enhanced Deflating using Deflate64(tm)
		METHOD_IMPLODE_PKWARE = 10, // PKWARE Date Compression Library Imploding
		METHOD_DEFLATE_AND_ENCRYPT               = 11, // Deflate + Custom encryption (TEA)
		METHOD_DEFLATE_AND_STREAMCIPHER          = 12, // Deflate + stream cipher encryption on a per file basis
		METHOD_STORE_AND_STREAMCIPHER_KEYTABLE   = 13, // Store + Timur's encryption technique on a per file basis
		METHOD_DEFLATE_AND_STREAMCIPHER_KEYTABLE = 14, // Deflate + Timur's encryption technique on a per file basis
	};

	// version numbers
	enum {
		VERSION_DEFAULT           = 10, // Default value
		
		VERSION_TYPE_VOLUMELABEL  = 11, // File is a volume label
		VERSION_TYPE_FOLDER       = 20, // File is a folder (directory)
		VERSION_TYPE_PATCHDATASET = 27, // File is a patch data set 
		VERSION_TYPE_ZIP64        = 45, // File uses ZIP64 format extensions
		
		VERSION_COMPRESSION_DEFLATE    = 20, // File is compressed using Deflate compression
		VERSION_COMPRESSION_DEFLATE64  = 21, // File is compressed using Deflate64(tm)
		VERSION_COMPRESSION_DCLIMPLODE = 25, // File is compressed using PKWARE DCL Implode 
		VERSION_COMPRESSION_BZIP2      = 46, // File is compressed using BZIP2 compression*
		VERSION_COMPRESSION_LZMA       = 63, // File is compressed using LZMA
		VERSION_COMPRESSION_PPMD       = 63, // File is compressed using PPMd+
		
		VERSION_ENCRYPTION_PKWARE   = 20, // File is encrypted using traditional PKWARE encryption
		VERSION_ENCRYPTION_DES      = 50, // File is encrypted using DES
		VERSION_ENCRYPTION_3DES     = 50, // File is encrypted using 3DES
		VERSION_ENCRYPTION_RC2      = 50, // File is encrypted using original RC2 encryption
		VERSION_ENCRYPTION_RC4      = 50, // File is encrypted using RC4 encryption
		VERSION_ENCRYPTION_AES      = 51, // File is encrypted using AES encryption
		VERSION_ENCRYPTION_RC2C     = 51, // File is encrypted using corrected RC2 encryption**
		VERSION_ENCRYPTION_RC4C     = 52, // File is encrypted using corrected RC2-64 encryption**
		VERSION_ENCRYPTION_NOOAEP   = 61, // File is encrypted using non-OAEP key wrapping***
		VERSION_ENCRYPTION_CDR      = 62, // Central directory encryption
		VERSION_ENCRYPTION_BLOWFISH = 63, // File is encrypted using Blowfish
		VERSION_ENCRYPTION_TWOFISH  = 63, // File is encrypted using Twofish
	};

	// creator numbers
	enum {
		CREATOR_MSDOS     =  0, // MS-DOS and OS/2 (FAT / VFAT / FAT32 file systems)
		CREATOR_AMIGA     =  1, // Amiga                     
		CREATOR_OpenVMS   =  2, // OpenVMS
		CREATOR_UNIX      =  3, // UNIX                      
		CREATOR_VM        =  4, // VM/CMS
		CREATOR_ATARI     =  5, // Atari ST                  
		CREATOR_OS2       =  6, // OS/2 H.P.F.S.
		CREATOR_MACINTOSH =  7, // Macintosh                 
		CREATOR_ZSYSTEM   =  8, // Z-System
		CREATOR_CPM       =  9, // CP/M                     
		CREATOR_WINDOWS   = 10, // Windows NTFS
		CREATOR_MVS       = 11, // MVS (OS/390 - Z/OS)      
		CREATOR_VSE       = 12, // VSE
		CREATOR_ACORN     = 13, // Acorn Risc               
		CREATOR_VFAT      = 14, // VFAT
		CREATOR_AMVS      = 15, // alternate MVS            
		CREATOR_BEOS      = 16, // BeOS
		CREATOR_TANDEM    = 17, // Tandem                   
		CREATOR_OS400     = 18, // OS/400
		CREATOR_OSX       = 19, // OS X (Darwin)        
		                      
		CREATOR_UNUSED    = 20, // 20 thru 255 - unused    
	};

	enum {
		ZIP64_SEE_EXTENSION = -1 // If an archive is in ZIP64 format
		// and a value in a field is 0xFFFFFFFF (or 0xFFFF), the size will be
		// in the corresponding 8 byte (or 4 byte) ZIP64 extended information.
	};

	// end of Central Directory Record
	// followed by the .zip file comment (variable size, can be empty, obtained from nCommentLength)
	struct CDREnd
	{
		enum {SIGNATURE = 0x06054b50};
		ulong  lSignature;       // end of central dir signature    4 bytes  (0x06054b50)
		ushort nDisk;            // number of this disk             2 bytes
		ushort nCDRStartDisk;    // number of the disk with the start of the central directory  2 bytes
		ushort numEntriesOnDisk; // total number of entries in the central directory on this disk  2 bytes
		ushort numEntriesTotal;  // total number of entries in the central directory           2 bytes
		ulong  lCDRSize;         // size of the central directory   4 bytes
		ulong  lCDROffset;       // offset of start of central directory with respect to the starting disk number        4 bytes
		ushort nCommentLength;   // .ZIP file comment length        2 bytes

		AUTO_STRUCT_INFO;

		// .ZIP file comment (variable size, can be empty) follows
	} PACK_GCC;

	// end of Central Directory Record
	// followed by the zip64 extensible data sector (variable size, can be empty, obtained from nExtDataLength)
	struct CDREnd_ZIP64
	{
		enum {SIGNATURE = 0x06064b50};
		ulong  lSignature;       // end of central dir signature    4 bytes  (0x06064b50)
		uint64 nExtDataLength;   // The value stored into the "size of zip64 end of central directory record" should be the size of the remaining record and should not include the leading 12 bytes.  8 bytes
		ushort nVersionMadeBy;   // version made by                 2 bytes
		ushort nVersionNeeded;   // version needed to extract       2 bytes
		ulong  nDisk;            // number of this disk             4 bytes
		ulong  nCDRStartDisk;    // number of the disk with the start of the central directory  4 bytes
		uint64 numEntriesOnDisk; // total number of entries in the central directory on this disk  8 bytes
		uint64 numEntriesTotal;  // total number of entries in the central directory           8 bytes
		uint64 lCDRSize;         // size of the central directory   8 bytes
		uint64 lCDROffset;       // offset of start of central directory with respect to the starting disk number        8 bytes

		AUTO_STRUCT_INFO;

		// zip64 extensible data sector (variable size, can be empty) follows
	} PACK_GCC;

	// end of Central Directory Locator
	struct CDRLocator_ZIP64
	{
		enum {SIGNATURE = 0x07064b50};
		ulong  lSignature;       // end of central loc signature    4 bytes  (0x07064b50)
		ulong  nCDR64StartDisk;  // number of the disk with the start of the zip64 end of central directory  4 bytes
		uint64 lCDR64EndOffset;  // relative offset of the zip64 end of central directory record  8 bytes
		ulong  nDisks;           // number of disks                 4 bytes

		AUTO_STRUCT_INFO;
	} PACK_GCC;

	// This descriptor exists only if bit 3 of the general
	// purpose bit flag is set (see below).  It is byte aligned
	// and immediately follows the last byte of compressed data.
	// This descriptor is used only when it was not possible to
	// seek in the output .ZIP file, e.g., when the output .ZIP file
	// was standard output or a non seekable device.  For Zip64 format
	// archives, the compressed and uncompressed sizes are 8 bytes each.
	struct DataDescriptor
	{
		ulong lCRC32;             // crc-32                          4 bytes
		ulong lSizeCompressed;    // compressed size                 4 bytes
		ulong lSizeUncompressed;  // uncompressed size               4 bytes

		DataDescriptor() 
			: lCRC32(0)
			, lSizeCompressed(0)
			, lSizeUncompressed(0)			
		{
		}

		bool operator == (const DataDescriptor& d) const
		{
			return lCRC32 == d.lCRC32 && lSizeCompressed == d.lSizeCompressed && lSizeUncompressed == d.lSizeUncompressed;
		}
		bool operator != (const DataDescriptor& d) const
		{
			return lCRC32 != d.lCRC32 || lSizeCompressed != d.lSizeCompressed || lSizeUncompressed != d.lSizeUncompressed;
		}

		bool IsZIP64(const DataDescriptor& d) const
		{
			return lSizeCompressed == (ulong)ZIP64_SEE_EXTENSION || lSizeUncompressed == (ulong)ZIP64_SEE_EXTENSION;
		}

		AUTO_STRUCT_INFO;
	} PACK_GCC;
	
	// When compressing files, compressed and uncompressed sizes 
	// should be stored in ZIP64 format (as 8 byte values) when a 
	// file's size exceeds 0xFFFFFFFF.   However ZIP64 format may be 
	// used regardless of the size of a file.  When extracting, if 
	// the zip64 extended information extra field is present for 
	// the file the compressed and uncompressed sizes will be 8
	// byte values.  
	struct DataDescriptor_ZIP64
	{
		ulong  lCRC32;            // crc-32                          4 bytes
		uint64 lSizeCompressed;   // compressed size                 8 bytes
		uint64 lSizeUncompressed; // uncompressed size               8 bytes

		bool operator == (const DataDescriptor& d) const
		{
			return lCRC32 == d.lCRC32 && lSizeCompressed == d.lSizeCompressed && lSizeUncompressed == d.lSizeUncompressed;
		}
		bool operator != (const DataDescriptor& d) const
		{
			return lCRC32 != d.lCRC32 || lSizeCompressed != d.lSizeCompressed || lSizeUncompressed != d.lSizeUncompressed;
		}

		AUTO_STRUCT_INFO;
	} PACK_GCC;

	// the File Header as it appears in the CDR
	// followed by:
	//    file name (variable size)
	//    extra field (variable size)
	//    file comment (variable size)
	struct CDRFileHeader
	{
		enum {SIGNATURE = 0x02014b50};
		ulong  lSignature;         // central file header signature   4 bytes  (0x02014b50)
		ushort nVersionMadeBy;     // version made by                 2 bytes
		ushort nVersionNeeded;     // version needed to extract       2 bytes
		ushort nFlags;             // general purpose bit flag        2 bytes
		ushort nMethod;            // compression method              2 bytes
		ushort nLastModTime;       // last mod file time              2 bytes
		ushort nLastModDate;       // last mod file date              2 bytes
		DataDescriptor desc;
		ushort nFileNameLength;    // file name length                2 bytes
		ushort nExtraFieldLength;  // extra field length              2 bytes
		ushort nFileCommentLength; // file comment length             2 bytes
		ushort nDiskNumberStart;   // disk number start               2 bytes
		ushort nAttrInternal;      // internal file attributes        2 bytes
		ulong  lAttrExternal;      // external file attributes        4 bytes

		// This is the offset from the start of the first disk on
		// which this file appears, to where the local header should
		// be found.  If an archive is in zip64 format and the value
		// in this field is 0xFFFFFFFF, the size will be in the
		// corresponding 8 byte zip64 extended information extra field.
		enum {ZIP64_LOCAL_HEADER_OFFSET = 0xFFFFFFFF};
		ulong  lLocalHeaderOffset; // relative offset of local header 4 bytes

		bool IsZIP64(const CDRFileHeader& d) const
		{
			return desc.IsZIP64(desc) || nDiskNumberStart == (ushort)ZIP64_SEE_EXTENSION || lLocalHeaderOffset == (ulong)ZIP64_SEE_EXTENSION;
		}

		AUTO_STRUCT_INFO;
	} PACK_GCC;


	// this is the local file header that appears before the compressed data
	// followed by:
	//    file name (variable size)
	//    extra field (variable size)
	struct LocalFileHeader
	{
		enum {SIGNATURE = 0x04034b50};
		ulong  lSignature;        // local file header signature     4 bytes  (0x04034b50)
		ushort nVersionNeeded;    // version needed to extract       2 bytes
		ushort nFlags;            // general purpose bit flag        2 bytes
		ushort nMethod;           // compression method              2 bytes
		ushort nLastModTime;      // last mod file time              2 bytes
		ushort nLastModDate;      // last mod file date              2 bytes
		DataDescriptor desc;
		ushort nFileNameLength;   // file name length                2 bytes
		ushort nExtraFieldLength; // extra field length              2 bytes

		bool IsZIP64(const LocalFileHeader& d) const
		{
			return desc.IsZIP64(desc);
		}

		AUTO_STRUCT_INFO;
	} PACK_GCC;

	// compression methods
	enum EExtraHeaderID {
		EXTRA_ZIP64  = 0x0001, // ZIP64 extended information extra field
		EXTRA_NTFS   = 0x000a, // NTFS 
		EXTRA_UNIX   = 0x000d, // UNIX 
		EXTRA_PATCH  = 0x000f, // Patch Descriptor
	};

	//////////////////////////////////////////////////////////////////////////
	// header1+data1 + header2+data2 . . .
	// Each header should consist of:
  //		Header ID - 2 bytes
	//    Data Size - 2 bytes
	struct ExtraFieldHeader
	{
		ushort headerID;
		ushort dataSize;

		AUTO_STRUCT_INFO;
	} PACK_GCC;

	struct ExtraNTFSHeader
	{
		ulong reserved;  // 4 bytes.
		ushort attrTag;  // 2 bytes.
		ushort attrSize; // 2 bytes.

		AUTO_STRUCT_INFO;
	} PACK_GCC;

	//////////////////////////////////////////////////////////////////////////
	// The following is the layout of the zip64 extended 
	// information "extra" block. If one of the size or
	// offset fields in the Local or Central directory
	// record is too small to hold the required data,
	// a Zip64 extended information record is created.
	// The order of the fields in the zip64 extended 
	// information record is fixed, but the fields MUST
	// only appear if the corresponding Local or Central
	// directory record field is set to 0xFFFF or 0xFFFFFFFF.
	//
	// The extended information in the Local header MUST include
	// BOTH original and compressed file size fields.

	struct ExtraZIP64LocalFileHeader
	{
		// LocalFileHeader overrides
		uint64 lSizeUncompressed;  // uncompressed size               4->8 bytes
		uint64 lSizeCompressed;    // compressed size                 4->8 bytes

		AUTO_STRUCT_INFO;
	} PACK_GCC;

	struct ExtraZIP64CDRFileHeader
	{
		// CDRFileHeader overrides
		uint64 lSizeUncompressed;  // uncompressed size               4->8 bytes
		uint64 lSizeCompressed;    // compressed size                 4->8 bytes

		uint64 lLocalHeaderOffset; // relative offset of local header               4->8 bytes
		ulong  nDiskNumberStart;   // Number of the disk on which this file starts  2->4 bytes

		AUTO_STRUCT_INFO;
	} PACK_GCC;

	// encryption settings for zip header - stored in m_headerExtended struct
	enum EHeaderEncryptionType
	{
		HEADERS_NOT_ENCRYPTED                   = 0,
		HEADERS_ENCRYPTED_STREAMCIPHER          = 1,
		HEADERS_ENCRYPTED_TEA                   = 2, //TEA = Tiny Encryption Algorithm
		HEADERS_ENCRYPTED_STREAMCIPHER_KEYTABLE = 3, //Timur's technique. Encrypt each file and the CDR with one of 16 stream cipher keys. Encrypt the table of keys with an RSA key.
	};
	// Signature settings for zip header
	enum EHeaderSignatureType
	{
		HEADERS_NOT_SIGNED = 0,
		HEADERS_CDR_SIGNED = 1  //Includes an RSA signature based on the hash of the archive's CDR. Verified in a console compatible way.
	};

	//Header for HEADERS_ENCRYPTED_CRYCUSTOM technique. Paired with a CrySignedCDRHeader to allow for signing as well as encryption.
	//i.e. the comment section for a file that uses this technique needs the following in order:
	//CryCustomExtendedHeader, CrySignedCDRHeader, CryCustomEncryptionHeader
	struct CryCustomEncryptionHeader
	{
		// Note: Not initialized for performance reasons
		// cppcheck-suppress uninitMemberVar
		CryCustomEncryptionHeader() {}

		uint32        nHeaderSize;                                               // Size of the extended header.
		unsigned char CDR_IV[RSA_KEY_MESSAGE_LENGTH];                            //Initial Vector is actually BLOCK_CIPHER_KEY_LENGTH bytes in length, but is encrypted as a RSA_KEY_MESSAGE_LENGTH byte message.
		unsigned char keys_table[BLOCK_CIPHER_NUM_KEYS][RSA_KEY_MESSAGE_LENGTH]; //As above, actually BLOCK_CIPHER_KEY_LENGTH but encrypted.
	} PACK_GCC;

	//Header for HEADERS_SIGNED_CDR technique implemented on consoles. The comment section needs to contain the following in order:
	//CryCustomExtendedHeader, CrySignedCDRHeader
	struct CrySignedCDRHeader
	{
		// Note: Not initialized for performance reasons
		// cppcheck-suppress uninitMemberVar
		CrySignedCDRHeader() {}

		uint32        nHeaderSize; // Size of the extended header.
		unsigned char CDR_signed[RSA_KEY_MESSAGE_LENGTH];
	} PACK_GCC;

	//Stores type of encryption and signing
	struct CryCustomExtendedHeader
	{
		uint32 nHeaderSize; // Size of the extended header.
		uint16 nEncryption; // Matches one of EHeaderEncryptionType: 0 = No encryption/extension
		uint16 nSigning;    // Matches one of EHeaderSignatureType: 0 = No signing

		CryCustomExtendedHeader() : nHeaderSize(0), nEncryption(0) {}
	} PACK_GCC;

}

#undef PACK_GCC

#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
#pragma pack(pop)
#endif

#endif // _ZIP_FILE_FORMAT_HDR_