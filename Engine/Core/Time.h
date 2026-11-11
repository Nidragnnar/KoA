#pragma once

namespace ke {

// Passo fixo com acumulador: a lógica roda a 60 Hz qualquer que seja o FPS
class FixedTimestep {
public:
    explicit FixedTimestep(double passo = 1.0 / 60.0, int maxPassos = 5) : passo_(passo), maxPassos_(maxPassos) {}

    // Soma o tempo real decorrido e devolve quantos passos de lógica rodar agora
    int avancar(double dtReal);
    // Fração do próximo passo (para interpolar a renderização)
    double alfa() const { return acumulado_ / passo_; }
    double passo() const { return passo_; }

private:
    double passo_;
    int maxPassos_;
    double acumulado_ = 0.0;
};

}
