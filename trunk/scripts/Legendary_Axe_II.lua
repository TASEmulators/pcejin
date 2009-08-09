--The Legendary Axe II

--relevant code located at A000 region in disassembler
--sets of values for each object
objInfo = {
	0x1F12e1,--object id
	0x1F12f9,--0 indicates the object is dead, 0x10 indicates invisible/no collision
	0x1F1311,--invincibility
	0x1F1329,
	0x1F1341,--y subpixel
	0x1F1359,--fine y pos
	0x1F1371,--coarse y pos
	0x1F1389,--x subpixel
	
	0x1F13a1,--fine x pos
	0x1F13b9,--coarse x pos
	0x1F13d1,--y subpixel
	0x1F13e9,--fine y pos (used in collision)
	0x1F1401,--coarse y pos (used in collision)
	0x1F1419,--x subpixel
	0x1F1431,--fine x pos (used in collision)
	0x1F1449,--coarse x pos (used in collision)
	
	0x1F1461,
	0x1F1479,
	0x1F1491,
	0x1F14a9,
	0x1F14c1,
	0x1F14d9,
	0x1F14f1,
	0x1F1509,
	
	0x1F1521,
	0x1F1539,
	0x1F1551,
	0x1F1569,
	0x1F1581,
	0x1F1599,
	0x1F15b1,
	0x1F15c9,
	
	0x1F15e1,
	0x1F15f9,
	0x1F1611,
	0x1F1629,
	0x1F1641,--current AI pattern
	0x1F1659,--y sprite pos
	0x1F1671,--x sprite pos
	0x1F1689,
	
	0x1F16a1,--damage taken
	0x1F16b9,
	0x1F16d1,--display with "damaged" palette if not 0
	0x1F16e9,
	0x1F1701,
	0x1F1719,
	0x1F1731,
	0x1F1749,
	
	0x1F1761,--held controller input
	0x1F1779,--1st press controller input
	0x1F1791,--jump timer
	0x1F17a9,--current weapon: 0 - sword, 1 - axe, 2 - chain
	0x1F17c1,--attack timer
	0x1F17d9,
	0x1F17f1,--weapon level
	0x1F1809,
	
	0x1F1821,
	0x1F1839,
	0x1F1851,
	0x1F1869,
	0x1F1881,
	0x1F1899,
	0x1F18b1,
	0x1F18c9
}

--change the absolute x and y values into ones relative to the screen
--so our HUD elements are visible and in the correct position
function correctForScroll(posFine, posCoarse, type)

	yScrollFine   = memory.readbyte(0x1F10E4)
	yScrollCoarse = memory.readbyte(0x1F10E5)
	xScrollFine   = memory.readbyte(0x1F10E6)
	xScrollCoarse = memory.readbyte(0x1F10E7)

	if(type == "x") then
		return(posFine+(posCoarse*255) - (xScrollFine+(xScrollCoarse*255)))	
	end
	if(type == "y") then
		return(posFine+(posCoarse*255) - (yScrollFine+(yScrollCoarse*255)))
	end
end

function dispXYDamage()

	agg.setFont("gse5x7")
	
	for i=0,20 do
	
		objId     = memory.readbyte(0x1F12e1+i)
		objDamage = memory.readbyte(0x1F16A1+i)
		
		hpOffsetInRom = 0x04AD4F
		
		ObjYPosFine   = memory.readbyte(0x1F13e9+i)
		ObjYPosCoarse = memory.readbyte(0x1F1401+i)
		ObjXPosFine   =	memory.readbyte(0x1F1431+i)
		ObjXPosCoarse = memory.readbyte(0x1F1449+i)
		
		--don't show explosions and dead objects since they aren't useful
		if( objId ~= 51 and memory.readbyte(0x1F12f9+i) ~= 0) then
			
			agg.text(
				correctForScroll(ObjXPosFine,ObjXPosCoarse, "x"),
				correctForScroll(ObjYPosFine,ObjYPosCoarse, "y"), 
		
				ObjXPosFine 
				.. " " .. ObjYPosFine
		
				.. " " .. objDamage
	  		.. "/" ..	memory.readbyte(hpOffsetInRom+objId)  --total hp
						
				--uncomment to show object number
--  			.. " #" .. i
				--uncomment to show object id
--				.. " ID " .. objId
			)
		end
	end
end

--display all 64 values for a given object
function dispObjInfo(n)
	index = 0
	agg.setFont("verdana12_bold")
	for y = 1, 8 do
		for x = 1, 8 do
			index = index+1;
			agg.text(-26+x*32, -5+y*12, memory.readbyte(objInfo[index]+n))
		end
	end
end

--see 6481 in disassembly for this algorithm
function dispWeaponDamage()
	currentWeapon = memory.readbyte(0x1F17A9)
	weaponLevel = memory.readbyte(0x1F17F1)

	a = currentWeapon
	a = AND(0xF,(a*2))
	a = AND(0xF,(a*2))
	a = a + currentWeapon
	a = a + weaponLevel
	agg.setFont("verdana12_bold")
	agg.text(4,220, "Dmg " .. memory.readbyte(0xC4985+a)) --damage table stored in rom
end

gui.register( function ()
		
	agg.lineColor(255,255,255,255)

	--object info viewer, uncomment and
	--set to the object number you want to view
--	dispObjInfo(0)

	--display x, y, and damage taken for objects
	dispXYDamage()

	--display current weapon damage
	dispWeaponDamage()
	
	agg.setFont("verdana18_bold")
end)

--[[ Notes

-----------------------------------------
Object Damage
-----------------------------------------

-Total health

0x1f16A1+ObjectNumber gets stored in $89

8F53 LDA $89      ; current object health
8F55 LDX $61      ; object id
8F57 CMP $CD4F,X  ; compare with the hp value table to see if the object is dead (0x04AD4F)
8F5A JSR $F195

-Where object damage is modified

6C29 LDA $1C      ; the amount of damage
6C2B ADC $36A1,X  ; add the damage
6C2E STA $36A1,X
6C34 AND #$F7
6C36 ORA #$06

-Where the amount of damage to be dealt is calculated and loaded to $1C

6841 LDA $94      ; current weapon (0x1F17A9)
6843 ASL
6844 ASL
6845 CLC
6846 ADC $94      ; current weapon (0x1F17A9)
6848 CLC
6849 ADC $97      ; weapon level (0x1F17F1)
684B TAY
684C LDA $6985,Y  ; damage table, 0x0C4985 in rom
684F STA $1C

]]