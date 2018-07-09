// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <time.h>
#include <vector>
#include <algorithm>
#include <errno.h>

#pragma warning( push )
#pragma warning( disable : 4477 ) // 'printf' : format string '%s' requires an argument of type 'char *', but variadic argument 1 has type 'const wchar_t *'

//Keys
static unsigned char g_rsa_private_key_data[EAAS_PRIV_SIZE]; 
static unsigned char g_rsa_public_key_data[EAAS_PUB_SIZE];

#if defined(_ENCRYPT)
const char* g_szProgramName = "PakEncrypt";
#elif defined(_SIGN)
const char* g_szProgramName = "PakSign";
#elif defined(_DECRYPT)	//Decrypt doesn't need a private key
const char* g_szProgramName = "PakDecrypt";
//#elif defined(_VERIFY)	//Will implement verify later if I have time
//const char* g_szProgramName = "PakVerify";
#endif	//_VERIFY

//////////////////////////////////////////////////////////////////
// TCHAR stuff
//////////////////////////////////////////////////////////////////

typedef std::basic_string<TCHAR> tstring;

//////////////////////////////////////////////////////////////////
// LibTom - Encryption
//////////////////////////////////////////////////////////////////

#include <tomcrypt.h>

#define mp_count_bits(a)             ltc_mp.count_bits(a)
#define mp_unsigned_bin_size(a)      ltc_mp.unsigned_size(a)

#define STREAM_CIPHER_NAME "twofish"
prng_state g_yarrow_prng_state;


int custom_rsa_encrypt_key_ex(const unsigned char *in,     unsigned long inlen,
	unsigned char *out,    unsigned long *outlen,
	const unsigned char *lparam, unsigned long lparamlen,
	prng_state *prng, int prng_idx, int hash_idx, int padding, rsa_key *key)
{
	unsigned long modulus_bitlen, modulus_bytelen, x;
	int           err;

	LTC_ARGCHK(in     != NULL);
	LTC_ARGCHK(out    != NULL);
	LTC_ARGCHK(outlen != NULL);
	LTC_ARGCHK(key    != NULL);

	/* valid padding? */
	if ((padding != LTC_LTC_PKCS_1_V1_5) &&
		(padding != LTC_LTC_PKCS_1_OAEP)) {
			return CRYPT_PK_INVALID_PADDING;
	}

	/* valid prng? */
	if ((err = prng_is_valid(prng_idx)) != CRYPT_OK) {
		return err;
	}

	if (padding == LTC_LTC_PKCS_1_OAEP) {
		/* valid hash? */
		if ((err = hash_is_valid(hash_idx)) != CRYPT_OK) {
			return err;
		}
	}

	/* get modulus len in bits */
	modulus_bitlen = mp_count_bits( (key->N));

	/* outlen must be at least the size of the modulus */
	modulus_bytelen = mp_unsigned_bin_size( (key->N));
	if (modulus_bytelen > *outlen) {
		*outlen = modulus_bytelen;
		return CRYPT_BUFFER_OVERFLOW;
	}

	if (padding == LTC_LTC_PKCS_1_OAEP) {
		/* OAEP pad the key */
		x = *outlen;
		if ((err = pkcs_1_oaep_encode(in, inlen, lparam,
			lparamlen, modulus_bitlen, prng, prng_idx, hash_idx,
			out, &x)) != CRYPT_OK) {
				return err;
		}
	} else {
		/* LTC_PKCS #1 v1.5 pad the key */
		x = *outlen;
		if ((err = pkcs_1_v1_5_encode(in, inlen, LTC_LTC_PKCS_1_EME,
			modulus_bitlen, prng, prng_idx,
			out, &x)) != CRYPT_OK) {
				return err;
		}
	}

	/* rsa exptmod the OAEP or LTC_PKCS #1 v1.5 pad */
	return ltc_mp.rsa_me(out, x, out, outlen, PK_PRIVATE, key);
}


int custom_rsa_decrypt_key_ex(const unsigned char *in,       unsigned long  inlen,
	unsigned char *out,      unsigned long *outlen,
	const unsigned char *lparam,   unsigned long  lparamlen,
	int            hash_idx, int            padding,
	int           *stat,     rsa_key       *key)
{
	unsigned long modulus_bitlen, modulus_bytelen, x;
	int           err;
	unsigned char *tmp;

	LTC_ARGCHK(out    != NULL);
	LTC_ARGCHK(outlen != NULL);
	LTC_ARGCHK(key    != NULL);
	LTC_ARGCHK(stat   != NULL);

	/* default to invalid */
	*stat = 0;

	/* valid padding? */

	if ((padding != LTC_LTC_PKCS_1_V1_5) &&
		(padding != LTC_LTC_PKCS_1_OAEP)) {
			return CRYPT_PK_INVALID_PADDING;
	}

	if (padding == LTC_LTC_PKCS_1_OAEP) {
		/* valid hash ? */
		if ((err = hash_is_valid(hash_idx)) != CRYPT_OK) {
			return err;
		}
	}

	/* get modulus len in bits */
	modulus_bitlen = mp_count_bits( (key->N));

	/* outlen must be at least the size of the modulus */
	modulus_bytelen = mp_unsigned_bin_size( (key->N));
	if (modulus_bytelen != inlen) {
		return CRYPT_INVALID_PACKET;
	}

	/* allocate ram */
	tmp = (unsigned char*)XMALLOC(inlen);
	if (tmp == NULL) {
		return CRYPT_MEM;
	}

	/* rsa decode the packet */
	x = inlen;
	if ((err = ltc_mp.rsa_me(in, inlen, tmp, &x, PK_PUBLIC, key)) != CRYPT_OK) {
		XFREE(tmp);
		return err;
	}

	if (padding == LTC_LTC_PKCS_1_OAEP) {
		/* now OAEP decode the packet */
		err = pkcs_1_oaep_decode(tmp, x, lparam, lparamlen, modulus_bitlen, hash_idx,
			out, outlen, stat);
	} else {
		/* now LTC_PKCS #1 v1.5 depad the packet */
		err = pkcs_1_v1_5_decode(tmp, x, LTC_LTC_PKCS_1_EME, modulus_bitlen, out, outlen, stat);
	}

	XFREE(tmp);
	return err;
}


bool EncryptBufferWithStreamCipher( unsigned char *inBuffer,size_t bufferSize,unsigned char key[16],unsigned char IV[16] )
{
	symmetric_CTR ctr;
	int err;

	int cipher_idx = find_cipher(STREAM_CIPHER_NAME);
	if (cipher_idx < 0)
		return false;

	err = ctr_start( cipher_idx,IV,key,16,0,CTR_COUNTER_LITTLE_ENDIAN,&ctr);
	if (err != CRYPT_OK)
	{
		//printf("ctr_start error: %s\n",error_to_string(errno));
		return false;
	}

	err = ctr_encrypt( inBuffer,inBuffer,bufferSize,&ctr);
	if (err != CRYPT_OK)
	{
		//printf("ctr_encrypt error: %s\n", error_to_string(errno));
		return false;
	}
	ctr_done(&ctr);

	return true;
}


bool DecryptBufferWithStreamCipher( unsigned char *inBuffer,size_t bufferSize,unsigned char key[16],unsigned char IV[16] )
{
	symmetric_CTR ctr;
	int err;

	int cipher_idx = find_cipher(STREAM_CIPHER_NAME);
	if (cipher_idx < 0)
		return false;

	err = ctr_start( cipher_idx,IV,key,16,0,CTR_COUNTER_LITTLE_ENDIAN,&ctr);
	if (err != CRYPT_OK)
	{
		//printf("ctr_start error: %s\n",error_to_string(errno));
		return false;
	}

	err = ctr_decrypt( inBuffer,inBuffer,bufferSize,&ctr);
	if (err != CRYPT_OK)
	{
		//printf("ctr_encrypt error: %s\n", error_to_string(errno));
		return false;
	}
	ctr_done(&ctr);

	return true;
}


int GetEncryptionKeyIndex( unsigned long lCRC32 )
{
	return (~(lCRC32 >> 2)) & 0xF;
	//return (~(pFileEntry->nFileDataOffset >> 2)) & 0xF;
}


void GetEncryptionInitialVector( unsigned long lSizeUncompressed, unsigned long lSizeCompressed, unsigned long lCRC32, unsigned char IV[16] )
{
	unsigned int intIV[4]; //16 byte
	//intIV[0] = pFileEntry->desc.lSizeUncompressed ^ pFileEntry->nEOFOffset ^ (pFileEntry->desc.lSizeCompressed << 12);
	//intIV[1] = pFileEntry->nFileDataOffset ^ (!pFileEntry->desc.lSizeCompressed) | (pFileEntry->nLastModDate) << 15;
	//intIV[2] = pFileEntry->nFileHeaderOffset ^ pFileEntry->desc.lCRC32;
	//intIV[3] = !pFileEntry->desc.lSizeUncompressed ^ pFileEntry->desc.lSizeCompressed | (pFileEntry->nLastModTime) << 15;

	intIV[0] = lSizeUncompressed ^ (lSizeCompressed << 12);
	intIV[1] = (!lSizeCompressed);
	intIV[2] = lCRC32 ^ (lSizeCompressed << 12);
	intIV[3] = !lSizeUncompressed ^ lSizeCompressed;

	memcpy( IV,intIV,sizeof(intIV) );
}

//////////////////////////////////////////////////////////////////
// Zip - archiving
//////////////////////////////////////////////////////////////////
#include <stdint.h>
#define uint16 uint16_t
#define uint32 uint32_t
#define uint64 uint64_t
#define AUTO_STRUCT_INFO
#include "ZipFileFormat.h"

bool FindCDREnd(FILE *pArchiveFile, ZipFile::CDREnd* pCDREnd, size_t* pCommentStart)
{
	fseek(pArchiveFile,0,SEEK_END);
	long fLength = ftell(pArchiveFile);
	if(fLength < sizeof(ZipFile::CDREnd))
	{
		printf("File isn't big enough to contain a CDREnd structure");
		return false;
	}

	//Search backwards through the file for the CDREnd structure
	long nOldBufPos = fLength;
	// start scanning well before the end of the file to avoid reading beyond the end
	long nScanPos = nOldBufPos - sizeof(ZipFile::CDREnd);

	//Scan the file, but don't scan far beyond the 64k limit of the comment size
	while(nScanPos>=0 && nScanPos>(fLength - long(sizeof(ZipFile::CDREnd)) - long(0xFFFF)))
	{
		uint32 signature;
		fseek(pArchiveFile,nScanPos,SEEK_SET);
		fread((void*)&signature,1,sizeof(uint32),pArchiveFile);
		if(signature == ZipFile::CDREnd::SIGNATURE)
		{
			//Found the CDREnd signature. Extract the CDREnd and test it.
			fseek(pArchiveFile,nScanPos,SEEK_SET);
			size_t sizeRead = fread((void*)pCDREnd,1,sizeof(ZipFile::CDREnd),pArchiveFile);
			if(sizeRead == sizeof(ZipFile::CDREnd))
			{
				//Test the CDREnd by examining the length of the comment
				size_t commentLength = fLength - ftell(pArchiveFile);
				if(pCDREnd->nCommentLength == commentLength)
				{
					//Got it.
					*pCommentStart = ftell(pArchiveFile);
					//printf("Found a CDREnd structure in the file\n");
					return true;
				}
			}
			//False positive! Keep going.
		}
		//Didn't find the signature. Keep going
		nScanPos -= 1;
	}

	printf("Couldn't find a CDREnd structure\n");
	return false;
}

void FillRandomData( unsigned char *buf, size_t size )
{
	yarrow_read(buf,size,&g_yarrow_prng_state);
}

bool ProcessFileSection(FILE *in, FILE *out, unsigned long length, bool needsDecrypting, bool needsEncrypting, unsigned char *key, unsigned char *IV)
{
	//Load and if necessary decrypt the compressed file
	unsigned char *dataBuffer = new unsigned char[length];
	if(!dataBuffer)
	{
		printf("Failed to allocate %d of buffer to encrypt an entry. Is the entry too large?\n",length);
		return false;
	}

	fread(dataBuffer,1,length,in);

	if(needsDecrypting)
	{
		if( !DecryptBufferWithStreamCipher( dataBuffer, length, key, IV ) )
		{
			printf("Failed to decrypt a compressed file\n");
			delete[] dataBuffer;
			return false;
		}
	}

#if defined(_ENCRYPT)
	if(needsEncrypting)
	{
		if( !EncryptBufferWithStreamCipher( dataBuffer, length, key, IV ) )
		{
			printf("Failed to encrypt a compressed file\n");
			delete[] dataBuffer;
			return false;
		}
	}
#endif //_ENCRYPT

	fwrite(dataBuffer, 1, length, out);
	delete[] dataBuffer;

	return true;
}

bool LoadKeysFromFile(unsigned long &pubLength, unsigned long &privLength)
{
	FILE *keyFile = fopen(EAAS_KEY_FILE, "rb");
	if (!keyFile) return false;

	unsigned long length = 0;
	bool bPubLen = fread(&length, sizeof(length), 1, keyFile) == 1;
	bool bPubKey = bPubLen && (length <= EAAS_PUB_SIZE) && (fread(g_rsa_public_key_data, 1, length, keyFile) == length);
	pubLength = bPubKey ? length : 0;
	bool bPrivLen = fread(&length, sizeof(length), 1, keyFile) == 1;
	bool bPrivKey = bPrivLen && (length <= EAAS_PRIV_SIZE) && (fread(g_rsa_private_key_data, 1, length, keyFile) == length);
	privLength = bPrivKey ? length : 0;
	bool bEndOfFile = (fread(&length, 1, 1, keyFile) != 1) && (feof(keyFile) != 0);
	
	fclose(keyFile);
	return bPrivKey && bPubKey && bEndOfFile;
}

//If encrypting do the following:
//1: Generate the 16 twofish keys
//2: Go through the files in the archive and encrypt them with one of the keys, then write to the destination
//3: Sign the hash of the CDR + filename using our private RSA key
//4: Encrypt the CDR using the first twofish cipher, then write to the destination
//5: Write the CDREnd to the destination
//6: Encrypt the 16 twofish keys and append them to the comment field of the archive

//If decrypting do the following:
//1: Read the 16 twofish keys
//2: Decrypt the CDR using our public RSA key
//3: Go through the files in the archive and decrypt them using their twofish key, then write to the destination
//4: Write the unencrypted CDR to the destination
//5: Write the CDREnd to the destination with no comment

//Expects the following arguments:
//input - path to a source zip archive
//output - path to a destination zip archive

void usage()
{
	fprintf(stderr, "%s.exe - Usage:\n", g_szProgramName);
	fprintf(stderr, "%s.exe source destination\n", g_szProgramName);
	fprintf(stderr, "Where source is the input pak file and destination is the resulting pak file; They can be the same file.\n");
	fprintf(stderr, "e.g.\n");
	fprintf(stderr, "%s.exe \"c:\\pakFiles\\Animations.pak\" \"c:\\encryptedPakFiles\\Animations.pak\"\n", g_szProgramName);
}

int _tmain(int argc, _TCHAR* argv[])
{
	//Argument IDs
	const static int SOURCE_PATH = 1;
	const static int DEST_PATH = 2;
	const static int OPTIONAL_PARAMS = 3;

	if( argc < 3 )
	{
		usage();
		return 1;
	}

	unsigned long pubLength, privLength;
	if(!LoadKeysFromFile(pubLength, privLength))
	{
		fprintf(stderr, "%s.exe - The " EAAS_KEY_FILE " file is missing or invalid, first run KeyGen.exe", g_szProgramName);
		return 1;
	}

	ltc_mp = ltm_desc;
	register_hash (&sha1_desc);
	register_hash (&sha256_desc);

	register_cipher (&twofish_desc);

	int prng_idx = register_prng(&yarrow_desc);
	//fprintf(stderr, "register_prng is %d\n", prng_idx);
	prng_idx = find_prng("yarrow");
	//fprintf(stderr, "prng_idx is %d\n", prng_idx);
	assert( prng_idx != -1 );
	int make_prng_result = rng_make_prng(128, prng_idx, &g_yarrow_prng_state, NULL);
	//fprintf(stderr, "rng_make_prng returned %d\n", make_prng_result);

	int sha256 = find_hash ("sha256");
	int prngIndex = find_prng("yarrow");

	rsa_key rsa_public_key;
	if (CRYPT_OK != rsa_import(g_rsa_public_key_data, pubLength, &rsa_public_key ))
	{
		printf("Invalid public key in code");
		return 1;
	}

#if defined(_ENCRYPT) || defined(_SIGN)
	rsa_key rsa_private_key;
	if (CRYPT_OK != rsa_import(g_rsa_private_key_data, privLength, &rsa_private_key ))
	{
		printf("Invalid private key in code");
		return 1;
	}
#endif

	FILE *pSourceArchive;
	{
		errno_t fopenRet = _tfopen_s(&pSourceArchive,argv[SOURCE_PATH], _T("rb"));
		if(pSourceArchive == NULL)
		{
			printf("Failed to open source archive\n");
			return fopenRet;
		}
	}

	//Additional command line params - define the list of files not to encrypt
	std::vector<std::string> filenameExcludeList;
	for(int paramIndex = OPTIONAL_PARAMS; paramIndex < argc; paramIndex++)
	{
#if !defined(UNICODE)
		filenameExcludeList.push_back(std::string(argv[paramIndex]));
#else
		size_t paramLen = _tcslen(argv[paramIndex]);
		char *szBuf = new char[paramLen+1];
		size_t numCharsConverted;
		wcstombs_s(&numCharsConverted, szBuf, paramLen+1, argv[paramIndex], paramLen);
		szBuf[paramLen] = '\0';
		filenameExcludeList.push_back(std::string(szBuf));
		delete[] szBuf;
#endif
	}

	//Find the filename by locating the last folder separator.
	long filepathLen = _tcslen(argv[DEST_PATH]);
	long filenameLen = filepathLen;
	char *szArchiveFileNameBuffer = new char[filepathLen+1];
#if defined(UNICODE)
	size_t numCharsConverted;
	wcstombs_s(&numCharsConverted, szArchiveFileNameBuffer, filepathLen+1, argv[DEST_PATH], filepathLen);
	szArchiveFileNameBuffer[filepathLen] = '\0';
#else
	strcpy_s(szArchiveFileNameBuffer, filepathLen, argv[DEST_PATH]);
#endif
	char *szArchiveFileName = strrchr(szArchiveFileNameBuffer,'/');
	szArchiveFileName = strrchr(szArchiveFileNameBuffer,'\\') > szArchiveFileName ? strrchr(szArchiveFileNameBuffer,'\\') : szArchiveFileName;
	if(szArchiveFileName == NULL)
	{
		szArchiveFileName = szArchiveFileNameBuffer;
	}
	else
	{
		++szArchiveFileName;
		filenameLen -= ( szArchiveFileName - szArchiveFileNameBuffer );
	}
	for(int i=0; i<filenameLen; i++)
	{
		szArchiveFileName[i] = tolower(szArchiveFileName[i]);
	}

	ZipFile::CDREnd fileCDREnd;
	size_t commentStart;
	if(FindCDREnd(pSourceArchive,&fileCDREnd,&commentStart) == false)
	{
		//Zero length or corrupt
		printf("Failed to read Zip headers from source archive - Possibly corrupt or zero-length.\n");
		fclose(pSourceArchive);
		return 1;
	}

	tstring destArchiveName(argv[DEST_PATH]);
	tstring destArchiveTempName(destArchiveName);
	destArchiveTempName.append(_T(".tmp"));

	FILE *pDestArchive;
	{
		errno_t fopenRet = _tfopen_s(&pDestArchive,destArchiveTempName.c_str(), _T("wb"));
		if(pDestArchive == NULL)
		{
			printf("Failed to open destination archive\n");
			fclose(pSourceArchive);
			return fopenRet;
		}
	}

	bool bEncryptedSourceArchive = false;

	unsigned char block_cipher_keys_table[ZipFile::BLOCK_CIPHER_NUM_KEYS][ZipFile::BLOCK_CIPHER_KEY_LENGTH];
	//unsigned char block_cipher_keys_buffer[BLOCK_CIPHER_NUM_KEYS*BLOCK_CIPHER_KEY_LEN];
	unsigned char CDR_initial_vector[ZipFile::BLOCK_CIPHER_KEY_LENGTH];

	//Found the Central Directory End structure and the comment
	//Is the file encrypted?
	if(fileCDREnd.nCommentLength > sizeof(ZipFile::CryCustomExtendedHeader))
	{
		ZipFile::CryCustomExtendedHeader extendedHeader;
		ZipFile::CryCustomEncryptionHeader encryptionHeader;
		fseek(pSourceArchive, commentStart, SEEK_SET);
		fread((void*)&extendedHeader,1,sizeof(ZipFile::CryCustomExtendedHeader),pSourceArchive);
		if(extendedHeader.nHeaderSize != sizeof(ZipFile::CryCustomExtendedHeader))
		{
			//Might have been signed with old plain text signing technique; just ignore it
			extendedHeader.nEncryption = ZipFile::HEADERS_NOT_ENCRYPTED;
			extendedHeader.nSigning = ZipFile::HEADERS_NOT_SIGNED;
		}
		//Matches one of ZipDir::EHeaderEncryptionType: 0 = No encryption/extension.
		switch(extendedHeader.nEncryption)
		{
		case ZipFile::HEADERS_ENCRYPTED_STREAMCIPHER_KEYTABLE:
			//Read the required data for decryption from the custom headers in the comment section of the archive.
			//Ignore the ZipFile::CrySignedCDRHeader
			fseek(pSourceArchive,sizeof(ZipFile::CrySignedCDRHeader),SEEK_CUR);
			//Read the encryption header
			bEncryptedSourceArchive = true;
			fread((void*)&encryptionHeader,1,sizeof(ZipFile::CryCustomEncryptionHeader),pSourceArchive);
			if(encryptionHeader.nHeaderSize != sizeof(ZipFile::CryCustomEncryptionHeader))
			{
				//Probably corrupt
				printf("Failed to read encryption information from source archive - Possibly corrupt.\n");
				fclose(pSourceArchive);
				fclose(pDestArchive);
				return 1;
			}
			//Decrypt the keys table and initial vector
#if 1
			for (int i = 0; i < ZipFile::BLOCK_CIPHER_NUM_KEYS; i++)
			{
				unsigned char decryptBuffer[ZipFile::RSA_KEY_MESSAGE_LENGTH];
				unsigned long len = ZipFile::RSA_KEY_MESSAGE_LENGTH;
				int stat = 0;
				int decryptResult = custom_rsa_decrypt_key_ex(encryptionHeader.keys_table[i],len, decryptBuffer, &len, NULL, 0, sha256, LTC_LTC_PKCS_1_OAEP, &stat, &rsa_public_key );
				if( decryptResult != CRYPT_OK || stat != 1 )
				{
					printf("Failed to decrypt twofish key in table from archive\n");
					fclose(pSourceArchive);
					fclose(pDestArchive);
					return 1;
				}
				memcpy( block_cipher_keys_table[i], decryptBuffer, ZipFile::BLOCK_CIPHER_KEY_LENGTH );
			}
			{
				unsigned char decryptBuffer[ZipFile::RSA_KEY_MESSAGE_LENGTH];
				unsigned long len = ZipFile::RSA_KEY_MESSAGE_LENGTH;
				int stat = 0;
				int decryptResult = custom_rsa_decrypt_key_ex(encryptionHeader.CDR_IV, len, decryptBuffer, &len, NULL, 0, sha256, LTC_LTC_PKCS_1_OAEP, &stat, &rsa_public_key );
				if( decryptResult != CRYPT_OK || stat != 1 )
				{
					printf("Failed to decrypt initial vector from archive\n");
					fclose(pSourceArchive);
					fclose(pDestArchive);
					return 1;
				}
				memcpy( CDR_initial_vector, decryptBuffer, ZipFile::BLOCK_CIPHER_KEY_LENGTH );
			}
#else
			memcpy(block_cipher_keys_table, encryptionHeader.keys_table, sizeof(block_cipher_keys_table));
			memcpy(CDR_initial_vector, encryptionHeader.CDR_IV, sizeof(CDR_initial_vector));
#endif
			break;
		case ZipFile::HEADERS_NOT_ENCRYPTED:
			//Do nothing
			break;
		default:
			printf("Found an unsupported encryption type: %d",extendedHeader.nEncryption);
			fclose(pSourceArchive);
			fclose(pDestArchive);
			return 1;
		}
	}

	if(!bEncryptedSourceArchive)
	{
		//File not already encrypted. Generate encryption data for stream cipher.
		//Generate an initial vector (IV) for stream cipher.
		FillRandomData(CDR_initial_vector, ZipFile::BLOCK_CIPHER_KEY_LENGTH);
		//Generate the 16 twofish cipher keys to encrypt the archive CDR and contents.
		for (int i = 0; i < ZipFile::BLOCK_CIPHER_NUM_KEYS; i++)
		{
			FillRandomData(block_cipher_keys_table[i],ZipFile::BLOCK_CIPHER_KEY_LENGTH);
		}
	}

	//CDR signature
	unsigned char signature[1024];
	unsigned long signatureLen = 1024;
	//Start by reading the entire CDR into memory if it exists
	if(fileCDREnd.lCDRSize > 0)
	{
		unsigned char *fileCDR = new unsigned char[fileCDREnd.lCDRSize];
		if(!fileCDR)
		{
			printf("Failed to allocate %d of buffer to load the Central Directory Record. Is this archive too large?\n",fileCDREnd.lCDRSize);
			fclose(pSourceArchive);
			fclose(pDestArchive);
			return 1;
		}

		fseek(pSourceArchive,fileCDREnd.lCDROffset,SEEK_SET);
		fread(fileCDR,1,fileCDREnd.lCDRSize,pSourceArchive);

		//Decrypt the CDR if necessary
		if(bEncryptedSourceArchive)
		{
			//CDR header is encrypted using the first key and a stored initial vector
			if( !DecryptBufferWithStreamCipher( fileCDR, fileCDREnd.lCDRSize, block_cipher_keys_table[0], CDR_initial_vector ) )
			{
				printf("Failed to decrypt the CDR from archive\n");
				fclose(pSourceArchive);
				fclose(pDestArchive);
				delete[] fileCDR;
				return 1;
			}
		}

		struct ExtendedCDRFileHeader
		{
			ZipFile::CDRFileHeader m_CDRHeader;
			bool bEncrypt;
		};

		struct less_than_offset
		{
			inline bool operator() (const ExtendedCDRFileHeader& struct1, const ExtendedCDRFileHeader& struct2)
			{
				return (struct1.m_CDRHeader.lLocalHeaderOffset < struct2.m_CDRHeader.lLocalHeaderOffset);
			}
		};

		std::vector<ExtendedCDRFileHeader> sortedCDRHeaders;
		sortedCDRHeaders.reserve(fileCDREnd.numEntriesTotal);

		//Parse each of the files in the archive
		//This will also poke new values into the file structures in-memory, to support new per-file method identifiers
		unsigned int currentCDRPos = 0;
		for(int fileIndex = 0; fileIndex < fileCDREnd.numEntriesTotal; fileIndex++)
		{
			ZipFile::CDRFileHeader* pfileCDRRecord = (ZipFile::CDRFileHeader*)(fileCDR+currentCDRPos);

			//Any additional parameters on the command line will be the paths of files to exclude from encryption
			//Check if the filename matches any of these
			bool bEncrypt = true;
			if(pfileCDRRecord->nFileNameLength > 0 && filenameExcludeList.size() > 0)
			{
				char *szFileName = new char[pfileCDRRecord->nFileNameLength+1];
				memcpy(szFileName, fileCDR+currentCDRPos+sizeof(ZipFile::CDRFileHeader), pfileCDRRecord->nFileNameLength);
				szFileName[pfileCDRRecord->nFileNameLength] = 0;
				for(unsigned short charIndex=0; charIndex<pfileCDRRecord->nFileNameLength; charIndex++)
				{
					szFileName[charIndex] = tolower(szFileName[charIndex]);
				}
				for( size_t excludeListIndex=0; excludeListIndex < filenameExcludeList.size(); excludeListIndex++ )
				{
					if(!strcmp(szFileName, filenameExcludeList[excludeListIndex].c_str()))
					{
						bEncrypt = false;
					}
				}
				delete[] szFileName;
			}

#if !defined(_ENCRYPT)
			if(bEncryptedSourceArchive)
			{
				//Decrypting pak. Method needs to be set from one of the encrypted methods to one of the standard methods
				pfileCDRRecord->nMethod = pfileCDRRecord->nMethod == ZipFile::METHOD_DEFLATE_AND_STREAMCIPHER_KEYTABLE ? ZipFile::METHOD_DEFLATE : pfileCDRRecord->nMethod;
				pfileCDRRecord->nMethod = pfileCDRRecord->nMethod == ZipFile::METHOD_STORE_AND_STREAMCIPHER_KEYTABLE ? ZipFile::METHOD_STORE : pfileCDRRecord->nMethod;
			}
#endif	//_ENCRYPT

#if defined(_ENCRYPT)
			if(bEncrypt)
			{
				//If encrypting, set the desired per-file encryption technique
				pfileCDRRecord->nMethod = pfileCDRRecord->nMethod == ZipFile::METHOD_DEFLATE ? ZipFile::METHOD_DEFLATE_AND_STREAMCIPHER_KEYTABLE : pfileCDRRecord->nMethod;
				pfileCDRRecord->nMethod = pfileCDRRecord->nMethod == ZipFile::METHOD_STORE ? ZipFile::METHOD_STORE_AND_STREAMCIPHER_KEYTABLE : pfileCDRRecord->nMethod;
			}
#endif //_ENCRYPT

			//Place into an extended header wrapper to store extra info
			ExtendedCDRFileHeader extHeader;
			memcpy(&extHeader.m_CDRHeader, pfileCDRRecord, sizeof(ZipFile::CDRFileHeader));
			extHeader.bEncrypt = bEncrypt;

			//Put the header into a vector for sorting
			sortedCDRHeaders.push_back(extHeader);
			currentCDRPos += sizeof(ZipFile::CDRFileHeader) + pfileCDRRecord->nFileNameLength + pfileCDRRecord->nExtraFieldLength + pfileCDRRecord->nFileCommentLength;
		}

		std::sort(sortedCDRHeaders.begin(), sortedCDRHeaders.end(), less_than_offset());

		unsigned short nextLog=fileCDREnd.numEntriesTotal / 10;
		unsigned short currentLog=nextLog;
		printf("Processing %d files:\n",fileCDREnd.numEntriesTotal);
		for(unsigned short fileIndex = 0; fileIndex < fileCDREnd.numEntriesTotal; fileIndex++)
		{
			if(fileIndex > currentLog)
			{
				printf("\t%d%%",int(((100.0f/float(fileCDREnd.numEntriesTotal))*float(fileIndex))+0.5f));
				currentLog += nextLog;
			}

			ZipFile::CDRFileHeader& fileCDRRecord = sortedCDRHeaders[fileIndex].m_CDRHeader;

			//Get the initial vector and twofish key index
			unsigned char file_initial_vector[ZipFile::BLOCK_CIPHER_KEY_LENGTH];
			GetEncryptionInitialVector( fileCDRRecord.desc.lSizeUncompressed, fileCDRRecord.desc.lSizeCompressed, fileCDRRecord.desc.lCRC32, file_initial_vector );
			int encryptionKeyIndex = GetEncryptionKeyIndex(fileCDRRecord.desc.lCRC32);

			//Seek to the file in the archive.
			fseek(pSourceArchive,fileCDRRecord.lLocalHeaderOffset,SEEK_SET);

			//Read in the local file header
			ZipFile::LocalFileHeader fileLocalRecord;
			fread(&fileLocalRecord,1,sizeof(ZipFile::LocalFileHeader),pSourceArchive);
			if(bEncryptedSourceArchive)
			{
				if(!DecryptBufferWithStreamCipher( (unsigned char*)&fileLocalRecord, sizeof(ZipFile::LocalFileHeader), block_cipher_keys_table[encryptionKeyIndex], file_initial_vector ))
				{
					printf("Failed to decrypt a compressed local file header\n");
					fclose(pSourceArchive); fclose(pDestArchive); return 1;
				}
			}
			//Seek back so we can read in the entire header with name and extra data later
			fseek(pSourceArchive,fileCDRRecord.lLocalHeaderOffset,SEEK_SET);

			long destPos = ftell(pDestArchive);
			long sourcePos = ftell(pSourceArchive);
			//Source archive should never seek backwards
			assert( destPos <= sourcePos );
			if(destPos<sourcePos)
			{
				//Empty space in archive. Pad it.
				const long bufSize = 65535;
				unsigned char buffer[bufSize];
				memset(buffer,0,sizeof(buffer));
				while(destPos<sourcePos)
				{
					long diff = sourcePos - destPos;
					fwrite( buffer, 1, diff < bufSize ? diff : bufSize, pDestArchive );
					destPos = ftell( pDestArchive );
				}
			}

			destPos = ftell(pDestArchive);
			//Load and if necessary decrypt the local file header
			long localHeaderLength = sizeof(ZipFile::LocalFileHeader) + fileLocalRecord.nFileNameLength + fileLocalRecord.nExtraFieldLength;
			if( !ProcessFileSection( pSourceArchive, pDestArchive, localHeaderLength, bEncryptedSourceArchive, sortedCDRHeaders[fileIndex].bEncrypt, block_cipher_keys_table[encryptionKeyIndex], file_initial_vector ) )
			{
				fclose(pSourceArchive);
				fclose(pDestArchive);
				delete[] fileCDR;
				return 1;
			}

			//Load and if necessary decrypt the compressed file
			if( !ProcessFileSection( pSourceArchive, pDestArchive, fileCDRRecord.desc.lSizeCompressed, bEncryptedSourceArchive, sortedCDRHeaders[fileIndex].bEncrypt, block_cipher_keys_table[encryptionKeyIndex], file_initial_vector ) )
			{
				fclose(pSourceArchive);
				fclose(pDestArchive);
				delete[] fileCDR;
				return 1;
			}

			//If there's an additional structure, read that in and compress it too.
			if( fileLocalRecord.nFlags & 0x08 )
			{
				unsigned long extraSize = sizeof(ZipFile::DataDescriptor);
				long currentPos = ftell(pSourceArchive);
				//Check for the extra optional signature of the extended section
				uint32 possibleSignature;
				fread(&possibleSignature,1,sizeof(uint32),pSourceArchive);
				if(bEncryptedSourceArchive)
				{
					DecryptBufferWithStreamCipher( (unsigned char*)&possibleSignature, sizeof(uint32), block_cipher_keys_table[encryptionKeyIndex], file_initial_vector );
				}
				if(possibleSignature == 0x08074b50)
				{
					extraSize += sizeof(uint32);
				}
				//Rewind
				fseek(pSourceArchive,currentPos,SEEK_SET);
				//Add the extra section, whether it contains the extra data or not
				if( !ProcessFileSection( pSourceArchive, pDestArchive, extraSize, bEncryptedSourceArchive, sortedCDRHeaders[fileIndex].bEncrypt, block_cipher_keys_table[encryptionKeyIndex], file_initial_vector ) )
				{
					fclose(pSourceArchive);
					fclose(pDestArchive);
					delete[] fileCDR;
					return 1;
				}
			}
		}
		printf("\t%d files processed.\n",fileCDREnd.numEntriesTotal);

		//Generate a sha256 hash from the CDR
		hash_state md;
		hash_descriptor[sha256].init(&md);
		hash_descriptor[sha256].process(&md, fileCDR, fileCDREnd.lCDRSize);
		//Append the filename for extra security
		hash_descriptor[sha256].process(&md, (const unsigned char *)szArchiveFileName, filenameLen);
		delete[] szArchiveFileNameBuffer;

		//Compose a digest
		unsigned char hash_digest[32]; // 32 bytes should be enough
		memset(hash_digest, 0, sizeof(hash_digest));
		hash_descriptor[sha256].done(&md, hash_digest); // 32 bytes

		memset(signature, 0, sizeof(signature));
		signatureLen = ZipFile::RSA_KEY_MESSAGE_LENGTH;

#if defined(_ENCRYPT) || defined(_SIGN)
		//Generate a signature with our private key
		int signRes = rsa_sign_hash(hash_digest, 32, signature, &signatureLen, &g_yarrow_prng_state, prng_idx, sha256, 0, &rsa_private_key );
#endif	//_ENCRYPT || _SIGN

#if defined(_ENCRYPT)
		//All the files have been read. Encrypt the CDR header
		EncryptBufferWithStreamCipher(fileCDR, fileCDREnd.lCDRSize, block_cipher_keys_table[0], CDR_initial_vector);
#endif	//_ENCRYPT

		if( fwrite(fileCDR, 1, fileCDREnd.lCDRSize, pDestArchive) != fileCDREnd.lCDRSize )
		{
			fclose(pSourceArchive);
			fclose(pDestArchive);
			delete[] fileCDR;
			return 1;
		}

		delete[] fileCDR;

#if defined(_ENCRYPT) || defined(_SIGN)
		if(signRes != CRYPT_OK)
		{
			printf("rsa_sign_hash returned %d - expected %d\n", signRes, CRYPT_OK);
			fclose(pSourceArchive);
			fclose(pDestArchive);
			return 1;
		}

		if(signatureLen != ZipFile::RSA_KEY_MESSAGE_LENGTH)
		{
			printf("rsa_sign_hash created a message that was not %d long as the engine expects. Are you using the wrong sized key?\n",ZipFile::RSA_KEY_MESSAGE_LENGTH);
			return 1;
		}
#endif	//_ENCRYPT || _SIGN
	}
	else
	{
		//No CDR! Hence no signature
		memset(signature,0,sizeof(signature));
		signatureLen = ZipFile::RSA_KEY_MESSAGE_LENGTH;
	}

#if defined(_ENCRYPT)	
	//Almost done. First, set the length of the comment field to equal the length of the structs we're packing in there.
	fileCDREnd.nCommentLength = sizeof(ZipFile::CryCustomExtendedHeader) + sizeof(ZipFile::CrySignedCDRHeader) + sizeof(ZipFile::CryCustomEncryptionHeader);
	fwrite(&fileCDREnd,1,sizeof(ZipFile::CDREnd),pDestArchive);

	//Pack the custom header, set the expected technique (HEADERS_ENCRYPTED_CRYCUSTOM)
	ZipFile::CryCustomExtendedHeader extHeader;
	extHeader.nHeaderSize = sizeof(ZipFile::CryCustomExtendedHeader);
	extHeader.nEncryption = ZipFile::HEADERS_ENCRYPTED_STREAMCIPHER_KEYTABLE;
	extHeader.nSigning = ZipFile::HEADERS_CDR_SIGNED;
	fwrite(&extHeader,1,sizeof(ZipFile::CryCustomExtendedHeader),pDestArchive);

	//Pack the CDR Signature header
	ZipFile::CrySignedCDRHeader signedHeader;
	signedHeader.nHeaderSize = sizeof(ZipFile::CrySignedCDRHeader);
	memcpy(signedHeader.CDR_signed, signature, signatureLen);
	fwrite(&signedHeader,1,sizeof(ZipFile::CrySignedCDRHeader),pDestArchive);

	//Setup the encryption header. Encrypt the initial vector and each twofish key
	ZipFile::CryCustomEncryptionHeader encHeader;
	encHeader.nHeaderSize = sizeof(ZipFile::CryCustomEncryptionHeader);

	unsigned long bufferSize = sizeof(encHeader.CDR_IV);
	int res = custom_rsa_encrypt_key_ex( CDR_initial_vector, sizeof(CDR_initial_vector), encHeader.CDR_IV, &bufferSize, NULL, 0, &g_yarrow_prng_state, prng_idx, sha256, LTC_LTC_PKCS_1_OAEP, &rsa_private_key);
	if (res != CRYPT_OK)
	{
		printf("custom_rsa_encrypt_key_ex returned %s\n", error_to_string(res));
		return 1;
	}
	for( int i = 0; i < ZipFile::BLOCK_CIPHER_NUM_KEYS; i++ )
	{
		bufferSize = ZipFile::RSA_KEY_MESSAGE_LENGTH;
		int res = custom_rsa_encrypt_key_ex( block_cipher_keys_table[i], sizeof(block_cipher_keys_table[i]), encHeader.keys_table[i], &bufferSize, NULL, 0, &g_yarrow_prng_state, prng_idx, sha256, LTC_LTC_PKCS_1_OAEP, &rsa_private_key);
		if (res != CRYPT_OK)
		{
			printf("custom_rsa_encrypt_key_ex returned %s\n", error_to_string(res));
			return 1;
		}
	}

	//Write it to the file
	fwrite(&encHeader,1,sizeof(ZipFile::CryCustomEncryptionHeader),pDestArchive);
#elif defined(_DECRYPT)
	//Knock out the comment
	fileCDREnd.nCommentLength = 0;
	fwrite(&fileCDREnd,1,sizeof(ZipFile::CDREnd),pDestArchive);
#elif defined(_SIGN)
	//Almost done. First, set the length of the comment field to equal the length of the structs we're packing in there.
	fileCDREnd.nCommentLength = sizeof(ZipFile::CryCustomExtendedHeader) + sizeof(ZipFile::CrySignedCDRHeader);
	fwrite(&fileCDREnd,1,sizeof(ZipFile::CDREnd),pDestArchive);

	//Pack the custom header, set the expected technique (HEADERS_SIGNED_CRC)
	ZipFile::CryCustomExtendedHeader extHeader;
	extHeader.nHeaderSize = sizeof(ZipFile::CryCustomExtendedHeader);
	extHeader.nEncryption = ZipFile::HEADERS_NOT_ENCRYPTED;
	extHeader.nSigning = ZipFile::HEADERS_CDR_SIGNED;
	fwrite(&extHeader,1,sizeof(ZipFile::CryCustomExtendedHeader),pDestArchive);

	//Pack the CDR Signature header
	ZipFile::CrySignedCDRHeader signedHeader;
	signedHeader.nHeaderSize = sizeof(ZipFile::CrySignedCDRHeader);
	memcpy(signedHeader.CDR_signed, signature, signatureLen);
	fwrite(&signedHeader,1,sizeof(ZipFile::CrySignedCDRHeader),pDestArchive);
#endif

	fclose(pSourceArchive);
	fclose(pDestArchive);

	//Finalise the encryption by moving the temp file to the final location
	int error_no;
	if( _tunlink(destArchiveName.c_str()) )
	{
		_get_errno(&error_no);
		if( error_no != ENOENT )	//Can ignore 'file not found'
		{
			printf("Failed to delete old destination pak. _tunlink returned %d",error_no);
			return error_no;
		}
	}
	error_no = _trename(destArchiveTempName.c_str(),destArchiveName.c_str());
	if(error_no != 0)
	{
		printf( "Failed to rename temp file (%ls) to destination file (%ls), _trename returned %d", destArchiveName.c_str(), destArchiveTempName.c_str(), error_no );
		return error_no;
	}

	return 0;
}

#pragma warning( pop )