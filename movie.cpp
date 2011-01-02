#include <assert.h>
#include <limits.h>
#include <fstream>
//#include "utils/guid.h"
#include "utils/xstring.h"
#include "movie.h"
#include "main.h"
#include "pcejin.h"

extern void PCE_Power();

#include "utils/readwrite.h"

using namespace std;
bool freshMovie = false;	  //True when a movie loads, false when movie is altered.  Used to determine if a movie has been altered since opening
bool autoMovieBackup = true;

#define MOVIE_VERSION 1
#define DESMUME_VERSION_NUMERIC 9

//----movie engine main state
extern bool PCE_IsCD;

EMOVIEMODE movieMode = MOVIEMODE_INACTIVE;

//this should not be set unless we are in MOVIEMODE_RECORD!
fstream* osRecordingMovie = 0;

//////////////////////////////////////////////////////////////////////////////

int LagFrameFlag;
int FrameAdvanceVariable=0;
int currFrameCounter;
char MovieStatusM[40];
int LagFrameCounter;
extern "C" {
int AutoAdvanceLag;
}
//////////////////////////////////////////////////////////////////////////////

uint32 cur_input_display = 0;
int pauseframe = -1;
bool movie_readonly = true;

char curMovieFilename[512] = {0};
MovieData currMovieData;
int currRerecordCount;

//--------------
#include "mednafen.h"

extern void DisplayMessage(char* str);

void MovieData::clearRecordRange(int start, int len)
{
	for(int i=0;i<len;i++)
		records[i+start].clear();
}

void MovieData::insertEmpty(int at, int frames)
{
	if(at == -1) 
	{
		int currcount = records.size();
		records.resize(records.size()+frames);
		clearRecordRange(currcount,frames);
	}
	else
	{
		records.insert(records.begin()+at,frames,MovieRecord());
		clearRecordRange(at,frames);
	}
}



void MovieRecord::clear()
{ 
	for (int i = 0; i < 5; i++)
		pad[i] = 0;
	commands = 0;
	touch.padding = 0;
}


const char MovieRecord::mnemonics[8] = {'U','D','L','R','1','2','N','S'};
void MovieRecord::dumpPad(std::ostream* os, uint16 pad)
{
	//these are mnemonics for each joystick bit.
	//since we usually use the regular joypad, these will be more helpful.
	//but any character other than ' ' or '.' should count as a set bit
	//maybe other input types will need to be encoded another way..
	for(int bit=0;bit<8;bit++)
	{
		int bitmask = (1<<(7-bit));
		char mnemonic = mnemonics[bit];
		//if the bit is set write the mnemonic
		if(pad & bitmask)
			os->put(mnemonic);
		else //otherwise write an unset bit
			os->put('.');
	}
}
#undef read
#undef open
#undef close
#define read read
#define open open
#define close close


void MovieRecord::parsePad(std::istream* is, uint16& pad)
{
	char buf[8];
	is->read(buf,8);
	pad = 0;
	for(int i=0;i<8;i++)
	{
		pad <<= 1;
		pad |= ((buf[i]=='.'||buf[i]==' ')?0:1);
	}
}


void MovieRecord::parse(MovieData* md, std::istream* is)
{
	//by the time we get in here, the initial pipe has already been extracted

	//extract the commands
	commands = u32DecFromIstream(is);
	
	is->get(); //eat the pipe

	for (int i = 0; i < md->ports; i++) {
		parsePad(is, pad[i]);
		is->get();
	}
		
	is->get(); //eat the pipe

	//should be left at a newline
}


void MovieRecord::dump(MovieData* md, std::ostream* os, int index)
{
	//dump the misc commands
	//*os << '|' << setw(1) << (int)commands;
	os->put('|');
	putdec<uint8,1,true>(os,commands);

	os->put('|');
//	dumpPad(os, pad);
	for (int i = 0; i < md->ports; i++) 
	{
		dumpPad(os,pad[i]); 
		os->put('|');
	}
	
	//each frame is on a new line
	os->put('\n');
}

MovieData::MovieData()
	: version(MOVIE_VERSION)
	, emuVersion(DESMUME_VERSION_NUMERIC)
	, rerecordCount(1)
	, binaryFlag(false)
	//, greenZoneCount(0)
{
//	memset(&romChecksum,0,sizeof(MD5DATA));
}

void MovieData::truncateAt(int frame)
{
	records.resize(frame);
}


void MovieData::installValue(std::string& key, std::string& val)
{
	//todo - use another config system, or drive this from a little data structure. because this is gross
	if(key == "PCECD")
		installInt(val,pcecd);
	else if(key == "version")
		installInt(val,version);
	else if(key == "emuVersion")
		installInt(val,emuVersion);
	else if(key == "rerecordCount")
		installInt(val,rerecordCount);
	else if(key == "romFilename")
		romFilename = val;
	else if(key == "ports")
		installInt(val, ports);
//	else if(key == "romChecksum")							//adelikat: TODO: implemement this header info one day
//		StringToBytes(val,&romChecksum,MD5DATA::size);
//	else if(key == "guid")
//		guid = Desmume_Guid::fromString(val);
	else if(key == "comment")
		comments.push_back(val);
	else if(key == "binary")
		installBool(val,binaryFlag);
	else if(key == "savestate")
	{
		int len = Base64StringToBytesLength(val);
		if(len == -1) len = HexStringToBytesLength(val); // wasn't base64, try hex
		if(len >= 1)
		{
			savestate.resize(len);
			StringToBytes(val,&savestate[0],len); // decodes either base64 or hex
		}
	}
}

char* RemovePath(char * input) {

	char* temp=(char*)malloc(1024);
	strcpy(temp, input);

	if (strrchr(temp, '/'))
        temp = 1 + strrchr(temp, '/');

	if (strrchr(temp, '\\'))
        temp = 1 + strrchr(temp, '\\');

	return temp;
}


int MovieData::dump(std::ostream *os, bool binary)
{
	int start = os->tellp();
	*os << "version " << version << endl;
	*os << "emuVersion " << emuVersion << endl;
	*os << "rerecordCount " << rerecordCount << endl;
	*os << "ports " << ports << endl;
	*os << "PCECD " << PCE_IsCD << endl;
/*	*os << "cdGameName " << cdip->gamename << endl;
	*os << "cdInfo " << cdip->cdinfo << endl;
	*os << "cdItemNum " << cdip->itemnum << endl;
	*os << "cdVersion " << cdip->version << endl;
	*os << "cdDate " << cdip->date << endl;
	*os << "cdRegion " << cdip->region << endl;

*/

//	*os << "romChecksum " << BytesToString(romChecksum.data,MD5DATA::size) << endl;
//	*os << "guid " << guid.toString() << endl;

	for(u32 i=0;i<comments.size();i++)
		*os << "comment " << comments[i] << endl;
	
	if(binary)
		*os << "binary 1" << endl;
		
	if(savestate.size() != 0)
		*os << "savestate " << BytesToString(&savestate[0],savestate.size()) << endl;
	if(binary)
	{
		//put one | to start the binary dump
		os->put('|');
		for(int i=0;i<(int)records.size();i++)
			records[i].dumpBinary(this,os,i);
	}
	else
		for(int i=0;i<(int)records.size();i++)
			records[i].dump(this,os,i);

	int end = os->tellp();
	return end-start;
}

//yuck... another custom text parser.
bool LoadMC2(MovieData& movieData, std::istream* fp, int size, bool stopAfterHeader)
{
	//TODO - start with something different. like 'PCEjin movie version 1"
	std::ios::pos_type curr = fp->tellg();

	//movie must start with "version 1"
	char buf[9];
	curr = fp->tellg();
	fp->read(buf,9);
	fp->seekg(curr);
	if(fp->fail()) return false;
	if(memcmp(buf,"version 1",9)) 
		return false;

	std::string key,value;
	enum {
		NEWLINE, KEY, SEPARATOR, VALUE, RECORD, COMMENT
	} state = NEWLINE;
	bool bail = false;
	for(;;)
	{
		bool iswhitespace, isrecchar, isnewline;
		int c;
		if(size--<=0) goto bail;
		c = fp->get();
		if(c == -1)
			goto bail;
		iswhitespace = (c==' '||c=='\t');
		isrecchar = (c=='|');
		isnewline = (c==10||c==13);
		if(isrecchar && movieData.binaryFlag && !stopAfterHeader)
		{
			LoadMC2_binarychunk(movieData, fp, size);
			return true;
		}
		switch(state)
		{
		case NEWLINE:
			if(isnewline) goto done;
			if(iswhitespace) goto done;
			if(isrecchar) 
				goto dorecord;
			//must be a key
			key = "";
			value = "";
			goto dokey;
			break;
		case RECORD:
			{
				dorecord:
				if (stopAfterHeader) return true;
				int currcount = movieData.records.size();
				movieData.records.resize(currcount+1);
				int preparse = fp->tellg();
				movieData.records[currcount].parse(&movieData, fp);
				int postparse = fp->tellg();
				size -= (postparse-preparse);
				state = NEWLINE;
				break;
			}

		case KEY:
			dokey: //dookie
			state = KEY;
			if(iswhitespace) goto doseparator;
			if(isnewline) goto commit;
			key += c;
			break;
		case SEPARATOR:
			doseparator:
			state = SEPARATOR;
			if(isnewline) goto commit;
			if(!iswhitespace) goto dovalue;
			break;
		case VALUE:
			dovalue:
			state = VALUE;
			if(isnewline) goto commit;
			value += c;
			break;
//		case COMMENT:
		default:
			break;
		}
		goto done;

		bail:
		bail = true;
		if(state == VALUE) goto commit;
		goto done;
		commit:
		movieData.installValue(key,value);
		state = NEWLINE;
		done: ;
		if(bail) break;
	}

	return true;
}


static void closeRecordingMovie()
{
	if(osRecordingMovie)
	{
		delete osRecordingMovie;
		osRecordingMovie = 0;
	}
}

/// Stop movie playback.
static void StopPlayback()
{
	DisplayMessage("Movie playback stopped.");
	strcpy(MovieStatusM, "");
	movieMode = MOVIEMODE_INACTIVE;
}


/// Stop movie recording
static void StopRecording()
{
	DisplayMessage("Movie recording stopped.");
	strcpy(MovieStatusM, "");
	movieMode = MOVIEMODE_INACTIVE;
	
	closeRecordingMovie();
}

void ResetFrameCount() {
	//for loading roms
	currFrameCounter = 0;
	LagFrameCounter = 0;
	pcejin.lagFrameCounter = 0;
}

void StopMovie()
{
	if(movieMode == MOVIEMODE_PLAY)
		StopPlayback();
	else if(movieMode == MOVIEMODE_RECORD)
		StopRecording();

	curMovieFilename[0] = 0;
	freshMovie = false;
}
	extern void PCE_Power(void);

extern uint8 SaveRAM[2048];

void ClearPCESRAM(void) {

  memset(SaveRAM, 0x00, 2048);
  memcpy(SaveRAM, "HUBM\x00\xa0\x10\x80", 8);  

}
//begin playing an existing movie
void LoadMovie(const char *fname, bool _read_only, bool tasedit, int _pauseframe)
{
	assert(fname);

	if(movieMode == MOVIEMODE_PLAY)
		StopPlayback();
	else if(movieMode == MOVIEMODE_RECORD)
		StopRecording();

	currMovieData = MovieData();
	
	strcpy(curMovieFilename, fname);

	currMovieData.ports = 1;	
	
	fstream fs (fname);
	LoadMC2(currMovieData, &fs, INT_MAX, false);
	fs.close();

	//TODO
	//fully reload the game to reinitialize everything before playing any movie
	//poweron(true);

//	extern bool _HACK_DONT_STOPMOVIE;	//adelikat: TODO: This will be needed in order to record power-cycles into movies
//	_HACK_DONT_STOPMOVIE = true;
	PCE_Power();
//	_HACK_DONT_STOPMOVIE = false;
	////WE NEED TO LOAD A SAVESTATE				//adelikat: TODO: Recording movies from savestates, this will need to be implemented one day
	//if(currMovieData.savestate.size() != 0)
	//{
	//	bool success = MovieData::loadSavestateFrom(&currMovieData.savestate);
	//	if(!success) return;
	//}
	
	pcejin.lagFrameCounter = 0;
	pcejin.isLagFrame = false;
	currFrameCounter = 0;
	pauseframe = _pauseframe;
	movie_readonly = _read_only;
	movieMode = MOVIEMODE_PLAY;
	currRerecordCount = currMovieData.rerecordCount;
	ClearPCESRAM();
	freshMovie = true;

	if(movie_readonly)
		DisplayMessage("Replay started Read-Only.");
	else
		DisplayMessage("Replay started Read+Write.");
}

static void openRecordingMovie(const char* fname)
{
	osRecordingMovie = new fstream(fname,std::ios_base::out);
	if(!osRecordingMovie)
		MDFN_PrintError("Error opening movie output file: %s",fname);
	strcpy(curMovieFilename, fname);
}


//begin recording a new movie
//TODO - BUG - the record-from-another-savestate doesnt work.
 void SaveMovie(const char *fname, std::string author, int controllers)
{
	assert(fname);

	StopMovie();

	openRecordingMovie(fname);

	currFrameCounter = 0;
	//LagCounterReset();

	currMovieData = MovieData();
//	currMovieData.guid.newGuid();

	if(author != "") currMovieData.comments.push_back("author " + author);
	//currMovieData.romChecksum = GameInfo->MD5;
//	currMovieData.romFilename = cdip->gamename;//GetRomName();
	
//	extern bool _HACK_DONT_STOPMOVIE;
//	_HACK_DONT_STOPMOVIE = true;
	pcejin.lagFrameCounter = 0;
	pcejin.isLagFrame = false;
	PCE_Power();
//	_HACK_DONT_STOPMOVIE = false;
	currMovieData.ports = controllers;

	//todo ?
	//poweron(true);
	//else
	//	MovieData::dumpSavestateTo(&currMovieData.savestate,Z_BEST_COMPRESSION);

	//we are going to go ahead and dump the header. from now on we will only be appending frames
	currMovieData.dump(osRecordingMovie, false);

	movieMode = MOVIEMODE_RECORD;
	movie_readonly = false;
	currRerecordCount = 0;
	ClearPCESRAM();
	LagFrameCounter=0;

	DisplayMessage("Movie recording started.");
}

#define MDFNNPCMD_RESET 	0x01
#define MDFNNPCMD_POWER 	0x02

uint16 pcepad;

void NDS_setPadFromMovie(uint16 pad[])
{
	for (int i = 0; i < currMovieData.ports; i++) {
	pcepad = 0;

	if(pad[i] &(1 << 7)) pcepad |= (1 << 4);//u
	if(pad[i] &(1 << 6)) pcepad |= (1 << 6);//d
	if(pad[i] &(1 << 5)) pcepad |= (1 << 7);//l
	if(pad[i] &(1 << 4)) pcepad |= (1 << 5);//r
	if(pad[i] &(1 << 3)) pcepad |= (1 << 0);//o
	if(pad[i] &(1 << 2)) pcepad |= (1 << 1);//t
	if(pad[i] &(1 << 0)) pcepad |= (1 << 2);//s
	if(pad[i] &(1 << 1)) pcepad |= (1 << 3);//n

	pcejin.pads[i] = pcepad;
	}

}
 static int _currCommand = 0;
extern void *PortDataCache[16];
 //the main interaction point between the emulator and the movie system.
 //either dumps the current joystick state or loads one state from the movie
void MOV_AddInputState()
 {
	 if(LagFrameFlag == 1)
		 LagFrameCounter++;
	 LagFrameFlag=1;

	 if(movieMode == MOVIEMODE_PLAY)
	 {
		 //stop when we run out of frames
		 if(currFrameCounter >= (int)currMovieData.records.size())
		 {
			 StopPlayback();
		 }
		 else
		 {
			 strcpy(MovieStatusM, "Playback");

			 MovieRecord* mr = &currMovieData.records[currFrameCounter];

			 //reset if necessary
			 if(mr->command_reset())
				 PCE_Power();


			 if(mr->command_power())
				 PCE_Power();

			 NDS_setPadFromMovie(mr->pad);
		 }

		 //if we are on the last frame, then pause the emulator if the player requested it
		 if(currFrameCounter == (int)currMovieData.records.size()-1)
		 {
			 /*if(FCEUD_PauseAfterPlayback())	//TODO: Implement pause after playback?
			 {
			 FCEUI_ToggleEmulationPause();
			 }*/
		 }
	 }
	 else if(movieMode == MOVIEMODE_RECORD)
	 {
		 MovieRecord mr;
		mr.commands = _currCommand;
		_currCommand = 0;

		 strcpy(MovieStatusM, "Recording");

		 int II, I, n, s, u, r, d, l;

		 for (int i = 0; i < currMovieData.ports; i++) {

			 pcepad = pcejin.pads[i];

#define FIX(b) (b?1:0)
			 II = FIX(pcepad &(1 << 1));
			 I = FIX(pcepad & (1 << 0));
			 n = FIX(pcepad & (1 << 2));
			 s = FIX(pcepad & (1 << 3));
			 u = FIX(pcepad & (1 << 4));
			 r = FIX(pcepad & (1 << 7));
			 d = FIX(pcepad & (1 << 6));
			 l = FIX(pcepad & (1 << 5));

			 mr.pad[i] =
				 (FIX(r)<<5)|
				 (FIX(l)<<4)|
				 (FIX(u)<<7)|
				 (FIX(d)<<6)|
				 (FIX(I)<<3)|
				 (FIX(II)<<2)|
				 (FIX(s)<<1)|
				 (FIX(n)<<0);
		 }

		 mr.dump(&currMovieData, osRecordingMovie,currMovieData.records.size());
		 currMovieData.records.push_back(mr);
	 }

	 currFrameCounter++;

	 /*extern u8 joy[4];
	 memcpy(&cur_input_display,joy,4);*/
 }


//TODO 
static void MOV_AddCommand(int cmd)
{
	// do nothing if not recording a movie
	if(movieMode != MOVIEMODE_RECORD)
		return;
	
	//printf("%d\n",cmd);

	//MBG TODO BIG TODO TODO TODO
	//DoEncode((cmd>>3)&0x3,cmd&0x7,1);
}

//little endian 4-byte cookies
static const int kMOVI = 0x49564F4D;
static const int kNOMO = 0x4F4D4F4E;

#define LOCAL_LE

int read32lemov(int *Bufo, std::istream *is)
{
	uint32 buf;
	if(is->read((char*)&buf,4).gcount() != 4)
		return 0;
#ifdef LOCAL_LE
	*(uint32*)Bufo=buf;
#else
	*(u32*)Bufo=((buf&0xFF)<<24)|((buf&0xFF00)<<8)|((buf&0xFF0000)>>8)|((buf&0xFF000000)>>24);
#endif
	return 1;
}


int write32lemov(uint32 b, std::ostream* os)
{
	uint8 s[4];
	s[0]=b;
	s[1]=b>>8;
	s[2]=b>>16;
	s[3]=b>>24;
	os->write((char*)&s,4);
	return 4;
}

void mov_savestate(std::ostream *os)//std::ostream* os
{
	if(movieMode == MOVIEMODE_RECORD || movieMode == MOVIEMODE_PLAY)
	{
		write32lemov(kMOVI,os);
		currMovieData.dump(os, true);
	}
	else
	{
		write32lemov(kNOMO,os);
	}
}

static bool load_successful;

bool mov_loadstate(std::istream* is, int size)//std::istream* is
{
	load_successful = false;

	int cookie;
	if(read32lemov(&cookie,is) != 1) return false;
	if(cookie == kNOMO)
		return true;
	else if(cookie != kMOVI)
		return false;

	size -= 4;

	if (!movie_readonly && autoMovieBackup && freshMovie) //If auto-backup is on, movie has not been altered this session and the movie is in read+write mode
		MakeBackupMovie(false);	//Backup the movie before the contents get altered, but do not display messages						  

	MovieData tempMovieData = MovieData();
	std::ios::pos_type curr = is->tellg();
	if(!LoadMC2(tempMovieData, is, size, false)) 		
		return false;

	//----------------
	//complex TAS logic for loadstate
	//fully conforms to the savestate logic documented in the Laws of TAS
	//http://tasvideos.org/LawsOfTAS/OnSavestates.html
	//----------------
		
	/*
	Playback or Recording + Read-only

    * Check that GUID of movie and savestate-movie must match or else error
          o on error: a message informing that the savestate doesn't belong to this movie. This is a GUID mismatch error. Give user a choice to load it anyway.
                + failstate: if use declines, loadstate attempt canceled, movie can resume as if not attempted if user has backup savstates enabled else stop movie
    * Check that movie and savestate-movie are in same timeline. If not then this is a wrong timeline error.
          o on error: a message informing that the savestate doesn't belong to this movie
                + failstate: loadstate attempt canceled, movie can resume as if not attempted if user has backup savestates enabled else stop movie
    * Check that savestate-movie is not greater than movie. If not then this is a future event error and is not allowed in read-only
          o on error: message informing that the savestate is from a frame after the last frame of the movie
                + failstate - loadstate attempt cancelled, movie can resume if user has backup savesattes enabled, else stop movie
    * Check that savestate framcount <= savestate movie length. If not this is a post-movie savestate
          o on post-movie: See post-movie event section. 
    * All error checks have passed, state will be loaded
    * Movie contained in the savestate will be discarded
    * Movie is now in Playback mode 

	Playback, Recording + Read+write

    * Check that GUID of movie and savestate-movie must match or else error
          o on error: a message informing that the savestate doesn't belong to this movie. This is a GUID mismatch error. Give user a choice to load it anyway.
                + failstate: if use declines, loadstate attempt canceled, movie can resume as if not attempted (stop movie if resume fails)canceled, movie can resume if backup savestates enabled else stopmovie
    * Check that savestate framcount <= savestate movie length. If not this is a post-movie savestate
          o on post-movie: See post-movie event section. 
    * savestate passed all error checks and will now be loaded in its entirety and replace movie (note: there will be no truncation yet)
    * current framecount will be set to savestate framecount
    * on the next frame of input, movie will be truncated to framecount
          o (note: savestate-movie can be a future event of the movie timeline, or a completely new timeline and it is still allowed) 
    * Rerecord count of movie will be incremented
    * Movie is now in record mode 

	Post-movie savestate event

    * Whan a savestate is loaded and is determined that the savestate-movie length is less than the savestate framecount then it is a post-movie savestate. These occur when a savestate was made during Movie Finished mode. 
	* If read+write, the entire savestate movie will be loaded and replace current movie.
    * If read-only, the savestate movie will be discarded
    * Mode will be switched to Move Finished
    * Savestate will be loaded
    * Current framecount changes to savestate framecount
    * User will have control of input as if Movie inactive mode 
	*/


	if(movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_RECORD)
	{
		//handle moviefile mismatch
/*		if(tempMovieData.guid != currMovieData.guid)
		{
			//mbg 8/18/08 - this code  can be used to turn the error message into an OK/CANCEL
			#ifdef WIN32
				//std::string msg = "There is a mismatch between savestate's movie and current movie.\ncurrent: " + currMovieData.guid.toString() + "\nsavestate: " + tempMovieData.guid.toString() + "\n\nThis means that you have loaded a savestate belonging to a different movie than the one you are playing now.\n\nContinue loading this savestate anyway?";
				//extern HWND pwindow;
				//int result = MessageBox(pwindow,msg.c_str(),"Error loading savestate",MB_OKCANCEL);
				//if(result == IDCANCEL)
				//	return false;
			#else
				DisplayMessage("Mismatch between savestate's movie and current movie.\ncurrent: %s\nsavestate: %s\n",currMovieData.guid.toString().c_str(),tempMovieData.guid.toString().c_str());
				return false;
			#endif
		}*/

		closeRecordingMovie();

		if(movie_readonly)
		{
			//-------------------------------------------------------------
			//this code would reload the movie from disk. allegedly it is helpful to hexers, but
			//it is way too slow with dsm format. so it is only here as a reminder, and in case someone
			//wants to play with it
			//-------------------------------------------------------------
			//{
			//	fstream fs (curMovieFilename);
			//	if(!LoadMC2(tempMovieData, &fs, INT_MAX, false))
			//	{
			//		MDFN_PrintError("Failed to reload DSM after loading savestate");
			//	}
			//	fs.close();
			//	currMovieData = tempMovieData;
			//}
			//-------------------------------------------------------------

			//if the frame counter is longer than our current movie, then error
			if(currFrameCounter > (int)currMovieData.records.size())
			{
				MDFN_PrintError("Savestate is from a frame (%d) after the final frame in the movie (%d). This is not permitted.", currFrameCounter, currMovieData.records.size()-1);
				return false;
			}

			movieMode = MOVIEMODE_PLAY;
		}
		else
		{
			//truncate before we copy, just to save some time
			tempMovieData.truncateAt(currFrameCounter);
			currMovieData = tempMovieData;

			currRerecordCount++;
			
			currMovieData.rerecordCount = currRerecordCount;

			openRecordingMovie(curMovieFilename);
			currMovieData.dump(osRecordingMovie, false);
			movieMode = MOVIEMODE_RECORD;
		}
	}
	
	load_successful = true;
	freshMovie = false;

	//// Maximus: Show the last input combination entered from the
	//// movie within the state
	//if(current!=0) // <- mz: only if playing or recording a movie
	//	memcpy(&cur_input_display, joop, 4);

	return true;
}

void SaveStateMovie(char* filename) {

	strcat (filename, "mov");
	filebuf fb;
	fb.open (filename, ios::out | ios::binary);//ios::out
	ostream os(&fb);
	mov_savestate(&os);
	fb.close();
}

void LoadStateMovie(char* filename) {

	std::string fname = (std::string)filename + "mov";

    FILE * fp = fopen( fname.c_str(), "r" );
	if(!fp)
		return;
	
	fseek( fp, 0, SEEK_END );
	int size = ftell(fp);
	fclose(fp);

	filebuf fb;
	fb.open (fname.c_str(), ios::in | ios::binary);//ios::in
	istream is(&fb);
	mov_loadstate(&is, size);
	fb.close();
}

static void MOV_PreLoad(void)
{
	load_successful = 0;
}

static bool MOV_PostLoad(void)
{
	if(movieMode == MOVIEMODE_INACTIVE)
		return true;
	else
		return load_successful;
}


bool MovieGetInfo(std::istream* fp, MOVIE_INFO& info, bool skipFrameCount)
{
	//adelikat: TODO: implement this !
	//MovieData md;
	//if(!LoadMC2(md, fp, INT_MAX, skipFrameCount))
	//	return false;
	//
	//info.movie_version = md.version;
	//info.poweron = md.savestate.size()==0;
	//info.pal = md.palFlag;
	//info.nosynchack = true;
	//info.num_frames = md.records.size();
	//info.md5_of_rom_used = md.romChecksum;
	//info.emu_version_used = md.emuVersion;
	//info.name_of_rom_used = md.romFilename;
	//info.rerecord_count = md.rerecordCount;
	//info.comments = md.comments;

	return true;
}

bool MovieRecord::parseBinary(MovieData* md, std::istream* is)
{
	commands=is->get();
		
	for (int i = 0; i < md->ports; i++)
	{
		is->read((char *) &pad[i], sizeof pad[i]);

	//	parseJoy(is,pad[0]); is->get(); //eat the pipe
	//	parseJoy(is,joysticks[1]); is->get(); //eat the pipe
//		parseJoy(is,joysticks[2]); is->get(); //eat the pipe
//		parseJoy(is,joysticks[3]); is->get(); //eat the pipe
	}
//	else
//		is->read((char *) &pad, sizeof pad);
//	is->read((char *) &touch.x, sizeof touch.x);
//	is->read((char *) &touch.y, sizeof touch.y);
//	is->read((char *) &touch.touch, sizeof touch.touch);
	return true;
}


void MovieRecord::dumpBinary(MovieData* md, std::ostream* os, int index)
{
	os->put(md->records[index].commands);
	for (int i = 0; i < md->ports; i++) {
	os->write((char *) &md->records[index].pad[i], sizeof md->records[index].pad[i]);
	}
//	os->write((char *) &md->records[index].touch.x, sizeof md->records[index].touch.x);
//	os->write((char *) &md->records[index].touch.y, sizeof md->records[index].touch.y);
//	os->write((char *) &md->records[index].touch.touch, sizeof md->records[index].touch.touch);
}

void LoadMC2_binarychunk(MovieData& movieData, std::istream* fp, int size)	//Load MC2 file
{
	int recordsize = 1; //1 for the command

	recordsize = 3;

	assert(size%3==0);

	//find out how much remains in the file
	int curr = fp->tellg();
	fp->seekg(0,std::ios::end);
	int end = fp->tellg();
	int flen = end-curr;
	fp->seekg(curr,std::ios::beg);

#undef min
	//the amount todo is the min of the limiting size we received and the remaining contents of the file
	int todo = std::min(size, flen);

	int numRecords = todo/recordsize;
	movieData.records.resize(numRecords);
	for(int i=0;i<numRecords;i++)
	{
		movieData.records[i].parseBinary(&movieData,fp);
	}
}

#include <sstream>

static bool CheckFileExists(const char* filename)
{
	//This function simply checks to see if the given filename exists
	string checkFilename; 

	if (filename)
		checkFilename = filename;
			
	//Check if this filename exists
	fstream test;
	test.open(checkFilename.c_str(),fstream::in);
		
	if (test.fail())
	{
		test.close();
		return false; 
	}
	else
	{
		test.close();
		return true; 
	}
}

void MakeBackupMovie(bool dispMessage)
{
	//This function generates backup movie files
	string currentFn;					//Current movie fillename
	string backupFn;					//Target backup filename
	string tempFn;						//temp used in back filename creation
	stringstream stream;
	int x;								//Temp variable for string manip
	bool exist = false;					//Used to test if filename exists
	bool overflow = false;				//Used for special situation when backup numbering exceeds limit

	currentFn = curMovieFilename;		//Get current moviefilename
	backupFn = curMovieFilename;		//Make backup filename the same as current moviefilename
	x = backupFn.find_last_of(".");		 //Find file extension
	backupFn = backupFn.substr(0,x);	//Remove extension
	tempFn = backupFn;					//Store the filename at this point
	for (unsigned int backNum=0;backNum<999;backNum++) //999 = arbituary limit to backup files
	{
		stream.str("");					 //Clear stream
		if (backNum > 99)
			stream << "-" << backNum;	 //assign backNum to stream
		 else if (backNum <= 99 && backNum >= 10)
			stream << "-0" << backNum;      //Make it 010, etc if two digits
		else
			stream << "-00" << backNum;	 //Make it 001, etc if single digit
		backupFn.append(stream.str());	 //add number to bak filename
		backupFn.append(".bak");		 //add extension

		exist = CheckFileExists(backupFn.c_str());	//Check if file exists
		
		if (!exist) 
			break;						//Yeah yeah, I should use a do loop or something
		else
		{
			backupFn = tempFn;			//Before we loop again, reset the filename
			
			if (backNum == 999)			//If 999 exists, we have overflowed, let's handle that
			{
				backupFn.append("-001.bak"); //We are going to simply overwrite 001.bak
				overflow = true;		//Flag that we have exceeded limit
				break;					//Just in case
			}
		}
	}

	MovieData md = currMovieData;								//Get current movie data
	std::fstream* outf = new fstream(backupFn.c_str(),std::ios_base::out);
	md.dump(outf,false);										//dump movie data
	delete outf;												//clean up, delete file object
	
	char* temp;
	//TODO, decide if fstream successfully opened the file and print error message if it doesn't
	if (dispMessage)	//If we should inform the user
	{
		if (overflow)
		{
			sprintf(temp, "Backup overflow, overwriting %s", backupFn.c_str());	//Inform user of overflow
			DisplayMessage(temp);
		}
		else;
		{
			sprintf(temp, "%s created", backupFn.c_str());	//Inform user of backup filename
			DisplayMessage(temp);
		}
	}
}

void ToggleReadOnly() {

	movie_readonly ^=1;

	if(movie_readonly==1) DisplayMessage("Movie is now read only.");
	else DisplayMessage("Movie is now read+write.");

}

int MovieIsActive() {

	if(movieMode == MOVIEMODE_INACTIVE)
		return false;
	
	return true;
}

void MakeMovieStateName(const char *filename) {

	if(movieMode != MOVIEMODE_INACTIVE)
		strcat ((char *)filename, "movie");
}

void FCEUI_MoviePlayFromBeginning(void)
{
	if (movieMode != MOVIEMODE_INACTIVE)
	{
		char *fname = strdup(curMovieFilename);
		LoadMovie(fname, true, false, 0);
		MDFN_DispMessage("Movie is now Read-Only. Playing from beginning.");
		free(fname);
	}
}