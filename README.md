# w.a.v.a.
**w**eird **a**udio **v**isualizer for **A**LSA

![new_video](https://user-images.githubusercontent.com/59339739/183812501-d06a0f05-e7dc-4e2a-9187-b9006cd23b6e.gif)

# about

wava is a commandline audio visualizer for Pulseaudio (ALSA coming soon :) ) that is heavily inspired by [cava](https://github.com/karlstav/cava). however, instead of visualizing
audio with bars like cava and many other visualizers, wava focuses on creating aesthetic scenes by using colors and shapes 
that react to characteristics of music like pitch and amplitude.

**WARNING**: **depending on the music being played, wava can produce a lot of flashing lights and colors that may be unsuitable for individuals with epilepsy.** if you would just like to use wava to create fun shapes, you can run wava and press 'm' to prevent any response to audio.

# installation

to build, wava requires FFTW and Pulseaudio/ALSA dev files.

to install all required dependenecies, simply run
```
sudo apt-get install -y libpulseaudio-dev libasound2-dev libfftw3-dev libconfig++-dev
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

upon starting wava, you will be greeted with a black screen because nothing is rendered by default.
in this state, only the background will respond to audio being played. briefly play something and verify the background changes color to make sure a connection was properly established to the audio server. 

connected? good. out of the box, the way the background responds to audio may require some tweaking to match your preference. to that end, wava supports keybinds for increasing/decreasing the noise gate (`c`/`v`), increasing/decreasing brightness (`b`/`n`), and increasing/decreasing the decay rate (`h`/`j`). 

generally speaking, if the background is not responding to sounds, you want to **decrease** the noise gate. if the colors are too bright and immediately forming white, **decrease** the brightness. if the colors are changing too quickly, **decrease** the decay rate. (and vice versa)

now that the background is configured to your liking, you can start adding shapes. to add a shape, press `1`, `2`, or `3` on your keyboard to generate a donut, a sphere, and a cube respectively. additionally, to increase real-time audio responsiveness, wava renders shapes with reduced detail by default. to increase/decrease detail along the x-axis for spheres and donuts, press `q`/`a`. to increase/detail along the y-axis, press `w`/`s`. to increase/detail for cubes, press `e`/`d`. 

wava has a range of color palettes that can be assigned to the background and individual shapes. to change the background color palette, press `z`/`x`. the name of the palette should appear under the rendering window. in order to change the palette of individual shapes, you must change to highlight mode.

wava has two modes to make better use of the keyboard. the first is normal mode in which you can generate new shapes and how they are rendered, change background colors, change audio settings as we did earlier, and read from/write to a config file. the second mode is "highlight" mode, in which you can select individual shapes and change their characteristics like color, size, and position on the screen. in order to change to highlight mode, have at least one shape on screen and press `L`. if it changed successfully, you should see a highlighted "HIGHLIGHT MODE" under the render screen.

now, one of the shapes on screen should be a different color--this means that it is selected and you can change its individual characteristics. to change its palette, press `z`/`x` like you did to change the background palette. again, the name of the palette should appear below the rendering window. you can also increase or decrease its size by pressing the up arrow and down arrow keys respectively, and you can change its position with the `i`, `j`, `k`, and `l` keys. 

to change which shape is being selected, press the left or right arrow keys. to delete the selected shape, press `D`.

after you've made the desired changes to your shapes, you can press `V` to change back to normal mode. 

if you really enjoy the scene you've made, you can save to the config file with `W`. if you would like to reload to your previously saved scene, press `R`.

once you're done using wava, press `ESC` to exit (exiting forcefully with Ctrl+Z is disabled because it can require a restart of the pulseaudio server).

that is pretty much all you need to know to use wava. an entire list of the keybinds is below:





