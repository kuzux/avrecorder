#include <alsa/asoundlib.h>

#include <system_error>
#include <stdexcept>

class AudioRecorder {
    snd_pcm_t* ahandle = nullptr;
    uint8_t abuf[128];

public:
    AudioRecorder() {
       snd_pcm_hw_params_t* aparams;
       int rc;

        if((rc = snd_pcm_open(&ahandle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0)
            throw std::system_error(rc, std::generic_category(), "snd_pcm_open");
        
        if((rc = snd_pcm_hw_params_malloc(&aparams)) < 0)
            throw std::system_error(rc, std::generic_category(), "snd_pcm_hw_params_malloc");
        if((rc = snd_pcm_hw_params_any(ahandle, aparams)) < 0)
            throw std::system_error(rc, std::generic_category(), "snd_pcm_hw_params_any");

        if((rc = snd_pcm_hw_params_set_access(ahandle, aparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
            throw std::system_error(rc, std::generic_category(), "snd_pcm_hw_params_set_access");
        if((rc = snd_pcm_hw_params_set_format(ahandle, aparams, SND_PCM_FORMAT_U8)) < 0)
            throw std::system_error(rc, std::generic_category(), "snd_pcm_hw_params_set_format");
        uint rate = 44100;
        int dir = 0;
        if((rc = snd_pcm_hw_params_set_rate_near(ahandle, aparams, &rate, &dir)) < 0)
            throw std::system_error(rc, std::generic_category(), "snd_pcm_hw_params_set_rate_near");
        if((rc = snd_pcm_hw_params_set_channels(ahandle, aparams, 1)) < 0)
            throw std::system_error(rc, std::generic_category(), "snd_pcm_hw_params_set_channels");

        if((rc = snd_pcm_hw_params(ahandle, aparams)) < 0)
            throw std::system_error(rc, std::generic_category(), "snd_pcm_hw_params");
        snd_pcm_hw_params_free(aparams);
    }

    ~AudioRecorder() {
        snd_pcm_close(ahandle);
    }

    void start() {
        int rc = snd_pcm_prepare(ahandle);
        if(rc < 0)
            throw std::system_error(rc, std::generic_category(), "snd_pcm_prepare");
    }

    // todo: return length alonside the buffer pointer
    uint8_t* getSample() {
        int rc = snd_pcm_readi(ahandle, abuf, 128);
        if(rc != 128)
            throw std::runtime_error("error reading audio samples");
        return abuf;
    }
};