# Building linOpenMPT

linOpenMPT builds with CMake and Ninja against the system's Qt6 and audio
libraries. Upstream OpenMPT is a git submodule in `upstream/`, and the build
applies the small patches in `patches/` to it at configure time, so `git`
must be installed and the submodule checked out.

- [Fedora Workstation 44](#fedora-workstation-44)
- [Ubuntu 24.04 / Debian](#ubuntu-2404--debian)
- [Build options](#build-options)

## Fedora Workstation 44

### 1. Install the build dependencies

```sh
sudo dnf install git cmake ninja-build gcc-c++ pkgconf-pkg-config python3 lame-devel \
  'pkgconfig(Qt6Widgets)' \
  'pkgconfig(jack)' 'pkgconfig(libpulse-simple)' 'pkgconfig(alsa)' 'pkgconfig(dbus-1)' \
  'pkgconfig(zlib)' 'pkgconfig(libmpg123)' 'pkgconfig(ogg)' 'pkgconfig(vorbis)' \
  'pkgconfig(vorbisfile)' 'pkgconfig(vorbisenc)' 'pkgconfig(flac)' 'pkgconfig(opus)' \
  'pkgconfig(opusfile)' 'pkgconfig(libopusenc)' 'pkgconfig(liblhasa)'
```

The `pkgconfig(...)` names are the exact libraries CMake looks for (the
`pkg_check_modules` calls in `cmake/`), and dnf installs whichever package
provides each one. On Fedora 44 they come to roughly these packages:
`qt6-qtbase-devel`, `pipewire-jack-audio-connection-kit-devel`,
`pulseaudio-libs-devel`, `alsa-lib-devel`, `dbus-devel`, `zlib-ng-compat-devel`,
`mpg123-devel`, `libogg-devel`, `libvorbis-devel`, `flac-devel`, `opus-devel`,
`opusfile-devel`, `libopusenc-devel` and `lhasa-devel`. LAME has no pkg-config
file, so `lame-devel` is named directly. All of them are in Fedora's own
repositories; RPM Fusion is not needed.

JACK on Fedora is PipeWire's JACK implementation, which is what you want on a
desktop: OpenMPT's JACK output then goes through PipeWire with low latency.
Do not install `jack-audio-connection-kit-devel` alongside it, as the two
conflict.

Optional, only for the full test suite (those tests are skipped without them):

```sh
sudo dnf install xorg-x11-server-Xvfb xorg-x11-xauth /usr/bin/xvfb-run
```

The VST tests also use DISTRHO's MVerb plugin as a Linux VST2 `.so`. If your
Fedora has DISTRHO's plugins (`dnf search dpf`), install them and point the
tests at the folder they use, normally `/usr/lib64/vst` on Fedora (see
[Running the tests](#4-run-the-tests-optional)).

For Windows VST plugins (run in Wine through the tracker's own bridge),
the bridge is built with MinGW and run with Wine; without MinGW the build
leaves it out, and the tracker says so when asked to load a `.dll`:

```sh
sudo dnf install mingw64-gcc-c++ mingw64-winpthreads-static wine 'pkgconfig(xcb)'
```

`xdotool` lets the tests click a bridged plugin's editor.

### 2. Get the source

```sh
git clone --recurse-submodules https://github.com/codicusmaximus/linopenmpt.git
cd linopenmpt
```

If you cloned without `--recurse-submodules`, fetch upstream OpenMPT with:

```sh
git submodule update --init --depth 1
```

### 3. Configure and build

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

The default build type is `RelWithDebInfo`. The first build compiles all of
upstream OpenMPT and takes a while. Ninja uses every core by default; if the
machine runs out of memory, limit it with `cmake --build build -j4`.

### 4. Run the tests (optional)

```sh
ctest --test-dir build --output-on-failure
```

For the VST tests on Fedora, where plugins live in `/usr/lib64/vst`:

```sh
cmake -S . -B build -DLINOPENMPT_TEST_VST=/usr/lib64/vst/MVerb-vst.so
LINOPENMPT_VST_DIR=/usr/lib64/vst ctest --test-dir build --output-on-failure
```

### 5. Run it

```sh
./build/app/openmpt
./build/app/openmpt path/to/song.it     # open a module
```

To install it to `/usr/local/bin/openmpt`:

```sh
sudo cmake --install build
```

Settings are kept in `~/.config/OpenMPT/` (`mptrack.ini` and friends), as
OpenMPT keeps them in `%APPDATA%\OpenMPT` on Windows. The smoke tools in
`build/tools/smoke/` use `~/.config/linOpenMPT/` instead, or
`$LINOPENMPT_CONFIG_DIR` when it is set.

### Audio on Fedora

Fedora Workstation runs PipeWire, which provides both a JACK and a
PulseAudio server, so OpenMPT works without any extra setup:

- With JACK available (always the case with PipeWire), OpenMPT ranks the JACK
  devices first. For the lowest latency, choose a JACK device in
  *View > Setup > Soundcard*.
- PulseAudio and ALSA devices are listed too, and a device you pick in the
  settings always wins.
- The audio thread asks rtkit (over D-Bus) for realtime priority, so no root
  or special group is needed. To see which devices OpenMPT finds, run
  `./build/tools/smoke/sounddev_list`.

### Wayland

Fedora Workstation uses a Wayland session. linOpenMPT starts through XWayland
by default (it sets `QT_QPA_PLATFORM=xcb` when `DISPLAY` is set), because
Linux VST plugin editors are X11 windows that need an X11 parent. XWayland is
part of Fedora Workstation, so nothing needs installing. To force native
Wayland anyway, run `QT_QPA_PLATFORM=wayland ./build/app/openmpt`; plugins
then show the generic parameter editor instead of their own.

### Plugins

Native Linux VST2 plugins (`.so`) load in the tracker. The plugin dialog
looks in the first of `$VST_PATH`, `~/.vst`, `~/.lxvst`, `/usr/local/lib/vst`,
`/usr/lib/vst`, `/usr/local/lib/lxvst` and `/usr/lib/lxvst` that exists.
Fedora's packaged plugins go to `/usr/lib64/vst`, which is not on that list,
so start it with `VST_PATH=/usr/lib64/vst ./build/app/openmpt` or browse to
that folder in the plugin dialog. Windows `.dll` plugins need the plugin
bridge, which is not built on Linux.

### Troubleshooting

**`upstream/ is empty. Run: git submodule update --init --depth 1`**
The submodule was not checked out. Run that command in the repository.

**`Upstream patch 000N-....patch does not apply`**
`upstream/` has local changes or is at a different commit than this
repository pins. Reset it with
`git submodule update --init --force upstream`, then configure again.

**`Package 'xyz', required by 'virtual:world', not found` or
`Could NOT find Qt6`**
A development package is missing. Install it with
`sudo dnf install 'pkgconfig(xyz)'` (for Qt, `qt6-qtbase-devel`), then delete
`build/CMakeCache.txt` and configure again.

**`Could not find ... lame/lame.h` or `mp3lame`**
Install `lame-devel`.

**dnf reports a conflict between `pipewire-jack-audio-connection-kit-devel`
and `jack-audio-connection-kit-devel`**
Keep the PipeWire one. If you really want the classic JACK server, remove the
PipeWire JACK packages first; OpenMPT builds with either.

**No sound, or no JACK devices**
Check that PipeWire is running (`systemctl --user status pipewire
wireplumber`) and that `pipewire-jack-audio-connection-kit` is installed. Then pick a device in *View > Setup > Soundcard*, and run
`./build/tools/smoke/sounddev_list` to see what OpenMPT can open. If you want
to avoid JACK entirely, build with `-DLINOPENMPT_WITH_JACK=OFF`.

**Crackles at small buffer sizes**
Raise the buffer in *View > Setup > Soundcard*, or set PipeWire's quantum,
for example `pw-metadata -n settings 0 clock.force-quantum 256`.

**The window does not open over SSH or in a container**
The tracker needs a display. Use a desktop session, `ssh -X`, or for a quick
check `QT_QPA_PLATFORM=offscreen ./build/app/openmpt`.

**A crash**
The tracker prints a backtrace to the terminal, and first saves any modified
modules into a crash folder, as OpenMPT does on Windows. Please include the
backtrace when reporting it.

**The compiler fails inside `upstream/`**
Fedora 44 ships a newer GCC and Qt than the Ubuntu 24.04 setup the port is
developed on, so a new compiler error is possible. Please report it with the
output of `gcc --version` and the first error message.

## Ubuntu 24.04 / Debian

```sh
sudo apt install git cmake ninja-build pkg-config g++ python3 qt6-base-dev \
  libjack-jackd2-dev libpulse-dev libasound2-dev libdbus-1-dev \
  zlib1g-dev libmpg123-dev libogg-dev libvorbis-dev libflac-dev \
  libopus-dev libopusfile-dev libopusenc-dev libmp3lame-dev liblhasa-dev
```

The VST tests use DISTRHO's plugins and Xvfb (`dpf-plugins-vst xvfb xauth`),
and are skipped without them. Windows VST plugins need the Wine bridge, built
with MinGW and run with Wine (`g++-mingw-w64-x86-64 wine64 libxcb1-dev`, and
`xdotool` for its click test). Then follow steps 2 to 5 above.

## Build options

Pass these to the first `cmake` command as `-DNAME=OFF`:

| Option | Default | What it does |
| --- | --- | --- |
| `LINOPENMPT_WITH_JACK` | ON | JACK audio (works on PipeWire through pipewire-jack) |
| `LINOPENMPT_WITH_PULSE` | ON | PulseAudio audio (works on PipeWire through pipewire-pulse) |
| `LINOPENMPT_WITH_ALSA` | ON | ALSA audio |
| `LINOPENMPT_BUILD_QT` | ON | The Qt6 application (`build/app/openmpt`) |
| `LINOPENMPT_BUILD_TESTS` | ON | Smoke tools and tests |
| `LINOPENMPT_TEST_VST` | `/usr/lib/vst/MVerb-vst.so` | VST2 plugin used by `vst_host_test` |

Turning off an audio backend also drops its library from the dependency list.
