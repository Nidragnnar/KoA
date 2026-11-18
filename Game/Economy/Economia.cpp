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

ResumoReino calcularResumo(const Kingdom& k, const GameData& dados, const EstadoReino& e, const ModificadorDia& mod) {
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
    r.seguranca = std::max(0, static_cast<int>(seguranca) + mod.seguranca);
    r.comidaProduzida = static_cast<int>(std::round(comida * mod.comida));
    r.comidaConsumida = static_cast<int>(std::ceil(e.populacao * mod.consumo));  // cada morador come 1 por dia
    static const int porMorador[] = {1, 2, 3};
    r.impostos = static_cast<int>(std::round(e.populacao * porMorador[static_cast<int>(e.imposto)] * mod.impostos));
    r.ouroProducao = static_cast<int>(std::round(ouro * mod.ouro)) + mod.ouroDia;  // pode ficar negativo (uma patrulha custa)
    r.madeiraDia = static_cast<int>(madeira) + mod.madeiraDia;
    r.pedraDia = static_cast<int>(pedra) + mod.pedraDia;
    // Felicidade: base sombria + construções + impostos + proteção
    static const float porImposto[] = {10.0f, 0.0f, -12.0f};
    // Fase 17: as obras alegram cada vez menos (a soma se aproxima de um teto em vez de crescer sem fim)
    const float dasObras = felicidade > 0.0f ? FELICIDADE_DAS_OBRAS * (1.0f - std::exp(-felicidade / FELICIDADE_DAS_OBRAS)) : felicidade;
    float alvo = 35.0f + dasObras + porImposto[static_cast<int>(e.imposto)] + mod.felicidade;
    if (r.seguranca >= e.populacao) alvo += 5.0f;  // povo protegido dorme tranquilo
    // Fase 17: vilarejo cheio demais reclama (segura o crescimento sem fim do povo)
    r.aglomeracao = std::max(0, e.populacao - POVO_CONFORTAVEL) * FELICIDADE_POR_MORADOR;
    alvo -= r.aglomeracao;
    r.capacidadeCeleiro = static_cast<int>(std::ceil(std::max(r.comidaConsumida, 8) * mod.diasCeleiro));
    r.felicidadeAlvo = std::clamp(alvo, 0.0f, 100.0f);
    return r;
}

RelatorioDia simularDia(const Kingdom& k, const GameData& dados, EstadoReino& e, Inventory& inv, const ModificadorDia& mod) {
    RelatorioDia rel;
    rel.populacaoAntes = e.populacao;
    rel.felicidadeAntes = e.felicidade;
    const ResumoReino r = calcularResumo(k, dados, e, mod);

    // Ouro, madeira e pedra
    const int ouro = std::max(r.impostos + r.ouroProducao, -inv.quantidade("ouro"));  // nunca deixa o cofre negativo
    if (ouro >= 0) inv.adicionar("ouro", ouro);
    else inv.remover("ouro", -ouro);
    rel.variacoes.push_back({"ouro", ouro});
    if (r.madeiraDia) inv.adicionar("madeira", r.madeiraDia), rel.variacoes.push_back({"madeira", r.madeiraDia});
    if (r.pedraDia) inv.adicionar("pedra", r.pedraDia), rel.variacoes.push_back({"pedra", r.pedraDia});

    // Comida: produz, o povo come; faltando, alguém vai embora
    const int estoque = inv.quantidade("comida") + r.comidaProduzida;
    const bool fome = estoque < r.comidaConsumida;
    inv.definir("comida", std::max(0, estoque - r.comidaConsumida));
    rel.variacoes.push_back({"comida", r.comidaProduzida - r.comidaConsumida});
    // Fase 17: o que não cabe no celeiro estraga aos poucos (a comida volta a ter valor no fim do jogo)
    if (const int sobra = inv.quantidade("comida") - r.capacidadeCeleiro; sobra > 0) {
        const int perde = static_cast<int>(std::ceil(sobra * ESTRAGA_POR_NOITE));
        inv.remover("comida", perde);
        rel.variacoes.push_back({"comida|estragou", -perde});
        rel.notas.push_back(std::format("O celeiro não dá conta: {} de comida estragou.", perde));
    }
    if (r.aglomeracao >= 3.0f) rel.notas.push_back(std::format("O vilarejo está apertado: o povo reclama (-{:.0f} de felicidade).", r.aglomeracao));

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
        const int chegam = std::min(r.populacaoMax - e.populacao, (e.felicidade >= 75.0f ? 2 : 1) + mod.viajantes);
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
