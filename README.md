# The Sound of a Dying Star

A cosmic shimmer reverb for ambient work. Feedback delay network, pitch-shifted feedback
paths and a soft clipper inside the loop — so the same instrument covers a barely-there
halo over a pad and the sub-heavy roar of something collapsing.

VST3 and standalone, Windows and Linux.

![The panel](docs/panel.png)

## What it does

The core is an 8-line feedback delay network with a Hadamard mixing matrix. Three things
are wrapped around it, and between them they are the whole plug-in:

- **A shimmer path.** The network's output is pitch-shifted and fed back in, so the tail
  climbs an octave (or whatever interval you set) each time round. Two voices, detuned
  against each other, so it widens into a chorus of itself rather than transposing.
- **A sub-octave "mass" path.** The same trick an octave down, low-passed to rumble.
  Wind it up and the tail acquires a bottom end that was never in the source.
- **A soft clipper inside the loop.** Linear below its threshold, so at low **Collapse**
  it colours nothing and simply guarantees that no combination of controls can run away.
  Driven hard, it *is* the collapse — the tail compresses, distorts and turns into a roar.

**Decay** runs from a quarter of a second to sixty; past 99 % it stops decaying at all.
**Freeze** opens the loop filters and holds what is in the network — measured at less
than 1.5 dB of loss over twenty seconds.

Zero latency. No oversampling, no lookahead, so bypass is a straight pass-through and the
dry path is bit-identical at 0 % mix.

### The controls

| Group | Controls |
|---|---|
| **Gravity** | Pre-Delay · Size · Decay · Diffusion |
| **Spectrum** | Low Cut · High Cut · Damping · Width |
| **Drift** | Shimmer · Pitch · Detune · Mod Rate · Mod Depth |
| **Collapse** | Collapse · Mass |
| **Emission** | Freeze · Mix · Output |

**Detune** does double duty: it spreads the two shimmer voices apart in cents, and it
sets the depth of a very slow independent drift on each delay line. That second half is
what stops a long tail sounding like a static chord.

Eight factory presets span the range, from *Whisper of Light* to *Black Hole Roar*, and
they are exposed as host programs as well as through the panel.

The star in the middle is driven by the controls and by the signal: its colour is the
loop's temperature, the accretion disk grows with **Mass**, the disk stops turning when
you **Freeze**, and past a certain amount of collapse an event horizon opens in the
middle of it.

## Building

Needs CMake 3.22+, a C++17 compiler and git.

```bash
git clone --recurse-submodules --shallow-submodules https://github.com/<owner>/the-sound-of-a-dying-star.git
cd the-sound-of-a-dying-star
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

If you already cloned without submodules, `git submodule update --init --recursive`.

Linux also needs:

```bash
sudo apt-get install -y build-essential cmake ninja-build libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libxrender-dev libgl1-mesa-dev fonts-dejavu-core
```

Windows needs Visual Studio 2022 with the Desktop C++ workload.

`DYINGSTAR_COPY_AFTER_BUILD` is `ON` by default and installs the built VST3 into your
user plug-in folder. Turn it off for CI or packaging builds.

## Verifying it without a DAW

`DYINGSTAR_BUILD_TOOLS=ON` builds a console tool that needs neither an audio device nor
a display, and both of its modes run in CI:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DDYINGSTAR_BUILD_TOOLS=ON -DDYINGSTAR_COPY_AFTER_BUILD=OFF
cmake --build build --parallel
./build/DyingStarDevTool_artefacts/Release/DyingStarDevTool check
./build/DyingStarDevTool_artefacts/Release/DyingStarDevTool shot panel.png
```

`check` asserts, among other things, that the dry path is untouched at 0 % mix, that the
wet path is level-matched to the dry, that the decay control changes the decay, that
shimmer really puts energy an octave above the source, that freeze holds for twenty
seconds, that nothing goes non-finite with every control at maximum, and that all of it
survives 44.1–192 kHz at block sizes from 16 to 2048. It prints the CPU cost as a number
so a regression is visible rather than merely suspected.

`shot` renders the editor to a PNG with no window and no display server, which is how the
screenshots in this README are made and how a panel that fails to paint fails the build.

## Licence

The code in `Source/` and `tools/` is **GPLv3-or-later**, copyright © 2026 doctorspider42.

It links against **JUCE 8 under the AGPLv3**, so any binary distributed from this project
is a combined work that additionally carries AGPLv3 section 13 — the clause about
offering source to users who interact with the program over a network. A plug-in running
in a DAW on someone's desktop does not do that, so in practice the clause is inert, but
it is not optional and anyone redistributing a build is entitled to know it applies.

Both texts are in the repository and in every release archive: `LICENSE` (GPLv3) and
`LICENSE.AGPLv3`. Third-party components are listed in [NOTICE.md](NOTICE.md).

Giving the binaries away is fine. What copyleft requires is *corresponding source*: JUCE
is pinned as a submodule and every release archive carries a `BUILD-INFO.txt` naming this
repository's commit and the exact JUCE revision, so "here is the source that built this"
is a link rather than an investigation.

## Still to do before calling it finished

CI proves the DSP computes and the panel paints. It cannot prove a host loads the thing.
Worth doing by hand in a DAW, or with [pluginval](https://github.com/Tracktion/pluginval):

- parameter automation from the host
- save/reload inside a session, and preset switching
- sample-rate and buffer-size changes while audio is running
- mono and stereo instantiations
