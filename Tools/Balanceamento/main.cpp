// Simulação de balanceamento: o jogador automático (JogadorAutomatico.h) joga N dias com a lógica real do jogo
// e mede o reino dia a dia. Uso: Balanceamento <pasta Data> [dias] [coletas por dia] [dia do chefe] [saída .md]
// Fase 17: joga também com o que veio depois da fase 8 (conselho, hordas, guilda, horta, castelo) e aponta alarmes
#include <cstdio>
#include <format>
#include <fstream>
#include <string>

#include "Tools/Balanceamento/JogadorAutomatico.h"

using namespace koa;
using namespace koa::balanceamento;

int main(int argc, char** argv) {
    const std::string pasta = argc > 1 ? argv[1] : "Data";
    const int dias = argc > 2 ? std::stoi(argv[2]) : 120;
    Ritmo ritmo;
    ritmo.coletasPorDia = argc > 3 ? std::stoi(argv[3]) : 25;
    ritmo.diaChefe = argc > 4 ? std::stoi(argv[4]) : 25;
    const std::string saida = argc > 5 ? argv[5] : "";

    GameData dados;
    std::string erro;
    if (!dados.carregar(pasta, &erro)) {
        std::fprintf(stderr, "dados: %s\n", erro.c_str());
        return 1;
    }
    World w;
    if (!w.carregar(dados, pasta, &erro)) {
        std::fprintf(stderr, "mundo: %s\n", erro.c_str());
        return 1;
    }
    JogadorAutomatico jogador(w, dados, ritmo);

    std::string md = "| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida (dias) | Estragou | Obras | Castelo/alas | Exped. | Hordas/saques |\n";
    md += "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
    int diaEra[4] = {1, -1, -1, -1};
    for (int d = 1; d <= dias; ++d) {
        const Amostra a = jogador.jogarDia();
        for (int e = 1; e <= a.era; ++e)
            if (diaEra[e] < 0) diaEra[e] = a.dia;
        if (a.dia % 10 == 0 || a.dia <= 3 || a.dia == 5)
            md += std::format("| {} | {} | {} | {}/{} | {:.0f} | {} | {} | {} | {} ({:.1f}) | {} | {} | {}/{} | {} | {}/{} |\n", a.dia, a.era,
                              a.desenvolvimento, a.populacao, a.populacaoMax, a.felicidade, a.ouro, a.madeira, a.pedra, a.comida,
                              a.diasDeComida, a.estragou, a.obras, a.nivelCastelo, a.alas, a.expedicoes, a.hordas, a.saques);
    }
    std::string resumo = std::format("Coletas por dia: {} | Chefe derrotado no dia: {} | Dias simulados: {}\n\n", ritmo.coletasPorDia,
                                     ritmo.diaChefe > 0 ? std::to_string(ritmo.diaChefe) : "nunca", dias);
    const char* nomes[] = {"Reino das Cinzas", "O Despertar da Coroa", "Reino Renascido", "Reino da Alta Fantasia"};
    for (int e = 1; e < 4; ++e)
        resumo += diaEra[e] > 0 ? std::format("- {}: dia {}\n", nomes[e], diaEra[e]) : std::format("- {}: não alcançado\n", nomes[e]);
    if (!jogador.historico().empty())
        resumo += std::format("- Desejos realizados: {} (dia 60: {}); desistiram de esperar: {}\n", jogador.historico().back().desejos,
                              jogador.historico().size() >= 60 ? jogador.historico()[59].desejos : 0, jogador.historico().back().partiram);
    if (!jogador.historico().empty()) {
        const Amostra& f = jogador.historico().back();
        resumo += std::format("- Vizinhos no fim: anões {:+.0f}, Ordem {:+.0f}, Culto {:+.0f}; tratados em vigor: {}; fama: justiça {:+.0f}, generosidade {:+.0f}, devoção {:+.0f}\n", f.anoes, f.ordem, f.culto, f.tratados, f.fama.justica, f.fama.generosidade, f.fama.devocao);
        resumo += std::format("- Intriga: desvios descobertos {}; ouro ainda sumido {}\n", f.descobertos, f.sumido);
        // Fase 20: a comida que sobra tem destino (o preço da comida no dia 60 e no fim mostra que ela segue valendo)
        const float comida60 = jogador.historico().size() >= 60 ? jogador.historico()[59].precoComida : 0.0f;
        resumo += std::format("- Mundo vivo: {} de {} aldeias de pé no fim; {} ajudas dadas; no máximo {} notícias num relatório\n", f.aldeiasVivas,
                              w.mundoVivo.aldeias.size(), f.ajudas, f.noticiasMax);
        resumo += std::format("- Guarda Real: {} esquadra(s) no fim; {} de ouro de soldo em 120 dias\n", f.esquadras, f.soldoPago);
        resumo += std::format("- Mercado: {} de comida vendida; {} de ouro em vendas e {} em compras; comida a {:.2f} a unidade no dia 60 e {:.2f} no fim\n",
                              f.comidaVendida, f.ouroVendas, f.ouroCompras, comida60, f.precoComida);
    }
    const auto al = alarmes(jogador.historico(), ritmo);
    resumo += al.empty() ? "- Alarmes: nenhum\n" : std::format("- Alarmes: {}\n", al.size());
    for (size_t i = 0; i < al.size() && i < 6; ++i) resumo += "  - " + al[i] + "\n";
    if (al.size() > 6) resumo += std::format("  - (e mais {})\n", al.size() - 6);
    std::printf("%s\n%s", resumo.c_str(), md.c_str());
    if (!saida.empty()) std::ofstream(saida, std::ios::app) << "\n### " << resumo << "\n" << md;
    return 0;
}
