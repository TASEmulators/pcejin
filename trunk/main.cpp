#define STRICT
#define WIN32_LEAN_AND_MEAN

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

Pcejin pcejin;

SoundDriver * soundDriver = 0;

EmulateSpecStruct espec;

uint8 convert_buffer[1024*768*3];

LRESULT CALLBACK WndProc( HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam );
BOOL Register( HINSTANCE hInst );
HWND Create( int nCmdShow, int w, int h );

// Message handlers
void OnDestroy(HWND hwnd);
void OnCommand(HWND hWnd, int iID, HWND hwndCtl, UINT uNotifyCode);
void OnPaint(HWND hwnd);

// Globals
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
	winClass.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_DIRECTX_ICON);
	winClass.hIconSm = LoadIcon(hInstance, (LPCTSTR)IDI_DIRECTX_ICON);
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
	
	g_hWnd = CreateWindowEx( NULL, "MY_WINDOWS_CLASS",
		"pcejin",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		0, 0, 256, 232, NULL, NULL, hInstance, NULL );

	if( g_hWnd == NULL )
		return E_FAIL;

	ScaleScreen(pcejin.windowSize);

	soundInit();

	LoadIniSettings();
	InitSpeedThrottle();

	DirectDrawInit();

	InitCustomControls();
	InitCustomKeys(&CustomKeys);
	LoadHotkeyConfig();
	LoadInputConfig();

	extern void Agg_init();
	Agg_init();

	if (osd)  {delete osd; osd =NULL; }
	osd  = new OSDCLASS(-1);

	di_init();

	hKeyInputTimer = timeSetEvent (30, 0, KeyInputTimer, 0, TIME_PERIODIC);

	ShowWindow( g_hWnd, nCmdShow );
	UpdateWindow( g_hWnd );

	initialize();

	while( uMsg.message != WM_QUIT )
	{
		if( PeekMessage( &uMsg, NULL, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &uMsg );
			DispatchMessage( &uMsg );
		}
		else {
			emulate();
		}
	}

	// shutDown();

	UnregisterClass( "MY_WINDOWS_CLASS", winClass.hInstance );

	return uMsg.wParam;
}

#include <fcntl.h>
#include <io.h>
HANDLE hConsole;
void OpenConsole()
{
	COORD csize;
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
	SMALL_RECT srect;
	char buf[256];

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
	}
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

}

void SaveIniSettings(){

	WritePrivateProfileInt("Video", "aspectratio", pcejin.aspectRatio, IniName);
	WritePrivateProfileInt("Video", "pcejin.windowSize", pcejin.windowSize, IniName);
	WritePrivateProfileInt("Main", "Muted", soundDriver->userMute, IniName);

}

std::wstring a = L"a";

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
		FCEUI_SaveMovie(szChoice, a, 1);

	//If user did not specify an extension, add .dsm for them
	// fname = szChoice;
	// x = fname.find_last_of(".");
	// if (x < 0)
	// fname.append(".mc2");



	

	// SetDlgItemText(hwndDlg, IDC_EDIT_FILENAME, fname.c_str());
	//if(GetSaveFileName(&ofn))
	// UpdateRecordDialogPath(hwndDlg,szChoice);

	soundDriver->resume();
}

void PlayMovie(HWND hWnd){
	char szChoice[MAX_PATH]={0};

	OPENFILENAME ofn;

	soundDriver->pause();

	// browse button
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = "Movie File (*.mc2)\0*.mc2\0All files(*.*)\0*.*\0\0";
	ofn.lpstrFile = (LPSTR)szChoice;
	ofn.lpstrTitle = "Play a movie";
	ofn.lpstrDefExt = "mc2";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	if(GetOpenFileName(&ofn))
		FCEUI_LoadMovie(szChoice, 1, 0, 0);
	// Replay_LoadMovie();
	
	soundDriver->resume();

}

LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
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
	case WM_ENTERMENULOOP:
		soundDriver->pause();
		EnableMenuItem(GetMenu(hWnd), IDM_RECORD_MOVIE, MF_BYCOMMAND | (movieMode == MOVIEMODE_INACTIVE && pcejin.romLoaded) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_PLAY_MOVIE, MF_BYCOMMAND | (movieMode == MOVIEMODE_INACTIVE && pcejin.romLoaded) ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(GetMenu(hWnd), IDM_STOPMOVIE, MF_BYCOMMAND | (movieMode != MOVIEMODE_INACTIVE) ? MF_ENABLED : MF_GRAYED);

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
		soundDriver->resume();
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
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDC_WINDOW1X:
			pcejin.windowSize=1;
			ScaleScreen(pcejin.windowSize);
			WritePrivateProfileInt("Video","Window Size",pcejin.windowSize,IniName);
			break;
		case IDC_WINDOW2X:
			pcejin.windowSize=2;
			ScaleScreen(pcejin.windowSize);
			WritePrivateProfileInt("Video","Window Size",pcejin.windowSize,IniName);
			break;
		case IDC_WINDOW3X:
			pcejin.windowSize=3;
			ScaleScreen(pcejin.windowSize);
			WritePrivateProfileInt("Video","Window Size",pcejin.windowSize,IniName);
			break;
		case IDC_WINDOW4X:
			pcejin.windowSize=4;
			ScaleScreen(pcejin.windowSize);
			WritePrivateProfileInt("Video","Window Size",pcejin.windowSize,IniName);
			break;
		case IDC_ASPECT:
			pcejin.aspectRatio ^= 1;
			ScaleScreen(pcejin.windowSize);
			WritePrivateProfileInt("Video","Aspect Ratio",pcejin.aspectRatio,IniName);
			break;
		case IDM_EXIT:
			PostQuitMessage(0);
			break;
		case IDM_RESET:
			PCE_Power();
			break;
		case IDM_OPEN_ROM:
			soundDriver->pause();
			LoadGame();
			soundDriver->resume();
			break;
		case IDM_RECORD_MOVIE:
			MovieRecordTo();
			return 0;
		case IDM_PLAY_MOVIE:
			Replay_LoadMovie();
			return 0;
		case IDM_INPUT_CONFIG:
			soundDriver->pause();
			DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_INPUTCONFIG), hWnd, DlgInputConfig);
			soundDriver->resume();
			// RunInputConfig();
			break;
		case IDM_HOTKEY_CONFIG:
			{
				soundDriver->pause();

				DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_KEYCUSTOM), hWnd, DlgHotkeyConfig);
				soundDriver->resume();

			}
			break;

		case IDM_BIOS_CONFIG:
			soundDriver->pause();

			DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_BIOS), hWnd, (DLGPROC) BiosSettingsDlgProc);
			// DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_KEYCUSTOM), hWnd, BiosSettingsDlgProc);
			soundDriver->resume();
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
		case IDM_MUTE:
			soundDriver->doUserMute();
			break;
		case IDM_STOPMOVIE:
			FCEUI_StopMovie();
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
		case IDM_ABOUT:
			soundDriver->pause();

			DialogBox(winClass.hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			soundDriver->resume();
			break;
		case IDM_FILE_RECORDAVI:
			soundDriver->pause();
			RecordAvi();
			soundDriver->resume();
			break;
		case IDM_FILE_STOPAVI:
			StopAvi();
			break;
		}
		break;
	}
	return DefWindowProc(hWnd, Message, wParam, lParam);
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

void emulate(){

	if(!pcejin.started  || !pcejin.romLoaded)
		return;

	pcejin.isLagFrame = true;

	S9xUpdateJoypadButtons();

	pcejin.pads[0] = joypads [0];
	pcejin.pads[1] = joypads [1];

	FCEUMOV_AddInputState();

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

	MDFNGameInfo->Emulate(&espec);

	Update_RAM_Search();
	Update_RAM_Watch();

	if(!espec.skip)
		render();

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
			HWND cur;

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