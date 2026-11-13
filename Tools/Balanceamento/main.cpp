// Simulação de balanceamento: um "jogador automático" joga N dias com a lógica real do jogo
// e mede quando o reino chega a cada era. Uso: Balanceamento <pasta Data> [dias] [coletas por dia] [saída .md]
// As suposições sobre o jogador (quanto coleta por dia, o que constrói primeiro) estão no topo deste arquivo.
#include <algorithm>
#include <cstdio>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "Game/Exploration/Components.h"
#include "Game/World/World.h"

using namespace koa;

namespace {

// Ordem de obras que um jogador razoável seguiria
const char* PRIORIDADE[] = {"campo", "taverna", "moradia", "mercado", "capela", "forja", "posto_guarda", "torre"};

bool posicionarJunto(World& w, entt::entity e) {
    const glm::vec2 p = w.reg.get<Posicao>(e).v;
    const auto obst = w.obstaculos();
    for (const glm::vec2 d : {glm::vec2{0, 12}, {0, -12}, {-12, -1}, {12, -1}}) {
        Jogador t = w.jogador;
        t.pos = p + d;
        if (ke::colide(w.mapa, t.caixa(), obst)) continue;
        w.jogador.pos = t.pos;
        const glm::vec2 v = p - t.pos;
        w.jogador.dir = std::abs(v.x) > std::abs(v.y) ? (v.x < 0 ? Direcao::Esquerda : Direcao::Direita) : (v.y < 0 ? Direcao::Cima : Direcao::Baixo);
        return true;
    }
    return false;
}

// Um alvo de coleta: o mais perto do que falta (madeira, pedra ou ouro de baús)
bool coletarUm(World& w, const std::string& recurso) {
    entt::entity melhor = entt::null;
    float dist = 1e9f;
    for (auto [e, p, c] : w.reg.view<Posicao, Coletavel>().each())
        if (!c.esgotado && c.recurso == recurso && glm::distance(p.v, w.jogador.pos) < dist) dist = glm::distance(p.v, w.jogador.pos), melhor = e;
    if (recurso == "ouro")
        for (auto [e, p, s] : w.reg.view<Posicao, Saqueavel>().each())
            if (!s.aberto && glm::distance(p.v, w.jogador.pos) < dist) dist = glm::distance(p.v, w.jogador.pos), melhor = e;
    if (melhor == entt::null || !posicionarJunto(w, melhor)) return false;
    for (int golpe = 0; golpe < 6 && w.reg.valid(melhor); ++golpe) {
        w.passo(1.0f / 60.0f, {{0, 0}, true, false});
        for (int i = 0; i < 22; ++i) w.passo(1.0f / 60.0f, {});
        const auto* c = w.reg.try_get<Coletavel>(melhor);
        const auto* s = w.reg.try_get<Saqueavel>(melhor);
        if ((c && c->esgotado) || (s && s->aberto)) break;
    }
    w.tirarEventos();
    return true;
}

glm::ivec2 lugarLivre(const World& w, const std::string& def) {
    const glm::ivec2 perto{32, 27};
    for (int raio = 0; raio < 32; ++raio)
        for (int dy = -raio; dy <= raio; ++dy)
            for (int dx = -raio; dx <= raio; ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == raio && w.podeConstruir(def, perto + glm::ivec2(dx, dy))) return perto + glm::ivec2(dx, dy);
    return {-1, -1};
}

// Gasta recursos: casa primeiro, depois obras novas (por prioridade), depois a melhoria mais barata
int construirEMelhorar(World& w, const GameData& dados) {
    int feitas = 0;
    for (int tentativa = 0; tentativa < 6; ++tentativa) {
        bool fez = false;
        BuildingData* casa = w.reino.construcao("casa_herdeiro");
        if (casa->level == 1 && w.melhorarConstrucao(casa->id)) fez = true;
        const ResumoReino r = w.resumo();
        for (const char* d : PRIORIDADE) {
            if (fez) break;
            const BuildingDef& def = dados.construcoes.at(d);
            const int tem = w.contarConstrucoes(d);
            const bool precisa = (std::string(d) == "moradia" && w.estado.populacao >= r.populacaoMax - 1) ||
                                 (std::string(d) == "campo" && r.comidaProduzida - r.comidaConsumida < 2) || tem == 0;
            if (!precisa || (def.limite > 0 && tem >= def.limite) || !w.inventario.tem(def.nivel(1)->custo)) continue;
            const glm::ivec2 t = lugarLivre(w, d);
            if (t.x >= 0 && !w.construir(d, t).empty()) fez = true;
        }
        if (!fez) {
            // Melhoria mais barata que dê para pagar
            BuildingData* alvo = nullptr;
            int menor = 1 << 30;
            for (auto& b : w.reino.construcoes) {
                const BuildingDef* d = w.def(b);
                const NivelConstrucao* prox = d ? d->nivel(b.level + 1) : nullptr;
                if (!b.constructed || !prox || !w.inventario.tem(prox->custo)) continue;
                int total = 0;
                for (const auto& [rid, q] : prox->custo) total += q;
                if (total < menor) menor = total, alvo = &b;
            }
            if (alvo && w.melhorarConstrucao(alvo->id)) fez = true;
        }
        if (!fez) break;
        ++feitas;
    }
    w.tirarEventos();
    return feitas;
}

}

int main(int argc, char** argv) {
    const std::string pasta = argc > 1 ? argv[1] : "Data";
    const int dias = argc > 2 ? std::stoi(argv[2]) : 60;
    const int coletasPorDia = argc > 3 ? std::stoi(argv[3]) : 25;
    const int diaChefe = argc > 4 ? std::stoi(argv[4]) : 25;  // quando o jogador derrota o Cavaleiro Caído (0 = nunca)
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

    std::string md;
    md += std::format("| Dia | Era | Desenv. | Povo | Feliz. | Ouro | Madeira | Pedra | Comida | Obras |\n");
    md += "|---|---|---|---|---|---|---|---|---|---|\n";
    int diaEra[4] = {1, -1, -1, -1};
    int obrasTotal = 0;
    for (int dia = 1; dia <= dias; ++dia) {
        w.relogio.minutos = 7 * 60;
        if (diaChefe > 0 && dia == diaChefe) {  // a luta em si é testada em outro lugar; aqui só o efeito
            w.marcos.insert("chefe_cavaleiro_caido");
            w.reino.magiaLiberta = true;
        }
        // Coleta: o que estiver mais em falta primeiro (madeira, depois pedra, depois ouro de baús)
        for (int i = 0; i < coletasPorDia; ++i) {
            const int m = w.inventario.quantidade("madeira"), p = w.inventario.quantidade("pedra");
            const std::string rec = m <= p * 2 ? "madeira" : "pedra";
            if (!coletarUm(w, rec) && !coletarUm(w, rec == "madeira" ? "pedra" : "madeira")) break;
        }
        coletarUm(w, "ouro");
        // Impostos: baixa quando o povo está desanimado
        w.estado.imposto = w.estado.felicidade < 50.0f ? Imposto::Baixo : Imposto::Medio;
        // Ouro sobrando vira pedra no mercado (até 5 lotes por dia, guardando 80 de reserva)
        for (int lote = 0; lote < 5 && w.precoLote("pedra") > 0 && w.inventario.quantidade("ouro") > 80 + w.precoLote("pedra"); ++lote)
            w.comprarLote("pedra");
        obrasTotal += construirEMelhorar(w, dados);
        w.dormir(false);
        w.reino.eraVisual = static_cast<float>(w.reino.eraAlvo());  // a simulação não espera a animação
        w.tirarEventos();
        const int era = static_cast<int>(w.reino.eraAlvo());
        for (int e = 1; e <= era; ++e)
            if (diaEra[e] < 0) diaEra[e] = dia;
        if (dia % 5 == 0 || dia <= 3)
            md += std::format("| {} | {} | {} | {}/{} | {:.0f} | {} | {} | {} | {} | {} |\n", dia, era, w.reino.desenvolvimento(), w.estado.populacao,
                              w.resumo().populacaoMax, w.estado.felicidade, w.inventario.quantidade("ouro"), w.inventario.quantidade("madeira"),
                              w.inventario.quantidade("pedra"), w.inventario.quantidade("comida"), obrasTotal);
    }
    std::string resumo = std::format("Coletas por dia: {} | Chefe derrotado no dia: {} | Dias simulados: {}\n\n", coletasPorDia,
                                     diaChefe > 0 ? std::to_string(diaChefe) : "nunca", dias);
    const char* nomes[] = {"Reino das Cinzas", "O Despertar da Coroa", "Reino Renascido", "Reino da Alta Fantasia"};
    for (int e = 1; e < 4; ++e)
        resumo += diaEra[e] > 0 ? std::format("- {}: dia {}\n", nomes[e], diaEra[e]) : std::format("- {}: não alcançado\n", nomes[e]);
    std::printf("%s\n%s", resumo.c_str(), md.c_str());
    if (!saida.empty()) std::ofstream(saida, std::ios::app) << "\n### " << resumo << "\n" << md;
    return 0;
}
