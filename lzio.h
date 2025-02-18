/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in ilya.h
*/


#ifndef lzio_h
#define lzio_h

#include "ilya.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : ilyaZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define ilyaZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define ilyaZ_buffer(buff)	((buff)->buffer)
#define ilyaZ_sizebuffer(buff)	((buff)->buffsize)
#define ilyaZ_bufflen(buff)	((buff)->n)

#define ilyaZ_buffremove(buff,i)	((buff)->n -= cast_sizet(i))
#define ilyaZ_resetbuffer(buff) ((buff)->n = 0)


#define ilyaZ_resizebuffer(L, buff, size) \
	((buff)->buffer = ilyaM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define ilyaZ_freebuffer(L, buff)	ilyaZ_resizebuffer(L, buff, 0)


ILYAI_FUNC void ilyaZ_init (ilya_State *L, ZIO *z, ilya_Reader reader,
                                        void *data);
ILYAI_FUNC size_t ilyaZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */

ILYAI_FUNC const void *ilyaZ_getaddr (ZIO* z, size_t n);


/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  ilya_Reader reader;		/* reader fn */
  void *data;			/* additional data */
  ilya_State *L;			/* Ilya state (for reader) */
};


ILYAI_FUNC int ilyaZ_fill (ZIO *z);

#endif
