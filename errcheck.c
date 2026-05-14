/*
 *  Check for OpenGL errors
 */
#include "CSCIx229.h"

void ErrCheck(const char* where)
{
   int err = glGetError();
#ifdef __EMSCRIPTEN__
   //  emcc's -lglut does not link GLU, so gluErrorString is unavailable.
   if (err) fprintf(stderr,"ERROR: code 0x%x [%s]\n",err,where);
#else
   if (err) fprintf(stderr,"ERROR: %s [%s]\n",gluErrorString(err),where);
#endif
}
