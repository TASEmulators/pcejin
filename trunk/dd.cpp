#include <stdio.h>
#include "types.h"
#include "dd.h"
#include "main.h"
#include "mednafen.h"
#include "movie.h"
#include "video.h"
#include "aggdraw.h"
#include "GPU_osd.h"
#include "pcejin.h"
#include "lua-engine.h"

DDSURFACEDESC2			ddsd;
LPDIRECTDRAW			lpdd = NULL;
LPDIRECTDRAW7			lpdd7 = NULL;
LPDIRECTDRAWSURFACE7	lpPrimary = NULL;
LPDIRECTDRAWSURFACE7 lpBack = NULL;
LPDIRECTDRAWCLIPPER		lpDDClipPrimary=NULL;

int lpitch = 0;
ushort *lpscreen = NULL;

bool quit = false;

int CreateDDrawBuffers()
{
	if (lpDDClipPrimary!=NULL) { IDirectDraw7_Release(lpDDClipPrimary); lpDDClipPrimary = NULL; }
	if (lpPrimary != NULL) { IDirectDraw7_Release(lpPrimary); lpPrimary = NULL; }
	if (lpBack != NULL) { IDirectDraw7_Release(lpBack); lpBack = NULL; }

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	ddsd.dwFlags = DDSD_CAPS;
	if (IDirectDraw7_CreateSurface(lpdd7, &ddsd, &lpPrimary, NULL) != DD_OK) return -1;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize          = sizeof(ddsd);
	ddsd.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.ddsCaps.dwCaps  = DDSCAPS_OFFSCREENPLAIN;

	ddsd.dwWidth         = pcejin.width;//MDFNGameInfo->DisplayRect.w;//256;//video.rotatedwidth();

	ddsd.dwHeight        = pcejin.height;//MDFNGameInfo->DisplayRect.h;//232;//video.rotatedheight();

		if (IDirectDraw7_CreateSurface(lpdd7, &ddsd, &lpBack, NULL) != DD_OK) return -2;

		if (IDirectDraw7_CreateClipper(lpdd7, 0, &lpDDClipPrimary, NULL) != DD_OK) return -3;

		if (IDirectDrawClipper_SetHWnd(lpDDClipPrimary, 0, g_hWnd) != DD_OK) return -4;
		if (IDirectDrawSurface7_SetClipper(lpPrimary, lpDDClipPrimary) != DD_OK) return -5;

		return 1;
}

void DirectDrawInit (void)
{

	if (DirectDrawCreateEx(NULL, (LPVOID*)&lpdd7, IID_IDirectDraw7, NULL) != DD_OK)
	{
		printf("Unable to initialize DirectDraw");
	//	MessageBox(hwnd,"Unable to initialize DirectDraw","Error",MB_OK);
		return;
	}

	if (IDirectDraw7_SetCooperativeLevel(lpdd7,g_hWnd, DDSCL_NORMAL) != DD_OK)
	{
		printf("Unable to set DirectDraw Cooperative Level");
	//	MessageBox(hwnd,"Unable to set DirectDraw Cooperative Level","Error",MB_OK);
		return;
	}

	if (CreateDDrawBuffers()<1)
	{
		printf("Unable to set DirectDraw buffers");
//		MessageBox(hwnd,"Unable to set DirectDraw buffers","Error",MB_OK);
		return;
	}

	CreateDDrawBuffers();
}

void setClientSize(int width, int height)
{
	int xborder, yborder;
	int ymenu, ymenunew;
	int ycaption;

	MENUBARINFO mbi;

	RECT wndRect;
	int finalx, finaly;

	/* Get the size of the border */
	xborder = GetSystemMetrics(SM_CXSIZEFRAME);
	yborder = GetSystemMetrics(SM_CYSIZEFRAME);

	/* Get the size of the menu bar */
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(g_hWnd, OBJID_MENU, 0, &mbi);
	ymenu = (mbi.rcBar.bottom - mbi.rcBar.top + 1);

	/* Get the size of the caption bar */
	ycaption = GetSystemMetrics(SM_CYCAPTION);

	/* Finally, resize the window */
	GetWindowRect(g_hWnd, &wndRect);
	finalx = (xborder + width + xborder);
	finaly = (ycaption + yborder + ymenu + height + yborder);
	MoveWindow(g_hWnd, wndRect.left, wndRect.top, finalx, finaly, TRUE);

	/* Oops, we also need to check if the height */
	/* of the menu bar has changed after the resize */
	ZeroMemory(&mbi, sizeof(mbi));
	mbi.cbSize = sizeof(mbi);
	GetMenuBarInfo(g_hWnd, OBJID_MENU, 0, &mbi);
	ymenunew = (mbi.rcBar.bottom - mbi.rcBar.top + 1);

	if(ymenunew != ymenu)
		MoveWindow(g_hWnd, wndRect.left, wndRect.top, finalx, (finaly + (ymenunew - ymenu)), TRUE);
}

void ScaleScreen(float factor)
{
	if(pcejin.aspectRatio)
		pcejin.width  = 309;

	if(pcejin.windowSize == 0)
		setClientSize(pcejin.width, pcejin.height);
	else
	{
		if(factor==65535)
			factor = 1.5f;
		else if(factor==65534)
			factor = 2.5f;
		setClientSize((int)(pcejin.width * factor), (int)(232 * factor));
	}
}

RECT MainScreenRect;
RECT MainScreenSrcRect;

void UpdateWndRects(HWND hwnd)
{
	POINT ptClient;
	RECT rc;

	int wndWidth, wndHeight;
	int defHeight = (pcejin.height);//; + video.screengap);
	float ratio;
	int oneScreenHeight;

	GetClientRect(g_hWnd, &rc);

	wndWidth = (rc.right - rc.left);
	wndHeight = (rc.bottom - rc.top);

	ratio = ((float)wndHeight / (float)defHeight);
	oneScreenHeight = (int)((pcejin.height) * ratio);

	// Main screen
	ptClient.x = rc.left;
	ptClient.y = rc.top;
	ClientToScreen(hwnd, &ptClient);
	MainScreenRect.left = ptClient.x;
	MainScreenRect.top = ptClient.y;
	ptClient.x = (rc.left + wndWidth);
	ptClient.y = (rc.top + oneScreenHeight);
	ClientToScreen(hwnd, &ptClient);
	MainScreenRect.right = ptClient.x;
	MainScreenRect.bottom = ptClient.y;
}

void UpdateSrcRect()
{

	// Main screen
	MainScreenSrcRect.left = 0;
	MainScreenSrcRect.top = 0;
	MainScreenSrcRect.right = pcejin.width;
	MainScreenSrcRect.bottom = pcejin.height;

}

template<typename T, int bpp> static void doRotate(void* dst) {

	u8* buffer = (u8*)dst;
	u8* src = (u8*)convert_buffer;

	if(ddsd.lPitch == 1024)
	{
		for(int i = 0; i < pcejin.width*pcejin.height; i++)
			((T*)buffer)[i] = ((T*)src)[i];
	}
	else
	{
		for(int y = 0; y < pcejin.height; y++)
		{
			for(int x = 0; x < pcejin.width; x++)
				((T*)buffer)[x] = ((T*)src)[(y * pcejin.width) + x];
			buffer += ddsd.lPitch;
		}
	}
}

//32bit to 32bit
static void __forceinline convert32(const uint8* buffer, EmulateSpecStruct *espec){

	uint8 *pb_ptr = (uint8*)convert_buffer;

	for(int y = MDFNGameInfo->DisplayRect.y; y < MDFNGameInfo->DisplayRect.y + MDFNGameInfo->DisplayRect.h; y++)
	{
		uint16 meow_width = (espec->LineWidths[0].w == ~0) ? MDFNGameInfo->DisplayRect.w : espec->LineWidths[y].w;
		int meow_x = (espec->LineWidths[0].w == ~0) ? MDFNGameInfo->DisplayRect.x : espec->LineWidths[y].x;
		uint32 *fb_line = espec->pixels + y * (MDFNGameInfo->pitch >> 2) + meow_x;

		for(int x = 0; x < meow_width; x++)
		{
			uint32 pixel = fb_line[x];

			int r, g, b;

			DECOMP_COLOR(pixel, r, g, b);

			*pb_ptr++ = b;
			*pb_ptr++ = g;
			*pb_ptr++ = r;
			*pb_ptr++ = 255;
		}
	}
}

static void convert24(const uint8* buffer, EmulateSpecStruct *espec){

	uint8 *pb_ptr = (uint8*)convert_buffer;

	for(int y = MDFNGameInfo->DisplayRect.y; y < MDFNGameInfo->DisplayRect.y + MDFNGameInfo->DisplayRect.h; y++)
	{
		uint16 meow_width = (espec->LineWidths[0].w == ~0) ? MDFNGameInfo->DisplayRect.w : espec->LineWidths[y].w;
		int meow_x = (espec->LineWidths[0].w == ~0) ? MDFNGameInfo->DisplayRect.x : espec->LineWidths[y].x;
		uint32 *fb_line = espec->pixels + y * (MDFNGameInfo->pitch >> 2) + meow_x;

		for(int x = 0; x < meow_width; x++)
		{
			uint32 pixel = fb_line[x];

			int r, g, b;

			DECOMP_COLOR(pixel, r, g, b);

			*pb_ptr++ = b;
			*pb_ptr++ = g;
			*pb_ptr++ = r;
		}
	}
}

static void convert16(const uint8* buffer, EmulateSpecStruct *espec){

	uint16 *pb_ptr = (uint16*)convert_buffer;

	for(int y = MDFNGameInfo->DisplayRect.y; y < MDFNGameInfo->DisplayRect.y + MDFNGameInfo->DisplayRect.h; y++)
	{
		uint16 meow_width = (espec->LineWidths[0].w == ~0) ? MDFNGameInfo->DisplayRect.w : espec->LineWidths[y].w;
		int meow_x = (espec->LineWidths[0].w == ~0) ? MDFNGameInfo->DisplayRect.x : espec->LineWidths[y].x;
		uint32 *fb_line = espec->pixels + y * (MDFNGameInfo->pitch >> 2) + meow_x;

		for(int x = 0; x < meow_width; x++)
		{
			uint32 pixel = fb_line[x];

			int r, g, b;

			DECOMP_COLOR(pixel, r, g, b);

			*pb_ptr++ = _16BIT(r,g,b);

			//	*pb_ptr++ = b;
			//	*pb_ptr++ = g;
			//	*pb_ptr++ = r;
		}
	}
}

bool AviNextSegment();
void render() {

	if(!pcejin.romLoaded || espec.skip)
		return;
 
	if(pcejin.width != MDFNGameInfo->DisplayRect.w) {
		pcejin.width = MDFNGameInfo->DisplayRect.w;
		pcejin.height = MDFNGameInfo->DisplayRect.h;
		CreateDDrawBuffers();
		if(!pcejin.maximized)
			ScaleScreen(pcejin.windowSize);
		AviNextSegment();
	}

	pcejin.width = MDFNGameInfo->DisplayRect.w;
	pcejin.height = MDFNGameInfo->DisplayRect.h;

	uint32 *src = (uint32*)convert_buffer;

	SetInputDisplayCharacters(pcejin.pads);

	UpdateSrcRect();
	UpdateWndRects(g_hWnd);

	int res = 0;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags=DDSD_ALL;

	res = lpBack->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL);

	if (res==DDERR_SURFACELOST)
	{
		printf("lost");
		if (IDirectDrawSurface7_Restore(lpPrimary)==DD_OK)
			IDirectDrawSurface7_Restore(lpBack);
	} 
	else if(res != DD_OK)
		return;

	switch(ddsd.ddpfPixelFormat.dwRGBBitCount)
	{
	case 32: convert32((uint8*)espec.pixels, &espec); break;
	case 24: convert24((uint8*)espec.pixels, &espec); break;
	case 16: convert16((uint8*)espec.pixels, &espec); break;
	default:
		{
			printf("wrong color");
		}
		break;
	}

	CallRegisteredLuaFunctions(LUACALL_AFTEREMULATIONGUI);

	osd->update();
	DrawHUD();
	osd->clear();

	aggDraw.hud->attach(convert_buffer, pcejin.width, pcejin.height, 4*pcejin.width);

	uint32 *dst = (uint32*)ddsd.lpSurface;

	switch(ddsd.ddpfPixelFormat.dwRGBBitCount)
	{
	case 32: doRotate<u32,32>(ddsd.lpSurface); break;
	case 24: doRotate<u32,24>(ddsd.lpSurface); break;
	case 16: doRotate<u16,16>(ddsd.lpSurface); break;
	default:
		{
			printf("wrong color");
		}
		break;
	}

	lpBack->Unlock((LPRECT)ddsd.lpSurface);

	if(lpPrimary->Blt(&MainScreenRect, lpBack, &MainScreenSrcRect, DDBLT_WAIT, 0) == DDERR_SURFACELOST)
	{
		printf("lost");
		if(IDirectDrawSurface7_Restore(lpPrimary) == DD_OK)
			IDirectDrawSurface7_Restore(lpBack);
	}
}
