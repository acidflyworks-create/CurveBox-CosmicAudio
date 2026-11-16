#!/usr/bin/env python3
"""Cross-checks that every parameter id used by the factory presets exists in
the AudioProcessorValueTreeState layout (and lists the layout ids)."""
import re, sys

prefix = {'Volume': 'vol', 'Filter': 'flt', 'Drive': 'drv', 'Crush': 'crs',
          'Pan': 'pan', 'Delay': 'dly', 'Grain': 'grn', 'Reverb': 'rev'}

params_h = open('Source/Params.h').read()
suffix = dict(re.findall(r'static const juce::String\s+(\w+)\s*\{\s*"([^"]+)"\s*\}', params_h))
globals_ = {'mix', 'output', 'smooth', 'offset', 'midiRetrig'}

proc = open('Source/PluginProcessor.cpp').read()
layout_ids = set()

for g in re.findall(r'ParameterID \{\s*cb::ids::(\w+)', proc):
    layout_ids.add('glob_' + suffix[g])

for s in re.findall(r'cb::pid \(m, cb::ids::(\w+)\)', proc):
    for p in prefix.values():
        layout_ids.add(p + '_' + suffix[s])

for mod, s in re.findall(r'cb::pid \(cb::(\w+), cb::ids::(\w+)\)', proc):
    layout_ids.add(prefix[mod] + '_' + suffix[s])

presets = open('Source/Presets.h').read()
preset_ids = set()

for g in re.findall(r'setP \(p, ids::(\w+)', presets):
    preset_ids.add('glob_' + suffix[g])

for mod, s in re.findall(r'setP \(p, pid \((\w+), ids::(\w+)\)', presets):
    if mod in prefix:
        preset_ids.add(prefix[mod] + '_' + suffix[s])

for mod in re.findall(r'addLayer \(p, (\w+),', presets):
    if mod not in prefix:
        continue  # the addLayer() helper definition itself
    for s in ('en', 'amt', 'rate'):
        preset_ids.add(prefix[mod] + '_' + s)

print('layout parameters :', len(layout_ids))
print('preset parameters :', len(preset_ids))
missing = sorted(preset_ids - layout_ids)
if missing:
    print('MISSING IN LAYOUT:', missing)
    sys.exit(1)

print('OK: every preset parameter id exists in the APVTS layout')
