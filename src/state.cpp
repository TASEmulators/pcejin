#include "state.h"
#include "endian.h"
#include "mednafen.h"
#include "general.h"
#include "video.h"
#include "main.h"
#include "movie.h"
#include "pcejin.h"

#define RLSB MDFNSTATE_RLSB //0x80000000

int CurrentState = 0;

static int SaveStateStatus[10];
static int RecentlySavedState = -1;

int32 smem_read(StateMem *st, void *buffer, uint32 len)
{
	if((len + st->loc) > st->len)
		return(0);

	memcpy(buffer, st->data + st->loc, len);
	st->loc += len;

	return(len);
}

int32 smem_write(StateMem *st, void *buffer, uint32 len)
{
	if((len + st->loc) > st->malloced)
	{
		uint32 newsize = (st->malloced >= 32768) ? st->malloced : (st->initial_malloc ? st->initial_malloc : 32768);

		while(newsize < (len + st->loc))
			newsize *= 2;
		st->data = (uint8 *)realloc(st->data, newsize);
		st->malloced = newsize;
	}
	memcpy(st->data + st->loc, buffer, len);
	st->loc += len;

	if(st->loc > st->len) st->len = st->loc;

	return(len);
}

int32 smem_putc(StateMem *st, int value)
{
	uint8 tmpval = value;
	if(smem_write(st, &tmpval, 1) != 1)
		return(-1);
	return(1);
}

int32 smem_tell(StateMem *st)
{
	return(st->loc);
}

int32 smem_seek(StateMem *st, int offset, int whence)
{
	switch(whence)
	{
	case SEEK_SET: st->loc = offset; break;
	case SEEK_END: st->loc = st->len - offset; break;
	case SEEK_CUR: st->loc += offset; break;
	}

	if(st->loc > st->len)
	{
		st->loc = st->len;
		return(-1);
	}

	if(st->loc < 0)
	{
		st->loc = 0;
		return(-1);
	}

	return(0);
}

int smem_write32le(StateMem *st, uint32 b)
{
	uint8 s[4];
	s[0]=b;
	s[1]=b>>8;
	s[2]=b>>16;
	s[3]=b>>24;
	return((smem_write(st, s, 4)<4)?0:4);
}

int smem_read32le(StateMem *st, uint32 *b)
{
	uint8 s[4];

	if(smem_read(st, s, 4) < 4)
		return(0);

	*b = s[0] | (s[1] << 8) | (s[2] << 16) | (s[3] << 24);

	return(4);
}

static int SubWrite(StateMem *st, SFORMAT *sf, int data_only, gzFile fp)
{
	uint32 acc=0;

	// FIXME? It's kind of slow, and we definitely don't want it on with state rewinding...
	//ValidateSFStructure(sf);

	while(sf->s || sf->desc) // Size can sometimes be zero, so also check for the text description. These two should both be zero only at the end of a struct.
	{
		if(!sf->s || !sf->v)
		{
			sf++;
			continue;
		}
		if(sf->s == (uint32)~0) /* Link to another struct. */
		{
			uint32 tmp;

			if(!(tmp=SubWrite(st,(SFORMAT *)sf->v, data_only, fp)))
				return(0);
			acc+=tmp;
			sf++;
			continue;
		}

		if(!data_only)
			acc+=32 + 4; /* Description + size */

		int32 bytesize = sf->s&(~(MDFNSTATE_RLSB32 | MDFNSTATE_RLSB16 | RLSB));

		acc += bytesize;
		//printf("%d %d %d\n", bytesize, data_only, fp);
		if(st || fp) /* Are we writing or calculating the size of this block? */
		{
			if(!data_only)
			{
				char desco[32];
				int slen = strlen(sf->desc);

				memset(desco, 0, 32);

				if(slen > 32)
				{
					printf("Warning: state variable name too long: %s %d\n", sf->desc, slen);
					slen = 32;
				}

				memcpy(desco, sf->desc, slen);
				smem_write(st, desco, 32);
				smem_write32le(st, bytesize);

				/* Flip the byte order... */
				if(sf->s & MDFNSTATE_RLSB32)
					Endian_A32_NE_to_LE(sf->v, bytesize / sizeof(uint32));
				else if(sf->s & MDFNSTATE_RLSB16)
					Endian_A16_NE_to_LE(sf->v, bytesize / sizeof(uint16));
				else if(sf->s&RLSB)
					Endian_V_NE_to_LE(sf->v, bytesize);
			}

			if(fp)
			{
				//printf("Wrote: %d\n", bytesize);
				gzwrite(fp, sf->v, bytesize);
			}
			else
			{
				smem_write(st, (uint8 *)sf->v, bytesize);
			}

			if(!data_only)
			{
				/* Now restore the original byte order. */
				if(sf->s & MDFNSTATE_RLSB32)
					Endian_A32_LE_to_NE(sf->v, bytesize / sizeof(uint32));
				else if(sf->s & MDFNSTATE_RLSB16)
					Endian_A16_LE_to_NE(sf->v, bytesize / sizeof(uint16));
				else if(sf->s&RLSB)
					Endian_V_LE_to_NE(sf->v, bytesize);
			}
		}
		sf++;
	}

	return(acc);
}

static int WriteStateChunk(StateMem *st, const char *sname, SFORMAT *sf, int data_only)
{
	int bsize;

	if(!data_only)
		smem_write(st, (uint8 *)sname, 4);

	bsize=SubWrite(0,sf, data_only, NULL);

	if(!data_only)
		smem_write32le(st, bsize);

	if(!SubWrite(st,sf, data_only, NULL)) return(0);

	if(data_only)
		return(bsize);
	else
		return (bsize + 4 + 4);
}

static SFORMAT *CheckS(SFORMAT *sf, uint32 tsize, const char *desc)
{
	while(sf->s || sf->desc) // Size can sometimes be zero, so also check for the text description. These two should both be zero only at the end of a struct.
	{
		if(!sf->s || !sf->v)
		{
			sf++;
			continue;
		}
		if(sf->s==(uint32)~0) /* Link to another SFORMAT structure. */
		{
			SFORMAT *tmp;
			if((tmp= CheckS((SFORMAT *)sf->v, tsize, desc) ))
				return(tmp);
			sf++;
			continue;
		}
		char check_str[32];
		memset(check_str, 0, sizeof(check_str));

		strncpy(check_str, sf->desc, 32);

		if(!memcmp(desc, check_str, 32))
		{
			uint32 bytesize = sf->s&(~(MDFNSTATE_RLSB32 | MDFNSTATE_RLSB16 | RLSB));

			if(tsize != bytesize)
			{
				printf("tsize != bytesize: %.32s\n", desc);
				return(0);
			}
			return(sf);
		}
		sf++;
	}
	return(0);
}

// Fast raw chunk reader
static void DOReadChunk(StateMem *st, SFORMAT *sf)
{
	while(sf->s || sf->desc)// Size can sometimes be zero, so also check for the text description.
		// These two should both be zero only at the end of a struct.
	{
		if(!sf->s || !sf->v)
		{
			sf++;
			continue;
		}

		if(sf->s == (uint32) ~0) // Link to another SFORMAT struct
		{
			DOReadChunk(st, (SFORMAT *)sf->v);
			sf++;
			continue;
		}

		int32 bytesize = sf->s&(~(MDFNSTATE_RLSB32 | MDFNSTATE_RLSB16 | RLSB));

		smem_read(st, (uint8 *)sf->v, bytesize);
		sf++;
	}
}

static int ReadStateChunk(StateMem *st, SFORMAT *sf, int size, int data_only)
{
	SFORMAT *tmp;
	int temp;

	if(data_only)
	{
		DOReadChunk(st, sf);
	}
	else
	{
		temp = smem_tell(st);
		while(smem_tell(st) < temp + size)
		{
			uint32 tsize;
			char toa[32];

			if(smem_read(st, toa, 32) <= 0)
			{
				puts("Unexpected EOF?");
				return 0;
			}

			smem_read32le(st, &tsize);
			if((tmp=CheckS(sf,tsize,toa)))
			{
				int32 bytesize = tmp->s&(~(MDFNSTATE_RLSB32 | MDFNSTATE_RLSB16 | RLSB));

				smem_read(st, (uint8 *)tmp->v, bytesize);

				if(tmp->s & MDFNSTATE_RLSB32)
					Endian_A32_LE_to_NE(tmp->v, bytesize / sizeof(uint32));
				else if(tmp->s & MDFNSTATE_RLSB16)
					Endian_A16_LE_to_NE(tmp->v, bytesize / sizeof(uint16));
				else if(tmp->s&RLSB)
					Endian_V_LE_to_NE(tmp->v, bytesize);
			}
			else
				if(smem_seek(st,tsize,SEEK_CUR) < 0)
				{
					puts("Seek error");
					return(0);
				}
		} // while(...)
	}
	return 1;
}

/* This function is called by the game driver(NES, GB, GBA) to save a state. */
int MDFNSS_StateAction(StateMem *st, int load, int data_only, std::vector <SSDescriptor> &sections)
{
	std::vector<SSDescriptor>::iterator section;

	if(load)
	{
		char sname[4];

		for(section = sections.begin(); section != sections.end(); section++)
		{
			if(data_only)
			{
				ReadStateChunk(st, section->sf, ~0, 1);
			}
			else
			{
				int found = 0;
				uint32 tmp_size;
				uint32 total = 0;
				while(smem_read(st, (uint8 *)sname, 4) == 4)
				{
					if(!smem_read32le(st, &tmp_size)) return(0);
					total += tmp_size + 8;
					// Yay, we found the section
					if(!memcmp(sname, section->name, 4))
					{
						if(!ReadStateChunk(st, section->sf, tmp_size, 0))
						{
							printf("Error reading chunk: %.4s\n", section->name);
							return(0);
						}
						found = 1;
						break;
					}
					else
					{
						//puts("SEEK");
						if(smem_seek(st, tmp_size, SEEK_CUR) < 0)
						{
							puts("Chunk seek failure");
							return(0);
						}
					}
				}
				if(smem_seek(st, -(int)total, SEEK_CUR) < 0)
				{
					puts("Reverse seek error");
					return(0);
				}
				if(!found && !section->optional) // Not found. We are sad!
				{
					printf("Chunk missing: %.4s\n", section->name);
					return(0);
				}
			}
		}
	}
	else
		for(section = sections.begin(); section != sections.end(); section++)
		{
			if(!WriteStateChunk(st, section->name, section->sf, data_only))
				return(0);
		}
		return(1);
}

int MDFNSS_StateAction(StateMem *st, int load, int data_only, SFORMAT *sf, const char *name, bool optional)
{
	std::vector <SSDescriptor> love;

	love.push_back(SSDescriptor(sf, name, optional));
	return(MDFNSS_StateAction(st, load, data_only, love));
}

int MDFNSS_SaveSM(StateMem *st, int wantpreview, int data_only, uint32 *fb, MDFN_Rect *LineWidths)
{
	static uint8 header[32]="MEDNAFENSVESTATE";
	int neowidth, neoheight;

	neowidth = MDFNGameInfo->ss_preview_width;
	neoheight = MDFNGameInfo->DisplayRect.h;

	if(!data_only)
	{
		memset(header+16,0,16);
		MDFN_en32lsb(header + 12, currFrameCounter);
		MDFN_en32lsb(header + 16, pcejin.lagFrameCounter);
//		MDFN_en32lsb(header + 16, MEDNAFEN_VERSION_NUMERIC);
		MDFN_en32lsb(header + 24, neowidth);
		MDFN_en32lsb(header + 28, neoheight);
		smem_write(st, header, 32);
	}

	if(wantpreview)
	{
		uint8 *previewbuffer = (uint8 *)malloc(17 * neowidth * neoheight);

//		MakeStatePreview(previewbuffer, fb, LineWidths);
		smem_write(st, fb, 17 * neowidth * neoheight);

		free(previewbuffer);
	}

	// State rewinding code path hack, FIXME
	//if(data_only)
	//{
	// if(!MDFN_RawInputStateAction(st, 0, data_only))
	// return(0);
	//}

	if(!MDFNGameInfo->StateAction(st, 0, data_only))
		return(0);

	if(!data_only)
	{
		uint32 sizy = smem_tell(st);
		smem_seek(st, 16 + 4, SEEK_SET);
		smem_write32le(st, sizy);
	}
	return(1);
}

int MDFNSS_Save(const char *fname, const char *suffix, uint32 *fb, MDFN_Rect *LineWidths)
{
	StateMem st;

	memset(&st, 0, sizeof(StateMem));

	if(!MDFNSS_SaveSM(&st, 1, 0, fb, LineWidths))
	{
		if(st.data)
			free(st.data);
		if(!fname && !suffix)
			MDFN_DispMessage(_("State %d save error."), CurrentState);
		return(0);
	}

	if(!MDFN_DumpToFile(fname ? fname : MDFN_MakeFName(MDFNMKF_STATE,CurrentState,suffix).c_str(), 6, st.data, st.len))
	{
		SaveStateStatus[CurrentState] = 0;
		free(st.data);

		if(!fname && !suffix)
			MDFN_DispMessage(_("State %d save error."),CurrentState);

		return(0);
	}

	SaveStateMovie((const char*)MDFN_MakeFName(MDFNMKF_STATE,CurrentState,suffix).c_str());

	free(st.data);

	SaveStateStatus[CurrentState] = 1;
	RecentlySavedState = CurrentState;

	if(!fname && !suffix)
		MDFN_DispMessage(_("State %d saved."),CurrentState);

	return(1);
}

int MDFNSS_LoadSM(StateMem *st, int haspreview, int data_only)
{
	uint8 header[32];
	uint32 stateversion;

	if(data_only)
	{
		stateversion = MEDNAFEN_VERSION_NUMERIC;
	}
	else
	{
		smem_read(st, header, 32);
/*		if(memcmp(header,"MEDNAFENSVESTATE",16))
			return(0);

		stateversion = MDFN_de32lsb(header + 16);

		if(stateversion < 0x0600)
		{
			printf("State too old: %08x\n", stateversion);
			return(0);
		}*/
	}

	currFrameCounter = MDFN_de32lsb(header + 12);  
	pcejin.lagFrameCounter = MDFN_de32lsb(header + 16);

	if(haspreview)
	{
		uint32 width = MDFN_de32lsb(header + 24);
		uint32 height = MDFN_de32lsb(header + 28);
		uint32 psize;

		psize = width * height * 17;
		smem_seek(st, psize, SEEK_CUR); // Skip preview
	}
	/*
	// State rewinding code path hack, FIXME
	if(data_only)
	{
	if(!MDFN_RawInputStateAction(st, stateversion, data_only))
	return(0);
	}
	*/
	return(MDFNGameInfo->StateAction(st, stateversion, data_only));
}

int MDFNSS_LoadFP(gzFile fp)
{
	uint8 header[32];
	StateMem st;

	memset(&st, 0, sizeof(StateMem));

	if(gzread(fp, header, 32) != 32)
	{
		return(0);
	}
	st.len = MDFN_de32lsb(header + 16 + 4);

	if(st.len < 32)
		return(0);

	if(!(st.data = (uint8 *)malloc(st.len)))
		return(0);

	memcpy(st.data, header, 32);
	if(gzread(fp, st.data + 32, st.len - 32) != ((int32)st.len - 32))
	{
		free(st.data);
		return(0);
	}
	if(!MDFNSS_LoadSM(&st, 1, 0))
	{
		free(st.data);
		return(0);
	}
	free(st.data);
	return(1);
}


void MDFNI_DisplayState()
{
	gzFile fp;

	fp = gzopen(MDFN_MakeFName(MDFNMKF_STATE,CurrentState,NULL).c_str(),"rb");
	if(fp)
	{
		uint8 header[32];

		gzread(fp, header, 32);
		uint32 width = MDFN_de32lsb(header + 24);
		uint32 height = MDFN_de32lsb(header + 28);

		uint8 *previewbuffer = (uint8*)alloca(3 * width * height);

		gzread(fp, espec.pixels, 17 * width * height);

		gzclose(fp);
	}
}  

int MDFNSS_Load(const char *fname, const char *suffix)
{
	gzFile st;

	if(fname)
		st=gzopen(fname, "rb");
	else
	{
		st=gzopen(MDFN_MakeFName(MDFNMKF_STATE,CurrentState,suffix).c_str(),"rb");
	}

	if(st == NULL)
	{
		if(!fname && !suffix)
		{
			MDFN_DispMessage(_("State %d load error."),CurrentState);
			SaveStateStatus[CurrentState]=0;
		}
		return(0);
	}

	

	if(MDFNSS_LoadFP(st))
	{
		MDFNI_DisplayState();
		LoadStateMovie((char*)MDFN_MakeFName(MDFNMKF_STATE,CurrentState,suffix).c_str());
		
		if(!fname && !suffix)
		{
			SaveStateStatus[CurrentState]=1;
			MDFN_DispMessage(_("State %d loaded."),CurrentState);
			SaveStateStatus[CurrentState]=1;
		}
		gzclose(st);
		return(1);
	}
	else
	{
		SaveStateStatus[CurrentState]=1;
		MDFN_DispMessage(_("State %d read error!"),CurrentState);
		gzclose(st);
		return(0);
	}
}

