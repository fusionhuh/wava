#include <math.h>
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/stream.h>

#include <fftw3.h>

#include "graphics.h"

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 8192 // FFT works better with powers of two

double fftw_data[BUFFER_SIZE] = {0};
double fftw_in[BUFFER_SIZE] = {0};             //
double fftw_backup_buffer[BUFFER_SIZE] = {0}; // Allocating these arrays on data section to avoid excessive and unnecessary
double amplitude_data[BUFFER_SIZE] = {0};   // malloc calls in pa_stream_callback
int curr_fftw_data_size = 0;
int curr_backup_buffer_size = 0;
bool should_read_from_fftw_data = false;

std::mutex mtx;

struct fftw_analysis {
  float average_bass_value = 0;
};

fftw_analysis analyze_fftw_data() {
  if (curr_fftw_data_size <= 0) { return (fftw_analysis) {0}; }

  float highest_bass_frequency = 200;
  int index = (int) ((highest_bass_frequency/(SAMPLE_RATE/2)) * curr_fftw_data_size);

  float average_value = 0.00001;
  if (index > 0) { // implement more elegant solution for checking later
    for (int i = 0; i < index; ++i) {
      //printf ("inside loop, data value is: %f\n", fftw_data[i]);
      if (fftw_data[i] > 0.3) average_value+=fftw_data[i];
    }

    average_value/=index;
  }

  for (int i = 0; i < BUFFER_SIZE; i++) fftw_data[i] = 0;
  //printf ("Average value is: %f", average_value);

  return (fftw_analysis) {average_value};
}

int access_fftw_buffer(bool write, double* data, int data_length /*only applicable for writes*/, fftw_analysis &analysis) {
  if (!(mtx.try_lock())) { // accessing failed, locked by other thread
    mtx.unlock();
    //printf ("locked\n");
    return -1; // failure code
  } 

  else {
    if(write) // pa thread is trying to write
    { 
      for (int i = 0; i < BUFFER_SIZE; i++) {
        fftw_data[i] = data[i];
      }
      should_read_from_fftw_data = true;
      mtx.unlock();
      return 0;
    }
    else if (should_read_from_fftw_data) { // draw thread is trying to read, need to check if new audio data has been processed
      analysis = analyze_fftw_data();
      should_read_from_fftw_data = false;
      mtx.unlock();
      return 0;
    }
    else {
      return 0;
    }
  }
}

void pa_stream_notify_cb(pa_stream *stream, void* userdata)
{
    const pa_stream_state state = pa_stream_get_state(stream);
    switch (state) {
    case PA_STREAM_FAILED:
	std::cout << "Stream failed\n";
	break;
    case PA_STREAM_READY:
	std::cout << "Stream ready\n";
	break;
    default:
	std::cout << "Stream state: " << state << std::endl;
    }
}

void pa_stream_read_cb(pa_stream *stream, const size_t nbytes, void* userdata)
{
    // Careful when to pa_stream_peek() and pa_stream_drop()!
    // c.f. https://www.freedesktop.org/software/pulseaudio/doxygen/stream_8h.html#ac2838c449cde56e169224d7fe3d00824
    int16_t *data = nullptr;
    size_t stream_length = 0;
    if (pa_stream_peek(stream, (const void**)&data, &stream_length) != 0) {
        std::cerr << "Failed to peek at stream data\n";
        return;
    }

    if (data == nullptr && stream_length == 0) {
	  // No data in the buffer, ignore.
        return;
    } else if (data == nullptr && stream_length > 0) {
        // Hole in the buffer. We must drop it.
      if (pa_stream_drop(stream) != 0) {
          std::cerr << "Failed to drop a hole! (Sounds weird, doesn't it?)\n";
          return;
      }
    }


    // process data
    fftw_complex *out;
    fftw_plan p;

    int space_until_full = BUFFER_SIZE - curr_fftw_data_size;

    if (curr_backup_buffer_size <= space_until_full) {
      for (int i = 0; i < curr_backup_buffer_size; i++) {
        fftw_in[curr_fftw_data_size + i] = fftw_backup_buffer[i];
        space_until_full-=curr_backup_buffer_size;
        curr_backup_buffer_size = 0;
      }
    }
    else {
      for (int i = 0; i < space_until_full; i++) {
        fftw_in[curr_fftw_data_size + i] = fftw_backup_buffer[i];
        space_until_full = 0;
        curr_backup_buffer_size-=space_until_full;
        curr_fftw_data_size = BUFFER_SIZE;
      }
    }

    

    int fftw_in_write_size, backup_buffer_write_size;

    if (space_until_full >= stream_length) { // i.e. enough space in fftw_buffer to store new stream data
      backup_buffer_write_size = 0;
      fftw_in_write_size = stream_length;
      curr_fftw_data_size += stream_length;
    }
    else { // not enough space in just fftw_buffer
      fftw_in_write_size = space_until_full;
      curr_fftw_data_size = BUFFER_SIZE;

      backup_buffer_write_size = stream_length - space_until_full;
      //printf ("REMAINING SPACE IS: %d\nREQUIRED SPACE IS: %d\n\n\n\n\n", (BUFFER_SIZE - curr_backup_buffer_size), backup_buffer_write_size);
      if ((BUFFER_SIZE - curr_backup_buffer_size) - backup_buffer_write_size >= 0) {
        curr_backup_buffer_size += backup_buffer_write_size;
      }
      else { // backup buffer overflow; need to dump for now, but can prob develop more elegant solution
        curr_backup_buffer_size = 0;
        backup_buffer_write_size = 0;
      }
    }

    for (int i = 0; i < fftw_in_write_size; i++) {
      fftw_in[curr_fftw_data_size + i] = data[i];
    }

    for (int i = 0; i < backup_buffer_write_size; i++) {
      fftw_backup_buffer[curr_backup_buffer_size + i] = data[space_until_full + i];
    }

    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * ((int )(BUFFER_SIZE/2 + 1)) );    
    p = fftw_plan_dft_r2c_1d(BUFFER_SIZE, fftw_in, out, FFTW_MEASURE);

    fftw_execute(p);
    // process frequency data for this "frame"

    double largest_amplitude = .01;

    for (int i = 0; i < (BUFFER_SIZE/2) + 1; i++) {
      double cos_amp = out[i][0], sin_amp = out[i][1];

      amplitude_data[i] = sqrt(cos_amp*cos_amp + sin_amp*sin_amp);

      if (amplitude_data[i] > largest_amplitude) largest_amplitude = amplitude_data[i];
    }

    for (int i = 0; i < (BUFFER_SIZE/2) + 1; i++) amplitude_data[i]/=largest_amplitude;

    fftw_analysis shut_the_fuck_up_gcc = (fftw_analysis) {0}; // <- this is HORRENDOUS PLEASE FIX THIS
    
    int err;
    if (curr_fftw_data_size == BUFFER_SIZE) {
     err = access_fftw_buffer(true, amplitude_data, stream_length, shut_the_fuck_up_gcc); // <- this is seriously flawed and needs work
     curr_fftw_data_size = 0;
    }

    fftw_destroy_plan(p);
    fftw_free(out);

    if (pa_stream_drop(stream) != 0) {
	    std::cerr << "Failed to drop data after peeking.\n";
    }
}

void pa_server_info_cb(pa_context *ctx, const pa_server_info *info, void* userdata)
{
    std::cout << "Default sink: " << info->default_sink_name << std::endl;

    pa_sample_spec spec;
    spec.format = PA_SAMPLE_S16LE;
    spec.rate = 44100;
    spec.channels = 1;
    // Use pa_stream_new_with_proplist instead?
    pa_stream *stream = pa_stream_new(ctx, "output monitor", &spec, nullptr);

    pa_stream_set_state_callback(stream, &pa_stream_notify_cb, userdata);
    pa_stream_set_read_callback(stream, &pa_stream_read_cb, userdata);

    std::string monitor_name(info->default_sink_name);
    monitor_name += ".monitor";
    if (pa_stream_connect_record(stream, monitor_name.c_str(), nullptr, PA_STREAM_NOFLAGS) != 0) {
      std::cerr << "connection fail\n";
      return;
    }

    std::cout << "Connected to " << monitor_name << std::endl;
}

void pa_context_notify_cb(pa_context *ctx, void* userdata)
{
    const pa_context_state state = pa_context_get_state(ctx);
    switch (state) {
    case PA_CONTEXT_READY:
	std::cout << "Context ready\n";
	pa_context_get_server_info(ctx, &pa_server_info_cb, userdata);
	break;
    case PA_CONTEXT_FAILED:
	std::cout << "Context failed\n";
	break;
    default:
	std::cout << "Context state: " << state << std::endl;
    }
}

int draw () {
  float A = 0, B = 0;
  float speed = 0.01;
  int time = 0;

  //main drawing loop

  fftw_analysis analysis;

  while (true)
  {
    if (time == (int) 1/speed) { A = 0; B = 0; time = 0; }
    
    int err = access_fftw_buffer(false, NULL, 0, analysis);
    
    if (err == 0) {
      printf ("Average bass value: %f\n", analysis.average_bass_value/2);
    }

    //render_frame (A, B, analysis.average_bass_value/2); // stuttering can come from average value being NaN
    usleep (10000);
    A += 3.14 * 2 * speed; 
    B += 3.14 * 2 * speed;
    time += 1;
  }
}

int main(int argc, char **argv)
{

  void* userdata;

  pa_mainloop *loop = pa_mainloop_new();
  pa_mainloop_api *api = pa_mainloop_get_api(loop);
  pa_context *ctx = pa_context_new(api, "padump");
  pa_context_set_state_callback(ctx, &pa_context_notify_cb, userdata);
  if (pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
    std::cerr << "PA connection failed.\n";
    return -1;
  }

  // pa_stream_disconnect(..)
  std::thread t1(draw);
  std::thread t2(pa_mainloop_run, loop, nullptr);

  t1.join();
  //t2.join();
  // gotta fix the segfault that occurs here

  //pa_context_disconnect(ctx);
  //pa_mainloop_free(loop);

  return 0;
}


