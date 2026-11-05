#include "audio.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <algorithm>

namespace pda { namespace hal {
namespace {
constexpr uint32_t rate=16000, framesPerBlock=128, totalFrames=11520;
AudioStats stats;
int16_t samples[framesPerBlock*2];
uint32_t frame=0, lastProgress=0;
size_t offset=0, length=0;
bool initialize() {
    if(stats.initialized) return true;
    i2s_config_t config={};
    config.mode=i2s_mode_t(I2S_MODE_MASTER|I2S_MODE_TX);
    config.sample_rate=rate;config.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
    // Identical PCM in both slots, regardless of amplifier channel selection.
    // No microphone or MCLK: GPIO0 belongs to the trackball button.
    config.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format=I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count=4;config.dma_buf_len=128;
    config.tx_desc_auto_clear=true;
    auto error=i2s_driver_install(I2S_NUM_0,&config,0,nullptr);
    if(error!=ESP_OK) {stats.error=error;return false;}
    i2s_pin_config_t pins={};pins.bck_io_num=7;pins.ws_io_num=5;pins.data_out_num=6;
    pins.data_in_num=pins.mck_io_num=I2S_PIN_NO_CHANGE;
    error=i2s_set_pin(I2S_NUM_0,&pins);
    if(error!=ESP_OK) {i2s_driver_uninstall(I2S_NUM_0);stats.error=error;return false;}
    stats.initialized=true;stats.error=0;
    i2s_zero_dma_buffer(I2S_NUM_0);
    return true;
}
void generate() {
    // Three 160 ms tones with 100 ms gaps, followed by 40 ms of silence.
    static const int16_t wave[]={0,3827,7071,9239,10000,9239,7071,3827,0,-3827,-7071,-9239,-10000,-9239,-7071,-3827};
    const auto count=std::min(framesPerBlock,totalFrames-frame);
    for(uint32_t i=0;i<count;++i) {
        const auto position=(frame+i)%4160;
        int16_t value=0;
        if(frame+i<10880 && position<2560) {
            const auto envelope=std::min<uint32_t>(16,std::min(position,2559-position));
            value=int16_t(int32_t(wave[(frame+i)%16])*envelope/16);
        }
        samples[i*2]=samples[i*2+1]=value;
    }
    length=count*2*sizeof(int16_t);offset=0;frame+=count;
}
}
bool audioAlert() {
    ++stats.requests;
    if(!initialize()) return false;
    if(stats.playing) return true; // Coalesce simultaneous notifications.
    stats.playing=true;stats.error=0;frame=0;offset=length=0;lastProgress=millis();
    return true;
}
void audioStop() {
    if(stats.initialized) i2s_zero_dma_buffer(I2S_NUM_0);
    stats.playing=false;offset=length=0;
}
void audioTick() {
    if(!stats.playing) return;
    // Bounded, nonblocking writes. No task or delay in the UI loop.
    for(unsigned block=0;block<4;++block) {
        if(offset==length) {
            if(frame==totalFrames) {
                // Let queued DMA, including the silent tail, finish naturally.
                stats.playing=false;++stats.completed;return;
            }
            generate();
        }
        size_t written=0;
        const auto error=i2s_write(I2S_NUM_0,reinterpret_cast<const uint8_t *>(samples)+offset,length-offset,&written,0);
        if(written>length-offset) {stats.error=ESP_FAIL;audioStop();return;}
        offset+=written;stats.bytes+=written;
        if(error!=ESP_OK && error!=ESP_ERR_TIMEOUT) {stats.error=error;audioStop();return;}
        if(written) lastProgress=millis();
        if(offset<length) break;
    }
    if(uint32_t(millis()-lastProgress)>1000) {stats.error=ESP_ERR_TIMEOUT;audioStop();}
}
AudioStats audioStats() {return stats;}
} }
