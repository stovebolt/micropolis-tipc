-- shots.lua: time-driven MAME script. env SNAPS="t1,t2,.." takes snapshots (emulated seconds);
-- KEYS="t:name:dur,..." holds an ioport key field (Left/Right/Up/Down) from t for dur seconds;
-- CHARS="t:char,..." posts a typed character (e.g. 32 = space, 27 = ESC); QUIT=t exits. LOG=file.
local out = io.open(os.getenv("LOG") or "/tmp/shots.log", "w")
local snaps, keys, chars = {}, {}, {}
for t in string.gmatch(os.getenv("SNAPS") or "", "[^,]+") do snaps[#snaps+1] = {tonumber(t), false} end
for t, n, d in string.gmatch(os.getenv("KEYS") or "", "([%d%.]+):(%a+):([%d%.]+)") do keys[#keys+1] = {tonumber(t), n, tonumber(d), false, nil} end
for t, c in string.gmatch(os.getenv("CHARS") or "", "([%d%.]+):(%d+)") do chars[#chars+1] = {tonumber(t), tonumber(c), false} end
-- SCRIPT=file START=t STEP=dt: type the file's text, one token per STEP emulated seconds from START.
-- A token is a character, or {NAME} for a coded key (e.g. {LEFT}); a newline is Enter.
local toks, tnext = {}, 0
if os.getenv("SCRIPT") then
  local f = io.open(os.getenv("SCRIPT"), "r"); local txt = f:read("a"); f:close()
  local i = 1
  while i <= #txt do
    local c = txt:sub(i, i)
    if c == "{" then local j = txt:find("}", i, true); toks[#toks+1] = txt:sub(i, j); i = j + 1
    elseif c == "\n" then toks[#toks+1] = "\r"; i = i + 1
    else toks[#toks+1] = c; i = i + 1 end
  end
  tnext = tonumber(os.getenv("START") or "0")
end
local step = tonumber(os.getenv("STEP") or "0.15")
local ti = 1
local reltime, relf = 0, nil
local quit = tonumber(os.getenv("QUIT") or "100000")
local function field(name)
  for _, port in pairs(manager.machine.ioport.ports) do
    for fname, f in pairs(port.fields) do if fname == name then return f end end
  end
end
emu.register_periodic(function()
  local now = manager.machine.time:as_double()
  for _, s in ipairs(snaps) do
    if not s[2] and now >= s[1] then s[2] = true; manager.machine.video:snapshot(); out:write(string.format("snap t=%.1f\n", now)); out:flush() end
  end
  for _, k in ipairs(keys) do
    if not k[4] and now >= k[1] then k[4] = true; k[5] = field(k[2]); if k[5] then k[5]:set_value(1) end; out:write(string.format("key %s down t=%.1f\n", k[2], now)) end
    if k[4] and k[5] and now >= k[1] + k[3] then k[5]:set_value(0); k[5] = nil; out:write(string.format("key %s up t=%.1f\n", k[2], now)) end
  end
  for _, c in ipairs(chars) do
    if not c[3] and now >= c[1] then c[3] = true; manager.machine.natkeyboard:post(string.char(c[2])); out:write(string.format("char %d t=%.1f\n", c[2], now)) end
  end
  if relf and now >= reltime then relf:set_value(0); relf = nil end
  while ti <= #toks and now >= tnext do
    local t = toks[ti]
    if #t > 1 then
      local nm = t:sub(2, #t - 1); nm = nm:sub(1, 1) .. nm:sub(2):lower()
      local f = field(nm)
      if f then f:set_value(1); reltime = now + step * 0.5; relf = f end
    else manager.machine.natkeyboard:post(t) end
    ti = ti + 1; tnext = tnext + step
    if ti % 50 == 0 then out:write(string.format("typed %d/%d t=%.1f\n", ti, #toks, now)); out:flush() end
  end
  if now >= quit then out:close(); manager.machine:exit() end
end)
