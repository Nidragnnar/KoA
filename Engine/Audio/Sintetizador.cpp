#include "Engine/Audio/Sintetizador.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ke {

namespace {
constexpr float DOIS_PI = 2.0f * std::numbers::pi_v<float>;

struct Estilo {
    float bpm;
    int raiz;           // nota MIDI
    int escala[7];
    int progressao[4];  // graus da escala
    float probMelodia, vento;
    bool coro, sinos, drone;
};

// Uma por era + a trilha de perigo (floresta e cripta)
const Estilo ESTILOS[6] = {
    {54, 45, {0, 2, 3, 5, 7, 8, 10}, {0, 5, 2, 6}, 0.30f, 0.16f, false, false, false},  // Cinzas: lá menor, esparso, vento
    {66, 50, {0, 2, 3, 5, 7, 9, 10}, {0, 6, 3, 4}, 0.45f, 0.07f, false, false, false},  // Despertar: ré dórico
    {78, 48, {0, 2, 4, 5, 7, 9, 11}, {0, 4, 5, 3}, 0.60f, 0.00f, false, false, false},  // Renascido: dó maior
    {88, 53, {0, 2, 4, 6, 7, 9, 11}, {0, 1, 5, 4}, 0.70f, 0.00f, true, true, false},    // Alta Fantasia: fá lídio, coro e sinos
    {50, 40, {0, 1, 3, 5, 7, 8, 10}, {0, 1, 0, 6}, 0.22f, 0.20f, false, false, true},   // Perigo: mi frígio com zumbido grave
    {72, 57, {0, 2, 4, 6, 7, 9, 11}, {0, 5, 3, 4}, 0.75f, 0.00f, true, true, true},     // Coroa Eterna (1.18): lá lídio, coro, sinos e um bordão
};
}

Sintetizador::Sintetizador(int taxa, uint32_t semente) : taxa_(taxa), rng_(semente) { vozes_.reserve(64); }

float Sintetizador::freqMidi(float midi) { return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f); }

int Sintetizador::estiloAtual() const {
    if (tensao_ > 0.5f) return 4;
    if (era_ >= 3.75f) return 5;  // 1.18: a Coroa Eterna
    return std::clamp(static_cast<int>(std::lround(era_)), 0, 3);
}

void Sintetizador::nota(int midi, float duracao, float amp, int forma, float ataque, float decaimento, float pan, float vibrato) {
    if (vozes_.size() > 48) return;  // segurança: nunca acumula vozes demais
    Voz v;
    v.freq = freqMidi(static_cast<float>(midi));
    v.duracao = duracao;
    v.amp = amp;
    v.forma = forma;
    v.ataque = ataque;
    v.decaimento = decaimento;
    v.pan = pan;
    v.vibrato = vibrato;
    v.corte = forma == 4 ? 0.08f : 1.0f;  // serra abafada (som de cordas)
    vozes_.push_back(v);
}

void Sintetizador::proximaBatida() {
    const Estilo& e = ESTILOS[estiloAtual()];
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    const float seg = 60.0f / e.bpm;
    const int nesteAcorde = batida_ % 8;
    if (nesteAcorde == 0) {
        acorde_ = (acorde_ + 1) % 4;
        // Acorde do grau atual: tônica, terça e quinta da escala
        const int grau = e.progressao[acorde_];
        for (int k = 0; k < 3; ++k) {
            const int g = grau + k * 2;
            const int midi = e.raiz + 12 + e.escala[g % 7] + 12 * (g / 7);
            nota(midi, seg * 8.0f, e.coro ? 0.035f : 0.045f, e.coro ? 0 : 4, seg * 2.0f, 0.15f, (k - 1) * 0.5f, e.coro ? 0.006f : 0.0f);
            if (e.coro) nota(midi + 12, seg * 8.0f, 0.02f, 0, seg * 3.0f, 0.2f, (1 - k) * 0.4f, 0.008f);
        }
        if (e.drone) nota(e.raiz - 12, seg * 8.0f, 0.06f, 4, seg, 0.05f, 0.0f);
    }
    // Baixo nos tempos fortes
    if (nesteAcorde == 0 || nesteAcorde == 4) {
        const int grau = e.progressao[acorde_];
        nota(e.raiz + e.escala[grau % 7], seg * 1.6f, 0.07f, 1, 0.02f, 1.6f, 0.0f);
    }
    // Melodia: passos curtos dentro da escala (mais rara à noite e nas eras sombrias)
    const float prob = e.probMelodia * (1.0f - 0.4f * noite_);
    if (u(rng_) < prob) {
        std::uniform_int_distribution<int> passo(-2, 2);
        grauMelodia_ = std::clamp(grauMelodia_ + passo(rng_), 0, 11);
        const int midi = e.raiz + 24 + e.escala[grauMelodia_ % 7] + 12 * (grauMelodia_ / 7);
        nota(midi, seg * 1.5f, 0.06f, e.raiz >= 48 ? 1 : 0, 0.01f, 2.2f, u(rng_) * 0.6f - 0.3f);
    }
    if (e.sinos && u(rng_) < 0.18f) {
        const int midi = e.raiz + 36 + e.escala[(acorde_ * 2 + batida_) % 7];
        nota(midi, seg * 4.0f, 0.03f, 0, 0.005f, 0.9f, u(rng_) * 1.4f - 0.7f);
    }
    ++batida_;
    ateBatida_ += seg * taxa_;
}

void Sintetizador::tocar(Som s) {
    auto efeito = [&](float freq, float dur, float amp, int forma, float decai, float atraso = 0.0f, float deslize = 0.0f, float corte = 1.0f) {
        if (vozes_.size() > 60) return;
        Voz v;
        v.freq = freq;
        v.duracao = dur;
        v.amp = amp;
        v.forma = forma;
        v.ataque = 0.003f;
        v.decaimento = decai;
        v.atraso = atraso;
        v.deslize = deslize;
        v.corte = corte;
        v.musica = false;
        vozes_.push_back(v);
    };
    switch (s) {
    case Som::Golpe:
        efeito(900, 0.09f, 0.22f, 3, 30.0f, 0.0f, 0.0f, 0.25f);
        efeito(220, 0.12f, 0.16f, 0, 18.0f, 0.0f, -2.5f);
        break;
    case Som::Acerto:
        efeito(160, 0.1f, 0.18f, 2, 20.0f, 0.0f, -1.5f, 0.3f);
        efeito(600, 0.06f, 0.12f, 3, 40.0f, 0.0f, 0.0f, 0.4f);
        break;
    case Som::Coleta:
        efeito(660, 0.08f, 0.12f, 1, 20.0f);
        efeito(990, 0.1f, 0.12f, 1, 18.0f, 0.06f);
        break;
    case Som::Moeda:
        efeito(1320, 0.12f, 0.1f, 0, 22.0f);
        efeito(1760, 0.2f, 0.1f, 0, 14.0f, 0.06f);
        break;
    case Som::Construir:
        efeito(120, 0.25f, 0.25f, 3, 10.0f, 0.0f, 0.0f, 0.05f);
        for (int i = 0; i < 3; ++i) efeito(880.0f * std::pow(1.26f, static_cast<float>(i)), 0.4f, 0.08f, 0, 6.0f, 0.12f + i * 0.08f);
        break;
    case Som::Dano:
        efeito(110, 0.28f, 0.2f, 2, 8.0f, 0.0f, -1.2f, 0.2f);
        break;
    case Som::Magia:
        for (int i = 0; i < 6; ++i) efeito(523.25f * std::pow(1.2599f, static_cast<float>(i % 3)) * (i >= 3 ? 2.0f : 1.0f), 0.9f, 0.07f, 0, 3.0f, i * 0.09f);
        break;
    case Som::Menu: efeito(880, 0.05f, 0.07f, 1, 40.0f); break;
    }
}

void Sintetizador::efeitoAmbiente(float freq, float dur, float amp, int forma, float decai, float atraso, float deslize, float corte, float pan) {
    if (vozes_.size() > 56) return;
    Voz v;
    v.freq = freq;
    v.duracao = dur;
    v.amp = amp;
    v.forma = forma;
    v.ataque = 0.004f;
    v.decaimento = decai;
    v.atraso = atraso;
    v.deslize = deslize;
    v.corte = corte;
    v.pan = pan;
    v.musica = false;
    vozes_.push_back(v);
}

void Sintetizador::somDeAmbiente() {
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    const float pan = u(rng_) * 1.6f - 0.8f;
    auto conta = [&](const char* tipo) { ++contagem_[tipo]; };
    auto passaro = [&](float amp) {
        const float f = 2400.0f + u(rng_) * 1400.0f;
        const int notas = 2 + static_cast<int>(u(rng_) * 3);
        for (int i = 0; i < notas; ++i) efeitoAmbiente(f * (1.0f + 0.08f * i), 0.09f, amp, 0, 26.0f, i * 0.11f, 2.5f, 1.0f, pan);
        conta("passaro");
    };
    auto gota = [&](float amp, bool eco) {
        const float f = 900.0f + u(rng_) * 700.0f;
        efeitoAmbiente(f, 0.16f, amp, 0, 22.0f, 0.0f, -2.2f, 1.0f, pan);
        if (eco)  // a cripta devolve a gota duas vezes, cada vez mais fraca
            for (int i = 1; i <= 2; ++i) efeitoAmbiente(f, 0.16f, amp * (i == 1 ? 0.45f : 0.2f), 0, 22.0f, 0.28f * i, -2.2f, 1.0f, -pan * 0.6f);
        conta(eco ? "gota_eco" : "gota");
    };
    float espera = 2.0f;
    switch (ambiente_) {
    case Ambiente::Nenhum: espera = 1.0f; break;
    case Ambiente::Vilarejo:
        if (noite_ > 0.55f) {  // grilos: três pulsos agudos
            for (int i = 0; i < 3; ++i) efeitoAmbiente(4300.0f + u(rng_) * 300.0f, 0.03f, 0.012f, 0, 60.0f, i * 0.06f, 0.0f, 1.0f, pan);
            conta("grilo");
            espera = 0.6f + u(rng_) * 1.2f;
        } else if (era_ >= 1.5f) {  // as eras felizes trazem os pássaros de volta
            passaro(0.016f);
            espera = 2.5f + u(rng_) * 4.0f;
        } else {
            espera = 3.0f;  // nas cinzas, só o vento
        }
        break;
    case Ambiente::Floresta:  // corvos: dois grasnidos roucos
        for (int i = 0; i < 2; ++i) efeitoAmbiente(330.0f + u(rng_) * 60.0f, 0.22f, 0.05f, 2, 9.0f, i * 0.3f, -0.7f, 0.12f, pan);
        conta("corvo");
        espera = 5.0f + u(rng_) * 7.0f;
        break;
    case Ambiente::FlorestaEncantada:
        passaro(0.02f);
        if (u(rng_) < 0.4f) {  // brilho: sininhos suaves
            for (int i = 0; i < 3; ++i) efeitoAmbiente(1760.0f * (1.0f + 0.25f * i), 0.8f, 0.01f, 0, 3.5f, i * 0.09f, 0.0f, 1.0f, -pan);
            conta("brilho");
        }
        espera = 1.5f + u(rng_) * 3.0f;
        break;
    case Ambiente::Cripta:
        gota(0.03f, true);
        espera = 2.5f + u(rng_) * 4.0f;
        break;
    case Ambiente::Minas:
        if (u(rng_) < 0.25f) {  // estrondo distante: ruído grave e longo
            efeitoAmbiente(80.0f, 1.6f, 0.06f, 3, 1.6f, 0.0f, 0.0f, 0.015f, pan);
            conta("estrondo");
        } else {
            gota(0.025f, false);
        }
        espera = 1.2f + u(rng_) * 2.5f;
        break;
    }
    ateAmbiente_ = espera;
}

float Sintetizador::onda(Voz& v) {
    const float vib = v.vibrato > 0.0f ? 1.0f + v.vibrato * std::sin(v.idade * 5.5f * DOIS_PI) : 1.0f;
    const float f = v.freq * vib * (1.0f + v.deslize * v.idade);
    v.fase += std::max(0.0f, f) / taxa_;
    v.fase -= std::floor(v.fase);
    float x;
    switch (v.forma) {
    case 1: x = 4.0f * std::abs(v.fase - 0.5f) - 1.0f; break;
    case 2: x = v.fase < 0.5f ? 0.6f : -0.6f; break;
    case 3: x = std::uniform_real_distribution<float>(-1.0f, 1.0f)(rng_); break;
    case 4: x = 2.0f * v.fase - 1.0f; break;
    default: x = std::sin(v.fase * DOIS_PI); break;
    }
    if (v.corte < 1.0f) x = v.filtro += v.corte * (x - v.filtro);
    return x;
}

void Sintetizador::gerar(float* saida, int quadros) {
    const float dt = 1.0f / taxa_;
    const Estilo& e = ESTILOS[estiloAtual()];
    std::uniform_real_distribution<float> ruido(-1.0f, 1.0f);
    for (int i = 0; i < quadros; ++i) {
        ateBatida_ -= 1.0;
        if (ateBatida_ <= 0.0) proximaBatida();
        ateAmbiente_ -= dt;
        if (ateAmbiente_ <= 0.0) somDeAmbiente();
        float l = 0.0f, r = 0.0f;
        for (Voz& v : vozes_) {
            if (v.atraso > 0.0f) {
                v.atraso -= dt;
                continue;
            }
            const float env = std::min(1.0f, v.idade / std::max(v.ataque, 1e-4f)) * std::exp(-v.idade * v.decaimento);
            const float x = onda(v) * v.amp * env * (v.musica ? volMusica_ * (1.0f - 0.25f * noite_) : volEfeitos_);
            l += x * (1.0f - std::max(0.0f, v.pan));
            r += x * (1.0f + std::min(0.0f, v.pan));
            v.idade += dt;
        }
        // Vento: ruído filtrado que sobe e desce devagar (some nas eras felizes)
        // Fase 20: a floresta amaldiçoada e as profundezas têm vento próprio, mesmo nas eras felizes
        const float ventoLugar = ambiente_ == Ambiente::Floresta ? 0.12f : ambiente_ == Ambiente::Cripta || ambiente_ == Ambiente::Minas ? 0.05f : 0.0f;
        if (const float vento = std::max(e.vento, ventoLugar); vento > 0.0f) {
            faseVento_ += dt * 0.13f;
            ventoFiltro_ += 0.02f * (ruido(rng_) - ventoFiltro_);
            const float w = ventoFiltro_ * vento * (0.55f + 0.45f * std::sin(faseVento_ * DOIS_PI)) * volMusica_;
            l += w;
            r += w * 0.9f;
        }
        // Limitador suave: nunca passa de ±1
        saida[2 * i] = std::tanh(l);
        saida[2 * i + 1] = std::tanh(r);
    }
    std::erase_if(vozes_, [](const Voz& v) { return v.atraso <= 0.0f && v.idade >= v.duracao; });
}

}
