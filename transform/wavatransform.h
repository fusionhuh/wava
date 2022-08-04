#ifndef WAVA_TRANSFORM
#define WAVA_TRANSFORM
#ifndef PI
#define PI 3.1415926535897932385
#endif
#include <stdint.h>
#include <fftw3.h>
#include <vector>

struct wava_plan {
	int fft_buffer_size;
	int audio_channels;
	static int freq_bands;

	int rate;
	static double freq_bin_size;

	double noise_reduction;

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

	int lower_cut_off; // ?
	int upper_cut_off; // ?

	static double band_lower_cutoff_freq[15];
	static double band_upper_cutoff_freq[15];
	static double band_octaves_count[15];

	double prev_wava_out[15];

	int screen_x;
	int screen_y;

	~wava_plan();
};

struct wava_plan wava_init(unsigned int rate, int channels, double noise_reduction, int low_cut_off, int high_cut_off);

std::vector<double> wava_execute(double* wava_in, int new_samples, struct wava_plan &plan);

#endif

