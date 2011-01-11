#include "mednafen.h"
#include "trio.h"
#include "general.h"
#include "unzip.h"
#include "memory.h"
#include "md5.h"
#include "windows.h"
#include "GPU_osd.h"
#include "pcejin.h"
#include "main.h"
#include "movie.h"

MDFNS FSettings;

MDFNGI *MDFNGameInfo = NULL;

int curindent = 0;

static std::string BaseDirectory;
static std::string FileBase;
static std::string FileExt; /* Includes the . character, as in ".nes" */
static std::string FileBaseDirectory;

void MDFND_Message(const char *s)
{
	fputs(s,stdout);
}


static uint8 lastchar = 0;
void MDFN_printf(const char *format, ...)
{
	char *format_temp;
	char *temp;
	unsigned int x, newlen;

	va_list ap;
	va_start(ap,format);


	// First, determine how large our format_temp buffer needs to be.
	uint8 lastchar_backup = lastchar; // Save lastchar!
	for(newlen=x=0;x<strlen(format);x++)
	{
		if(lastchar == '\n' && format[x] != '\n')
		{
			int y;
			for(y=0;y<curindent;y++)
				newlen++;
		}
		newlen++;
		lastchar = format[x];
	}

	format_temp = (char *)malloc(newlen + 1); // Length + NULL character, duh

	// Now, construct our format_temp string
	lastchar = lastchar_backup; // Restore lastchar
	for(newlen=x=0;x<strlen(format);x++)
	{
		if(lastchar == '\n' && format[x] != '\n')
		{
			int y;
			for(y=0;y<curindent;y++)
				format_temp[newlen++] = ' ';
		}
		format_temp[newlen++] = format[x];
		lastchar = format[x];
	}

	format_temp[newlen] = 0;

	temp = trio_vaprintf(format_temp, ap);
	free(format_temp);

	MDFND_Message(temp);
	free(temp);

	va_end(ap);
}

int Player_Init(int tsongs, UTF8 *album, UTF8 *artist, UTF8 *copyright, UTF8 **snames) {return 1;}
void Player_Draw(uint32 *XBuf, int CurrentSong, int16 *samples, int32 sampcount){}

void MDFNMP_AddRAM(uint32 size, uint32 A, uint8 *RAM)
{
}

std::string MDFN_MakeFName(MakeFName_Type type, int id1, const char *cd1)
{
	char tmp_path[4096];
	char numtmp[64];
	struct stat tmpstat;
	bool tmp_dfmd5 = MDFN_GetSettingB("dfmd5");
	std::string eff_dir;

	switch(type)
	{
	default: tmp_path[0] = 0;
		break;

	case MDFNMKF_MOVIE:
		if(MDFN_GetSettingB("filesys.movie_samedir"))
			eff_dir = FileBaseDirectory;
		else
		{
			std::string overpath = MDFN_GetSettingS("path_movie");
			if(overpath != "" && overpath != "0")
				eff_dir = overpath;
			else
				eff_dir = std::string(BaseDirectory) + std::string(PSS) + std::string("mcm");
		}		
		snprintf(tmp_path, 4096, "%s"PSS"%s.%d.mcm", eff_dir.c_str(), FileBase.c_str(), id1);

		if(tmp_dfmd5 && stat(tmp_path, &tmpstat) == -1)
			snprintf(tmp_path, 4096, "%s"PSS"%s.%s.%d.mcm",eff_dir.c_str(),FileBase.c_str(),md5_context::asciistr(MDFNGameInfo->MD5, 0).c_str(),id1);

		break;

	case MDFNMKF_STATE:
		if(MDFN_GetSettingB("filesys.state_samedir"))
			eff_dir = FileBaseDirectory;
		else
		{
			std::string overpath = MDFN_GetSettingS("path_state");
			if(overpath != "" && overpath != "0")
				eff_dir = overpath;
			else
				eff_dir = std::string(BaseDirectory) + std::string(PSS) + std::string("mcs");
		}

		sprintf(numtmp, "nc%d", id1);
		if (pcejin.AssociateSSMovie & MovieIsActive()) 
		{
			if(MDFNGameInfo->GameSetMD5Valid)
				snprintf(tmp_path, 4096, "%s"PSS"%s.%s.%s.%s", eff_dir.c_str(), MDFNGameInfo->shortname, md5_context::asciistr(MDFNGameInfo->GameSetMD5, 0).c_str(),pcejin.ShortMovieName,cd1?cd1:numtmp);
			else
			{
				snprintf(tmp_path, 4096, "%s"PSS"%s.%s.%s", eff_dir.c_str(), FileBase.c_str(),pcejin.ShortMovieName, cd1?cd1:numtmp);	
				if(tmp_dfmd5 && stat(tmp_path, &tmpstat) == -1)
					snprintf(tmp_path, 4096, "%s"PSS"%s.%s.%s.%s", eff_dir.c_str(), FileBase.c_str(), md5_context::asciistr(MDFNGameInfo->MD5, 0).c_str(),pcejin.ShortMovieName,cd1?cd1:numtmp);
			}
		}

		else
		{
			if(MDFNGameInfo->GameSetMD5Valid)
				snprintf(tmp_path, 4096, "%s"PSS"%s.%s.%s", eff_dir.c_str(), MDFNGameInfo->shortname, md5_context::asciistr(MDFNGameInfo->GameSetMD5, 0).c_str(),cd1?cd1:numtmp);
			else
			{
				snprintf(tmp_path, 4096, "%s"PSS"%s.%s", eff_dir.c_str(), FileBase.c_str(), cd1?cd1:numtmp);	
				if(tmp_dfmd5 && stat(tmp_path, &tmpstat) == -1)
					snprintf(tmp_path, 4096, "%s"PSS"%s.%s.%s", eff_dir.c_str(), FileBase.c_str(), md5_context::asciistr(MDFNGameInfo->MD5, 0).c_str(),cd1?cd1:numtmp);
			}
		}
		break;

	case MDFNMKF_SNAP_DAT:
		if(MDFN_GetSettingB("filesys.snap_samedir"))
			eff_dir = FileBaseDirectory;
		else
		{
			std::string overpath = MDFN_GetSettingS("path_snap");
			if(overpath != "" && overpath != "0")
				eff_dir = overpath;
			else
				eff_dir = std::string(BaseDirectory) + std::string(PSS) + std::string("snaps");
		}

		if(MDFN_GetSettingB("snapname"))
			snprintf(tmp_path, 4096, "%s"PSS"%s.txt", eff_dir.c_str(), FileBase.c_str());
		else
			snprintf(tmp_path, 4096, "%s"PSS"global.txt", eff_dir.c_str());
		break;

	case MDFNMKF_SNAP:
		if(MDFN_GetSettingB("filesys.snap_samedir"))
			eff_dir = FileBaseDirectory;
		else
		{
			std::string overpath = MDFN_GetSettingS("path_snap");
			if(overpath != "" && overpath != "0")
				eff_dir = overpath;
			else
				eff_dir = std::string(BaseDirectory) + std::string(PSS) + std::string("snaps");
		}

		if(MDFN_GetSettingB("snapname"))
			snprintf(tmp_path, 4096, "%s"PSS"%s-%d.%s", eff_dir.c_str(),FileBase.c_str(),id1,cd1);
		else
			snprintf(tmp_path, 4096, "%s"PSS"%d.%s", eff_dir.c_str(),id1,cd1);

		break;

	case MDFNMKF_SAV:
		if(MDFN_GetSettingB("filesys.sav_samedir"))
			eff_dir = FileBaseDirectory;
		else
		{
			std::string overpath = MDFN_GetSettingS("path_sav");
			if(overpath != "" && overpath != "0")
				eff_dir = overpath;
			else
				eff_dir = std::string(BaseDirectory) + std::string(PSS) + std::string("sav");
		}

		if(MDFNGameInfo->GameSetMD5Valid)
			snprintf(tmp_path, 4096, "%s"PSS"%s-%s.%s", eff_dir.c_str(), MDFNGameInfo->shortname, md5_context::asciistr(MDFNGameInfo->GameSetMD5, 0).c_str(),cd1);
		else
		{
			snprintf(tmp_path, 4096, "%s"PSS"%s.%s", eff_dir.c_str(),FileBase.c_str(),cd1);

			if(tmp_dfmd5 && stat(tmp_path,&tmpstat) == -1)
				snprintf(tmp_path, 4096, "%s"PSS"%s.%s.%s",eff_dir.c_str(),FileBase.c_str(),md5_context::asciistr(MDFNGameInfo->MD5, 0).c_str(),cd1);
		}
		break;

	case MDFNMKF_CHEAT_TMP:
	case MDFNMKF_CHEAT:
		{
			std::string overpath = MDFN_GetSettingS("path_cheat");
			if(overpath != "" && overpath != "0")
				snprintf(tmp_path, 4096, "%s"PSS"%s.%scht",overpath.c_str(), MDFNGameInfo->shortname, (type == MDFNMKF_CHEAT_TMP) ? "tmp" : "");
			else
				snprintf(tmp_path, 4096, "%s"PSS"cheats"PSS"%s.%scht",BaseDirectory.c_str(), MDFNGameInfo->shortname, (type == MDFNMKF_CHEAT_TMP) ? "tmp" : "");
		}
		break;

	case MDFNMKF_AUX: snprintf(tmp_path, 4096, "%s"PSS"%s", FileBaseDirectory.c_str(), (char *)cd1); break;

	case MDFNMKF_IPS: snprintf(tmp_path, 4096, "%s"PSS"%s%s.ips",FileBaseDirectory.c_str(),FileBase.c_str(),FileExt.c_str());
		break;

	case MDFNMKF_GGROM:
		{
			std::string overpath = MDFN_GetSettingS("nes.ggrom");
			if(overpath != "" && overpath != "0")
				strncpy(tmp_path, overpath.c_str(), 4096);
			else
				snprintf(tmp_path, 4096, "%s"PSS"gg.rom",BaseDirectory.c_str());
		}
		break;

	case MDFNMKF_LYNXROM: snprintf(tmp_path, 4096, "%s"PSS"lynxboot.img",BaseDirectory.c_str());break;

	case MDFNMKF_FDSROM:snprintf(tmp_path, 4096, "%s"PSS"disksys.rom",BaseDirectory.c_str());break;

	case MDFNMKF_PALETTE:
		{
			std::string overpath = MDFN_GetSettingS("path_palette");
			if(overpath != "" && overpath != "0")
				snprintf(tmp_path, 4096, "%s"PSS"%s.pal",overpath.c_str(),FileBase.c_str());
			else
				snprintf(tmp_path, 4096, "%s"PSS"gameinfo"PSS"%s.pal",BaseDirectory.c_str(),FileBase.c_str());
		}
		break;
	}
	return(tmp_path);
}

void MDFND_PrintError(const char *s)
{
	puts(s);
}

void MDFN_PrintError(const char *format, ...)
{
	char *temp;

	va_list ap;

	va_start(ap, format);

	temp = trio_vaprintf(format, ap);
	MDFND_PrintError(temp);
	free(temp);

	va_end(ap);
}

void GetFileBase(const char *f)
{
	const char *tp1,*tp3;

#if PSS_STYLE==4
	tp1=((char *)strrchr(f,':'));
#elif PSS_STYLE==1
	tp1=((char *)strrchr(f,'/'));
#else
	tp1=((char *)strrchr(f,'\\'));
#if PSS_STYLE!=3
	tp3=((char *)strrchr(f,'/'));
	if(tp1<tp3) tp1=tp3;
#endif
#endif
	if(!tp1)
	{
		tp1=f;
		FileBaseDirectory = ".";
	}
	else
	{
		char *tmpfn = (char*)alloca(tp1 - f + 1);

		memcpy(tmpfn,f,tp1-f);
		tmpfn[tp1-f]=0;
		FileBaseDirectory = std::string(tmpfn);

		tp1++;
	}

	if(((tp3=strrchr(f,'.'))!=NULL) && (tp3>tp1))
	{
		char *tmpbase = (char*)alloca(tp3 - tp1 + 1);

		memcpy(tmpbase,tp1,tp3-tp1);
		tmpbase[tp3-tp1]=0;
		FileBase = std::string(tmpbase);
		FileExt = std::string(tp3);
	}
	else
	{
		FileBase = std::string(tp1);
		FileExt = "";
	}
}

void MDFN_indent(int indent)
{
	curindent += indent;
}

void MDFN_DispMessage(char const *format, ...)
{
	va_list ap;
	va_start(ap,format);
	char *msg = NULL;

	trio_vasprintf(&msg, (char *)format,ap);
	va_end(ap);

	osd->addLine("%s", msg);

	if(!pcejin.romLoaded)
		MessageBox(g_hWnd, msg, "Message", NULL);

	puts(msg);
}

void MDFN_FlushGameCheats(int nosave)
{
}
void MDFNMP_ApplyPeriodicCheats(void)
{
}
void MDFNMP_InstallReadPatches(void)
{
}
bool MDFNMP_Init(uint32 ps, uint32 numpages)
{
	return true;
}
bool MDFND_ExitBlockingLoop(void)
{
	return false;
}



int MDFN_fclose(MDFNFILE *fp)
{
	if(fp->ext)
		free(fp->ext);

	if(fp->data)
	{
#if HAVE_MMAP
		if(fp->is_mmap)
			munmap(fp->data, fp->size);
		else
#endif
			free(fp->data);
	}
	free(fp);

	return(1);
}

void MDFN_LoadGameCheats(FILE *override)
{
}

int MDFN_fseek(MDFNFILE *fp, int64 offset, int whence)
{
	switch(whence)
	{
	case SEEK_SET:if(offset>=fp->size)
					  return(-1);
		fp->location=offset;break;
	case SEEK_CUR:if(offset+fp->location>fp->size)
					  return (-1);
		fp->location+=offset;
		break;
	}
	return 0;
}

uint64 MDFN_fread(void *ptr, size_t size, size_t nmemb, MDFNFILE *fp)
{
	uint32 total=size*nmemb;

	if(fp->location>=fp->size) return 0;

	if((fp->location+total)>fp->size)
	{
		int ak=(int)(fp->size-fp->location);
		memcpy((uint8*)ptr,fp->data+fp->location,ak);
		fp->location=fp->size;
		return(ak/size);
	}
	else
	{
		memcpy((uint8*)ptr,fp->data+fp->location,total);
		fp->location+=total;
		return nmemb;
	}
}

static const char *unzErrorString(int error_code)
{
	if(error_code == UNZ_OK)
		return("ZIP OK");
	else if(error_code == UNZ_END_OF_LIST_OF_FILE)
		return("ZIP End of file list");
	else if(error_code == UNZ_EOF)
		return("ZIP EOF");
	else if(error_code == UNZ_PARAMERROR)
		return("ZIP Parameter error");
	else if(error_code == UNZ_BADZIPFILE)
		return("ZIP file bad");
	else if(error_code == UNZ_INTERNALERROR)
		return("ZIP Internal error");
	else if(error_code == UNZ_CRCERROR)
		return("ZIP CRC error");
	else if(error_code == UNZ_ERRNO)
		return(strerror(errno));
	else
		return("ZIP Unknown");
}

enum
{
	MDFN_FILETYPE_PLAIN = 0,
	MDFN_FILETYPE_GZIP = 1,
	MDFN_FILETYPE_ZIP = 2,
};

static const int64 MaxROMImageSize = (int64)1 << 26;

// This function should ALWAYS close the system file "descriptor"(gzip library, zip library, or FILE *) it's given,
// even if it errors out.
static MDFNFILE *MakeMemWrap(void *tz, int type)
{
	MDFNFILE *tmp = NULL;

	if(!(tmp=(MDFNFILE *)MDFN_malloc(sizeof(MDFNFILE), _("file read buffer"))))
		goto doret;

#ifdef HAVE_MMAP
	tmp->is_mmap = FALSE;
#endif
	tmp->location=0;

	if(type == MDFN_FILETYPE_PLAIN)
	{
		fseek((FILE *)tz,0,SEEK_END);
		tmp->size=ftell((FILE *)tz);
		fseek((FILE *)tz,0,SEEK_SET);

		if(tmp->size > MaxROMImageSize)
		{
			MDFN_PrintError(_("ROM image is too large; maximum size allowed is %llu bytes."), (unsigned long long)MaxROMImageSize);
			free(tmp);
			tmp = NULL;
			goto doret;
		}

#ifdef HAVE_MMAP
		if((void *)-1 != (tmp->data = (uint8 *)mmap(NULL, tmp->size, PROT_READ, MAP_SHARED, fileno((FILE *)tz), 0)))
		{
			//puts("mmap'ed");
			tmp->is_mmap = TRUE;
#ifdef HAVE_MADVISE
			if(0 == madvise(tmp->data, tmp->size, MADV_SEQUENTIAL | MADV_WILLNEED))
			{
				//puts("madvised");
			}
#endif
		}
		else
		{
#endif
			if(!(tmp->data=(uint8*)MDFN_malloc((uint32)(tmp->size), _("file read buffer"))))
			{
				free(tmp);
				tmp=0;
				goto doret;
			}
			fread(tmp->data,1,(size_t)tmp->size,(FILE *)tz);
#ifdef HAVE_MMAP
		}
#endif
	}
	else if(type == MDFN_FILETYPE_GZIP)
	{
		uint32_t cur_size = 0;
		uint32_t cur_alloced = 65536;
		int howmany;

		if(!(tmp->data=(uint8*)MDFN_malloc(cur_alloced, _("file read buffer"))))
		{
			free(tmp);
			tmp = NULL;
			goto doret;
		}

		while((howmany = gzread(tz, tmp->data + cur_size, cur_alloced - cur_size)) > 0)
		{
			cur_size += howmany;
			cur_alloced <<= 1;
			if(!(tmp->data = (uint8 *)MDFN_realloc(tmp->data, cur_alloced, _("file read buffer"))))
			{
				free(tmp);
				tmp = NULL;
				goto doret;
			}
		}

		if(!(tmp->data = (uint8 *)MDFN_realloc(tmp->data, cur_size, _("file read buffer"))))
		{
			free(tmp);
			tmp = NULL;
			goto doret;
		}

		tmp->size = cur_size;
	}
	else if(type == MDFN_FILETYPE_ZIP)
	{
		unz_file_info ufo;
		unzGetCurrentFileInfo(tz,&ufo,0,0,0,0,0,0);

		tmp->size=ufo.uncompressed_size;

		if(tmp->size > MaxROMImageSize)
		{
			MDFN_PrintError(_("ROM image is too large; maximum size allowed is %llu bytes."), (unsigned long long)MaxROMImageSize);
			free(tmp);
			tmp = NULL;
			goto doret;
		}

		if(!(tmp->data=(uint8 *)MDFN_malloc(ufo.uncompressed_size, _("file read buffer"))))
		{
			free(tmp);
			tmp = NULL;
			goto doret;
		}
		unzReadCurrentFile(tz,tmp->data,ufo.uncompressed_size);
	}

doret:
	if(type == MDFN_FILETYPE_PLAIN)
	{
		fclose((FILE *)tz);
	}
	else if(type == MDFN_FILETYPE_GZIP)
	{
		gzclose(tz);
	}
	else if(type == MDFN_FILETYPE_ZIP)
	{
		unzCloseCurrentFile(tz);
		unzClose(tz);
	}

	return(tmp);
}

MDFNFILE * MDFN_fopen(const char *path, const char *ipsfn, const char *mode, const char *ext)
{
	MDFNFILE *fceufp = NULL;
	void *t;
	unzFile tz;

	// Try opening it as a zip file first
	if((tz=unzOpen(path)))
	{
		char tempu[1024];
		int errcode;

		if((errcode = unzGoToFirstFile(tz)) != UNZ_OK)
		{
			MDFN_PrintError(_("Could not seek to first file in ZIP archive: %s"), unzErrorString(errcode));
			unzClose(tz);
			return(NULL);
		}

		if(ext)
		{
			bool FileFound = FALSE;
			while(!FileFound)
			{
				size_t tempu_strlen;
				size_t ttmeow;
				const char *extpoo = ext;

				if((errcode = unzGetCurrentFileInfo(tz, 0, tempu, 1024, 0, 0, 0, 0)) != UNZ_OK)
				{
					MDFN_PrintError(_("Could not get file information in ZIP archive: %s"), unzErrorString(errcode));
					unzClose(tz);
					return(NULL);
				}

				tempu[1023] = 0;
				tempu_strlen = strlen(tempu);

				while((ttmeow = strlen(extpoo)) && !FileFound)
				{
					if(tempu_strlen >= ttmeow)
					{
						if(!strcasecmp(tempu + tempu_strlen - ttmeow, extpoo))
							FileFound = TRUE;
					}
					extpoo += ttmeow + 1;
				}

				if(FileFound)
					break;

				if((errcode = unzGoToNextFile(tz)) != UNZ_OK)
				{
					if(errcode != UNZ_END_OF_LIST_OF_FILE)
					{
						MDFN_PrintError(_("Error seeking to next file in ZIP archive: %s"), unzErrorString(errcode));
						unzClose(tz);
						return(NULL);
					}

					if((errcode = unzGoToFirstFile(tz)) != UNZ_OK)
					{
						MDFN_PrintError(_("Could not seek to first file in ZIP archive: %s"), unzErrorString(errcode));
						unzClose(tz);
						return(NULL);
					}
					break;
				}

			} // end to while(!FileFound)
		} // end to if(ext)

		if((errcode = unzOpenCurrentFile(tz)) != UNZ_OK)
		{
			MDFN_PrintError(_("Could not open file in ZIP archive: %s"), unzErrorString(errcode));
			unzClose(tz);
			return(NULL);
		}

		if(!(fceufp=MakeMemWrap(tz, MDFN_FILETYPE_ZIP)))
			return(0);

		char *ld = strrchr(tempu, '.');
		fceufp->ext = strdup(ld ? ld + 1 : "");
	}
	else // If it's not a zip file, handle it as...another type of file!
	{
		t = fopen(path,"rb");
		if(!t)
		{
			MDFN_PrintError(_("Error opening \"%s\": %s"), path, strerror(errno));
			return(0);
		}

		uint32 gzmagic;

		gzmagic=fgetc((FILE *)t);
		gzmagic|=fgetc((FILE *)t)<<8;
		gzmagic|=fgetc((FILE *)t)<<16;

		if(gzmagic!=0x088b1f) /* Not gzip... */
		{
			fseek((FILE *)t,0,SEEK_SET);

			if(!(fceufp = MakeMemWrap(t, 0)))
				return(0);

			const char *ld = strrchr(path, '.');
			fceufp->ext = strdup(ld ? ld + 1 : "");
		}
		else /* Probably gzip */
		{
			int fd;

			fd = dup(fileno( (FILE *)t));
			lseek(fd, 0, SEEK_SET);

			if(!(t=gzdopen(fd, mode)))
			{
				close(fd);
				return(0);
			}

			if(!(fceufp = MakeMemWrap(t, 1)))
			{
				gzclose(t);
				return(0);
			}

			char *tmp_path = strdup(path);
			char *ld = strrchr(tmp_path, '.');

			if(ld && ld > tmp_path)
			{
				char *last_ld = ld;
				*ld = 0;
				ld = strrchr(tmp_path, '.');
				if(!ld) { *last_ld = '.'; ld = last_ld; }
			}
			fceufp->ext = strdup(ld ? ld + 1 : "");
			free(tmp_path);
		} // End gzip handling
	} // End normal and gzip file handling else to zip

	// if(strchr(mode,'r') && ipsfn)
	// {
	// FILE *ipsfile = fopen(ipsfn, "rb");
	//
	// if(!ipsfile)
	// {
	// if(errno != ENOENT)
	// {
	// MDFN_PrintError(_("Error opening \"%s\": %s"), path, strerror(errno));
	// MDFN_fclose(fceufp);
	// return(NULL);
	// }
	// }
	// else
	// {
	//#ifdef HAVE_MMAP
	// // If the file is mmap()'d, move it to malloc()'d RAM
	// if(fceufp->is_mmap)
	// {
	// void *tmp_ptr = MDFN_malloc(fceufp->size, _("file read buffer"));
	// if(!tmp_ptr)
	// {
	// MDFN_fclose(fceufp);
	// fclose(ipsfile);
	// }
	// memcpy(tmp_ptr, fceufp->data, fceufp->size);
	//
	// munmap(fceufp->data, fceufp->size);
	//
	// fceufp->is_mmap = FALSE;
	// fceufp->data = (uint8 *)tmp_ptr;
	// }
	//#endif
	//
	// /* if(!ApplyIPS(ipsfile, fceufp, ipsfn))
	// {
	// MDFN_fclose(fceufp);
	// return(0);
	// }*/
	// }
	// }

	return(fceufp);
}

#include "mempatcher.h"

std::vector<SUBCHEAT> SubCheats[8];

static INLINE bool MDFN_DumpToFileReal(const char *filename, int compress, const std::vector<PtrLengthPair> &pearpairs)
{
	if(MDFN_GetSettingB("filesys.disablesavegz"))
		compress = 0;

	if(compress)
	{
		char mode[64];
		gzFile gp;

		snprintf(mode, 64, "wb%d", compress);

		gp = gzopen(filename, mode);

		if(!gp)
		{
			MDFN_PrintError(_("Error opening \"%s\": %m"), filename, errno);
			return(0);
		}

		for(unsigned int i = 0; i < pearpairs.size(); i++)
		{
			const void *data = pearpairs[i].GetData();
			const uint64 length = pearpairs[i].GetLength();

			if(gzwrite(gp, (uint8*)data, (unsigned int)length) != length)
			{
				int errnum;

				MDFN_PrintError(_("Error writing to \"%s\": %m"), filename, gzerror(gp, &errnum));
				gzclose(gp);
				return(0);
			}
		}

		if(gzclose(gp) != Z_OK) // FIXME: Huhm, how should we handle this?
		{
			MDFN_PrintError(_("Error closing \"%s\""), filename);
			return(0);
		}
	}
	else
	{
		FILE *fp = fopen(filename, "wb");
		if(!fp)
		{
			MDFN_PrintError(_("Error opening \"%s\": %m"), filename, errno);
			return(0);
		}

		for(unsigned int i = 0; i < pearpairs.size(); i++)
		{
			const void *data = pearpairs[i].GetData();
			const uint64 length = pearpairs[i].GetLength();

			if(fwrite(data, 1, (size_t)length, fp) != length)
			{
				MDFN_PrintError(_("Error writing to \"%s\": %m"), filename, errno);
				fclose(fp);
				return(0);
			}
		}

		fclose(fp);
	}
	return(1);
}

bool MDFN_DumpToFile(const char *filename, int compress, const std::vector<PtrLengthPair> &pearpairs)
{
	return(MDFN_DumpToFileReal(filename, compress, pearpairs));
}

bool MDFN_DumpToFile(const char *filename, int compress, const void *data, uint64 length)
{
	std::vector<PtrLengthPair> tmp_pairs;
	tmp_pairs.push_back(PtrLengthPair(data, length));
	return(MDFN_DumpToFileReal(filename, compress, tmp_pairs));
}


static bool CDInUse = 0;

#include "cdrom/cdromif.h"
static float LastSoundMultiplier;
void MDFNI_CloseGame(void)
{
	if(MDFNGameInfo)
	{
#ifdef NETWORK
		if(MDFNnetplay)
			MDFNI_NetplayStop();
#endif
		// MDFNMOV_Stop();
		MDFNGameInfo->CloseGame();
		if(MDFNGameInfo->name)
		{
			free(MDFNGameInfo->name);
			MDFNGameInfo->name=0;
		}
		// MDFNMP_Kill();

		MDFNGameInfo = NULL;
		//MDFN_StateEvilEnd();
#ifdef NEED_CDEMU
		if(CDInUse)
		{
			CDIF_Close();
			CDInUse = 0;
		}
#endif
	}
	//VBlur_Kill();

#ifdef WANT_DEBUGGER
	MDFNDBG_Kill();
#endif
}

MDFNGI *MDFNI_LoadCD(const char *sysname, const char *devicename)
{
	MDFNI_CloseGame();

	LastSoundMultiplier = 1;

	int ret = CDIF_Open(devicename);
	if(!ret)
	{
		MDFN_PrintError(_("Error opening CD."));
		return(0);
	}

	if(sysname == NULL)
	{
		uint8 sector_buffer[2048];

		memset(sector_buffer, 0, sizeof(sector_buffer));

		sysname = "pce";

		for(int32 track = CDIF_GetFirstTrack(); track <= CDIF_GetLastTrack(); track++)
		{
			CDIF_Track_Format format;
			if(CDIF_GetTrackFormat(track, format) && format == CDIF_FORMAT_MODE1)
			{
				CDIF_ReadSector(sector_buffer, NULL, CDIF_GetTrackStartPositionLBA(track), 1);
				if(!strncmp("PC-FX:Hu_CD-ROM", (char*)sector_buffer, strlen("PC-FX:Hu_CD-ROM")))
				{
					sysname = "pcfx";
					break;
				}
			}
		}

	}

	// for(unsigned int x = 0; x < MDFNSystemCount; x++)
	// {
	if(!strcasecmp(EmulatedPCE.shortname, sysname))
	{
		if(!EmulatedPCE.LoadCD)
		{
			MDFN_PrintError(_("Specified system \"%s\" doesn't support CDs!"), sysname);
			return(0);
		}
		MDFNGameInfo = &EmulatedPCE;

		if(!(EmulatedPCE.LoadCD()))
		{
			CDIF_Close();
			MDFNGameInfo = NULL;
			return(0);
		}
		CDInUse = 1;

#ifdef WANT_DEBUGGER
		MDFNDBG_PostGameLoad();
#endif

		// MDFNSS_CheckStates();
		//MDFNMOV_CheckMovies();

		// MDFN_ResetMessages(); // Save state, status messages, etc.

		// VBlur_Init();

		// MDFN_StateEvilBegin();
		return(MDFNGameInfo);
	}
	// }
	MDFN_PrintError(_("Unrecognized system \"%s\"!"), sysname);
	return(0);
}

const char * GetFNComponent(const char *str)
{
	const char *tp1;

#if PSS_STYLE==4
	tp1=((char *)strrchr(str,':'));
#elif PSS_STYLE==1
	tp1=((char *)strrchr(str,'/'));
#else
	tp1=((char *)strrchr(str,'\\'));
#if PSS_STYLE!=3
	{
		const char *tp3;
		tp3=((char *)strrchr(str,'/'));
		if(tp1<tp3) tp1=tp3;
	}
#endif
#endif

	if(tp1)
		return(tp1+1);
	else
		return(str);
}

#include "unixstuff.h"
MDFNGI *MDFNI_LoadGame(const char *name)
{
	MDFNFILE *fp;
	struct stat stat_buf;
	static const char *valid_iae = ".nes\0.fds\0.nsf\0.nsfe\0.unf\0.unif\0.nez\0.gb\0.gbc\0.gba\0.agb\0.cgb\0.bin\0.pce\0.hes\0.sgx\0.ngp\0.ngc\0.ws\0.wsc\0";

#ifdef NEED_CDEMU
	if(strlen(name) > 4 && (!strcasecmp(name + strlen(name) - 4, ".cue") || !strcasecmp(name + strlen(name) - 4, ".toc")))
	{
		return(MDFNI_LoadCD(NULL, name));
	}

	if(!stat(name, &stat_buf) && !S_ISREG(stat_buf.st_mode))
	{
		MDFNGI * tmp = MDFNI_LoadCD(NULL, name);
		if(tmp) return tmp;
	}
#endif

	MDFNI_CloseGame();

	LastSoundMultiplier = 1;

	MDFNGameInfo = NULL;

	MDFN_printf(_("Loading %s...\n\n"),name);

	MDFN_indent(1);

	GetFileBase(name);

	fp=MDFN_fopen(name, MDFN_MakeFName(MDFNMKF_IPS,0,0).c_str(),"rb", valid_iae);
	if(!fp)
	{
		MDFNGameInfo = NULL;
		return 0;
	}

	// for(unsigned int x = 0; x < MDFNSystemCount; x++)
	// {
	int t;

	if(!EmulatedPCE.Load)
		puts("oops");// continue;
	MDFNGameInfo = &EmulatedPCE;
	MDFNGameInfo->soundchan = 0;
	MDFNGameInfo->soundrate = 0;
	MDFNGameInfo->name = NULL;
	MDFNGameInfo->rotated = 0;

	t = MDFNGameInfo->Load(name, fp);

	if(t == 0)
	{
		MDFN_fclose(fp);
		MDFN_indent(-1);
		MDFNGameInfo = NULL;
		return(0);
	}
	else if(t == -1)
	{
		puts("failed");
		/* if(x == MDFNSystemCount - 1)
		{
		MDFN_PrintError(_("Unrecognized file format. Sorry."));
		MDFN_fclose(fp);
		MDFN_indent(-1);
		MDFNGameInfo = NULL;
		return 0;
		}*/
	}
	// else
	// break; // File loaded successfully.
	// }

	MDFN_fclose(fp);

#ifdef WANT_DEBUGGER
	MDFNDBG_PostGameLoad();
#endif

	//MDFNSS_CheckStates();
	//MDFNMOV_CheckMovies();

	// MDFN_ResetMessages(); // Save state, status messages, etc.

	MDFN_indent(-1);

	if(!MDFNGameInfo->name)
	{
		unsigned int x;
		char *tmp;

		MDFNGameInfo->name = (UTF8 *)strdup(GetFNComponent(name));

		for(x=0;x<strlen((char *)MDFNGameInfo->name);x++)
		{
			if(MDFNGameInfo->name[x] == '_')
				MDFNGameInfo->name[x] = ' ';
		}
		if((tmp = strrchr((char *)MDFNGameInfo->name, '.')))
			*tmp = 0;
	}

	// VBlur_Init();

	//MDFN_StateEvilBegin();
	return(MDFNGameInfo);
}

void MDFNI_SetPixelFormat(int rshift, int gshift, int bshift, int ashift)
{
	FSettings.rshift = rshift;
	FSettings.gshift = gshift;
	FSettings.bshift = bshift;
	FSettings.ashift = ashift;

	if(MDFNGameInfo)
		MDFNGameInfo->SetPixelFormat(rshift, gshift, bshift);
}

int MDFN_SavePNGSnapshot(const char *fname, uint32 *fb, const MDFN_Rect *rect, uint32 pitch);

void MDFNI_SaveSnapshot(void)
{
 try
 {
  FILE *pp=NULL;
  std::string fn;
  int u;

  if(!(pp = fopen(MDFN_MakeFName(MDFNMKF_SNAP_DAT, 0, NULL).c_str(), "rb")))
   u = 0;
  else
  {
   if(fscanf(pp, "%d", &u) == 0)
    u = 0;
   fclose(pp);
  }

  if(!(pp = fopen(MDFN_MakeFName(MDFNMKF_SNAP_DAT, 0, NULL).c_str(), "wb")))
   throw(0);

  fseek(pp, 0, SEEK_SET);
  fprintf(pp, "%d\n", u + 1);
  fclose(pp);

  fn = MDFN_MakeFName(MDFNMKF_SNAP, u, "png");

  if(!MDFN_SavePNGSnapshot(fn.c_str(), (uint32*)MDFNGameInfo->fb, &MDFNGameInfo->DisplayRect, MDFNGameInfo->pitch))
   throw(0);

  MDFN_DispMessage(_("Screen snapshot %d saved."), u);
 }
 catch(int x)
 {
	 (void)x;
  MDFN_DispMessage(_("Error saving screen snapshot."));
 }
}