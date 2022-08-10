# w.a.v.a.
**w**eird **a**udio **v**isualizer for **A**LSA

![new_video](https://user-images.githubusercontent.com/59339739/183812501-d06a0f05-e7dc-4e2a-9187-b9006cd23b6e.gif)

# about

wava is a commandline audio visualizer for ALSA and Pulseaudio that is heavily inspired by [cava](https://github.com/karlstav/cava). however, instead of visualizing
audio with bars like cava and many other visualizers, wava focuses on creating aesthetic visuals by using colors and shapes 
to visualize characteristics of music like pitch and amplitude.

**WARNING**: **depending on the music being played, wava can produce a lot of flashing lights and colors that may be unsuitable for individuals with epilepsy.**

# installation

to build, wava requires FFTW and Pulseaudio/ALSA dev files.

to install all required dependenecies, simply run
```
sudo apt-get install -y libpulseaudio-dev libasound2-dev libfftw3-dev
```
in your terminal on Ubuntu/Debian systems.

after dependencies are installed, run
```
make
sudo make install
```
in the cloned repo to install wava.

to uninstall, run
```
make uninstall
```
in the cloned repo. 

# usage

with no options, wava will simply render a colored background that responds to audio. if you want to render shapes, you must specify which shapes and how many using the options below:



