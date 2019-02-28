// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if CRY_PLATFORM_DURANGO
	#pragma warning(disable: 4530)
#endif

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

unsigned long int
  textsize = 0,                                            /* text size counter */
  printcount = 0;                                          /* counter for reporting progress every 1K bytes */
int match_position, match_length,                          /* of longest match.  These are
                                                              set by the InsertNode() procedure. */
    lson[N_SIZE + 1], rson[N_SIZE + 257], dad[N_SIZE + 1]; /* left & right children &
                                                              parents -- These constitute binary search trees. */
FILE* infile, * outfile;                                   /* input & output files */

void InitTree(void)  /* initialize trees */
{
	int i;

	/* For i = 0 to N_SIZE - 1, rson[i] and lson[i] will be the right and
	   left children of node i.  These nodes need not be initialized.
	   Also, dad[i] is the parent of node i.  These are initialized to
	   NIL (= N_SIZE), which stands for 'not used.'
	   For i = 0 to 255, rson[N_SIZE + i + 1] is the root of the tree
	   for strings that begin with character i.  These are initialized
	   to NIL.  Note there are 256 trees. */

	for (i = N_SIZE + 1; i <= N_SIZE + 256; i++)
		rson[i] = NIL;
	for (i = 0; i < N_SIZE; i++)
		dad[i] = NIL;
}

void InsertNode(int r, unsigned char text_buf[])
/* Inserts string of length F_UPPER, text_buf[r..r+F_UPPER-1], into one of the
   trees (text_buf[r]'th tree) and returns the longest-match position
   and length via the global variables match_position and match_length.
   If match_length = F_UPPER, then removes the old node in favor of the new
   one, because the old one will be deleted sooner.
   Note r plays double role, as tree node and position in buffer. */
{
	int i, p, cmp;
	unsigned char* key;

	cmp = 1;
	key = &text_buf[r];
	p = N_SIZE + 1 + key[0];
	rson[r] = lson[r] = NIL;
	match_length = 0;
	for (;; )
	{
		if (cmp >= 0)
		{
			if (rson[p] != NIL) p = rson[p];
			else { rson[p] = r;  dad[r] = p;  return;  }
		}
		else
		{
			if (lson[p] != NIL) p = lson[p];
			else { lson[p] = r;  dad[r] = p;  return;  }
		}
		for (i = 1; i < F_UPPER; i++)
			if ((cmp = key[i] - text_buf[p + i]) != 0)  break;
		if (i > match_length)
		{
			match_position = p;
			if ((match_length = i) >= F_UPPER)  break;
		}
	}
	dad[r] = dad[p];
	lson[r] = lson[p];
	rson[r] = rson[p];
	dad[lson[p]] = r;
	dad[rson[p]] = r;
	if (rson[dad[p]] == p) rson[dad[p]] = r;
	else                   lson[dad[p]] = r;
	dad[p] = NIL;  /* remove p */
}

void DeleteNode(int p)  /* deletes node p from tree */
{
	int q;

	if (dad[p] == NIL) return;  /* not in tree */
	if (rson[p] == NIL) q = lson[p];
	else if (lson[p] == NIL)
		q = rson[p];
	else
	{
		q = lson[p];
		if (rson[q] != NIL)
		{
			do
			{
				q = rson[q];
			}
			while (rson[q] != NIL);
			rson[dad[q]] = lson[q];
			dad[lson[q]] = dad[q];
			lson[q] = lson[p];
			dad[lson[p]] = q;
		}
		rson[q] = rson[p];
		dad[rson[p]] = q;
	}
	dad[q] = dad[p];
	if (rson[dad[p]] == p) rson[dad[p]] = q;  else lson[dad[p]] = q;
	dad[p] = NIL;
}

inline int getcm(int& Count, int Size, unsigned char*& getcmP)
{
	if (Count++ >= Size)
		return EOF;
	return (int)*getcmP++;
};

inline void putcm(int c, unsigned char*& putcmP)
{
	*putcmP++ = (unsigned char)c;
};

inline bool putcm(int c, unsigned char*& putcmP, unsigned char* putcmpPEnd)
{
	if (putcmP != putcmpPEnd)
	{
		*putcmP++ = (unsigned char)c;
		return true;
	}
	return false;
};

int Encodem(unsigned char* getcmP, unsigned char* putcmP, int Size)
{
	unsigned char text_buf[N_SIZE + F_UPPER - 1]; /* ring buffer of size N_SIZE, with extra F_UPPER-1 bytes to facilitate string comparison */
	int Count = 0;
	int i, c, len, r, s, last_match_length, code_buf_ptr;
	unsigned char code_buf[17], mask;
	unsigned int codesize = 0;
	InitTree();      /* initialize trees */
	code_buf[0] = 0; /* code_buf[1..16] saves eight units of code, and
	                    code_buf[0] works as eight flags, "1" representing that the unit
	                    is an unencoded letter (1 byte), "0" a position-and-length pair
	                    (2 bytes).  Thus, eight units require at most 16 bytes of code. */
	code_buf_ptr = mask = 1;
	s = 0;
	r = N_SIZE - F_UPPER;
	for (i = s; i < r; i++)
		text_buf[i] = ' ';                        /* Clear the buffer with
	                                               any character that will appear often. */
	for (len = 0; len < F_UPPER && (c = getcm(Count, Size, getcmP)) != EOF; len++)
		text_buf[r + len] = c;  /* Read F_UPPER bytes into the last F_UPPER bytes of
	                             the buffer */
	if ((textsize = len) == 0)
		return codesize;  /* text of size zero */
	for (i = 1; i <= F_UPPER; i++)
		InsertNode(r - i, text_buf);  /* Insert the F_UPPER strings,
	                                   each of which begins with one or more 'space' characters.  Note
	                                   the order in which these strings are inserted.  This way,
	                                   degenerate trees will be less likely to occur. */
	InsertNode(r, text_buf);        /* Finally, insert the whole string just read.  The
	                                   global variables match_length and match_position are set. */
	do
	{
		if (match_length > len) match_length = len;  /* match_length
		                                                may be spuriously long near the end of text. */
		if (match_length <= THRESHOLD)
		{
			match_length = 1;                       /* Not long enough match.  Send one byte. */
			code_buf[0] |= mask;                    /* 'send one byte' flag */
			code_buf[code_buf_ptr++] = text_buf[r]; /* Send uncoded. */
		}
		else
		{
			code_buf[code_buf_ptr++] = (unsigned char) match_position;
			code_buf[code_buf_ptr++] = (unsigned char)
			                           (((match_position >> 4) & 0xf0)
			                            | (match_length - (THRESHOLD + 1))); /* Send position and
			                                                                    length pair. Note match_length > THRESHOLD. */
		}
		if ((mask <<= 1) == 0)    /* Shift mask left one bit. */
		{
			for (i = 0; i < code_buf_ptr; i++) /* Send at most 8 units of */
				putcm(code_buf[i], putcmP);      /* code together */
			codesize += code_buf_ptr;
			code_buf[0] = 0;
			code_buf_ptr = mask = 1;
		}
		last_match_length = match_length;
		for (i = 0; i < last_match_length &&
		     (c = getcm(Count, Size, getcmP)) != EOF; i++)
		{
			DeleteNode(s);                                 /* Delete old strings and */
			text_buf[s] = c;                               /* read new bytes */
			if (s < F_UPPER - 1) text_buf[s + N_SIZE] = c; /* If the position is
			                                                  near the end of buffer, extend the buffer to make
			                                                  string comparison easier. */
			s = (s + 1) & (N_SIZE - 1);
			r = (r + 1) & (N_SIZE - 1);
			/* Since this is a ring buffer, increment the position
			   modulo N_SIZE. */
			InsertNode(r, text_buf);  /* Register the string in text_buf[r..r+F_UPPER-1] */
		}
		///
		textsize += i;

		//if ((textsize += i) > printcount) {
		//  printf("%12ld\r", textsize);  printcount += 1024;
		//    /* Reports progress each time the textsize exceeds
		//       multiples of 1024. */
		//}

		////////

		while (i++ < last_match_length)   /* After the end of text, */
		{
			DeleteNode(s);          /* no need to read, but */
			s = (s + 1) & (N_SIZE - 1);
			r = (r + 1) & (N_SIZE - 1);
			if (--len)
				InsertNode(r, text_buf);    /* buffer may not be empty. */
		}
	}
	while (len > 0);    /* until length of string to be processed is zero */
	if (code_buf_ptr > 1)
	{
		/* Send remaining code. */
		for (i = 0; i < code_buf_ptr; i++)
			putcm(code_buf[i], putcmP);
		codesize += code_buf_ptr;
	}
	//  printf("In : %ld bytes\n", textsize); /* Encoding is done. */
	//  printf("Out: %ld bytes\n", codesize);
	//  printf("Out/In: %.3f\n", (double)codesize / textsize);

	return codesize;
}

// Thread safe
bool Decodem(unsigned char* getcmP, unsigned char* putcmP, int Size, size_t outBufCapacity) /* Just the reverse of Encode(). */
{
	int i, j, k, r, c;
	unsigned int flags;
	int Count = 0;
	unsigned char text_buf[N_SIZE + F_UPPER - 1]; /* ring buffer of size N_SIZE, with extra F_UPPER-1 bytes to facilitate string comparison */
	unsigned char* putcmpPEnd = putcmP + outBufCapacity;

	for (i = 0; i < N_SIZE - F_UPPER; i++)
	{
		text_buf[i] = ' ';
	}
	r = N_SIZE - F_UPPER;
	flags = 0;
	for (;; )
	{
		if (((flags >>= 1) & 0x100) == 0)
		{
			if ((c = getcm(Count, Size, getcmP)) == EOF)
				break;
			flags = c | 0xff00;   /* uses higher byte cleverly */
		}                       /* to count eight */
		if (flags & 1)
		{
			if ((c = getcm(Count, Size, getcmP)) == EOF)
				break;
			if (!putcm(c, putcmP, putcmpPEnd))
				return false;
			text_buf[r++] = c;
			r &= (N_SIZE - 1);
		}
		else
		{
			if ((i = getcm(Count, Size, getcmP)) == EOF)
				break;
			if ((j = getcm(Count, Size, getcmP)) == EOF)
				break;
			i |= ((j & 0xf0) << 4);
			j = (j & 0x0f) + THRESHOLD;
			for (k = 0; k <= j; k++)
			{
				c = text_buf[(i + k) & (N_SIZE - 1)];
				if (!putcm(c, putcmP, putcmpPEnd))
					return false;
				text_buf[r++] = c;
				r &= (N_SIZE - 1);
			}
		}
	}

	return true;
}
