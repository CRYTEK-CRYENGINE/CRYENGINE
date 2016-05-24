#ifndef LZSS_H
#define LZSS_H

#define EXIT_SUCCESS 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define N_SIZE    4096   /* size of ring buffer */
#define F_UPPER   18     /* upper limit for match_length */
#define THRESHOLD 2      /* encode string into position and length
	                          if match_length is greater than this */
#define NIL       N_SIZE /* index for root of binary search trees */

extern unsigned long int
  textsize,   /* text size counter */
  codesize,   /* code size counter */
  printcount; /* counter for reporting progress every 1K bytes */
extern unsigned char
  text_buf[N_SIZE + F_UPPER - 1];                                 /* ring buffer of size N_SIZE,
                                                                     with extra F_UPPER-1 bytes to facilitate string comparison */
extern int match_position, match_length,                          /* of longest match.  These are
                                                                     set by the InsertNode() procedure. */
           lson[N_SIZE + 1], rson[N_SIZE + 257], dad[N_SIZE + 1]; /* left & right children &
                                                                     parents -- These constitute binary search trees. */
extern FILE* infile, * outfile;                                   /* input & output files */

int  Encodem(unsigned char* InBuf, unsigned char* OutBuf, int ASize);
bool Decodem(unsigned char* InBuf, unsigned char* OutBuf, int ASize, size_t outBufCapacity = (size_t)-1);

#endif
