#include "recentmenu.h"
#include <shlwapi.h>	//For CompactPath() Note: shlwapi.lib must be included as a dependency

//**************************************************************

RecentMenu::RecentMenu(int baseID, HWND GUI_hWnd, HINSTANCE instance, int menuItem, std::string type)
{
	BaseID = baseID;
	GhWnd = GUI_hWnd;
	ClearID = baseID + MAX_RECENT_ITEMS;
	menuItem = menuItem;
	recentmenu = CreateMenu();
}

RecentMenu::RecentMenu()
{
	BaseID = 0;
	GhWnd = 0;
	ClearID = BaseID + MAX_RECENT_ITEMS;
	recentmenu = 0;
	rtype = "";
}

//**************************************************************
int RecentMenu::GetClearID()
{
	return ClearID;
}

int RecentMenu::GetAutoloadID()
{
	return ClearID+1;
}

void RecentMenu::SetID(int base)
{
	BaseID = base;
	ClearID = BaseID + MAX_RECENT_ITEMS;
}

void RecentMenu::SetGUI_hWnd(HWND GUI_hWnd)
{
	GhWnd = GUI_hWnd;
}

void RecentMenu::SetMenuID(int menuID)
{
	menuItem = menuID;
}

void RecentMenu::MakeRecentMenu(HINSTANCE instance)
{
	recentmenu = CreateMenu();
}

//**************************************************************
//These functions assume filename is a file that was successfully loaded
void RecentMenu::UpdateRecentItems(const char* filename)
{
	UpdateRecent(filename);	//Converts it to std::string
}

void RecentMenu::UpdateRecentItems(std::string filename) //Overloaded function
{	
	UpdateRecent(filename); 
}

//**************************************************************

void RecentMenu::RemoveRecentItem(std::string filename)
{
	RemoveRecent(filename);
}

void RecentMenu::RemoveRecentItem(const char* filename)
{
	RemoveRecent(filename);
}

//**************************************************************

void RecentMenu::GetRecentItemsFromIni(std::string iniFilename, std::string section)
{
	//This function retrieves the recent items stored in the .ini file
	//Then is populates the RecentMenu array
	//Then it calls Update RecentMenu() to populate the menu

	std::stringstream temp;
	char tempstr[256];
	
	RecentItems.clear(); // (Avoids duplicating when changing languages)

	//Loops through all available recent slots
	for (int x = 0; x < MAX_RECENT_ITEMS; x++)
	{
		temp.str("");
		temp << "Recent " << rtype.c_str() << " " << (x+1);

		GetPrivateProfileString(section.c_str(), temp.str().c_str(),"", tempstr, 256, iniFilename.c_str());
		if (tempstr[0])
			RecentItems.push_back(tempstr);
	}

	autoload = GetPrivateProfileInt(section.c_str(), autoloadstr.c_str(), 0, iniFilename.c_str());	//TODO: Don't just call it autoload, add type parameter to it
	UpdateRecentItemsMenu();
}

void RecentMenu::SaveRecentItemsToIni(std::string iniFilename, std::string section)
{
	//This function stores the RecentItems vector to the .ini file

	std::stringstream temp;

	//Loops through all available recent slots
	for (unsigned int x = 0; x < MAX_RECENT_ITEMS; x++)
	{
		temp.str("");
		temp << "Recent " << rtype.c_str() <<  " " << (x+1);
		if (x < RecentItems.size())	//If it exists in the array, save it
			WritePrivateProfileString(section.c_str(), temp.str().c_str(), RecentItems[x].c_str(), iniFilename.c_str());
		else						//Else, make it empty
			WritePrivateProfileString(section.c_str(), temp.str().c_str(), "", iniFilename.c_str());
	}
	
	char STR[8];
	sprintf(STR, "%d", autoload);
	
	WritePrivateProfileString(section.c_str(), autoloadstr.c_str(), STR, iniFilename.c_str());	//TODO: Don't just call it autoload, add type parameter to it

}

void RecentMenu::ClearRecentItems()
{
	RecentItems.clear();
	UpdateRecentItemsMenu();
}

std::string RecentMenu::GetRecentItem(unsigned int listNum)
{
	if (listNum >= MAX_RECENT_ITEMS) return ""; 
	if (RecentItems.size() <= listNum) return "";

	return RecentItems[listNum];
}

void RecentMenu::SetType(std::string type)
{
	rtype = type;
	autoloadstr = "Autoload" + type;
}

std::string RecentMenu::GetType()
{
	return rtype;
}

//*******************************************************************
//Private functions
//*******************************************************************
void RecentMenu::RemoveRecent(std::string filename)
{
	//Called By RemoveRecentItem to do the actual work for the overloaded functions

	std::vector<std::string>::iterator x;
	std::vector<std::string>::iterator theMatch;
	bool match = false;
	for (x = RecentItems.begin(); x < RecentItems.end(); ++x)
	{
		if (filename == *x)
		{
			//We have a match
			match = true;	//Flag that we have a match
			theMatch = x;	//Hold on to the iterator	(Note: the loop continues, so if for some reason we had a duplicate (which wouldn't happen under normal circumstances, it would pick the last one in the list)
		}
	}
	//----------------------------------------------------------------
	//If there was a match, remove it
	if (match)
		RecentItems.erase(theMatch);

	UpdateRecentItemsMenu();
}

void RecentMenu::UpdateRecent(std::string newItem)
{
	//Called By UpdateRecentItems to do the actual work for these overloaded functions

	//--------------------------------------------------------------
	//Check to see if filename is in list
	std::vector<std::string>::iterator x;
	std::vector<std::string>::iterator theMatch;
	bool match = false;
	for (x = RecentItems.begin(); x < RecentItems.end(); ++x)
	{
		if (newItem == *x)
		{
			//We have a match
			match = true;	//Flag that we have a match
			theMatch = x;	//Hold on to the iterator	(Note: the loop continues, so if for some reason we had a duplicate (which wouldn't happen under normal circumstances, it would pick the last one in the list)
		}
	}
	//----------------------------------------------------------------
	//If there was a match, remove it
	if (match)
		RecentItems.erase(theMatch);

	RecentItems.insert(RecentItems.begin(), newItem);	//Add to the array

	//If over the max, we have too many, so remove the last entry
	if (RecentItems.size() == MAX_RECENT_ITEMS)	
		RecentItems.pop_back();

	UpdateRecentItemsMenu();
}

void RecentMenu::UpdateRecentItemsMenu()
{
	//This function will be called to populate the Recent Menu
	//The array must be in the proper order ahead of time

	//UpdateRecentRoms will always call this
	//This will be always called by GetRecentItems as well

	//----------------------------------------------------------------------
	//Get Menu item info

	MENUITEMINFO moo;
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU | MIIM_STATE;

	GetMenuItemInfo(GetSubMenu(GetMenu(GhWnd), 0), menuItem, FALSE, &moo);
	moo.hSubMenu = recentmenu;
	moo.fState = MFS_ENABLED;
	SetMenuItemInfo(GetSubMenu(GetMenu(GhWnd), 0), menuItem, FALSE, &moo);

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
	//Clear the current menu items
	for(int x = 0; x < MAX_RECENT_ITEMS; x++)
	{
		DeleteMenu(recentmenu, BaseID + x, MF_BYCOMMAND);	
	}
	DeleteMenu(recentmenu, ClearID,   MF_BYCOMMAND);
	DeleteMenu(recentmenu, ClearID+1, MF_BYCOMMAND);
	DeleteMenu(recentmenu, ClearID+2, MF_BYCOMMAND);

	if(RecentItems.size() == 0)
	{
		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_TYPE;
		moo.cch = 5;
		moo.fType = 0;
		moo.wID = ClearID+1;
		moo.dwTypeData = "Auto-load";
		moo.fState = MF_ENABLED;
		
		InsertMenuItem(recentmenu, 0, TRUE, &moo);
		
		moo.wID = ClearID;
		moo.dwTypeData = "Clear";
		moo.fState = MF_GRAYED;

		InsertMenuItem(recentmenu, 0, TRUE, &moo);

		moo.fType = MFT_SEPARATOR;
		moo.wID = ClearID+2;
		InsertMenuItem(recentmenu, 0, TRUE, &moo);

		moo.fType = 0;
		moo.dwTypeData = "None";
		moo.wID = BaseID;
		InsertMenuItem(recentmenu, 0, TRUE, &moo);

		return;
	}

	

	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_TYPE;
	moo.cch = 5;
	moo.fType = 0;
	moo.wID = ClearID+1;
	moo.dwTypeData = "Auto-load";
	moo.fState = MF_ENABLED;
	InsertMenuItem(recentmenu, 0, TRUE, &moo);

	moo.wID = ClearID;
	moo.dwTypeData = "Clear";
	InsertMenuItem(recentmenu, 0, TRUE, &moo);

	moo.fType = MFT_SEPARATOR;
	moo.wID = ClearID+2;
	InsertMenuItem(recentmenu, 0, TRUE, &moo);

	EnableMenuItem(recentmenu, ClearID, MF_ENABLED);
	CheckMenuItem(recentmenu,ClearID+1,MF_BYCOMMAND   | (autoload ? MF_CHECKED:MF_UNCHECKED));
	DeleteMenu(recentmenu, BaseID, MF_BYCOMMAND);

	HDC dc = GetDC(GhWnd);

	//-----------------------------------------------------------------------
	//Update the list using RecentRoms vector
	for(int x = RecentItems.size()-1; x >= 0; x--)	//Must loop in reverse order since InsertMenuItem will insert as the first on the list
	{
		std::string tmp = RecentItems[x];
		LPSTR tmp2 = (LPSTR)tmp.c_str();

		PathCompactPath(dc, tmp2, 500);	//TODO: figure out how to use this without unresolved external

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;

		moo.cch = tmp.size();
		moo.fType = 0;
		moo.wID = BaseID + x;
		moo.dwTypeData = tmp2;

		InsertMenuItem(recentmenu, 0, 1, &moo);
	}
	ReleaseDC(GhWnd, dc);
	//-----------------------------------------------------------------------

	DrawMenuBar(GhWnd);
}