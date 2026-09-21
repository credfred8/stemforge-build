# MasterForge ONE

Original all-in-one VST3 mastering suite for Windows x64, built with JUCE 9.0.2.

## Signal flow

Input Coach / Smart Gain -> Clean EQ -> Dynamic EQ -> Resonance Control -> Glue Compressor -> 4-zone Multiband -> Impact -> Analog Color -> Exciter -> Bass Mono -> Stereo Imager -> Dry/Wet -> 4x Oversampled Clipper -> 4x Oversampled Maximizer -> Output Trim -> Dither.

Every processing module has an enable switch. The editor is resizable and exposes visual spectrum/EQ response plus numeric input/output metering, correlation, crest factor and an approximate loudness readout.

## Factory presets

- Boom Bap - DENSE PUNCH
- Boom Bap - DUSTY ANALOG
- Hip-Hop - MODERN DENSE
- Trap - LOUD CLEAN
- Streaming - TRANSPARENT
- Vinyl - WARM GLUE
- Drums - HARD PUNCH
- Mixbus - OPEN DYNAMIC
- Safe Master - CLEAN

Presets are starting points, not guaranteed loudness targets. Final level depends on the source mix. The input coach is designed to make source level the first thing to fix rather than hiding bad gain staging deeper in the chain.

## Build

Requirements: Windows x64, Visual Studio 2022, CMake 3.24+, internet access during configure (JUCE is fetched at tag `9.0.2`).

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target MasterForgeOne_VST3 MasterForgeSmoke --parallel 2
```

The VST3 bundle is generated under the JUCE artifacts directory. Run `MasterForgeSmoke.exe` as a basic DSP sanity check.

## Notes

- The code is an original implementation inspired by common mastering workflows. It does not embed or clone proprietary iZotope, FabFilter, UAD, Waves or other vendor code.
- `LUFS EST` in v1 is a fast loudness estimate for workflow feedback, not a standards-certified EBU R128/ITU BS.1770 integrated meter.
- Oversampled final processing is 4x to reduce aliasing and improve inter-sample peak handling.

CI validates the DSP smoke test before packaging the Windows VST3 artifact.
