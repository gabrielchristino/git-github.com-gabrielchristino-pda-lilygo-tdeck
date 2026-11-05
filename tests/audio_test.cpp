#include "hal/audio.h"
#include <driver/i2s.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
uint32_t fakeMillis=0;
static int installError=ESP_FAIL,pinError=0,writeError=0,installs=0,uninstalls=0,zeros=0,writes=0;
static size_t budget=512;
static std::vector<uint8_t> pcm;
esp_err_t i2s_driver_install(int port,const i2s_config_t *c,int,void *) {
    ++installs;assert(port==I2S_NUM_0&&c->sample_rate==16000&&c->bits_per_sample==16);
    assert(c->channel_format==I2S_CHANNEL_FMT_RIGHT_LEFT&&c->tx_desc_auto_clear);
    return installError;
}
esp_err_t i2s_driver_uninstall(int) {++uninstalls;return ESP_OK;}
esp_err_t i2s_set_pin(int,const i2s_pin_config_t *p) {
    assert(p->bck_io_num==7&&p->ws_io_num==5&&p->data_out_num==6);
    assert(p->data_in_num==-1&&p->mck_io_num==-1);return pinError;
}
esp_err_t i2s_zero_dma_buffer(int) {++zeros;return ESP_OK;}
esp_err_t i2s_write(int,const void *data,size_t n,size_t *written,uint32_t timeout) {
    assert(timeout==0);++writes;
    *written=writeError?0:std::min(budget,n);
    const auto *p=static_cast<const uint8_t *>(data);pcm.insert(pcm.end(),p,p+*written);
    return writeError?writeError:(*written<n?ESP_ERR_TIMEOUT:ESP_OK);
}
static void finish() {
    for(unsigned i=0;i<10000&&pda::hal::audioStats().playing;++i) {
        const auto before=writes;pda::hal::audioTick();assert(writes-before<=4);fakeMillis+=2;
    }
    assert(!pda::hal::audioStats().playing&&pda::hal::audioStats().error==0);
}
int main() {
    using namespace pda::hal;
    assert(!audioAlert()&&!audioStats().initialized&&audioStats().error==ESP_FAIL);
    installError=0;pinError=ESP_FAIL;assert(!audioAlert()&&uninstalls==1&&!audioStats().initialized);
    pinError=0;assert(audioAlert()&&audioStats().initialized&&audioStats().playing);
    const auto initialized=installs;assert(audioAlert()&&installs==initialized);
    const auto initialZeros=zeros;finish();
    assert(pcm.size()==11520*4&&audioStats().completed==1&&zeros==initialZeros);
    std::vector<int16_t> wave(pcm.size()/2);memcpy(wave.data(),pcm.data(),pcm.size());
    int peak=0;
    for(size_t frame=0;frame<11520;++frame) {
        assert(wave[frame*2]==wave[frame*2+1]);peak=std::max(peak,int(wave[frame*2]));
        if(frame>=10880||frame%4160>=2560) assert(wave[frame*2]==0);
    }
    assert(peak==10000);
    const auto reference=pcm;pcm.clear();budget=77;assert(audioAlert());finish();
    assert(pcm==reference&&audioStats().completed==2); // partial sample/frame writes are preserved
    budget=0;assert(audioAlert());fakeMillis+=1001;audioTick();
    assert(!audioStats().playing&&audioStats().error==ESP_ERR_TIMEOUT);
    budget=512;writeError=ESP_FAIL;assert(audioAlert());audioTick();
    assert(!audioStats().playing&&audioStats().error==ESP_FAIL);
    writeError=0;assert(audioAlert());audioTick();audioStop();assert(!audioStats().playing);
    pcm.clear();assert(audioAlert());finish();assert(pcm==reference);
    puts("audio: I2S setup/errors/retry, nonblocking writes, partial writes, stereo PCM, three tones, silent tail, coalescing, cancellation and stall timeout OK");
}
