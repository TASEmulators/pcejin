#ifndef MEDNAFEN_H
#define MEDNAFEN_H

#include <string>
#include "types.h"
#include "git.h"
#include "settings-common.h"

extern MDFNGI *MDFNGameInfo;

typedef struct {
           int NetworkPlay;
	   int SoundVolume;
	   uint32 SndRate;
	   double soundmultiplier;
	   int rshift, gshift, bshift, ashift;
} MDFNS;

extern MDFNS FSettings;

void MDFN_indent(int indent);

void MDFND_PrintError(const char *s);
void MDFN_PrintError(const char *format, ...);
void MDFN_printf(const char *format, ...);
#define MDFNI_printf MDFN_printf
void MDFN_DispMessage(const char *format, ...);

# define gettext(Msgid) ((const char *) (Msgid))
#define gettext_noop(String) String

#ifndef _
#define _(String) gettext(String)
#endif

#ifndef MDFN_SETTINGS_H
#define MDFN_SETTINGS_H

#include <string>

// This should assert() or something if the setting isn't found, since it would
// be a totally tubular error!
uint64 MDFN_GetSettingUI(const char *name);
int64 MDFN_GetSettingI(const char *name);
double MDFN_GetSettingF(const char *name);
bool MDFN_GetSettingB(const char *name);
std::string MDFN_GetSettingS(const char *name);

extern MDFNGI EmulatedPCE;

void MDFN_LoadGameCheats(FILE *override);
void MDFN_FlushGameCheats(int nosave);

#define MDFNNPCMD_RESET 	0x01
#define MDFNNPCMD_POWER 	0x02

/* Called from the physical CD disc reading code. */
bool MDFND_ExitBlockingLoop(void);

void MDFNI_SetSoundVolume(uint32 volume);
void MDFNI_Sound(int Rate);

void MDFNI_SetPixelFormat(int rshift, int gshift, int bshift, int ashift);

MDFNGI *MDFNI_LoadGame(const char *name);

#endif
#endif
