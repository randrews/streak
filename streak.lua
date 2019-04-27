#!/usr/local/bin/lua

local device

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
        id = true, status = true, reset = true
    }

    if #{...} == 0 or not valid_commands[params.command] then
        local usage = "Usage: streak <command> [options...]\n"
        usage = usage .. "\t-d, --device: serial device\n"
        usage = usage .. "\tCommands are read, write, erase, id, status, reset"
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
        send_command(sIn, sOut, 'resethigh')
    else
        send_command(sIn, sOut, 'resetlow')
    end
end

function send_command(sIn, sOut, command)
    sOut:write(command .. ';')
    sOut:flush()
    local response = nil
    while response == nil or response:sub(1,5) == 'unrec' do
        response = read_serial(sIn)
    end
    return response
end

function spinner(count)
    if count > 0 then
        io.write('\b\b')
    end
    count = count + 1
    local chars = { '| ', '/ ', '- ', '\\ ' }
    io.write(chars[count % 4 + 1])
    return count
end

--------------------------------------------------

local params = parse_params(...)

if params.error then
    print(params.error)
    os.exit(1)
end

device = params['-d'] or params['--device'] or '/dev/ttyACM0'
os.execute('stty -F ' .. device .. ' 9600 raw -echo')
local sIn = io.open(device, 'r')
local sOut = io.open(device, 'w')

if params.command == 'status' then
    print(send_command(sIn, sOut, 'status'))

elseif params.command == 'id' then
    reset(sOut, sIn, false)
    print(send_command(sIn, sOut, 'id'))
    reset(sOut, sIn, true)

elseif params.command == 'erase' then
    reset(sOut, sIn, false)
    print(send_command(sIn, sOut, 'erase'))
    reset(sOut, sIn, true)

elseif params.command == 'write' then
    local file = params['f'] or params['--file']
    if not file then
        print('No file specified')
        os.exit(1)
    end
    local infile = io.open(file, 'r')
    if infile == nil then
        print('File not found: ' .. file)
        os.exit(1)
    end
    reset(sOut, sIn, false)
    send_command(sIn, sOut, 'erase')
    send_command(sIn, sOut, '@000000')
    chunk = 0
    while true do
        local str = infile:read(256)
        if not str then break end
        send_command(sIn, sOut, '=' .. hex_encode(str))
        chunk = spinner(chunk)
    end
    print('Done!')
    reset(sOut, sIn, true)

elseif params.command == 'reset' then
    reset(sOut, sIn, false)
    print('Device reset')
    reset(sOut, sIn, true)

elseif params.command == 'read' then
    reset(sOut, sIn, false)
    send_command(sIn, sOut, '@000000')
    send_command(sIn, sOut, '+16')
    print(send_command(sIn, sOut, 'read'))
    reset(sOut, sIn, true)
end

sOut:close()
sIn:close()
