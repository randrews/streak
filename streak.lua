#!/usr/local/bin/lua

function parse_params(...)
    local key = nil
    local params = {}

    for i, a in ipairs({...}) do
        i = tonumber(i)
        if i == 1 then params.command = a
        elseif i % 2 == 0 then key = a
        else params[key] = a end
    end

    local valid_commands = {
        read = true, write = true, erase = true,
        id = true, status = true
    }

    if #{...} == 0 or not valid_commands[params.command] then
        local usage = "Usage: streak <command> [options...]\n"
        usage = usage .. "\t-d, --device: serial device\n"
        usage = usage .. "\tCommands are read, write, erase, id, status"
        return { error = usage }
    end

    return params
end

function read_serial(port)
    local line = ''
    while line == nil or #line == 0 do
        line = port:read()
    end
    return line
end

function hex_encode(str)
    local bytes = { str:byte(1, #str) }
    local encoded = ''
    for _, b in ipairs(bytes) do
        if b < 16 then encoded = encoded .. '0' .. string.format('%x', b)
        else encoded = encoded .. string.format('%x', b) end
    end
    return encoded
end

function reset(sOut, sIn, value)
    if value then
        sOut:write('resethigh\r\n')
    else
        sOut:write('resetlow\r\n')
    end
    sOut:flush()
    read_serial(sIn)
end

--------------------------------------------------

local params = parse_params(...)

if params.error then
    print(params.error)
    os.exit(1)
end

local device = params['-d'] or params['--device'] or '/dev/ttyACM0'
local sIn = io.open(device, 'r')
local sOut = io.open(device, 'w')

if params.command == 'status' then
    reset(sOut, sIn, false)
    sOut:write('status\r\n')
    sOut:flush()
    print(read_serial(sIn))
    reset(sOut, sIn, true)

elseif params.command == 'id' then
    reset(sOut, sIn, false)
    sOut:write('id\r\n')
    sOut:flush()
    print(read_serial(sIn))
    reset(sOut, sIn, true)

elseif params.command == 'erase' then
    reset(sOut, sIn, false)
    sOut:write('erase\r\n')
    sOut:flush()
    print(read_serial(sIn))
    reset(sOut, sIn, true)

elseif params.command == 'write' then
    local file = params['f'] or params['--file']
    if not file then
        print('No file specified')
        os.exit(1)
    end
    local infile = io.open(file, 'r')
    reset(sOut, sIn, false)
    sOut:write('@000000\r\n')
    print(read_serial(sIn))
    while true do
        local str = infile:read(128)
        if not str then break end
        sOut:write('=' .. hex_encode(str) .. '\r\n')
        sOut:flush()
        print(read_serial(sIn))
    end
    reset(sOut, sIn, true)
end

os.exit(0)
