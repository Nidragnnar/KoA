#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <random>
#include <vector>

namespace ke {

enum class Som { Golpe, Acerto, Coleta, Moeda, Construir, Dano, Magia, Menu };
// Os sons de fundo de cada lugar (por cima da música): grilos e pássaros no vilarejo, corvos na floresta amaldiçoada,
// pássaros e brilhos na encantada, gotas com eco na cripta, gotas e estrondos nas minas
enum class Ambiente { Nenhum, Vilarejo, Floresta, FlorestaEncantada, Cripta, Minas };

// Música e efeitos feitos por código (sem arquivos de áudio). Parte pura: dá para testar sem placa de som.
// A música é generativa: acordes, baixo e melodia sorteada dentro da escala da era.
class Sintetizador {
public:
    explicit Sintetizador(int taxa = 48000, uint32_t semente = 1);

    // Preenche `quadros` amostras estéreo intercaladas (L, R) entre -1 e 1
    void gerar(float* estereo, int quadros);

    void definirEra(float era) { era_ = era; }          // 0..3: escala, andamento e instrumentos; 4: a Coroa Eterna
    void definirNoite(float n) { noite_ = n; }          // 0..1: música mais calma
    void definirTensao(float t) { tensao_ = t; }        // 0..1: floresta e cripta (trilha sombria)
    void definirVolumes(float musica, float efeitos) { volMusica_ = musica, volEfeitos_ = efeitos; }
    void definirAmbiente(Ambiente a) { ambiente_ = a; }
    Ambiente ambiente() const { return ambiente_; }
    // Quantos sons de ambiente de cada tipo já tocaram ("grilo", "passaro", "corvo"...): para os testes
    const std::map<std::string, int>& sonsDeAmbiente() const { return contagem_; }
    void tocar(Som s);
    int taxa() const { return taxa_; }
    size_t vozesAtivas() const { return vozes_.size(); }

private:
    struct Voz {
        float freq = 440.0f, fase = 0.0f, amp = 0.1f;
        float ataque = 0.01f, decaimento = 3.0f, duracao = 0.5f, idade = 0.0f, atraso = 0.0f;
        float deslize = 0.0f;   // variação de frequência por segundo (fração)
        float vibrato = 0.0f;
        float pan = 0.0f;       // -1 esquerda, 1 direita
        float corte = 1.0f, filtro = 0.0f;  // passa-baixa de um polo (1 = sem filtro)
        int forma = 0;          // 0 seno, 1 triângulo, 2 quadrada, 3 ruído, 4 serra
        bool musica = true;
    };
    int estiloAtual() const;  // índice na tabela de estilos (0..3 eras, 4 perigo)
    void somDeAmbiente();     // sorteia o próximo som do lugar e quando ele vem
    void efeitoAmbiente(float freq, float dur, float amp, int forma, float decai, float atraso, float deslize, float corte, float pan);
    void proximaBatida();
    void nota(int midi, float duracao, float amp, int forma, float ataque, float decaimento, float pan, float vibrato = 0.0f);
    float onda(Voz& v);
    static float freqMidi(float midi);

    int taxa_;
    std::mt19937 rng_;
    std::vector<Voz> vozes_;
    float era_ = 0.0f, noite_ = 0.0f, tensao_ = 0.0f;
    float volMusica_ = 0.55f, volEfeitos_ = 0.8f;
    double ateBatida_ = 0.0;  // amostras até a próxima batida
    int batida_ = 0, acorde_ = 0, grauMelodia_ = 4;
    float faseVento_ = 0.0f, ventoFiltro_ = 0.0f;
    Ambiente ambiente_ = Ambiente::Nenhum;
    double ateAmbiente_ = 0.0;  // segundos até o próximo som do lugar
    std::map<std::string, int> contagem_;
};

}
