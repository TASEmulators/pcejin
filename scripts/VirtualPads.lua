input_file = "d:\\emu\\pcejin\\PCEctrls.txt"

function read_inputs(name)
	local f = assert(io.open(name,"r"))
	local t = f:read("*all")
	local kind, nplayer, rest, num
	_, _, kind, nplayer, rest = string.find(t, "^(%a+)[%c+](%d+)[%c+](.+)$")
	local inputs = {}
	inputs.kind = kind
	inputs.n    = tonumber(nplayer)
	for num in string.gfind(rest, "(%d+)") do
		table.insert(inputs, tonumber(num))
	end
	f:close()
	assert(#inputs==inputs.n)
	return inputs
end

trans = {
	["PCE"] = { ["Up"] = 0x0008, ["Down"] = 0x0004, ["Left"] = 0x0002,
		["Right"] = 0x0001, ["I"] = 0x4000, ["II"] = 0x2000, ["Start"] = 0x0040,
		["Select"] = 0x0020 }
}

function parse_inputs(input)
	local tr = assert(trans[input.kind])
	local res = {}
	for i=1,input.n do
		res[i] = {}
		for key, val in pairs(tr) do
--			res[i][key] = AND(input[i], val)
			if AND(input[i], val) ~=0 then res[i][key] = true end
		end
	end
	return res
end

function apply_inputs(input)
	for i=1, #input do
		joypad.set(i, input[i])
print (input)
--print (i)
	end
end

gui.register( function ()
--while true do
	apply_inputs(parse_inputs(read_inputs(input_file)))
--	gens.frameadvance()
end)
