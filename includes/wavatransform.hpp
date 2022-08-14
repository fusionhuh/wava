#pragma once
#ifndef PI
#define PI 3.1415926535897932385
#endif
#define FFT_OUT_SIZE 8192
#define FFT_IN_SIZE 16384

#include <stdint.h>
#include <fftw3.h>
#include <vector>


struct wava_plan {
	int fft_buffer_size;
	int audio_channels;
	static int freq_bands;

	int rate;
	static double freq_bin_size;

	double noise_gate;
	double boost;
	double decay_rate;

	static double calibration;

	fftw_plan plan_left;
	fftw_plan plan_right;

	int input_buffer_size;

	double* input_buffer;

	double* l_sample_data; // default if only one channel is present
	double* r_sample_data; // used only if there are two channels

	double* l_sample_data_raw; // need to distinguish between raw and filtered data
	double* r_sample_data_raw; // because hanning window will be applied multiple times otherwise

	double hann_multiplier[8192];

	fftw_complex* l_output_data;
	fftw_complex* r_output_data;

	std::vector<double> l_output_data_magnitude;
	std::vector<double> r_output_data_magnitude;

	int lower_cut_off; // ?
	int upper_cut_off; // ?

	static double band_lower_cutoff_freq[15];
	static double band_upper_cutoff_freq[15];

	double prev_wava_out[15];

	int screen_x;
	int screen_y;

	wava_plan(unsigned int rate, int channels, double noise_gate, double boost, double decay_rate);

	~wava_plan();
};

struct wava_plan wava_init(unsigned int rate, int channels, double noise_reduction, int low_cut_off, int high_cut_off);

std::vector<double> wava_execute(double* wava_in, int new_samples, struct wava_plan &plan);

