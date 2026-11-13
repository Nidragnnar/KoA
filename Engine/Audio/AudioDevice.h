#pragma once
#include <mutex>
#include <vector>

#include "Engine/Audio/Sintetizador.h"

struct SDL_AudioStream;

namespace ke {

// Liga o sintetizador à placa de som (SDL3). Sem dispositivo, o jogo segue mudo, sem erro.
class AudioDevice {
public:
    ~AudioDevice() { fechar(); }
    bool abrir();
    void fechar();

    void tocar(Som s);
    void definir(float era, float noite, float tensao);
    void alternarMudo();
    void definirVolumes(float musica, float efeitos);
    bool mudo() const { return mudo_; }
    bool aberto() const { return stream_ != nullptr; }

private:
    static void callback(void* usuario, SDL_AudioStream* stream, int adicional, int total);

    SDL_AudioStream* stream_ = nullptr;
    Sintetizador synth_{48000, 7};
    std::mutex trava_;
    std::vector<float> buffer_;
    bool mudo_ = false;
};

}
