// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _ZIP_FILE_FORMAT_HDR_
#define _ZIP_FILE_FORMAT_HDR_

#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE
	#pragma pack(push)
	#pragma pack(1)
	#define PACK_GCC
#else
	#define PACK_GCC __attribute__ ((packed))
#endif

namespace ZipFile
{
// General-purpose bit field flags
enum
{
	GPF_ENCRYPTED                     = 1,      // If set, indicates that the file is encrypted.
	GPF_DATA_DESCRIPTOR               = 1 << 3, // if set, the CRC32 and sizes aren't set in the file header, but only in the data descriptor following compressed data
	GPF_RESERVED_8_ENHANCED_DEFLATING = 1 << 4, // Reserved for use with method 8, for enhanced deflating.
	GPF_COMPRESSED_PATCHED            = 1 << 5, // the file is compressed patched data
};

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
	RSA_KEY_MESSAGE_LENGTH = 128    //The modulus of our private/public key pair for signing, verification, encryption and decryption
};

// compression methods
enum
{
	METHOD_STORE                             = 0,  // The file is stored (no compression)
	METHOD_SHRINK                            = 1,  // The file is Shrunk
	METHOD_REDUCE_1                          = 2,  // The file is Reduced with compression factor 1
	METHOD_REDUCE_2                          = 3,  // The file is Reduced with compression factor 2
	METHOD_REDUCE_3                          = 4,  // The file is Reduced with compression factor 3
	METHOD_REDUCE_4                          = 5,  // The file is Reduced with compression factor 4
	METHOD_IMPLODE                           = 6,  // The file is Imploded
	METHOD_TOKENIZE                          = 7,  // Reserved for Tokenizing compression algorithm
	METHOD_DEFLATE                           = 8,  // The file is Deflated
	METHOD_DEFLATE64                         = 9,  // Enhanced Deflating using Deflate64(tm)
	METHOD_IMPLODE_PKWARE                    = 10, // PKWARE Date Compression Library Imploding
	METHOD_DEFLATE_AND_ENCRYPT               = 11, // Deflate + Custom encryption (TEA)
	METHOD_DEFLATE_AND_STREAMCIPHER          = 12, // Deflate + stream cipher encryption on a per file basis
	METHOD_STORE_AND_STREAMCIPHER_KEYTABLE   = 13, // Store + Timur's encryption technique on a per file basis
	METHOD_DEFLATE_AND_STREAMCIPHER_KEYTABLE = 14, // Deflate + Timur's encryption technique on a per file basis
};

// end of Central Directory Record
// followed by the .zip file comment (variable size, can be empty, obtained from nCommentLength)
struct CDREnd
{
	enum {SIGNATURE = 0x06054b50};
	uint32 lSignature;         // end of central dir signature    4 bytes  (0x06054b50)
	uint16 nDisk;              // number of this disk             2 bytes
	uint16 nCDRStartDisk;      // number of the disk with the start of the central directory  2 bytes
	uint16 numEntriesOnDisk;   // total number of entries in the central directory on this disk  2 bytes
	uint16 numEntriesTotal;    // total number of entries in the central directory           2 bytes
	uint32 lCDRSize;           // size of the central directory   4 bytes
	uint32 lCDROffset;         // offset of start of central directory with respect to the starting disk number        4 bytes
	uint16 nCommentLength;     // .ZIP file comment length        2 bytes

	AUTO_STRUCT_INFO;

	// .ZIP file comment (variable size, can be empty) follows
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

// This descriptor exists only if bit 3 of the general
// purpose bit flag is set (see below).  It is byte aligned
// and immediately follows the last byte of compressed data.
// This descriptor is used only when it was not possible to
// seek in the output .ZIP file, e.g., when the output .ZIP file
// was standard output or a non seekable device.  For Zip64 format
// archives, the compressed and uncompressed sizes are 8 bytes each.
struct DataDescriptor
{
	uint32 lCRC32;               // crc-32                          4 bytes
	uint32 lSizeCompressed;      // compressed size                 4 bytes
	uint32 lSizeUncompressed;    // uncompressed size               4 bytes

	bool operator==(const DataDescriptor& d) const
	{
		return lCRC32 == d.lCRC32 && lSizeCompressed == d.lSizeCompressed && lSizeUncompressed == d.lSizeUncompressed;
	}
	bool operator!=(const DataDescriptor& d) const
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
	uint32         lSignature;     // central file header signature   4 bytes  (0x02014b50)
	uint16         nVersionMadeBy; // version made by                 2 bytes
	uint16         nVersionNeeded; // version needed to extract       2 bytes
	uint16         nFlags;         // general purpose bit flag        2 bytes
	uint16         nMethod;        // compression method              2 bytes
	uint16         nLastModTime;   // last mod file time              2 bytes
	uint16         nLastModDate;   // last mod file date              2 bytes
	DataDescriptor desc;
	uint16         nFileNameLength;    // file name length                2 bytes
	uint16         nExtraFieldLength;  // extra field length              2 bytes
	uint16         nFileCommentLength; // file comment length             2 bytes
	uint16         nDiskNumberStart;   // disk number start               2 bytes
	uint16         nAttrInternal;      // internal file attributes        2 bytes
	uint32         lAttrExternal;      // external file attributes        4 bytes

	// This is the offset from the start of the first disk on
	// which this file appears, to where the local header should
	// be found.  If an archive is in zip64 format and the value
	// in this field is 0xFFFFFFFF, the size will be in the
	// corresponding 8 byte zip64 extended information extra field.
	enum {ZIP64_LOCAL_HEADER_OFFSET = 0xFFFFFFFF};
	uint32 lLocalHeaderOffset;    // relative offset of local header 4 bytes

	AUTO_STRUCT_INFO;
} PACK_GCC;

// this is the local file header that appears before the compressed data
// followed by:
//    file name (variable size)
//    extra field (variable size)
struct LocalFileHeader
{
	enum {SIGNATURE = 0x04034b50};
	uint32         lSignature;     // local file header signature     4 bytes  (0x04034b50)
	uint16         nVersionNeeded; // version needed to extract       2 bytes
	uint16         nFlags;         // general purpose bit flag        2 bytes
	uint16         nMethod;        // compression method              2 bytes
	uint16         nLastModTime;   // last mod file time              2 bytes
	uint16         nLastModDate;   // last mod file date              2 bytes
	DataDescriptor desc;
	uint16         nFileNameLength;   // file name length                2 bytes
	uint16         nExtraFieldLength; // extra field length              2 bytes

	AUTO_STRUCT_INFO;
} PACK_GCC;

// compression methods
enum EExtraHeaderID
{
	EXTRA_ZIP64 = 0x0001,    //  ZIP64 extended information extra field
	EXTRA_NTFS  = 0x000a,    //  NTFS
};

//////////////////////////////////////////////////////////////////////////
// header1+data1 + header2+data2 . . .
// Each header should consist of:
//		Header ID - 2 bytes
//    Data Size - 2 bytes
struct ExtraFieldHeader
{
	uint16 headerID;
	uint16 dataSize;
};
struct ExtraNTFSHeader
{
	uint32 reserved;   // 4 bytes.
	uint16 attrTag;    // 2 bytes.
	uint16 attrSize;   // 2 bytes.
};
}

#undef PACK_GCC

#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE
	#pragma pack(pop)
#endif

#endif // _ZIP_FILE_FORMAT_HDR_
