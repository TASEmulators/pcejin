#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include "dd.h"
#include "resource.h"
#include "stdio.h"
#include "types.h"
#include "movie.h"
#include <ddraw.h>
#include "hotkey.h"
#include "CommDlg.h"
#include "mednafen.h"
#include "general.h"
#include "CWindow.h"
#include "memView.h"
#include "ramwatch.h"
#include "ramsearch.h"
#include "Mmsystem.h"
#include "sound.h"
#include "aviout.h"
#include "video.h"
#include "aggdraw.h"
#include "GPU_osd.h"
#include "replay.h"
#include "pcejin.h"
#include "svnrev.h"
#include "xstring.h"
#include "lua-engine.h"
#include "recentmenu.h"
#include "shellapi.h"
#include "ParseCmdLine.h"

const int RECENTROM_START = 65000;
const int RECENTMOVIE_START = 65020;
const int RECENTLUA_START = 65040;
char Tmp_Str[1024];

//These are so that the commandline arguments can override autoload ones
bool skipAutoLoadROM = false;
bool skipAutoLoadMovie = false;
bool skipAutoLoadLua = false;

Pcejin pcejin;

SoundDriver * soundDriver = 0;

EmulateSpecStruct espec;

uint8 convert_buffer[1024*768*3];

LRESULT CALLBACK WndProc( HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK LuaScriptProc(HWND, UINT, WPARAM, LPARAM);
std::vector<HWND> LuaScriptHWnds;
BOOL Register( HINSTANCE hInst );
HWND Create( int nCmdShow, int w, int h );

// Prototypes
std::string RemovePath(std::string filename);

// Message handlers
void OnDestroy(HWND hwnd);
void OnCommand(HWND hWnd, int iID, HWND hwndCtl, UINT uNotifyCode);
void OnPaint(HWND hwnd);

// Globals
int WndX = 0;	//Window position
int WndY = 0;

static char g_szAppName[] = "DDSamp";
HWND g_hWnd;
HINSTANCE g_hInstance;
bool g_bRunning;
void emulate();
void initialize();
void OpenConsole();
void LoadIniSettings();
void SaveIniSettings();
DWORD hKeyInputTimer;
int KeyInDelayMSec = 0;
int KeyInRepeatMSec = 16;

RecentMenu RecentROMs;
RecentMenu RecentMovies;
RecentMenu RecentLua;

WNDCLASSEX winClass;
int WINAPI WinMain( HINSTANCE hInstance,
				   HINSTANCE hPrevInstance,
				   LPSTR lpCmdLine,
				   int nCmdShow )
{
	MSG uMsg;

	memset(&uMsg,0,sizeof(uMsg));

	winClass.lpszClassName = "MY_WINDOWS_CLASS";
	winClass.cbSize = sizeof(WNDCLASSEX);
	winClass.style = CS_HREDRAW | CS_VREDRAW;
	winClass.lpfnWndProc = WndProc;
	winClass.hInstance = hInstance;
	winClass.hIcon = LoadIcon(hInstance, "IDI_ICON1");
	winClass.hIconSm = LoadIcon(hInstance, "IDI_ICON1");
	winClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	winClass.lpszMenuName = MAKEINTRESOURCE(IDC_CV);
	winClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	winClass.cbClsExtra = 0;
	winClass.cbWndExtra = 0;

	OpenConsole();

	

	if( !RegisterClassEx(&winClass) )
		return E_FAIL;

	GetINIPath();

	pcejin.aspectRatio = GetPrivateProfileInt("Video", "aspectratio", 0, IniName);
	pcejin.windowSize = GetPrivateProfileInt("Video", "pcejin.windowSize", 1, IniName);
	
	WndX = GetPrivateProfileInt("Main", "WndX", 0, IniName);
	WndY = GetPrivateProfileInt("Main", "WndY", 0, IniName);
	
	g_hWnd = CreateWindowEx( NULL, "MY_WINDOWS_CLASS",
		pcejin.versionName.c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		WndX, WndY, 256, 232, NULL, NULL, hInstance, NULL );

	if( g_hWnd == NULL )
		return E_FAIL;

	ScaleScreen(pcejin.windowSize);

	soundInit();

	LoadIniSettings();
	InitSpeedThrottle();

	RecentROMs.SetGUI_hWnd(g_hWnd);
	RecentROMs.SetID(RECENTROM_START);
	RecentROMs.SetMenuID(ID_FILE_RECENTROM);
	RecentROMs.SetType("ROM");
	RecentROMs.MakeRecentMenu(hInstance);
	RecentROMs.GetRecentItemsFromIni(IniName, "General");

	RecentMovies.SetGUI_hWnd(g_hWnd);
	RecentMovies.SetID(RECENTMOVIE_START);
	RecentMovies.SetMenuID(ID_MOVIE_RECENT);
	RecentMovies.SetType("Movie");
	RecentMovies.MakeRecentMenu(hInstance);
	RecentMovies.GetRecentItemsFromIni(IniName, "General");

	RecentLua.SetGUI_hWnd(g_hWnd);
	RecentLua.SetID(RECENTLUA_START);
	RecentLua.SetMenuID(ID_LUA_RECENT);
	RecentLua.SetType("Lua");
	RecentLua.MakeRecentMenu(hInstance);
	RecentLua.GetRecentItemsFromIni(IniName, "General");

	DirectDrawInit();

	InitCustomControls();
	InitCustomKeys(&CustomKeys);
	LoadHotkeyConfig();
	LoadInputConfig();

	DragAcceptFiles(g_hWnd, true);

	extern void Agg_init();
	Agg_init();

	if (osd)  {delete osd; osd =NULL; }
	osd  = new OSDCLASS(-1);

	di_init();

	DWORD wmTimerRes;
	TIMECAPS tc;
	if (timeGetDevCaps(&tc, sizeof(TIMECAPS))== TIMERR_NOERROR)
	{
		wmTimerRes = std::min(std::max(tc.wPeriodMin, (UINT)1), tc.wPeriodMax);
		timeBeginPeriod (wmTimerRes);
	}
	else
	{
		wmTimerRes = 5;
		timeBeginPeriod (wmTimerRes);
	}

	if (KeyInDelayMSec == 0) {
		DWORD dwKeyboardDelay;
		SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &dwKeyboardDelay, 0);
		KeyInDelayMSec = 250 * (dwKeyboardDelay + 1);
	}
	if (KeyInRepeatMSec == 0) {
		DWORD dwKeyboardSpeed;
		SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &dwKeyboardSpeed, 0);
		KeyInRepeatMSec = (int)(1000.0/(((30.0-2.5)/31.0)*dwKeyboardSpeed+2.5));
	}
	if (KeyInRepeatMSec < (int)wmTimerRes)
		KeyInRepeatMSec = (int)wmTimerRes;
	if (KeyInDelayMSec < KeyInRepeatMSec)
		KeyInDelayMSec = KeyInRepeatMSec;

	hKeyInputTimer = timeSetEvent (KeyInRepeatMSec, 0, KeyInputTimer, 0, TIME_PERIODIC);

	ShowWindow( g_hWnd, nCmdShow );
	UpdateWindow( g_hWnd );

	initialize();
	
	if (lpCmdLine[0])ParseCmdLine(lpCmdLine, g_hWnd);

	if (RecentROMs.GetAutoLoad() && (!skipAutoLoadROM))
	{
		ALoad(RecentROMs.GetRecentItem(0).c_str());
	
	}
	
	//Intentionally does not prompt for a game if no game specified, user should be forced to autoload roms as well, for this to work properly
	if (RecentMovies.GetAutoLoad() && (!skipAutoLoadMovie))
	{	
		LoadMovie(RecentMovies.GetRecentItem(0).c_str(), 1, false, false);
	}

	if (RecentLua.GetAutoLoad() && (!skipAutoLoadLua))
	{
		char temp [1024];
		strcpy(temp, RecentLua.GetRecentItem(0).c_str());
		HWND hDlg = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_LUA), g_hWnd, (DLGPROC) LuaScriptProc);
		SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)temp);
		
	}

	while( uMsg.message != WM_QUIT )
	{
		if( PeekMessage( &uMsg, NULL, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &uMsg );
			DispatchMessage( &uMsg );
		}
		else {
			emulate();
			render();	
		}
		if(!pcejin.started)
			Sleep(1);
	}

	// shutDown();

	timeEndPeriod (wmTimerRes);

	CloseAllToolWindows();

	UnregisterClass( "MY_WINDOWS_CLASS", winClass.hInstance );
	
	return uMsg.wParam;
}

#include <fcntl.h>
#include <io.h>
HANDLE hConsole;
void OpenConsole()
{
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

	if (hConsole) return;
	AllocConsole();

	//redirect stdio
	long lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
	int hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	FILE *fp = _fdopen( hConHandle, "w" );
	*stdout = *fp;
}

HWND Create( int nCmdShow, int w, int h )
{
	RECT rc;

	// Calculate size of window based on desired client window size
	rc.left = 0;
	rc.top = 0;
	rc.right = w;
	rc.bottom = h;
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );

	HWND hwnd = CreateWindow(g_szAppName, g_szAppName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right-rc.left, rc.bottom-rc.top,
		NULL, NULL, g_hInstance, NULL);

	if (hwnd == NULL)
		return hwnd;

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	OpenConsole();

	return hwnd;
}

void LoadGame(){

	char szChoice[MAX_PATH]={0};
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrFilter = "PC Engine Files (*.pce, *.cue, *.toc, *.sgx *.zip)\0*.pce;*.cue;*.toc;*.sgx;*.zip\0All files(*.*)\0*.*\0\0";
	ofn.lpstrFile = (LPSTR)szChoice;
	ofn.lpstrTitle = "Select a file to open";
	ofn.lpstrDefExt = "pce";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	if(GetOpenFileName(&ofn)) {
		pcejin.romLoaded = true;
		pcejin.started = true;
		if(strlen(szChoice) > 4 && (!strcasecmp(szChoice + strlen(szChoice) - 4, ".cue") || !strcasecmp(szChoice + strlen(szChoice) - 4, ".toc"))) {
			char ret[MAX_PATH];
			GetPrivateProfileString("Main", "Bios", "pce.cdbios PATH NOT SET", ret, MAX_PATH, IniName);
			if(std::string(ret) == "pce.cdbios PATH NOT SET") {
				pcejin.started = false;
				pcejin.romLoaded = false;
				MDFN_DispMessage("specify your PCE CD bios");
				return;
			}
		}
		if(!MDFNI_LoadGame(szChoice)) {
			pcejin.started = false;
			pcejin.romLoaded = false;
			
		}
		if (AutoRWLoad)
		{
			//Open Ram Watch if its auto-load setting is checked
			OpenRWRecentFile(0);
			RamWatchHWnd = CreateDialog(winClass.hInstance, MAKEINTRESOURCE(IDD_RAMWATCH), g_hWnd, (DLGPROC) RamWatchProc);
		}
		StopMovie();
		ResetFrameCount();
		RecentROMs.UpdateRecentItems(szChoice);

		std::string romname = RemovePath(szChoice);
		std::string temp = pcejin.versionName;
		temp.append(" ");
		temp.append(romname);
		
		SetWindowText(g_hWnd, temp.c_str());
	}
}
void LoadGameShort(char filename[256]) {
	pcejin.romLoaded = true;
	pcejin.started = true;
if(!MDFNI_LoadGame(filename)) {
			pcejin.started = false;
			pcejin.romLoaded = false;
			
		}
		if (AutoRWLoad)
		{
			//Open Ram Watch if its auto-load setting is checked
			OpenRWRecentFile(0);
			RamWatchHWnd = CreateDialog(winClass.hInstance, MAKEINTRESOURCE(IDD_RAMWATCH), g_hWnd, (DLGPROC) RamWatchProc);
		}
		StopMovie();
		RecentROMs.UpdateRecentItems(filename);

		std::string romname = RemovePath(filename);
		std::string temp = pcejin.versionName;
		temp.append(" ");
		temp.append(romname);		
		SetWindowText(g_hWnd, temp.c_str());
	}

void RecordAvi()
{

	char szChoice[MAX_PATH]={0};
	std::string fname;
	int x;
	std::wstring la = L"";
	OPENFILENAME ofn;

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hWnd;
	ofn.lpstrFilter = "Avi file (*.avi)\0*.avi\0All files(*.*)\0*.*\0\0";
	ofn.lpstrFile = (LPSTR)szChoice;
	ofn.lpstrTitle = "Avi Thingy";
	ofn.lpstrDefExt = "avi";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    if(GetSaveFileName(&ofn))
		DRV_AviBegin(szChoice);
}

void StopAvi()
{
	DRV_AviEnd();
}

void UpdateToolWindows()
{
	Update_RAM_Search();	//Update_RAM_Watch() is also called; hotkey.cpp - HK_StateLoadSlot & State_Load also call these functions
	RefreshAllToolWindows();
}

DWORD checkMenu(UINT idd, bool check)
{
	return CheckMenuItem(GetMenu(g_hWnd), idd, MF_BYCOMMAND | (check?MF_CHECKED:MF_UNCHECKED));
}

void LoadIniSettings(){
	Hud.FrameCounterDisplay = GetPrivateProfileBool("Display","FrameCounter", false, IniName);
	Hud.ShowInputDisplay = GetPrivateProfileBool("Display","Display Input", false, IniName);
	Hud.ShowLagFrameCounter = GetPrivateProfileBool("Display","Display Lag Counter", false, IniName);
	Hud.DisplayStateSlots = GetPrivateProfileBool("Display","Display State Slots", false, IniName);
	soundDriver->userMute = (GetPrivateProfileBool("Main", "Muted", false, IniName));
	if(soundDriver->userMute)
		soundDriver->mute();

	//RamWatch Settings
	AutoRWLoad = GetPrivateProfileBool("RamWatch", "AutoRWLoad", 0, IniName);
	RWSaveWindowPos = GetPrivateProfileBool("RamWatch", "SaveWindowPos", 0, IniName);
	ramw_x = GetPrivateProfileInt("RamWatch", "WindowX", 0, IniName);
	ramw_y = GetPrivateProfileInt("RamWatch", "WindowY", 0, IniName);
	
	for(int i = 0; i < MAX_RECENT_WATCHES; i++)
	{
		char str[256];
		sprintf(str, "Recent Watch %d", i+1);
		GetPrivateProfileString("Watches", str, "", &rw_recent_files[i][0], 1024, IniName);
	}
}

void SaveIniSettings(){

	WritePrivateProfileInt("Video", "aspectratio", pcejin.aspectRatio, IniName);
	WritePrivateProfileInt("Video", "pcejin.windowSize", pcejin.windowSize, IniName);
	WritePrivateProfileInt("Main", "Muted", soundDriver->userMute, IniName);
	WritePrivateProfileInt("Main", "WndX", WndX, IniName);
	WritePrivateProfileInt("Main", "WndY", WndY, IniName);

	//RamWatch Settings
	WritePrivateProfileBool("RamWatch", "AutoRWLoad", AutoRWLoad, IniName);
	WritePrivateProfileBool("RamWatch", "SaveWindowPos", RWSaveWindowPos, IniName);
	WritePrivateProfileInt("RamWatch", "WindowX", ramw_x, IniName);
	WritePrivateProfileInt("RamWatch", "WindowY", ramw_y, IniName);
	
	for(int i = 0; i < MAX_RECENT_WATCHES; i++)
		{
			char str[256];
			sprintf(str, "Recent Watch %d", i+1);
			WritePrivateProfileString("Watches", str, &rw_recent_files[i][0], IniName);	
		}
	RecentROMs.SaveRecentItemsToIni(IniName, "General");
	RecentMovies.SaveRecentItemsToIni(IniName, "General");
	RecentLua.SaveRecentItemsToIni(IniName, "General");

}

std::string a = "a";

void RecordMovie(HWND hWnd){
	char szChoice[MAX_PATH]={0};
	soundDriver->pause();
	OPENFILENAME ofn;

	// browse button
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = "Movie File (*.mc2)\0*.mc2\0All files(*.*)\0*.*\0\0";
	ofn.lpstrFile = (LPSTR)szChoice;
	ofn.lpstrTitle = "Record a new movie";
	ofn.lpstrDefExt = "mc2";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	if(GetSaveFileName(&ofn))
		SaveMovie(szChoice, a, 1);

	//If user did not specify an extension, add .dsm for them
	// fname = szChoice;
	// x = fname.find_last_of(".");
	// if (x < 0)
	// fname.append(".mc2");

	// SetDlgItemText(hwndDlg, IDC_EDIT_FILENAME, fname.c_str());
	//if(GetSaveFileName(&ofn))
	// UpdateRecordDialogPath(hwndDlg,szChoice);

	pcejin.tempUnPause();

}

void PlayMovie(HWND hWnd){
	char szChoice[MAX_PATH]={0};

	OPENFILENAME ofn;

	soundDriver->pause();

	// browse button
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = "Movie File (*.mc2, *.mcm)\0*.mc2;*.mcm\0All files(*.*)\0*.*\0\0";
	ofn.lpstrFile = (LPSTR)szChoice;
	ofn.lpstrTitle = "Play a movie";
	ofn.lpstrDefExt = "mc2";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	if(GetOpenFileName(&ofn)) {

		if(toupper(strright(szChoice,4)) == ".MC2")
			LoadMovie(szChoice, 1, 0, 0);
		else if(toupper(strright(szChoice,4)) == ".MCM")
			LoadMCM(szChoice, true);
	}
	
	pcejin.tempUnPause();
}

void ConvertMCM(HWND hWnd){
	char szChoice[MAX_PATH]={0};

	OPENFILENAME ofn;

	soundDriver->pause();

	// browse button
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = "Mednafen Movie File (*.mcm)\0*.mcm\0All files(*.*)\0*.*\0\0";
	ofn.lpstrFile = (LPSTR)szChoice;
	ofn.lpstrTitle = "Select a movie to convert";
	ofn.lpstrDefExt = "mcm";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	if(GetOpenFileName(&ofn)) {
		LoadMCM(szChoice, false);
		if(pcejin.romLoaded) {
			osd->addLine("Check that directory");
			osd->addLine("for an .mc2 file");
		}
		else
			MessageBox(hWnd, "Check that directory for an .mc2 file", "Conversion", NULL);
	}

	pcejin.tempUnPause();
}

void ALoad(const char* filename)
{
	pcejin.romLoaded = true;
	pcejin.started = true;
	if(!MDFNI_LoadGame(filename))
	{
		pcejin.started = false;
		pcejin.romLoaded = false;		
		SetWindowText(g_hWnd, pcejin.versionName.c_str());
	}
	
	if (AutoRWLoad)
	{
		//Open Ram Watch if its auto-load setting is checked
		OpenRWRecentFile(0);
		RamWatchHWnd = CreateDialog(winClass.hInstance, MAKEINTRESOURCE(IDD_RAMWATCH), g_hWnd, (DLGPROC) RamWatchProc);
	}
	RecentROMs.UpdateRecentItems(filename);
	std::string romname = RemovePath(filename);
	std::string temp = pcejin.versionName;
	temp.append(" ");
	temp.append(romname);		
	SetWindowText(g_hWnd, temp.c_str());
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	HWND hDlg;
	switch(Message)
	{
	case WM_KEYDOWN:
		if(wParam != VK_PAUSE)
			break;
		// case WM_SYSKEYDOWN:
	case WM_CUSTKEYDOWN:
		{
			int modifiers = GetModifiers(wParam);
			if(!HandleKeyMessage(wParam,lParam, modifiers))
				return 0;
			break;
		}
	case WM_KEYUP:
		if(wParam != VK_PAUSE)
			break;
	case WM_SYSKEYUP:
	case WM_CUSTKEYUP:
		{
			int modifiers = GetModifiers(wParam);
			HandleKeyUp(wParam, lParam, modifiers);
		}
		break;
	case WM_SIZE:
		switch(wParam)
		{
		case SIZE_MINIMIZED:
			break;
		case SIZE_MAXIMIZED:
			pcejin.maximized = true;
			break;
		case SIZE_RESTORED:
			pcejin.maximized = false;
			break;
		default:
			break;
		}
		return 0;
	case WM_MOVE:
		RECT rect;
		GetWindowRect(hWnd,&rect);
		WndX = rect.left;
		WndY = rect.top;
		return 0;
	case WM_DROPFILES:
		{
			char filename[MAX_PATH] = "";
			DragQueryFile((HDROP)wParam,0,filename,MAX_PATH);
			DragFinish((HDROP)wParam);
			
			std::string fileDropped = filename;
			//-------------------------------------------------------
			//Check if mcm file, if so auto-convert and play
			//-------------------------------------------------------
			if (!(fileDropped.find(".mcm") == std::string::npos) && (fileDropped.find(".mcm") == fileDropped.length()-4))
			{
				if (!pcejin.romLoaded)	//If no ROM is loaded, prompt for one
				{
					soundDriver->pause();
					LoadGame();
					pcejin.tempUnPause();
				}
				if (pcejin.romLoaded && !(fileDropped.find(".mcm") == std::string::npos))	
					LoadMCM(fileDropped.c_str(), true);
			}
			//-------------------------------------------------------
			//Check if Movie file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".mc2") == std::string::npos) && (fileDropped.find(".mc2") == fileDropped.length()-4))
			{
				if (!pcejin.romLoaded)	//If no ROM is loaded, prompt for one
				{
					soundDriver->pause();
					LoadGame();
					pcejin.tempUnPause();
				}
				if (pcejin.romLoaded && !(fileDropped.find(".mc2") == std::string::npos))
				{
					LoadMovie(fileDropped.c_str(), 1, false, false);
					RecentMovies.UpdateRecentItems(fileDropped);
				}
			}
			
			//-------------------------------------------------------
			//Check if Savestate file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".nc") == std::string::npos))
			{
				if (fileDropped.find(".nc") == fileDropped.length()-4)
				{
					if ((fileDropped[fileDropped.length()-1] >= '0' && fileDropped[fileDropped.length()-1] <= '9'))
					{
						MDFNSS_Load(filename, NULL);
						UpdateToolWindows();
					}
				}
			}
			
			//-------------------------------------------------------
			//Check if Lua script file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".lua") == std::string::npos) && (fileDropped.find(".lua") == fileDropped.length()-4))	 //ROM is already loaded and .dsm in filename
			{
				if(LuaScriptHWnds.size() < 16)
				{
					char temp [1024];
					strcpy(temp, fileDropped.c_str());
					HWND IsScriptFileOpen(const char* Path);
					RecentLua.UpdateRecentItems(fileDropped);
					if(!IsScriptFileOpen(temp))
					{
						HWND hDlg = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_LUA), hWnd, (DLGPROC) LuaScriptProc);
						SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)temp);
					}
				}
			}
			
			//-------------------------------------------------------
			//Check if watchlist file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".wch") == std::string::npos) && (fileDropped.find(".wch") == fileDropped.length()-4))	 //ROM is already loaded and .dsm in filename
			{
				if(!RamWatchHWnd)
				{
					RamWatchHWnd = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_RAMWATCH), hWnd, (DLGPROC) RamWatchProc);
				}
				else
					SetForegroundWindow(RamWatchHWnd);
				Load_Watches(true, fileDropped.c_str());
			}
			
			//-------------------------------------------------------
			//Else load it as a ROM
			//-------------------------------------------------------
			
			else
			{
				ALoad(fileDropped.c_str());
			}
		}
		return 0;
	case WM_ENTERMENULOOP:
		soundDriver->pause();
		EnableMenuItem(GetMenu(hWnd), IDM_RECORD_MOVIE, MF_BYCOMMAND | (movieMode == MOVIEMODE_INACTIVE && pcejin.romLoaded) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_PLAY_MOVIE, MF_BYCOMMAND | (movieMode == MOVIEMODE_INACTIVE && pcejin.romLoaded) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_STOPMOVIE, MF_BYCOMMAND | (movieMode != MOVIEMODE_INACTIVE) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_RESTARTMOVIE, MF_BYCOMMAND | (movieMode != MOVIEMODE_INACTIVE) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_FILE_STOPAVI, MF_BYCOMMAND | (DRV_AviIsRecording()) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_FILE_RECORDAVI, MF_BYCOMMAND | (!DRV_AviIsRecording()) ? MF_ENABLED : MF_GRAYED);
		
		//Window Size
		checkMenu(IDC_WINDOW1X, ((pcejin.windowSize==1)));
		checkMenu(IDC_WINDOW2X, ((pcejin.windowSize==2)));
		checkMenu(IDC_WINDOW3X, ((pcejin.windowSize==3)));
		checkMenu(IDC_WINDOW4X, ((pcejin.windowSize==4)));
		checkMenu(IDC_ASPECT, ((pcejin.aspectRatio)));
		checkMenu(ID_VIEW_FRAMECOUNTER,Hud.FrameCounterDisplay);
		checkMenu(ID_VIEW_DISPLAYINPUT,Hud.ShowInputDisplay);
		checkMenu(ID_VIEW_DISPLAYSTATESLOTS,Hud.DisplayStateSlots);
		checkMenu(ID_VIEW_DISPLAYLAG,Hud.ShowLagFrameCounter);
		checkMenu(IDM_MUTE,soundDriver->userMute);
		break;
	case WM_EXITMENULOOP:
		pcejin.tempUnPause();
		break;

	case WM_CLOSE:
		{
			SaveIniSettings();
			PostQuitMessage(0);
		}

	case WM_DESTROY:
		{
			PostQuitMessage(0);
		}
		// HANDLE_MSG(hWnd, WM_DESTROY, OnDestroy);
		// HANDLE_MSG(hWnd, WM_PAINT, OnPaint);
		// HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
	case WM_COMMAND:
			//Recent ROMs
			if(wParam >= RECENTROM_START && wParam <= RECENTROM_START + RecentROMs.MAX_RECENT_ITEMS - 1)
			{
				ALoad(RecentROMs.GetRecentItem(wParam - RECENTROM_START).c_str());
				break;
			}
			else if (wParam == RecentROMs.GetClearID())
			{
				RecentROMs.ClearRecentItems();
				break;
			}
			else if (wParam == RecentROMs.GetAutoloadID())
			{
				RecentROMs.FlipAutoLoad();
				break;
			}
			//Recent Movies
			if(wParam >= RECENTMOVIE_START && wParam <= RECENTMOVIE_START + RecentMovies.MAX_RECENT_ITEMS - 1)
			{
				strcpy(Tmp_Str, RecentMovies.GetRecentItem(wParam - RECENTMOVIE_START).c_str());
				RecentMovies.UpdateRecentItems(Tmp_Str);
				//WIN32_StartMovieReplay(Str_Tmp);
				break;
			}
			else if (wParam == RecentMovies.GetClearID())
			{
				RecentMovies.ClearRecentItems();
				break;
			}
			else if (wParam == RecentMovies.GetAutoloadID())
			{
				RecentMovies.FlipAutoLoad();
				break;
			}
			//Recent Lua
			if(wParam >= RECENTLUA_START && wParam <= RECENTLUA_START + RecentLua.MAX_RECENT_ITEMS - 1)
			{
				if(LuaScriptHWnds.size() < 16)
				{
					char temp [1024];
					strcpy(temp, RecentLua.GetRecentItem(wParam - RECENTROM_START).c_str());
					HWND IsScriptFileOpen(const char* Path);
					RecentLua.UpdateRecentItems(temp);
					if(!IsScriptFileOpen(temp))
					{
						HWND hDlg = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_LUA), hWnd, (DLGPROC) LuaScriptProc);
						SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)temp);
					}
				}

				break;
			}
			else if (wParam == RecentLua.GetClearID())
			{
				RecentLua.ClearRecentItems();
				break;
			}
			else if (wParam == RecentLua.GetAutoloadID())
			{
				RecentLua.FlipAutoLoad();
				break;
			}
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDC_WINDOW1X:
			pcejin.windowSize=1;
			ScaleScreen(pcejin.windowSize);
			break;
		case IDC_WINDOW2X:
			pcejin.windowSize=2;
			ScaleScreen(pcejin.windowSize);
			break;
		case IDC_WINDOW3X:
			pcejin.windowSize=3;
			ScaleScreen(pcejin.windowSize);
			break;
		case IDC_WINDOW4X:
			pcejin.windowSize=4;
			ScaleScreen(pcejin.windowSize);
			break;
		case IDC_ASPECT:
			pcejin.aspectRatio ^= 1;
			ScaleScreen(pcejin.windowSize);
			break;
		case IDM_EXIT:
			SaveIniSettings();
			PostQuitMessage(0);
			break;
		case IDM_RESET:
			PCE_Power();
			break;
		case IDM_OPEN_ROM:
			soundDriver->pause();
			LoadGame();
			pcejin.tempUnPause();
			break;
		case IDM_RECORD_MOVIE:
			soundDriver->pause();
			MovieRecordTo();
			pcejin.tempUnPause();
			return 0;
		case IDM_PLAY_MOVIE:
			soundDriver->pause();
			Replay_LoadMovie();
			pcejin.tempUnPause();
			return 0;
		case IDM_INPUT_CONFIG:
			soundDriver->pause();
			DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_INPUTCONFIG), hWnd, DlgInputConfig);
			pcejin.tempUnPause();
			// RunInputConfig();
			break;
		case IDM_HOTKEY_CONFIG:
			{
				soundDriver->pause();

				DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_KEYCUSTOM), hWnd, DlgHotkeyConfig);
				pcejin.tempUnPause();

			}
			break;

		case IDM_BIOS_CONFIG:
			soundDriver->pause();

			DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_BIOS), hWnd, (DLGPROC) BiosSettingsDlgProc);
			// DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_KEYCUSTOM), hWnd, BiosSettingsDlgProc);
			pcejin.tempUnPause();
			break;
		case ID_VIEW_FRAMECOUNTER:
			Hud.FrameCounterDisplay ^= true;
			WritePrivateProfileBool("Display", "FrameCounter", Hud.FrameCounterDisplay, IniName);
			return 0;

		case ID_VIEW_DISPLAYINPUT:
			Hud.ShowInputDisplay ^= true;
			WritePrivateProfileBool("Display", "Display Input", Hud.ShowInputDisplay, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_DISPLAYSTATESLOTS:
			Hud.DisplayStateSlots ^= true;
			WritePrivateProfileBool("Display", "Display State Slots", Hud.DisplayStateSlots, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_DISPLAYLAG:
			Hud.ShowLagFrameCounter ^= true;
			WritePrivateProfileBool("Display", "Display Lag Counter", Hud.ShowLagFrameCounter, IniName);
			osd->clear();
			return 0;
		case IDC_NEW_LUA_SCRIPT:
			if(LuaScriptHWnds.size() < 16)
			{
				CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_LUA), g_hWnd, (DLGPROC) LuaScriptProc);
			}
			break;
		case IDM_CONVERT_MCM:
			ConvertMCM(hWnd);

			break;
		case IDM_MUTE:
			soundDriver->doUserMute();
			break;
		case IDM_STOPMOVIE:
			StopMovie();
			return 0;
			break;
		case IDM_RESTARTMOVIE:
			PCE_Power();
			ResetFrameCount();
			movieMode = MOVIEMODE_PLAY;
			movie_readonly = true;
			return 0;
			break; 
		case ID_RAM_SEARCH:
			if(!RamSearchHWnd)
			{
				InitRamSearch();
				RamSearchHWnd = CreateDialog(winClass.hInstance, MAKEINTRESOURCE(IDD_RAMSEARCH), hWnd, (DLGPROC) RamSearchProc);
			}
			else
				SetForegroundWindow(RamSearchHWnd);
			break;

		case ID_RAM_WATCH:
			if(!RamWatchHWnd)
			{
				RamWatchHWnd = CreateDialog(winClass.hInstance, MAKEINTRESOURCE(IDD_RAMWATCH), hWnd, (DLGPROC) RamWatchProc);
			}
			else
				SetForegroundWindow(RamWatchHWnd);
			return 0;
		case IDM_MEMORY:
			if (!RegWndClass("MemView_ViewBox", MemView_ViewBoxProc, 0, sizeof(CMemView*)))
				return 0;

			OpenToolWindow(new CMemView());
			return 0;
		case IDM_ABOUT:
			soundDriver->pause();

			DialogBox(winClass.hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			pcejin.tempUnPause();
			break;
		case IDM_FILE_RECORDAVI:
			soundDriver->pause();
			RecordAvi();
			pcejin.tempUnPause();
			break;
		case IDM_FILE_STOPAVI:
			StopAvi();
			break;
		}
		break;
	}
	return DefWindowProc(hWnd, Message, wParam, lParam);
}

void ScriptLoad(char* ScriptFile)
{
	if(LuaScriptHWnds.size() < 16)
	{
		char temp [1024];
		strcpy(temp, ScriptFile);
		HWND IsScriptFileOpen(const char* Path);
		if(!IsScriptFileOpen(temp))
		{
			HWND hDlg = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_LUA), g_hWnd, (DLGPROC) LuaScriptProc);
			SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)temp);
			RecentLua.UpdateRecentItems(ScriptFile);
		}
	}
}


// Command message handler
void OnCommand(HWND hWnd, int iID, HWND hwndCtl, UINT uNotifyCode)
{
	//switch (iID)
	//{
	//case IDM_QUIT:
	// OnDestroy(hWnd);
	// break;
	//}
}

// Handle WM_DESTROY
void OnDestroy(HWND )
{
	g_bRunning = false;
	PostQuitMessage(0);
}

// Window painting function
void OnPaint(HWND hwnd)
{

	// Let Windows know we've redrawn the Window - since we've bypassed
	// the GDI, Windows can't figure that out by itself.
	ValidateRect( hwnd, NULL );
}

/*
if (lpDDClipPrimary!=NULL) IDirectDraw7_Release(lpDDClipPrimary);
if (lpPrimary != NULL) IDirectDraw7_Release(lpPrimary);
if (lpBack != NULL) IDirectDraw7_Release(lpBack);
if (lpdd7 != NULL) IDirectDraw7_Release(lpdd7);*/

void initespec();
void initsound();

void initialize(){

	puts("initialize");

	MDFNGameInfo = &EmulatedPCE;

//	MDFNI_LoadGame("m:\\pce roms\\leg.pce");
//	pcejin.started = true;
//	pcejin.romLoaded = true;
	initespec();
	initsound();
}

uint32 *VTBuffer[2] = { NULL, NULL };
MDFN_Rect *VTLineWidths[2] = { NULL, NULL };
volatile int VTBackBuffer = 0;

int16 *sound;
int32 ssize;

void initvideo();

void initinput(){

	PCEINPUT_SetInput(0, "gamepad", &pcejin.pads[0]);
	PCEINPUT_SetInput(1, "gamepad", &pcejin.pads[1]);
	PCEINPUT_SetInput(2, "gamepad", &pcejin.pads[2]);
	PCEINPUT_SetInput(3, "gamepad", &pcejin.pads[3]);
	PCEINPUT_SetInput(4, "gamepad", &pcejin.pads[4]);
}

const char* Buttons[8] = {"I ", "II ", "S", "Run ", "U", "R", "D", "L"};
const char* Spaces[8] = {" ", " ", " ", " ", " ", " ", " ", " "};

char str[64];

void SetInputDisplayCharacters(uint8 new_data[]){

	strcpy(str, "");

	for ( int i = 0; i < 5; i++) {

		for (int x = 0; x < 8; x++) {

			if(new_data[i] & (1 << x)) {
				strcat(str, Buttons[x]);
			}
			else
				strcat(str, Spaces[x]);
		}
	}

	strcpy(pcejin.inputDisplayString, str);
}

void initespec(){

	VTBuffer[0] = (uint32 *)malloc(MDFNGameInfo->pitch * 256);
	VTBuffer[1] = (uint32 *)malloc(MDFNGameInfo->pitch * 256);
	VTLineWidths[0] = (MDFN_Rect *)calloc(256, sizeof(MDFN_Rect));
	VTLineWidths[1] = (MDFN_Rect *)calloc(256, sizeof(MDFN_Rect));
	initvideo();
	initinput();

}

void MDFNI_SetSoundVolume(uint32 volume)
{
	FSettings.SoundVolume=volume;
	if(MDFNGameInfo)
	{
		MDFNGameInfo->SetSoundVolume(volume);
	}
}

void MDFNI_Sound(int Rate)
{
	FSettings.SndRate=Rate;
	if(MDFNGameInfo)
	{
		EmulatedPCE.Sound(Rate);
	}
}

bool soundInit()
{
	if( S_OK != CoInitializeEx( NULL, COINIT_APARTMENTTHREADED ) ) {
		puts("IDS_COM_FAILURE");
		// systemMessage( IDS_COM_FAILURE, NULL );
	}

	soundDriver = newDirectSound();//systemSoundInit();
	if ( !soundDriver )
		return false;

	if (!soundDriver->init(44100))
		return false;

	soundDriver->resume();
}

void initsound(){
	MDFNI_Sound(44100);
	MDFNI_SetSoundVolume(100);
	FSettings.soundmultiplier = 1;
}


void initvideo(){

	MDFNI_SetPixelFormat(0,8,16,24);
}

extern u32 joypads [8];

void emulateLua()
{
	pcejin.isLagFrame = true;

	S9xUpdateJoypadButtons();

	pcejin.pads[0] = joypads [0];
	pcejin.pads[1] = joypads [1];
	pcejin.pads[2] = joypads [2];
	pcejin.pads[3] = joypads [3];
	pcejin.pads[4] = joypads [4];
	VTLineWidths[VTBackBuffer][0].w = ~0;

	memset(&espec, 0, sizeof(EmulateSpecStruct));

	espec.pixels = (uint32 *)VTBuffer[VTBackBuffer];
	espec.LineWidths = (MDFN_Rect *)VTLineWidths[VTBackBuffer];
	espec.SoundBuf = &sound;
	espec.SoundBufSize = &ssize;
	espec.soundmultiplier = 1;
	
	MOV_AddInputState();
	MDFNGameInfo->Emulate(&espec);
	
	if (pcejin.isLagFrame)
		pcejin.lagFrameCounter++;


	
}

void emulate(){
	
	if(!pcejin.started  || !pcejin.romLoaded)
		return;
	
	if (startPaused) 
	{
		pcejin.pause();
		startPaused = false;	//This should never be set true except from ParseCmdLine!
	}
	pcejin.isLagFrame = true;

	S9xUpdateJoypadButtons();

	pcejin.pads[0] = joypads [0];
	pcejin.pads[1] = joypads [1];
	pcejin.pads[2] = joypads [2];
	pcejin.pads[3] = joypads [3];
	pcejin.pads[4] = joypads [4];



	VTLineWidths[VTBackBuffer][0].w = ~0;

	memset(&espec, 0, sizeof(EmulateSpecStruct));

	espec.pixels = (uint32 *)VTBuffer[VTBackBuffer];
	espec.LineWidths = (MDFN_Rect *)VTLineWidths[VTBackBuffer];
	espec.SoundBuf = &sound;
	espec.SoundBufSize = &ssize;
	espec.soundmultiplier = 1;

	if(pcejin.fastForward && currFrameCounter%30 && !DRV_AviIsRecording())
		espec.skip = 1;
	else
		espec.skip = 0;
		CallRegisteredLuaFunctions(LUACALL_BEFOREEMULATION);
	
	MOV_AddInputState();

	MDFNGameInfo->Emulate(&espec);

	

		CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);
		UpdateToolWindows();

	soundDriver->write((u16*)*espec.SoundBuf, *espec.SoundBufSize);
	DRV_AviSoundUpdate(*espec.SoundBuf, *espec.SoundBufSize);
	DRV_AviVideoUpdate((uint16*)espec.pixels, &espec);


	if (pcejin.frameAdvance)
	{
		pcejin.frameAdvance = false;
		pcejin.started = false;
	}

	if (pcejin.isLagFrame)
		pcejin.lagFrameCounter++;


	if(pcejin.slow  && !pcejin.fastForward)
		while(SpeedThrottle())
		{
		}

	//adelikat: For commandline loadstate, I know this is a hacky place to do it, but 1 frame needs to be emulated before loadstates are possible
	if (stateToLoad != -1)
	{
		HK_StateLoadSlot(stateToLoad);
		stateToLoad = -1;				//Never set this other than from ParseCmdLine
	}
}

bool first;

void FrameAdvance(bool state)
{
	if(state) {
		if(first) {
			pcejin.started = true;
			// execute = TRUE;
			soundDriver->resume();
			pcejin.frameAdvance=true;
			first=false;
		}
		else {
			pcejin.started = true;
			soundDriver->resume();
			// execute = TRUE;
		}
	}
	else {
		first = true;
		if(pcejin.frameAdvance)
		{}
		else
		{
			pcejin.started = false;
			// emu_halt();
			soundDriver->pause();
			// SPU_Pause(1);
			// emu_paused = 1;
		}
	}
}

LRESULT CALLBACK BiosSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		{

			char ret[MAX_PATH];
			GetPrivateProfileString("Main", "Bios", "pce.cdbios PATH NOT SET", ret, MAX_PATH, IniName);
			SetDlgItemText(hDlg, IDC_BIOSTEXT, ret);
		}
		return TRUE;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDOK:
				{
					int val = 0;

					HWND cur;

					char bios[MAX_PATH];

					cur = GetDlgItem(hDlg, IDC_BIOSTEXT);
					GetWindowText(cur, bios, 256);

					WritePrivateProfileString("Main", "Bios", bios, IniName);
					EndDialog(hDlg, TRUE);
				}
				break;
			case IDCANCEL:
				EndDialog(hDlg, TRUE);
				return TRUE;

			case IDC_BIOSBROWSE:
				{
					char fileName[256] = "";
					OPENFILENAME ofn;

					ZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hDlg;
					ofn.lpstrFilter = "Any file(*.*)\0*.*\0\0";
					ofn.nFilterIndex = 1;
					ofn.lpstrFile = fileName;
					ofn.nMaxFile = 256;
					ofn.lpstrDefExt = "bin";
					ofn.Flags = OFN_NOCHANGEDIR;

					char buffer[MAX_PATH];
					ZeroMemory(buffer, sizeof(buffer));
					// path.getpath(path.SOUNDS, buffer);
					// ofn.lpstrInitialDir = buffer;

					if(GetOpenFileName(&ofn))
					{
						HWND cur;

						switch(LOWORD(wParam))
						{
						case IDC_BIOSBROWSE: cur = GetDlgItem(hDlg, IDC_BIOSTEXT); break;
						}

						SetWindowText(cur, fileName);
					}
				}
				return TRUE;
			}//end switch
		}
		return TRUE;
	}
	return FALSE;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		SetDlgItemText(hDlg, IDC_TXT_VERSION, pcejin.versionName.c_str());
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

#define MCM_SIGNATURE "MDFNMOVI"
#define MCM_HDR_SIZE 0x100
#define MCM_FRAME_SIZE 0x0B

static void read_mcm(const char* path,
                     unsigned char** body, unsigned long* size)
{
    FILE* in;
    unsigned long filesize;
    unsigned char hdr[MCM_HDR_SIZE];

    if(!(in = fopen(path, "rb"))) printf("Can't open file");

    fseek(in, 0, SEEK_END);
    filesize = ftell(in);
    fseek(in, 0, SEEK_SET);
    if(filesize <= MCM_HDR_SIZE) printf("Not MCM file (filesize <= %d)", MCM_HDR_SIZE);
    if((filesize-MCM_HDR_SIZE) % MCM_FRAME_SIZE) printf("Broken MCM file?");

    fread(hdr, 1, MCM_HDR_SIZE, in);

    if(0 != memcmp(hdr, MCM_SIGNATURE, 8)) printf("Not MCM file (signature not found)");

    *size = filesize - MCM_HDR_SIZE;
	if(!(*body = (unsigned char*)malloc(*size))) {printf("malloc() failed"); return;};

    fread(*body, 1, *size, in);

    fclose(in);
}
//this doesn't convert commands, one controller only, and loses the rerecord count
static void dump_frames(const char* path, std::string mc2, const unsigned char* body, unsigned long size)
{
    const unsigned char* p;
    unsigned char pad;
    char pad_str[9];
    unsigned long frame_count;
    unsigned long i;

	freopen( mc2.c_str(), "w", stdout );

    p = body;
    pad_str[8] = '\0';
    frame_count = size / MCM_FRAME_SIZE;

	printf("%s\n", "version 1");
	printf("%s\n", "emuVersion 9");
	printf("%s\n", "rerecordCount 1");

    for(i = 0; i < frame_count; ++i){

        pad = p[1];
		pad_str[0] = (pad&0x10) ? 'U' : '.';
		pad_str[1] = (pad&0x40) ? 'D' : '.';
		pad_str[2] = (pad&0x80) ? 'L' : '.';
        pad_str[3] = (pad&0x20) ? 'R' : '.';
        pad_str[4] = (pad&0x01) ? '1' : '.';
        pad_str[5] = (pad&0x02) ? '2' : '.';
        pad_str[6] = (pad&0x08) ? 'N' : '.';
        pad_str[7] = (pad&0x04) ? 'S' : '.';

		int command = 0;
		
		printf("%s%d%s%s%s\n", "|", command, "|", pad_str, "|");

        p += MCM_FRAME_SIZE;
    }
}

static void mcmdump(const char* path, std::string mc2)
{
    unsigned long size;
    unsigned char* body;

    read_mcm(path, &body, &size);

    dump_frames(path, mc2, body, size);

    free(body);
}

std::string noExtension(std::string path) {

	for(int i = int(path.size()) - 1; i >= 0; --i)
	{
		if (path[i] == '.') {
			return path.substr(0, i);
		}
	}
	return path;
}

std::string LoadMCM(const char* path, bool load) {

	std::string mc2;
	mc2 = noExtension(std::string(path));
	mc2 += ".mc2";
	mcmdump(path, mc2);
	if(load)
	{
		LoadMovie(mc2.c_str(), 1, 0, 0);
		RecentMovies.UpdateRecentItems(mc2);
	}

	return mc2;
}

//adelikat: Removes the path from a filename and returns the result
//C:\ROM\bob.pce becomes bob.pce
std::string RemovePath(std::string filename)
{
	std::string name = "";						//filename with path removed
	int pos = filename.find_last_of("\\") + 1;	//position of first character of name
	int length = filename.length() - pos;		//length of name
	
	if (length != -1)							//If there was a path
	{
		name.append(filename,pos,length);
		return name;
	}
	else										//Else there is no path so return what it gave us
	{
		return filename;	
	}
}

//So other files can update recent menus without access to objects
void UpdateRecentMovieMenu(std::string filename)
{
	RecentMovies.UpdateRecentItems(filename);
}

void UpdateRecentLuaMenu(std::string filename)
{
	RecentLua.UpdateRecentItems(filename);
}