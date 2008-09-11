#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <iconv.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

static void error( unsigned, unsigned, char const *fmt, ... )
{
   va_list ap;
   va_start(ap,fmt);
   vfprintf(stderr,fmt,ap);
   va_end(ap);
}

int file2wcs (int fd, const char *charset, wchar_t *outbuf, size_t avail)
{
       char inbuf[BUFSIZ];
       size_t insize = 0;
       char *wrptr = (char *) outbuf;
       int result = 0;
       iconv_t cd;
     
       cd = iconv_open ("WCHAR_T", charset);
       if (cd == (iconv_t) -1)
         {
           /* Something went wrong.  */
           if (errno == EINVAL)
             error (0, 0, "conversion from '%s' to wchar_t not available",
                    charset);
           else
             perror ("iconv_open");
     
           /* Terminate the output string.  */
           *outbuf = L'\0';
     
           return -1;
         }
     
       while (avail > 0)
         {
           size_t nread;
           size_t nconv;
           char *inptr = inbuf;
     
           /* Read more input.  */
           nread = read (fd, inbuf + insize, sizeof (inbuf) - insize);
           if (nread == 0)
             {
               /* When we come here the file is completely read.
                  This still could mean there are some unused
                  characters in the inbuf.  Put them back.  */
               if (lseek (fd, -insize, SEEK_CUR) == -1)
                 result = -1;
     
               /* Now write out the byte sequence to get into the
                  initial state if this is necessary.  */
               iconv (cd, NULL, NULL, &wrptr, &avail);
     
               break;
             }
           insize += nread;
     
           /* Do the conversion.  */
           nconv = iconv (cd, &inptr, &insize, &wrptr, &avail);
           if (nconv == (size_t) -1)
             {
               /* Not everything went right.  It might only be
                  an unfinished byte sequence at the end of the
                  buffer.  Or it is a real problem.  */
               if (errno == EINVAL)
                 /* This is harmless.  Simply move the unused
                    bytes to the beginning of the buffer so that
                    they can be used in the next round.  */
                 memmove (inbuf, inptr, insize);
               else
                 {
                   /* It is a real problem.  Maybe we ran out of
                      space in the output buffer or we have invalid
                      input.  In any case back the file pointer to
                      the position of the last processed byte.  */
                   lseek (fd, -insize, SEEK_CUR);
                   result = -1;
                   break;
                 }
             }
         }
     
       /* Terminate the output string.  */
       if (avail >= sizeof (wchar_t))
         *((wchar_t *) wrptr) = L'\0';
     
       if (iconv_close (cd) != 0)
         perror ("iconv_close");
     
       return (wchar_t *) wrptr - outbuf;
}


