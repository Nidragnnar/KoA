#pragma once
// Fase 17: o jogador automático do balanceamento, usado pela ferramenta (Tools/Balanceamento) e por um teste.
// Joga com a lógica real do jogo: coleta, constrói, cuida da horta, nomeia o conselho, decide os casos, repara o
// portão, manda expedições, reconstrói o castelo, ergue alas e assina os tratados que não mexem com o Culto. Não luta (a horda é resolvida pelas defesas).
#include <string>
#include <vector>

#include "Game/World/World.h"

namespace koa::balanceamento {

struct Ritmo {
    int coletasPorDia = 25;
    int diaChefe = 25;  // quando o Cavaleiro Caído cai (0 = nunca)
};

// Como o reino está ao fim de um dia
struct Amostra {
    int dia = 0, era = 0, desenvolvimento = 0, populacao = 0, populacaoMax = 0;
    float felicidade = 0.0f;
    int ouro = 0, madeira = 0, pedra = 0, comida = 0, cristal = 0;
    float diasDeComida = 0.0f;  // estoque dividido pelo consumo de um dia
    int obras = 0, nivelCastelo = 0, alas = 0, expedicoes = 0, hordas = 0, saques = 0, estragou = 0;
    int desejos = 0, partiram = 0;  // fase 18: desejos realizados e gente que desistiu de esperar (desde o começo)
    // Fase 19: relação com cada povo, a fama, tratados em vigor e a intriga (desvios descobertos, ouro ainda sumido)
    float anoes = 0.0f, ordem = 0.0f, culto = 0.0f;
    Reputacao fama;
    int tratados = 0, descobertos = 0, sumido = 0;
    // Fase 20: o mercado (comida vendida pelo jogador automático, ouro recebido e pago desde o começo, preço da comida hoje)
    int comidaVendida = 0, ouroVendas = 0, ouroCompras = 0;
    float precoComida = 0.0f;
    // Mundo vivo: aldeias de pé, ajudas dadas e o máximo de notícias num relatório
    int aldeiasVivas = 0, ajudas = 0, noticiasMax = 0;
};

class JogadorAutomatico {
public:
    JogadorAutomatico(World& w, const GameData& dados, Ritmo r) : w_(w), dados_(dados), ritmo_(r) {}
    // Joga um dia inteiro (da manhã até dormir) e devolve como o reino amanheceu
    Amostra jogarDia();
    const std::vector<Amostra>& historico() const { return historico_; }

private:
    void coletar();
    void cuidarDaHorta();
    void governar();
    void construirEMelhorar();
    void guilda();
    void castelo();

    World& w_;
    const GameData& dados_;
    Ritmo ritmo_;
    int obras_ = 0, expedicoes_ = 0, comidaVendida_ = 0, ajudas_ = 0, noticiasMax_ = 0;
    std::vector<Amostra> historico_;
};

// Faixas que o balanceamento precisa respeitar (o que sair delas vira alarme no relatório e falha no teste)
std::vector<std::string> alarmes(const std::vector<Amostra>& h, const Ritmo& r);

}
