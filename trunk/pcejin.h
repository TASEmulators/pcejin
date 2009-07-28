class Pcejin
{
public:

	Pcejin() : started(false), aspectRatio(true), romLoaded(false), frameAdvance(false), 
		width(256), height(232) {
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

	bool frameAdvance;
	bool fastForward;

	u8 pads[5];

};

extern Pcejin pcejin;