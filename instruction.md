# Build and run linOpenMPT on Fedora Workstation 44

The full guide, with troubleshooting and other distributions, is in
[docs/BUILDING.md](docs/BUILDING.md#fedora-workstation-44). The short version:

```sh
# 1. Dependencies
sudo dnf install git cmake ninja-build gcc-c++ pkgconf-pkg-config python3 lame-devel \
  'pkgconfig(Qt6Widgets)' \
  'pkgconfig(jack)' 'pkgconfig(libpulse-simple)' 'pkgconfig(alsa)' 'pkgconfig(dbus-1)' \
  'pkgconfig(zlib)' 'pkgconfig(libmpg123)' 'pkgconfig(ogg)' 'pkgconfig(vorbis)' \
  'pkgconfig(vorbisfile)' 'pkgconfig(vorbisenc)' 'pkgconfig(flac)' 'pkgconfig(opus)' \
  'pkgconfig(opusfile)' 'pkgconfig(libopusenc)' 'pkgconfig(liblhasa)'

# 2. Source, with upstream OpenMPT as a submodule
git clone --recurse-submodules https://github.com/codicusmaximus/linopenmpt.git
cd linopenmpt

# 3. Build
cmake -S . -B build -G Ninja
cmake --build build

# 4. Run
./build/app/openmpt
```

Sound works out of the box through PipeWire (JACK devices are preferred; pick
another in *View > Setup > Soundcard*). The tracker runs through XWayland on
the Wayland desktop so Linux VST editors can embed. Fedora's VST plugins are
in `/usr/lib64/vst`; start with `VST_PATH=/usr/lib64/vst ./build/app/openmpt`
to have the plugin dialog look there.
