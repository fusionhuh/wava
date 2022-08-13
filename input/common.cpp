#include <common.h>
#include <math.h>
#include <string.h>

audio_data::audio_data(int channels, int rate) : 
    channels(channels), rate(rate), input_buffer_size(channels*512), samples_counter(0), wava_buffer_size(input_buffer_size*8), terminate(0) 
{
    wava_in = new double[wava_buffer_size];   
    source = new char[1]; 
}

audio_data::~audio_data() {
    delete [] wava_in;
    delete [] source;
}

int write_to_wava_input_buffers(int16_t size, int16_t* buf, void *data) {
    if (size == 0)
        return 0;
    struct audio_data *audio = (struct audio_data *)data;
    
    audio->mtx.lock();
    if (audio->samples_counter + size > audio->wava_buffer_size) {
        // buffer overflow, discard what ever is in the buffer and start over
        for (uint16_t n = 0; n < audio->wava_buffer_size; n++) {
            audio->wava_in[n] = 0;
        }
        audio->samples_counter = 0;
    }
    for (uint16_t i = 0; i < size; i++) {
        audio->wava_in[i + audio->samples_counter] = buf[i];
    }
    audio->samples_counter += size;
    audio->mtx.unlock();
    return 0;
}

void reset_output_buffers(struct audio_data *data) {
    struct audio_data *audio = (struct audio_data *)data;
    audio->mtx.lock();
    for (uint16_t n = 0; n < audio->wava_buffer_size; n++) {
        audio->wava_in[n] = 0;
    }
    audio->mtx.unlock();
}
