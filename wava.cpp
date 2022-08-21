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

#include <common.hpp>
#include <pulse.hpp>

#include <wavatransform.hpp>

#include <graphics.hpp>
#include <cli.hpp>

#include <colors.hpp>

// Credit to: https://github.com/Dugy/quick_arg_parser

using namespace libconfig;

struct WavaArgs : MainArguments<WavaArgs> {
	float phi_spacing = option("phi_spacing") = 0.9;
	float theta_spacing = option("theta_spacing") = 0.9;
	float prism_spacing = option("prism_spacing") = 0.9;

	int light_smoothness = option("light_smoothness") = -1;

	int bg_palette = option("bg_palette", 'y', "Color palette for background.") = PRIDE_FLAG_PALETTE;

	bool ignore_config = option("ignore_config", 'i');

	int noise_gate = option("noise_gate", 'X') = 50;
	int boost = option("boost", 'Y') = 50;
	int decay_rate = option("decay_rate", 'Z') = 50;
};

int main(int argc, char** argv) {
	srand(time(0)); // set rand() seed
	
	WavaArgs wava_args{{argc, argv}};

	Config wava_cfg;
	//wava_cfg.setOptions(Config::OptionAllowScientificNotation); // only works in 1.7

	std::string path = std::string(getenv("HOME")) + std::string("/.config/wava/wava.cfg");
	try {
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

	try {
		bool read_warning = wava_cfg.lookup("read_warning");
		Setting& setting = wava_cfg.lookup("read_warning");
		if (!read_warning) {
			std::cout << "WARNING: wava can produce flashing lights that may not be suitable for individuals with epilepsy."
			" Please use this software at your own risk." << std::endl << "Do you understand? Y/n (will not appear again) " << std::endl;
			char n;
			std::cin >> n;
			if (n == 'Y' || n == 'y') {
				setting = true;
				wava_cfg.writeFile(path.c_str());
			}
			else if (n == 'N' || n == 'n') {
				std::cout << "Thank you, wava is now exiting." << std::endl;
				exit(0);
			}
			else {
				std::cout << "Invalid input, wava is now closing." << std::endl;
				exit(-1);
			}
		}
	}
	catch(const SettingNotFoundException& nfex) {
		std::cerr << "Could not find 'read_warning' setting." << std::endl;
		exit(-1);
	}
	catch(const SettingTypeException& stex) {
		std::cerr << "Could not find 'read_warning' setting." << std::endl;
		exit(-1);
	}
	catch(const FileIOException &fioex) {
		std::cerr << "Error occurred while writing to config file, it may be missing. Try pasting the config file from the repo into ~/.config/wava/ again." << std::endl;
		exit(-1);
	}

	set_raw_mode(true); // necessary for reading keyboard input

	// main loop	
	bool quit = false;
	bool mute = false;
	bool hint = true;

	bool highlight_mode = false;
	int shape_pointer = -1;	

	std::string last_pressed_key_message("");

	while (!quit) {
		int screen_x = 40; 
		int screen_y = 40;

		// Load config values
		std::vector<Shape*> shapes;
		if (!wava_args.ignore_config) {
			try {
				wava_cfg.readFile(path.c_str());

				wava_args.phi_spacing = wava_cfg.lookup("rendering.phi_spacing");
				wava_args.theta_spacing = wava_cfg.lookup("rendering.theta_spacing");
				wava_args.prism_spacing = wava_cfg.lookup("rendering.prism_spacing");
				wava_args.light_smoothness = wava_cfg.lookup("rendering.light_smoothness");
				wava_args.bg_palette = wava_cfg.lookup("rendering.bg_palette");
		
				shapes = generate_shapes(wava_cfg.lookup("shapes_list"), wava_plan::freq_bands);

				wava_args.noise_gate = wava_cfg.lookup("control.noise_gate");
				wava_args.boost = wava_cfg.lookup("control.brightness");
				wava_args.decay_rate = wava_cfg.lookup("control.decay_rate");
			}
			catch(const SettingNotFoundException &nfex) {
				std::cerr << "Error occurred while doing lookup for setting." << std::endl;
				exit(-1);
			}
		}
		else {
			wava_args.ignore_config = true;
			//shapes = generate_shapes(wava_cfg.lookup("shapes_list"), wava_plan::freq_bands);
		}

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

		while (!reload_config) { // while (!reloadConf) 
			printf("\x1b[2J"); // clear screen

			const auto [term_rows, term_cols] = get_terminal_size();
			if (screen_x*2 >= term_cols + 1) screen_x = (term_cols/2);
			if (hint) {
				if (highlight_mode) {
					if (screen_y + 10 >= term_rows + 1) screen_y = term_rows - 10;
				}
				else { 
					if (screen_y + 6 >= term_rows + 1) screen_y = term_rows - 6;
				}
			}
			else {
				if (screen_y >= term_rows + 1) screen_y = term_rows;
			}

			if (screen_x < 5) screen_x = 5; // avoids segfault with negative values
			if (screen_y < 5) screen_y = 5;

			if (wava_args.phi_spacing < 0.04) wava_args.phi_spacing = 0.04;
			if (wava_args.theta_spacing < 0.04) wava_args.theta_spacing = 0.04;
			if (wava_args.prism_spacing < 0.04) wava_args.prism_spacing = 0.04;

			if (wava_args.light_smoothness > 100) wava_args.light_smoothness = 100;
			if (wava_args.light_smoothness < 2) wava_args.light_smoothness = 4;

			if (wava_args.bg_palette < 0) wava_args.bg_palette = WAVA_PALETTE_COUNT - 1;
			if (wava_args.bg_palette == WAVA_PALETTE_COUNT) wava_args.bg_palette = 0;

			if (wava_args.noise_gate < 0) wava_args.noise_gate = 0;
			if (wava_args.noise_gate > 100) wava_args.noise_gate = 100;

			if (wava_args.decay_rate < 0) wava_args.decay_rate = 0;
			if (wava_args.decay_rate > 100) wava_args.decay_rate = 100;

			if (wava_args.boost < 0) wava_args.boost = 0;
			if (wava_args.boost > 100) wava_args.boost = 100;

			if (highlight_mode) {
				if (shape_pointer >= shapes.size()) { 
					shape_pointer = 0; 
				}
				else if (shape_pointer < 0) { 
					shape_pointer = shapes.size() - 1; 
				}

				for (int i = 0; i < shapes.size(); i++) {
					if (i == shape_pointer) {
						shapes[i]->highlight = true;
					}
					else { shapes[i]->highlight = false; }// need to do this to "unhighlight" shape pointed to previously
				}
			} 
			else {
				for (int i = 0; i < shapes.size(); i++) {
					shapes[i]->highlight = false;
				}
			}

			struct wava_plan plan(44100, 2, wava_args.noise_gate, wava_args.boost, wava_args.decay_rate);

			struct wava_screen screen(screen_y, screen_x, wava_args.theta_spacing, wava_args.phi_spacing, wava_args.prism_spacing,
				wava_args.light_smoothness, wava_args.bg_palette);

			bool change_screen_or_plan = false;
			bool draw = true; // used to prevent bright flashing colors when changing render args
			while (!change_screen_or_plan) { 
				audio.mtx.lock();

				std::vector<double> wava_out = wava_execute(audio.wava_in, audio.samples_counter, plan);
				if (audio.samples_counter > 0) audio.samples_counter = 0;

				audio.mtx.unlock();

				if (mute) fill(wava_out.begin(), wava_out.end(), 0);
				if (draw) render_cli_frame(shapes, screen, wava_out);
                
				change_screen_or_plan = true; // assuming that something is going to change to take statement out of cases
				draw = false;

				if (hint) {
					if (highlight_mode) {
						printf("\x1b[48;2;%d;%d;%d;38;2;%d;%d;%dmHIGHTLIGHT MODE\n", 255, 255, 255, 0, 0, 0);
						printf("Highlighting shape: %d\n", shape_pointer+1);
						printf("Shape palette: %s\n", shapes[shape_pointer]->palette.name.c_str());
					}
            		printf("\x1b[48;2;%d;%d;%d;38;2;%d;%d;%dmBackground palette: %s\nNoise gate: %d\nBrightness: %d\nDecay rate: %d\n", 0, 0, 0, 255, 255, 255, screen.bg_palette.name.c_str(), wava_args.noise_gate, wava_args.boost, wava_args.decay_rate);
					std::cout << last_pressed_key_message;

				}

				int ch = quick_read();
				switch (ch) { // for behavior common to both modes
					case ERR:
						draw = true;
						change_screen_or_plan = false;
					break;
					case 'm':
						mute = mute ? false : true;
						last_pressed_key_message = std::string("Last key pressed: m, mute or unmute audio");
						printf("\x1b[2J"); // clear screen
					break;
					case 'h':
						hint = hint ? false : true;
						last_pressed_key_message = std::string("Last key pressed: h, turn on hints");
					break;
					case ESC:
						reload_config = true;
						quit = true;
					break;
					default:
						if (!highlight_mode) {
							switch (ch) {
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
									wava_args.phi_spacing-=0.02;
									last_pressed_key_message = std::string("Last key pressed: q, increase phi detail");
								break;
								case 'a': // decrease detail on phi dimension
									wava_args.phi_spacing+=0.02;
									last_pressed_key_message = std::string("Last key pressed: a, decrease phi detail");
								break;
								case 'w': // increase detail on theta dimension
									wava_args.theta_spacing-=0.02;
									last_pressed_key_message = std::string("Last key pressed: w, increase theta detail");
								break;
								case 's': // decrease detail on theta_dimension
									wava_args.theta_spacing+=0.02;
									last_pressed_key_message = std::string("Last key pressed: s, decrease theta detail");
								break;
								case 'e':
									wava_args.prism_spacing-=0.02;
									last_pressed_key_message = std::string("Last key pressed: e, increase prism detail");
								break;
								case 'd':
									wava_args.prism_spacing+=0.02;
									last_pressed_key_message = std::string("Last key pressed: d, decrease prism detail");
								break;
								case 'r':
									wava_args.light_smoothness+=2;
									last_pressed_key_message = std::string("Last key pressed: r, increase light smoothness");
								break;
								case 'f':
									wava_args.light_smoothness-=2;
									last_pressed_key_message = std::string("Last key pressed: f, decrease light smoothness");
								break;
								case 'z':
									wava_args.bg_palette++;
									last_pressed_key_message = std::string("Last key pressed: z, increment background color palette");
								break;
								case 'x':
									wava_args.bg_palette--;
									last_pressed_key_message = std::string("Last key pressed: x, decrement background color palette");
								break;
								case 'R':
									reload_config = true;
									wava_args.ignore_config = false;
									last_pressed_key_message = std::string("Last key pressed: R, reload config");
								break;
								case 'W':
									{
										// write to config
										wava_cfg.lookup("rendering.bg_palette") = wava_args.bg_palette;
										wava_cfg.lookup("rendering.light_smoothness") = wava_args.light_smoothness;
										wava_cfg.lookup("rendering.phi_spacing") = wava_args.phi_spacing;
										wava_cfg.lookup("rendering.theta_spacing") = wava_args.theta_spacing;
										wava_cfg.lookup("rendering.prism_spacing") = wava_args.prism_spacing;

										wava_cfg.lookup("control.noise_gate") = wava_args.noise_gate;
										wava_cfg.lookup("control.brightness") = wava_args.boost;
										wava_cfg.lookup("control.decay_rate") = wava_args.decay_rate;

										last_pressed_key_message = std::string("Last key pressed: W, write to config");

										Setting& shapes_list = wava_cfg.lookup("shapes_list");
										shapes_list.remove("list");
										Setting& list = shapes_list.add("list", Setting::TypeList);
										for (int i = 0; i < shapes.size(); i++) {
											Setting& curr_shape_entry = list.add(Setting::TypeList);
											switch(shapes[i]->shape_type) {
												case SPHERE_SHAPE:
													{
														Sphere* sphere = (Sphere*) shapes[i];
														curr_shape_entry.add(Setting::TypeInt) = SPHERE_SHAPE;
														curr_shape_entry.add(Setting::TypeFloat) = sphere->radius;
														curr_shape_entry.add(Setting::TypeFloat) = sphere->x_offset;
														curr_shape_entry.add(Setting::TypeFloat) = sphere->y_offset;
														curr_shape_entry.add(Setting::TypeInt) = sphere->color_index;
													}
												break;
												case DONUT_SHAPE:
													{
														Donut* donut = (Donut*) shapes[i];
														curr_shape_entry.add(Setting::TypeInt) = DONUT_SHAPE;
														curr_shape_entry.add(Setting::TypeFloat) = donut->radius;
														curr_shape_entry.add(Setting::TypeFloat) = donut->thickness;
														curr_shape_entry.add(Setting::TypeFloat) = donut->x_offset;
														curr_shape_entry.add(Setting::TypeFloat) = donut->y_offset;
														curr_shape_entry.add(Setting::TypeInt) = donut->color_index;
													}
												break;
												case RECT_PRISM_SHAPE:
													{
														RectPrism* rect_prism = (RectPrism*) shapes[i];
														curr_shape_entry.add(Setting::TypeInt) = RECT_PRISM_SHAPE;
														curr_shape_entry.add(Setting::TypeFloat) = rect_prism->height;
														curr_shape_entry.add(Setting::TypeFloat) = rect_prism->width;
														curr_shape_entry.add(Setting::TypeFloat) = rect_prism->depth;
														curr_shape_entry.add(Setting::TypeFloat) = rect_prism->x_offset;
														curr_shape_entry.add(Setting::TypeFloat) = rect_prism->y_offset;
														curr_shape_entry.add(Setting::TypeInt) = rect_prism->color_index;
													}
												break;
											}
										}
										shapes_list.lookup("shapes_count") = (int) shapes.size();
										wava_cfg.writeFile(path.c_str());
									}
								break;
								case '1': // add donut
									{
										Donut* donut = new Donut(0.5, 0.2, 0, 0, 2, wava_plan::freq_bands, 0);
										shapes.push_back(donut);
									}
								break;
								case '2': // add sphere
									{
										Sphere* sphere = new Sphere(1, 0, 0, 2, wava_plan::freq_bands, 0);
										shapes.push_back(sphere);
									}
								break;
								case '3': // add rect prism
									{
										RectPrism* rect_prism = new RectPrism(1, 1, 1, 0, 0, 2, wava_plan::freq_bands, 0);
										shapes.push_back(rect_prism);
									}
								break;
								case 'c':
									wava_args.noise_gate--;
									last_pressed_key_message = std::string("Last key pressed: c, decrease noise gate");
								break;
								case 'v':
									wava_args.noise_gate++;
									last_pressed_key_message = std::string("Last key pressed: v, increase noise gate");
								break;
								case 'n':
									wava_args.boost++;
									last_pressed_key_message = std::string("Last key pressed: n, increase brightness");
								break;
								case 'b':
									wava_args.boost--;
									last_pressed_key_message = std::string("Last key pressed: b, decrease brightness");
								break;
								case 'k':
									wava_args.decay_rate++;
									last_pressed_key_message = std::string("Last key pressed: j, increase decay rate");
								break;
								case 'j':
									wava_args.decay_rate--;
									last_pressed_key_message = std::string("Last key pressed: h, decrease decay rate");
								break;
								case 'H': // enter shape lock mode
									if (shapes.size() > 0) { 
										highlight_mode = true; 
										last_pressed_key_message = std::string("Last key pressed: H, shift to shape highlight mode");
									}
								break;
								default:

								break;
							}
						}
						else { // in shape highlighting mode
							switch(ch) {
								case 'N':
									highlight_mode = false;
									last_pressed_key_message = std::string("Last key pressed: N, change back to normal mode");
								break;
								case LEFT_ARROW:
									shape_pointer--;
								break;
								case RIGHT_ARROW:
									shape_pointer++;
								break;
								case UP_ARROW:
									shapes[shape_pointer]->increase_size();
									last_pressed_key_message = std::string("Last key pressed: UP, increase shape size");
								break;
								case DOWN_ARROW:
									shapes[shape_pointer]->decrease_size();
									last_pressed_key_message = std::string("Last key pressed: DOWN, decrease shape size");
								break;
								case 'D':
									if (shapes.size() > 0) {
										delete shapes[shape_pointer];
										shapes.erase(shapes.begin()+shape_pointer);
										last_pressed_key_message = std::string("Last key pressed: D, delete shape");
									}
									if (shapes.size() == 0) { // checking if now empty, leave highlight mode if so
										highlight_mode = false;
										shape_pointer = -1;
									}
								break;
								case 'z':
									shapes[shape_pointer]->decrement_palette();
									last_pressed_key_message = std::string("Last key pressed: z, decrement shape palette");
								break;
								case 'x':
									shapes[shape_pointer]->increment_palette();
									last_pressed_key_message = std::string("Last key pressed: x, increment shape palette");
								break;
								case 'w':
									shapes[shape_pointer]->x_offset-=0.04;
								break;
								case 'a':
									shapes[shape_pointer]->y_offset+=0.04;
								break;
								case 's':
									shapes[shape_pointer]->x_offset+=0.04;
								break;
								case 'd':
									shapes[shape_pointer]->y_offset-=0.04;
								break;
								default:

								break;
							}
						}
					break;
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
	printf("\n");


	return 0;
}


