-- Run in an isolated portable REAPER instance; the runner supplies CURVEBOX_REAPER_LOG.
local path = os.getenv('CURVEBOX_REAPER_LOG')
local function log(s)
  local f = assert(io.open(path, 'a'))
  f:write(s .. '\n'); f:close()
end
log('script started')
reaper.InsertTrackAtIndex(0, true)
local track = assert(reaper.GetTrack(0, 0), 'track not created')
log('adding CurveBox')
local index = reaper.TrackFX_AddByName(track, 'VST3: CurveBox (CurveBox Audio)', false, -1)
log('FX index: ' .. index)
assert(index >= 0, 'CurveBox not found or not loaded')
local ok, name = reaper.TrackFX_GetFXName(track, index, '')
log('FX name: ' .. tostring(name))
assert(ok and name:find('CurveBox', 1, true), 'wrong FX loaded')
reaper.TrackFX_Show(track, index, 3)
log('editor opened')
local begin = reaper.time_precise()
local function finish()
  if reaper.time_precise() - begin < 3 then reaper.defer(finish); return end
  reaper.TrackFX_Show(track, index, 2)
  reaper.TrackFX_Delete(track, index)
  log('FX removed')
  log('PASS')
  reaper.Main_OnCommand(40004, 0) -- exit without saving the isolated project
end
reaper.defer(finish)
