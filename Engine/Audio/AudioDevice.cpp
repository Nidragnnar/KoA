#include "Engine/Audio/AudioDevice.h"

#include <SDL3/SDL.h>

#include "Engine/Core/Logger.h"

namespace ke {

bool AudioDevice::abrir() {
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, synth_.taxa()};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioDevice::callback, this);
    if (!stream_) {
        log::aviso("áudio indisponível ({}); o jogo segue sem som", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream_);
    log::info("áudio aberto: {} Hz estéreo ({})", synth_.taxa(), SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?");
    return true;
}

void AudioDevice::fechar() {
    if (stream_) SDL_DestroyAudioStream(stream_);  // também fecha o dispositivo
    stream_ = nullptr;
}

void AudioDevice::callback(void* usuario, SDL_AudioStream* stream, int adicional, int) {
    auto* self = static_cast<AudioDevice*>(usuario);
    const int quadros = adicional / static_cast<int>(2 * sizeof(float));
    if (quadros <= 0) return;
    std::lock_guard lk(self->trava_);
    self->buffer_.resize(static_cast<size_t>(quadros) * 2);
    self->synth_.gerar(self->buffer_.data(), quadros);
    if (self->mudo_) std::fill(self->buffer_.begin(), self->buffer_.end(), 0.0f);
    SDL_PutAudioStreamData(stream, self->buffer_.data(), quadros * static_cast<int>(2 * sizeof(float)));
}

void AudioDevice::tocar(Som s) {
    std::lock_guard lk(trava_);
    synth_.tocar(s);
}

void AudioDevice::definir(float era, float noite, float tensao) {
    std::lock_guard lk(trava_);
    synth_.definirEra(era);
    synth_.definirNoite(noite);
    synth_.definirTensao(tensao);
}

void AudioDevice::definirVolumes(float musica, float efeitos) {
    std::lock_guard lk(trava_);
    synth_.definirVolumes(musica, efeitos);
}

void AudioDevice::alternarMudo() {
    std::lock_guard lk(trava_);
    mudo_ = !mudo_;
}

}
