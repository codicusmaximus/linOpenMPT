# linOpenMPT

A native Linux port of the [OpenMPT](https://openmpt.org/) tracker, with a Qt6
user interface, aiming for full feature parity with the Windows application so
it no longer has to run under Wine.

The port reuses as much upstream code as possible. Upstream OpenMPT sits
unmodified in `upstream/` (a git submodule pinned to a release), and only the
Windows-specific UI layer is rewritten in Qt6. That keeps this repo small and
makes it practical to follow upstream releases.

**Status:** early. The upstream core and sound device layer build and run on
Linux. There is no GUI yet. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
for the plan and [docs/PORTING-STATUS.md](docs/PORTING-STATUS.md) for progress.

## Building

Ubuntu 24.04 / Debian packages:

```sh
sudo apt install cmake ninja-build pkg-config g++ qt6-base-dev \
  libjack-jackd2-dev libpulse-dev libasound2-dev libdbus-1-dev \
  zlib1g-dev libmpg123-dev libogg-dev libvorbis-dev libflac-dev \
  libopus-dev libopusfile-dev libopusenc-dev libmp3lame-dev liblhasa-dev
```

The VST tests use DISTRHO's plugins and Xvfb (`dpf-plugins-vst xvfb xauth`),
and are skipped without them.

```sh
git clone --recurse-submodules https://github.com/codicusmaximus/linopenmpt.git
cd linopenmpt
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Smoke tools built so far:

- `build/tools/smoke/render_module <file> [seconds]` loads and renders a module
  with the upstream engine.
- `build/tools/smoke/sounddev_list` lists audio devices through OpenMPT's own
  sound device layer (JACK, PulseAudio, ALSA).
- `build/tools/smoke/tracker_engine <file> <out.it>` starts the tracker's app
  object, renders the module, edits a pattern cell and saves it as IT, using
  the tracker's own engine.

Settings are kept in `$XDG_CONFIG_HOME/linOpenMPT/mptrack.ini` (normally
`~/.config/linOpenMPT`). Set `LINOPENMPT_CONFIG_DIR` to use another folder.

## Following upstream

```sh
scripts/update-upstream.sh                     # show current and newest releases
scripts/update-upstream.sh OpenMPT-1.32.13.00  # move to a release, rebuild, test
```

## License

Port code in this repository is BSD-3-Clause (see `LICENSE`), matching
upstream OpenMPT, whose own code keeps its license in `upstream/LICENSE`.
