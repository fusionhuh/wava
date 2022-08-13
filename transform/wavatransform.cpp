#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wavatransform.hpp>

double* allocate_initialized_fftw_real(int size) {
	double* pointer = fftw_alloc_real(size); memset(pointer, 0, size);
	return pointer;
}	

fftw_complex* allocate_initialized_fftw_complex(int size) {
	fftw_complex* pointer = fftw_alloc_complex(size); memset(pointer, 0, size);
	return pointer;
}

double wava_plan::band_lower_cutoff_freq[15] = {50, 190, 300, 435/*A*/, 461/*A#*/, 488/*B*/, 518/*C*/, 549/*C#*/, 583/*D*/, 617/*D#*/, 654/*E*/, 693/*F*/, 734/*F#*/, 778 /*G*/, 826/*G#*/};
double wava_plan::band_upper_cutoff_freq[15] = {180, 200, 310,445,      471,       498,      528,      559,       593,      627,       664,      703,      744,       788,       836};

double wava_plan::calibration = 0;

int wava_plan::freq_bands = 15;

wava_plan::wava_plan(unsigned int rate, int channels, double noise_reduction, int low_cut_off, int high_cut_off) :
	rate(rate), audio_channels(channels), noise_reduction(noise_reduction), input_buffer_size(FFT_IN_SIZE * channels)
{
	// Hann Window multipliers
	for (int i = 0; i < 8192; i++) {
		hann_multiplier[i] = 0.5 * (1 - cos(2*PI * i / (8192-1)));
	}

	// Allocate + initialize buffers
	input_buffer = new double[input_buffer_size]();

	l_sample_data_raw = allocate_initialized_fftw_real(16384);
	l_sample_data = allocate_initialized_fftw_real(16384);
	l_output_data = allocate_initialized_fftw_complex(8192 + 1);
	l_output_data_magnitude = std::vector<double>(8192 + 1);	

    plan_left = fftw_plan_dft_r2c_1d(16384, l_sample_data, l_output_data, FFTW_MEASURE);

	if (audio_channels == 2) { // two channels
		r_sample_data_raw = allocate_initialized_fftw_real(16384);
		r_sample_data = allocate_initialized_fftw_real(16384);
		r_output_data = allocate_initialized_fftw_complex(8192 + 1);
		r_output_data_magnitude = std::vector<double>(8192 + 1);

		plan_right = fftw_plan_dft_r2c_1d(16384, r_sample_data, r_output_data, FFTW_MEASURE);
	}
}

wava_plan::~wava_plan() {
	delete [] input_buffer;

	fftw_free(l_sample_data);
	fftw_free(l_sample_data_raw);	
	fftw_free(l_output_data);

	fftw_destroy_plan(plan_left);

	if (audio_channels == 2) {
		fftw_free(r_sample_data);
		fftw_free(r_sample_data_raw);
		fftw_free(r_output_data);

		fftw_destroy_plan(plan_right);
	}
}

std::vector<double> downsample(const std::vector<double>& magnitudes, int harmonic) {
	std::vector<double> downsampled_magnitudes(magnitudes.size()/harmonic);

	for (int i = 0; i < downsampled_magnitudes.size(); i++) {
		downsampled_magnitudes[i] = magnitudes[i * harmonic];
	}

	return downsampled_magnitudes;
}

std::vector<double> calculate_hps(const std::vector<double>& magnitudes, int harmonics) {
	int hps_output_size = (magnitudes.size()/2); // 2 is  the temp number of harmonics for now
	std::vector<double> hps_output(hps_output_size);

	std::vector<double> hps2 = downsample(magnitudes, 2);
	//std::vector<double> hps3 = downsample(magnitudes, 3);
	//std::vector<double> hps4 = downsample(magnitudes, 4);

	for (int i = 0; i < hps_output_size; i++) {
		hps_output[i] = magnitudes[i] * hps2[i];
	}

	return hps_output;
}

int adjust_freq_to_note(double freq) { // currently having issues detecting octaves bc of noise gate, also has a hard time with standard A note
	int note = -1;
	if (freq > 0) {
		while (freq > wava_plan::band_upper_cutoff_freq[14]) { freq *= 0.5; } // bring within note band ranges
		while (freq < wava_plan::band_lower_cutoff_freq[3]) { freq *= 2; }
	}

	for (int i = 3; i < 14; i++) {
		if (freq < wava_plan::band_upper_cutoff_freq[i] && freq > wava_plan::band_lower_cutoff_freq[i]) {
			note = i;
			break;
		}

	}
	return note;
}

std::vector<double> wava_execute(double* wava_in, int new_samples, wava_plan &plan) {
	
	// do not overflow
	if (new_samples > plan.input_buffer_size) {
		new_samples = plan.input_buffer_size;
	}

	bool silence = true; // plays a role in sensitivity adjustment
	if (new_samples > 0) {
		// something framerate related
		//
		//
		// shifting input buffer
		for (uint16_t n = plan.input_buffer_size - 1; n >= new_samples; n--) {
			plan.input_buffer[n] = plan.input_buffer[n - new_samples];
		}

		// fill the input buffer
		for (uint16_t n = 0; n < new_samples; n++) {
			plan.input_buffer[new_samples - n - 1] = wava_in[n];
			if (wava_in[n]) {
				silence = false;
			}
		}
	}
	else {
		// frame skip or smth? might be relevant later
	}

	// Fill the input buffers AND measure signal energy
    // only fill up half of sample arrays because the other half is padded
	double signal_energy = 0;
	for (uint16_t n = 0; n < 8192; n++) {
		if (plan.audio_channels == 2) {
			plan.l_sample_data_raw[n] = plan.input_buffer[n*2];
			signal_energy += plan.l_sample_data_raw[n] * plan.l_sample_data_raw[n];
			plan.r_sample_data_raw[n] = plan.input_buffer[n*2 + 1];
		}
		else {
			plan.l_sample_data_raw[n] = plan.input_buffer[n];
		}
	}

	signal_energy = sqrt(signal_energy) * 1/100000;

	// Actually applying Hann Window
	// again, only apply window to half of sample data because of padding
	for (int i = 0; i < 8192; i++) {
		plan.l_sample_data[i] = plan.hann_multiplier[i] * plan.l_sample_data_raw[i]; 
		if (plan.audio_channels == 2) plan.r_sample_data[i] = plan.hann_multiplier[i] * plan.r_sample_data_raw[i];
	}

	for (int i = 0; i < 8192; i++) {
		plan.l_sample_data[i + 8192] = 0;
	}

	// Execute FFT
	fftw_execute(plan.plan_left);
	//if (plan.audio_channels == 2) fftw_execute(plan.plan_right);

	std::vector<double> wava_out(plan.freq_bands); 

	for (int i = 0; i < (8192 + 1); i++) {
		plan.l_output_data_magnitude[i] = hypot(plan.l_output_data[i][0], plan.l_output_data[i][1]);
		//if (plan.audio_channels == 2) plan.r_output_data_magnitude[i] = hypot(plan.r_output_data[i][0], plan.r_output_data[i][1]);
	}

	std::vector<double> hps_output = calculate_hps(plan.l_output_data_magnitude, 3);

	for (int i = 0; i < hps_output.size(); i++) {
		if (hps_output[i] > 1e10) {
			int note = adjust_freq_to_note( ((double) ((44100/2)/(hps_output.size()))) * i );
			if (note != -1) {
				if (hps_output[i] > wava_out[note] * 1.0e12) wava_out[note] = hps_output[i] * 1/1.0e12;
			}
		}
	}

	double magnitude = 0;
	for (int i = 3; i < wava_out.size(); i++) {
		magnitude += wava_out[i]*wava_out[i];
	}
	magnitude = sqrt(magnitude);
	magnitude = (magnitude >= 1.0l) ? 1/magnitude : 1.0l;

	for (int i = 3; i < wava_out.size(); i++) {
		wava_out[i] *= magnitude;
	}


	for (int i = 3; i < wava_out.size(); i++) {
		if (wava_out[i] < plan.prev_wava_out[i] && (plan.prev_wava_out[i] - wava_out[i]) > 0.01) wava_out[i] = plan.prev_wava_out[i] - 0.01;
		plan.prev_wava_out[i] = wava_out[i];
	}

	
	wava_out[0] = signal_energy/4;

	return wava_out;
}
