/*
 *  Print message to stderr and exit
 */
#include "CSCIx229.h"

_Noreturn void Fatal(const char* format , ...)
{
   va_list args;
   va_start(args,format);
   vfprintf(stderr,format,args);
   va_end(args);
   exit(1);
}
