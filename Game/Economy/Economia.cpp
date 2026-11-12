#include "Game/Economy/Economia.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace koa {

const char* nomeImposto(Imposto i) {
    switch (i) {
    case Imposto::Baixo: return "Baixo";
    case Imposto::Medio: return "Médio";
    case Imposto::Alto: return "Alto";
    }
    return "?";
}

ResumoReino calcularResumo(const Kingdom& k, const GameData& dados, const EstadoReino& e) {
    ResumoReino r;
    float felicidade = 0.0f, comida = 0.0f, ouro = 0.0f, madeira = 0.0f, pedra = 0.0f, seguranca = 0.0f, pop = 0.0f;
    for (const BuildingData& b : k.construcoes) {
        if (!b.constructed) continue;  // obra em andamento ainda não rende nada
        const auto it = dados.construcoes.find(b.defId);
        const NivelConstrucao* nv = it == dados.construcoes.end() ? nullptr : it->second.nivel(b.level);
        if (!nv) continue;
        pop += nv->efeito("populacao_max");
        felicidade += nv->efeito("felicidade");
        seguranca += nv->efeito("seguranca");
        comida += nv->efeito("comida_dia");
        ouro += nv->efeito("ouro_dia");
        madeira += nv->efeito("madeira_dia");
        pedra += nv->efeito("pedra_dia");
    }
    r.populacaoMax = e.populacaoBase + static_cast<int>(pop);
    r.seguranca = static_cast<int>(seguranca);
    r.comidaProduzida = static_cast<int>(comida);
    r.comidaConsumida = e.populacao;  // cada morador come 1 por dia
    static const int porMorador[] = {1, 2, 3};
    r.impostos = e.populacao * porMorador[static_cast<int>(e.imposto)];
    r.ouroProducao = static_cast<int>(ouro);
    r.madeiraDia = static_cast<int>(madeira);
    r.pedraDia = static_cast<int>(pedra);
    // Felicidade: base sombria + construções + impostos + proteção
    static const float porImposto[] = {10.0f, 0.0f, -12.0f};
    float alvo = 35.0f + felicidade + porImposto[static_cast<int>(e.imposto)];
    if (r.seguranca >= e.populacao) alvo += 5.0f;  // povo protegido dorme tranquilo
    r.felicidadeAlvo = std::clamp(alvo, 0.0f, 100.0f);
    return r;
}

RelatorioDia simularDia(const Kingdom& k, const GameData& dados, EstadoReino& e, Inventory& inv) {
    RelatorioDia rel;
    rel.populacaoAntes = e.populacao;
    rel.felicidadeAntes = e.felicidade;
    const ResumoReino r = calcularResumo(k, dados, e);

    // Ouro, madeira e pedra
    const int ouro = r.impostos + r.ouroProducao;
    inv.adicionar("ouro", ouro);
    rel.variacoes.push_back({"ouro", ouro});
    if (r.madeiraDia) inv.adicionar("madeira", r.madeiraDia), rel.variacoes.push_back({"madeira", r.madeiraDia});
    if (r.pedraDia) inv.adicionar("pedra", r.pedraDia), rel.variacoes.push_back({"pedra", r.pedraDia});

    // Comida: produz, o povo come; faltando, alguém vai embora
    const int estoque = inv.quantidade("comida") + r.comidaProduzida;
    const bool fome = estoque < r.comidaConsumida;
    inv.definir("comida", std::max(0, estoque - r.comidaConsumida));
    rel.variacoes.push_back({"comida", r.comidaProduzida - r.comidaConsumida});

    float alvo = r.felicidadeAlvo;
    if (fome) alvo = std::max(0.0f, alvo - 25.0f);
    e.felicidade += (alvo - e.felicidade) * 0.4f;
    e.felicidade = std::clamp(e.felicidade, 0.0f, 100.0f);

    if (fome && e.populacao > 3) {
        --e.populacao;
        rel.notas.push_back("Faltou comida! Uma família partiu em busca de pão.");
    } else if (e.felicidade < 25.0f && e.populacao > 3) {
        --e.populacao;
        rel.notas.push_back("Descontentes, uma família deixou o vilarejo.");
    } else if (!fome && e.felicidade >= 50.0f && e.populacao < r.populacaoMax) {
        const int chegam = std::min(r.populacaoMax - e.populacao, e.felicidade >= 75.0f ? 2 : 1);
        e.populacao += chegam;
        rel.notas.push_back(chegam == 1 ? "Uma família chegou ao vilarejo." : std::format("{} famílias chegaram ao vilarejo.", chegam));
    } else if (!fome && e.felicidade >= 50.0f && e.populacao >= r.populacaoMax) {
        rel.notas.push_back("Viajantes querem ficar, mas não há onde morar. Construa moradias.");
    } else if (e.felicidade < 50.0f) {
        rel.notas.push_back("O povo ainda está desanimado: ninguém novo quer morar aqui.");
    }
    if (e.populacao > r.populacaoMax) e.populacao = std::max(3, r.populacaoMax);
    rel.populacaoDepois = e.populacao;
    rel.felicidadeDepois = e.felicidade;
    return rel;
}

}
