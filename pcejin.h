class Pcejin
{
public:

	Pcejin() : started(false), aspectratio(true), romloaded(false), frameAdvance(false), 
		width(256), height(232) {
	}

	bool aspectratio;
	bool started;
	bool romloaded;

	int lagFrameFlag;
	int lagFrameCounter;
	char InputDisplayString[128];
	int height;
	int width;

	bool frameAdvance;

	u8 pads[5];

};

extern Pcejin pcejin;