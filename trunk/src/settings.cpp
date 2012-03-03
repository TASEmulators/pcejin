#include "windows.h"
#include "types.h"
#include "main.h"
#include <string>


char IniName[MAX_PATH];

void GetINIPath()
{
	char vPath[MAX_PATH], *szPath, currDir[MAX_PATH];
	/*if (*vPath)
	szPath = vPath;
	else
	{*/
	char *p;
	ZeroMemory(vPath, sizeof(vPath));
	GetModuleFileName(NULL, vPath, sizeof(vPath));
	p = vPath + lstrlen(vPath);
	while (p >= vPath && *p != '\\') p--;
	if (++p >= vPath) *p = 0;
	szPath = vPath;
	if (strlen(szPath) + strlen("pcejin.ini") < MAX_PATH)
	{
		sprintf(IniName, "%s\pcejin.ini",szPath);
	} else if (MAX_PATH> strlen(".\\pcejin.ini")) {
		sprintf(IniName, ".\\pcejin.ini");
	} else
	{
		memset(IniName,0,MAX_PATH) ;
	}
}

void WritePrivateProfileBool(char* appname, char* keyname, bool val, char* file)
{
	char temp[256] = "";
	sprintf(temp, "%d", val?1:0);
	WritePrivateProfileString(appname, keyname, temp, file);
}

bool GetPrivateProfileBool(const char* appname, const char* keyname, bool defval, const char* filename)
{
	return GetPrivateProfileInt(appname,keyname,defval?1:0,filename) != 0;
}

void WritePrivateProfileInt(char* appname, char* keyname, int val, char* file)
{
	char temp[256] = "";
	sprintf(temp, "%d", val);
	WritePrivateProfileString(appname, keyname, temp, file);
}


static int pce_cdpsgvolume;
static int pce_cddavolume;
static int pce_adpcmvolume;

void LoadSettings()
{
	pce_cdpsgvolume = GetPrivateProfileInt("Sound","pce.cdpsgvolume",100,IniName);
	pce_cddavolume = GetPrivateProfileInt("Sound","pce.cddavolume",100,IniName);
	pce_adpcmvolume = GetPrivateProfileInt("Sound","pce.adpcmvolume",100,IniName);
}

void SaveSettings()
{
	WritePrivateProfileInt("Sound","pce.cdpsgvolume",pce_cdpsgvolume,IniName);
	WritePrivateProfileInt("Sound","pce.cddavolume",pce_cddavolume,IniName);
	WritePrivateProfileInt("Sound","pce.adpcmvolume",pce_adpcmvolume,IniName);
}


bool MDFN_GetSettingB(const char *name)
{
	std::string nm = std::string(name);

	if(nm == "pce.adpcmlp")
		return false;
	if(nm == "pce.forcesgx")
		return false;
	if(nm == "pce.arcadecard")
		return true;
	if(nm == "pce.nospritelimit")
		return false;
	if(nm == "pce.forcemono")
		return false;
	if(nm == "filesys.movie_samedir")
		return false;
	if(nm == "filesys.state_samedir")
		return false;
	if(nm == "filesys.snap_samedir")
		return false;
	if(nm == "filesys.sav_samedir")
		return false;


		return true;
}

uint64 MDFN_GetSettingUI(const char *name)
{
	//ZOOM
	std::string nm = std::string(name);
	if(nm == "pce.cdpsgvolume") 
		return pce_cdpsgvolume;
	if(nm == "pce.cddavolume") 
		return pce_cddavolume;
	if(nm == "pce.adpcmvolume") 
		return pce_adpcmvolume;
	if(nm == "pce.ocmultiplier")
		return 1;
	if(nm == "pce.cdspeed")
		return 1;
	if(nm == "pce.slstart")//First rendered scanline.
		return 4;
	if(nm == "pce.slend")//Last rendered scanline.
		return 235;
}

double MDFN_GetSettingF(const char *name)
{
	return 1;
}

std::string MDFN_GetSettingS(const char *name)
{
	std::string nm = std::string(name);
	char ret[MAX_PATH];
	if(nm == "pce.colormap")
		return "";
	if(nm == "pce.cdbios")	{
		GetPrivateProfileString("Main", "Bios", "pce.cdbios PATH NOT SET", ret, MAX_PATH, IniName);
		return std::string(ret);
	}

	char *p;
	char pathToModule[MAX_PATH];
	ZeroMemory(pathToModule, sizeof(pathToModule));
	GetModuleFileName(NULL, pathToModule, sizeof(pathToModule));
	p = pathToModule + lstrlen(pathToModule);
	while (p >= pathToModule && *p != '\\') p--;
	if (++p >= pathToModule) *p = 0;

	if(nm == "path_movie")
		return std::string(pathToModule);
	if(nm == "path_state")
		return std::string(pathToModule);
	if(nm == "path_snap")
		return std::string(pathToModule);
	if(nm == "path_sav")
		return std::string(pathToModule);

	return(std::string("test"));
}