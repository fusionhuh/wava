#include <math.h>
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <quick_arg_parser.hpp>

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/stream.h>

// keyboard input includes
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
//

#include <libconfig.h++>

#include <fftw3.h>

#include <common.h>
#include <pulse.h>

#include <wavatransform.hpp>

#include <graphics.hpp>
#include <cli.hpp>

#include <colors.hpp>

// Credit to: https://github.com/Dugy/quick_arg_parser

using namespace libconfig;

struct RenderingArgs : MainArguments<RenderingArgs> {
	int donut_count = option("donuts", 'd', "Number of donuts to be rendered.") = 0;
	int sphere_count = option("spheres", 's', "Number of spheres to be rendered.") = 0;
	int rect_prism_count = option("rect_prisms", 'r', "Number of rectangular prisms to be rendered.") = 0;
	
	int shape_palette = option("shape_palette", 'x', "Color palette for background.") = PRIDE_FLAG_PALETTE;

	float phi_spacing = option("phi_spacing") = 0.9;
	float theta_spacing = option("theta_spacing") = 0.9;
	float prism_spacing = option("prism_spacing") = 0.9;

	int light_smoothness = option("light_smoothness") = -1;

	int bg_palette = option("bg_palette", 'y', "Color palette for background.") = PRIDE_FLAG_PALETTE;

	bool ignore_config = option("ignore_config", 'i');

};

int main(int argc, char** argv) {
	srand(time(0)); // set rand() seed
	
	RenderingArgs render_args{{argc, argv}};

	Config wava_cfg;
	//wava_cfg.setOptions(Config::OptionAllowScientificNotation); // only works in 1.7

	try {
		std::string path = std::string(getenv("HOME")) + std::string("/.config/wava/wava.cfg");
		wava_cfg.readFile(path.c_str());
	}
	catch(const FileIOException &fioex) {
		std::cerr << "Error occurred while reading config file, it may be missing. Try reinstalling program." << std::endl;
		exit(-1);
	}
	catch(const ParseException &pex) {
		std::cerr << "Error occurred while parsing config gile." << std::endl;
		exit(-1);
	}

	set_raw_mode(true); // necessary for reading keyboard input

	// main loop	
	bool quit = false;
	bool mute = false;

	while (!quit) {
		int screen_x = 30; 
		int screen_y = 30;

		// Load config values
		if (!render_args.ignore_config) {
			try {
				render_args.phi_spacing = wava_cfg.lookup("rendering.phi_spacing");
				render_args.theta_spacing = wava_cfg.lookup("rendering.theta_spacing");
				render_args.prism_spacing = wava_cfg.lookup("rendering.prism_spacing");
				render_args.light_smoothness = wava_cfg.lookup("rendering.light_smoothness");
				render_args.bg_palette = wava_cfg.lookup("rendering.bg_palette");

				render_args.donut_count = wava_cfg.lookup("shapes.donut_count");
				render_args.sphere_count = wava_cfg.lookup("shapes.sphere_count");
				render_args.rect_prism_count = wava_cfg.lookup("shapes.rect_prism_count");
				render_args.shape_palette = wava_cfg.lookup("shapes.shape_palette");
			}
			catch(const SettingNotFoundException &nfex) {
				std::cerr << "Error occurred while doing lookup for setting." << std::endl;
				exit(-1);
			}
		}
		render_args.ignore_config = false;

		std::vector<Shape*> shapes = generate_shapes(render_args.donut_count, render_args.sphere_count, render_args.rect_prism_count, 0, wava_plan::freq_bands, render_args.shape_palette);

		struct audio_data audio(2, 44100);
		// here is where we would have a switch for audio input
		// case INPUT_PULSE:

		//if (strcmp(audio.source, "auto") == 0) {
		get_pulse_default_sink((void*) &audio);
		//}
		// starting pulsemusic listener
		std::thread listening_thread(input_pulse, (void*) &audio);

		//break

		bool reload_config = false;

		bool highlight_mode = false;
		int shape_pointer = -10;	

		while (!reload_config) { // while (!reloadConf) 
			printf("\x1b[2J"); // clear screen

			if (screen_x < 5) screen_x = 5; // avoids segfault with negative values
			if (screen_y < 5) screen_y = 5;

			if (render_args.phi_spacing < 0.04) render_args.phi_spacing = 0.04;
			if (render_args.theta_spacing < 0.04) render_args.theta_spacing = 0.04;
			if (render_args.prism_spacing < 0.04) render_args.prism_spacing = 0.04;

			if (render_args.light_smoothness > 100) render_args.light_smoothness = 100;
			if (render_args.light_smoothness < 2) render_args.light_smoothness = 4;

			if (render_args.bg_palette < 0) render_args.bg_palette = 0;
			if (render_args.bg_palette == WAVA_PALETTE_COUNT) render_args.bg_palette--;

			if (render_args.shape_palette < 0) render_args.shape_palette = 0;
			if (render_args.bg_palette == WAVA_PALETTE_COUNT) render_args.bg_palette--;

			if (highlight_mode) {
				if (shape_pointer >= shapes.size()) { shape_pointer = 0; } // loop around
				if (shape_pointer < 0) { shape_pointer = shapes.size() - 1; }
				for (int i = 0; i < shapes.size(); i++) {
					if (i == shape_pointer) {
						shapes[i]->highlight = true;
					}
					else { shapes[i]->highlight = false; }// need to do this to "unhighlight" shape pointed to previously
				}
			}


			struct wava_plan plan(44100, 2, 0.77, 100, 10000);

			struct wava_screen screen(screen_y, screen_x, render_args.theta_spacing, render_args.phi_spacing, render_args.prism_spacing,
				render_args.light_smoothness, render_args.bg_palette);

			bool change_screen_or_plan = false;
			bool draw = true; // used to prevent bright flashing colors when changing render args
			while (!change_screen_or_plan) { 
				printf("%d %d %d %d\n", render_args.donut_count, render_args.sphere_count, render_args.rect_prism_count, shape_pointer);

				audio.mtx.lock();

				std::vector<double> wava_out = wava_execute(audio.wava_in, audio.samples_counter, plan);
				if (audio.samples_counter > 0) audio.samples_counter = 0;

				audio.mtx.unlock();

				if (mute) {
					fill(wava_out.begin(), wava_out.end(), 0);
				}

				if (draw) render_cli_frame(shapes, screen, wava_out);

				change_screen_or_plan = true; // assuming that something is going to change to take statement out of cases
				draw = false;

				// check for relevant keyboard inputs
				int ch = quick_read();
				if (!highlight_mode) {
					switch (ch) {
						case ERR: // do nothing
							draw = true;
							change_screen_or_plan = false;
						break;
						case RIGHT_ARROW: // increase screen_x
							screen_x++;
						break;
						case DOWN_ARROW: // increase screen_y
							screen_y++;
						break;
						case LEFT_ARROW: // decrease screen_x
							screen_x--;
						break;
						case UP_ARROW: // decrease screen_y
							screen_y--;
						break;
						case 'q': // increase detail on phi dimension
							render_args.phi_spacing-=0.02;
						break;
						case 'a': // decrease detail on phi dimension
							render_args.phi_spacing+=0.02;
						break;
						case 'w': // increase detail on theta dimension
							render_args.theta_spacing-=0.02;
						break;
						case 's': // decrease detail on theta_dimension
							render_args.theta_spacing+=0.02;
						break;
						case 'e':
							render_args.prism_spacing-=0.02;
						break;
						case 'd':
							render_args.prism_spacing+=0.02;
						break;
						case 'r':
							render_args.light_smoothness+=2;
						break;
						case 'f':
							render_args.light_smoothness-=2;
						break;
						case 'z':
							render_args.bg_palette++;
						break;
						case 'x':
							render_args.bg_palette--;
						break;
						case 'S':
							reload_config = true;
						break;
						case 'W':
							// write to config
						break;
						case '1': // add donut
							render_args.donut_count++;
							reload_config = true;
							render_args.ignore_config = true;
						break;
						case '2': // add sphere
							render_args.sphere_count++;
							reload_config = true;
							render_args.ignore_config = true;
						break;
						case '3': // add rect prism
							render_args.rect_prism_count++;
							reload_config = true;
							render_args.ignore_config = true;
						break;
						case 'L': // enter shape lock mode
							if (shapes.size() > 0) { highlight_mode = true; }
						break;
						case 'm':
							mute = mute ? false : true;
							change_screen_or_plan = false;
						break;
						case ESC:
							reload_config = true;
							quit = true;
						break;
						default:
						break;
					}
				}
				else { // in shape highlighting mode
					switch(ch) {
						case ERR: // do nothing
							draw = true;
							change_screen_or_plan = false;
						break;
						case 'm':
							mute = mute ? false : true;
							change_screen_or_plan = false;
						break;
						case 'V':
							highlight_mode = false;
						break;
						case LEFT_ARROW:
							shape_pointer--;
						break;
						case RIGHT_ARROW:
							shape_pointer++;
						break;
						case UP_ARROW:
							shapes[shape_pointer]->increase_size();
						break;
						case DOWN_ARROW:
							shapes[shape_pointer]->decrease_size();
						break;
						case 'D':
							if (shapes.size() > 0) {
								switch(shapes[shape_pointer]->shape_type) {
									case DONUT_SHAPE:
										render_args.donut_count--;
									break;
									case SPHERE_SHAPE:
										render_args.sphere_count--;
									break;
									case RECT_PRISM_SHAPE:
										render_args.rect_prism_count--;
									break;
								}
								delete shapes[shape_pointer];
								shapes.erase(shapes.begin()+shape_pointer);
							}
							if (shapes.size() == 0) { // checking if now empty, leave highlight mode if so
								highlight_mode = false;
								shape_pointer = -1;
							}
						break;
						default:

						break;
					}
				}
				usleep(1000); // NEED this or some kind of delay to get results that make sense apparently
			}

		}
		audio.mtx.lock();
		audio.terminate = 1;
		audio.mtx.unlock();

		listening_thread.join();

		for (int i = 0; i < shapes.size(); i++) delete shapes[i];
	}

	set_raw_mode(false);

	return 0;
}


