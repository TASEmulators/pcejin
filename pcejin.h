#include <string>
#include "utils\svnrev.h"
#include "main.h"

class Pcejin
{
public:

	Pcejin() : started(false), 
		aspectRatio(true), 
		romLoaded(false), 
		frameAdvance(false), 
		width(256), 
		height(232), 
		versionName(std::string("pcejin svn") + std::string(SVN_REV_STR)),
		slow(false) {
	}

	bool aspectRatio;
	bool started;
	bool romLoaded;

	bool isLagFrame;
	int lagFrameCounter;
	char inputDisplayString[128];
	int height;
	int width;
	int windowSize;

	std::string versionName;

	bool frameAdvance;
	bool fastForward;
	bool slow;

	u8 pads[5];

	virtual void pause() {

		if(romLoaded) {
			started ^=1;

		if(started && !soundDriver->userMute)
			soundDriver->unMute();
		else
			soundDriver->mute();
		}
	}
};

extern Pcejin pcejin;
