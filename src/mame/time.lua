-- time.lua: find the SIMTEST timing marker in RAM, log emulated time at every CityTime change,
-- sample CS:IP every frame while the sim runs. env: OUT log, PROF profile file, NRUNS runs before exit.
local cpu=manager.machine.devices[":maincpu"]; local sp=cpu.spaces["program"]
local out=io.open(os.getenv('OUT'),'w'); local NR=tonumber(os.getenv('NRUNS') or '1')
local exit_at=nil; local addr=nil; local fr=0; local lastct=nil; local runs=0; local h={}; local lastphase=0
local SIG={0x4d,0x50,0x54,0x4d,0x41,0x52,0x4b,0x21}
local function find()
  for a=0x600,0x9fff0,2 do
    if sp:read_u8(a)==0x4d and sp:read_u8(a+1)==0x50 then
      local ok=true; for k=3,8 do if sp:read_u8(a+k-1)~=SIG[k] then ok=false break end end
      if ok then return a end
    end
  end
end
SUB=emu.add_machine_frame_notifier(function()
  fr=fr+1; local t=manager.machine.time:as_double()
  if exit_at then if t>=exit_at then manager.machine:exit() end return end
  if not addr then if fr%30==0 then addr=find(); if addr then out:write(string.format('%.3f FOUND %05x\n',t,addr)) lastphase=0 end end return end
  local ph=sp:read_u16(addr+12); local ct=sp:read_u32(addr+8)
  if ph==1 then
    local k=string.format('%04x:%04x',cpu.state['CS'].value,cpu.state['IP'].value); h[k]=(h[k] or 0)+1
  end
  if ph~=lastphase then out:write(string.format('%.3f PHASE %d ct=%d\n',t,ph,ct)); lastphase=ph end
  if ph==1 and ct~=lastct then out:write(string.format('%.3f CT %d\n',t,ct)); lastct=ct end
  if ph==2 then
    runs=runs+1; for a=addr,addr+7 do sp:write_u8(a,0) end; addr=nil; lastct=nil
    if runs>=NR then out:close(); local f=io.open(os.getenv('PROF'),'w'); for k,v in pairs(h) do f:write(v..' '..k..'\n') end f:close(); exit_at=t+20 end
  end
end)
