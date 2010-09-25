--PCEMultiTrack

--***************************************************************************************
--USER EDIT SECTION

--           Up 1    Down 1    None   All   C1        C2         C3       C4         C5
keymat = {'pageup','pagedown','N',   'A', 'numpad1','numpad2','numpad3','numpad4','numpad5'}

-- Controller Input
--               Up   Down   Left   Right   I  II   Run  Select
local tabinp = {'up','down','left','right','Z','X','C',  'V'};

--Names of Controllers
--Example:
--NameMatrix = {'Fighter','Warlock','Elf','Bard','Thief','All','None'};
NameMatrix = {'C1','C2','C3','C4','C5','All','None'};

--END USER EDIT SECTION
--****************************************************************************************


agg.setFont("verdana12_bold")		
Buttons = {"Up","Down","Left","Right","I","II","Run","Select"};
print("Welcome to DarkKobold's Multitrack Script!");
print("To use, select controller you wish to record, with either the number pad (1-5) or pageup and pagedown. (A) selects all controllers, and (N) selects no controllers.");
print("To change mapping, see the user edit section in the code.");

AllFrames = {};
local i = {};
local inp ={};
local keys = {};

local StartFrame = emu.framecount();
local RecFrames = 0;
currRec = 7;


function before()
	keys = input.get();
	RF = {};		
	if keys[keymat[1]] then
		currRec = currRec + 1;
		if currRec > 4 then currRec = 1; end;
	elseif keys[keymat[2]] then
		currRec = currRec - 1;
		if currRec == 0 then currRec = 4; end;		
	elseif keys[keymat[3]] then
		currRec = 7;
	elseif keys[keymat[4]] then
		currRec = 6;
	elseif keys[keymat[5]] then
		currRec = 1;
	elseif keys[keymat[6]] then
		currRec = 2;
	elseif keys[keymat[7]] then
		currRec = 3;
	elseif keys[keymat[8]] then
		currRec = 4;
	elseif keys[keymat[9]] then
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
	agg.text(1,15,"Recording: " .. NameMatrix[currRec]);
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