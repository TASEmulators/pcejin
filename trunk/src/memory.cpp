#include "types.h"
#include "mednafen.h"

void *MDFN_calloc_real(uint32 nmemb, uint32 size, const char *purpose, const char *_file, const int _line)
{
 void *ret;

 ret = calloc(nmemb, size);

 if(!ret)
 {
  MDFN_PrintError(_("Error allocating(calloc) %u bytes for \"%s\" in %s(%d)!"), size, purpose, _file, _line);
  return(0);
 }
 return ret;
}

void *MDFN_malloc_real(uint32 size, const char *purpose, const char *_file, const int _line)
{
 void *ret;

 ret = malloc(size);

 if(!ret)
 {
  MDFN_PrintError(_("Error allocating(malloc) %u bytes for \"%s\" in %s(%d)!"), size, purpose, _file, _line);
  return(0);
 }
 return ret;
}

void *MDFN_realloc_real(void *ptr, uint32 size, const char *purpose, const char *_file, const int _line)
{
 void *ret;

 ret = realloc(ptr, size);

 if(!ret)
 {
  MDFN_PrintError(_("Error allocating(realloc) %u bytes for \"%s\" in %s(%d)!"), size, purpose, _file, _line);
  return(0);
 }
 return ret;
}

void MDFN_free(void *ptr)
{
 free(ptr);
}