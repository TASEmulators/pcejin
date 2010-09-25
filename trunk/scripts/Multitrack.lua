--PCEMultiTrack
agg.setFont("verdana12_bold")		


Buttons = {"Up","Down","Left","Right","I","II","Run","Select"};
AllFrames = {};
local i = {};
local inp ={};
local keys = {};

local StartFrame = emu.framecount();
local RecFrames = 0;
currRec = 1;
local tabinp = {'up','down','left','right','Z','X','C','V'};

function before()
	keys = input.get();
	RF = {};		
	if keys['pageup'] then
		currRec = currRec + 1;
		if currRec > 4 then currRec = 1; end;
	elseif keys['pagedown'] then
		currRec = currRec - 1;
		if currRec == 0 then currRec = 4; end;		
	elseif keys['N'] then
		currRec = 7;
	elseif keys['A'] then
		currRec = 6;
	elseif keys['numpad1'] then
		currRec = 1;
	elseif keys['numpad2'] then
		currRec = 2;
	elseif keys['numpad3'] then
		currRec = 3;
	elseif keys['numpad4'] then
		currRec = 4;
	elseif keys['numpad5'] then
		currRec = 5;	
	end;
	if movie.mode() ~= 'playback' then							
		for n = 4,0,-1 do
			if currRec == n+1 or currRec == 6 then 
				m = 0;
				for j = 8,1,-1 do									
					m = m * 2;
					if keys[tabinp[j]] then 
						inp[Buttons[j]] = true;						
						m = m+1;
					else			
						inp[Buttons[j]] = false;
					end;
				end;
				RF[n+1] = m;
			else						
				if (emu.framecount()-StartFrame) >= RecFrames then
					for j = 8,1,-1 do				
						inp[Buttons[j]] = false;
					end;
					RF[n+1] = 0;
				else			
					TF = AllFrames[emu.framecount()-StartFrame+1];	
					control = TF[n+1];							
					for j = 1,8,1 do 
						if math.mod(control,2) == 1 then
							inp[Buttons[j]] = true;
						else
							inp[Buttons[j]] = false;
						end;
						control = math.floor(control/2);
					end;
					RF[n+1] = TF[n+1];	
				end;									
			end;
			joypad.set(n+1,inp);					
		end;	
	AllFrames[emu.framecount()-StartFrame+1] = RF;	
	end;					
	RecFrames = math.max(RecFrames, emu.framecount()-StartFrame);		

end;
			
gui.register(function()
	agg.lineColor(255,255,255,255);	
	agg.text(1,15,"Recording: " .. currRec);
end);


function after()

	
	if movie.mode() == 'playback' then	
		RF = {};	
		for n = 4,0,-1 do
			inpm = joypad.get(n+1);				
			m = 0;
			for j = 8,1,-1 do
				if inpm[Buttons[j]] then
				z = 1 else z = 0; end;
				m = m*2; 
				m = m + z;
			end;		
			RF[n+1] = m;
		end;
		AllFrames[emu.framecount()-StartFrame] = RF;	
		RecFrames = math.max(RecFrames, emu.framecount()-StartFrame);			
	end;	
	
end;



emu.registerbefore(before);
emu.registerafter(after);