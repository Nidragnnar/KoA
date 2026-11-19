#include "Tools/Balanceamento/JogadorAutomatico.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <map>
#include <set>

#include <glm/geometric.hpp>

#include "Game/Exploration/Components.h"

namespace koa::balanceamento {

namespace {

// Ordem de obras que um jogador razoável seguiria (o que ainda não está liberado simplesmente não acontece)
const char* PRIORIDADE[] = {"campo",   "taverna", "moradia", "mercado",    "capela", "forja",  "posto_guarda", "torre",
                            "galinheiro", "curral", "guilda", "memorial", "jardim", "estatua"};

bool posicionarJunto(World& w, entt::entity e) {
    const glm::vec2 p = w.reg.get<Posicao>(e).v;
    const auto& obst = w.obstaculos();
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
        EntradaMundo in;
        in.interagir = true;
        w.passo(1.0f / 60.0f, in);
        for (int i = 0; i < 22; ++i) w.passo(1.0f / 60.0f, {});
        const auto* c = w.reg.try_get<Coletavel>(melhor);
        const auto* s = w.reg.try_get<Saqueavel>(melhor);
        if ((c && c->esgotado) || (s && s->aberto)) break;
    }
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

}

void JogadorAutomatico::coletar() {
    for (int i = 0; i < ritmo_.coletasPorDia; ++i) {
        const int m = w_.inventario.quantidade("madeira"), p = w_.inventario.quantidade("pedra");
        const std::string rec = m <= p * 2 ? "madeira" : "pedra";
        if (!coletarUm(w_, rec) && !coletarUm(w_, rec == "madeira" ? "pedra" : "madeira")) break;
    }
    coletarUm(w_, "ouro");
}

// Colhe, planta e rega cada canteiro; compra sementes do Corvin quando ele passa e sobra ouro
void JogadorAutomatico::cuidarDaHorta() {
    if (w_.inventario.quantidade("ouro") > 150)
        for (const auto& it : dados_.npcs.at("corvin").loja)
            for (const auto& [item, q] : it.ganho)
                if (item.starts_with("semente_") && w_.inventario.quantidade(item) < 2) w_.comprar("corvin", it.id);
    for (int i = 0; i < static_cast<int>(w_.canteiros.size()); ++i)
        for (int k = 0; k < 3 && w_.usarCanteiro(i); ++k) {
        }
}

// Impostos, conselho (nomeia quem estiver livre, segue a maioria nos casos) e o portão
void JogadorAutomatico::governar() {
    w_.estado.imposto = w_.estado.felicidade < 50.0f ? Imposto::Baixo : Imposto::Medio;
    if (w_.conselhoFormado()) {
        std::set<std::string> ocupados;
        for (const CargoDef& c : dados_.conselho.cargos)
            if (!w_.ocupante(c.id).empty()) ocupados.insert(w_.ocupante(c.id));
        for (const CargoDef& c : dados_.conselho.cargos) {
            if (!w_.ocupante(c.id).empty()) continue;
            for (const auto& npc : w_.candidatos())
                if (!ocupados.contains(npc) && w_.nomear(c.id, npc)) {
                    ocupados.insert(npc);
                    break;
                }
        }
    }
    if (const DecisaoDef* d = w_.decisaoAtual(); d && !d->opcoes.empty()) {
        std::map<std::string, int> votos;
        for (const auto& [cargo, apoio] : d->conselho)
            if (!w_.ocupante(cargo).empty()) ++votos[apoio.first];
        std::string escolha = d->opcoes.front().id;
        int mais = 0;
        for (const auto& [op, n] : votos)
            if (n > mais) mais = n, escolha = op;
        w_.decidir(escolha);
    }
    if (const Custo c = w_.custoReparo(); !c.empty() && w_.inventario.tem(c)) w_.repararPortao();
    // Tratados com os anões e a Ordem quando sobra ouro (o do Culto custa felicidade: o jogador médio evita)
    for (const TratadoDef& t : dados_.faccoes.tratados) {
        const auto ouro = t.custo.find("ouro");
        if (t.faccao != "culto" && w_.podeAssinar(t.id).empty() &&
            w_.inventario.quantidade("ouro") >= (ouro == t.custo.end() ? 0 : ouro->second) + 150)
            w_.assinarTratado(t.id);
    }
}

// Casa primeiro, depois obras novas (por prioridade), depois a melhoria mais barata (castelo incluído)
void JogadorAutomatico::construirEMelhorar() {
    // Fase 20: a comida que sobra além de 8 dias vai para o mercado (antes ela só estragava); depois compra pedra
    const int consumo = std::max(1, w_.resumo().comidaConsumida);
    for (int lote = 0; lote < 4 && w_.inventario.quantidade("comida") > consumo * 8 + 10 && w_.podeNegociar("comida", false).empty(); ++lote)
        if (w_.venderNoMercado("comida")) comidaVendida_ += 10;
    for (int lote = 0; lote < 5 && w_.podeNegociar("pedra", true).empty() && w_.inventario.quantidade("ouro") > 80 + w_.precoCompraLote("pedra"); ++lote)
        w_.comprarNoMercado("pedra");
    for (int tentativa = 0; tentativa < 6; ++tentativa) {
        bool fez = false;
        BuildingData* casa = w_.reino.construcao("casa_herdeiro");
        if (casa && casa->level == 1 && w_.melhorarConstrucao(casa->id)) fez = true;
        const ResumoReino r = w_.resumo();
        for (const char* d : PRIORIDADE) {
            if (fez) break;
            const BuildingDef& def = dados_.construcoes.at(d);
            const int tem = w_.contarConstrucoes(d);
            const bool precisa = (std::string(d) == "moradia" && w_.estado.populacao >= r.populacaoMax - 1) ||
                                 (std::string(d) == "campo" && (r.comidaProduzida - r.comidaConsumida < 2 ||
                                                                  (w_.inventario.quantidade("comida") < 2 * r.comidaConsumida && r.comidaProduzida - r.comidaConsumida < 5))) ||  // reserva mínima
                                 tem == 0;
            if (!precisa || (def.limite > 0 && tem >= def.limite) || !w_.monumentoLiberado(def) || !w_.inventario.tem(def.nivel(1)->custo)) continue;
            const glm::ivec2 t = lugarLivre(w_, d);
            if (t.x >= 0 && !w_.construir(d, t).empty()) fez = true;
        }
        if (!fez) {
            BuildingData* alvo = nullptr;
            int menor = 1 << 30;
            for (auto& b : w_.reino.construcoes) {
                const BuildingDef* d = w_.def(b);
                const NivelConstrucao* prox = d ? d->nivel(b.level + 1) : nullptr;
                if (!b.constructed || !prox || !w_.inventario.tem(prox->custo) || !w_.requisitoProximoNivel(b).empty()) continue;
                int total = 0;
                for (const auto& [rid, q] : prox->custo) total += q;
                if (total < menor) menor = total, alvo = &b;
            }
            if (alvo && w_.melhorarConstrucao(alvo->id)) fez = true;
        }
        if (!fez) break;
        ++obras_;
    }
}

// Uma expedição por vez, com os dois primeiros aventureiros prontos, ao destino mais seguro liberado
void JogadorAutomatico::guilda() {
    if (!w_.expedicao.destino.empty()) return;
    std::vector<std::string> prontos;
    for (const auto& [id, a] : w_.aventureiros)
        if (w_.aventureiroPronto(id)) prontos.push_back(id);
    if (prontos.size() < 2) return;
    prontos.resize(2);
    for (const DestinoDef& d : dados_.guilda.destinos)
        if ((d.requer.empty() || w_.marcos.contains(d.requer)) && w_.partir(d.id, prontos)) {
            ++expedicoes_;
            return;
        }
}

void JogadorAutomatico::castelo() {
    if (static_cast<int>(w_.alas.size()) >= w_.vagasAlas()) return;
    for (const AlaDef& a : dados_.castelo.alas)
        if (std::ranges::find(w_.alas, a.id) == w_.alas.end() && w_.inventario.tem(a.custo) && w_.erguerAla(a.id)) return;
}

Amostra JogadorAutomatico::jogarDia() {
    const int dia = w_.relogio.dia;
    w_.relogio.minutos = 7 * 60;
    if (ritmo_.diaChefe > 0 && dia == ritmo_.diaChefe) {  // a luta em si é testada em outro lugar; aqui só o efeito
        w_.marcos.insert("chefe_cavaleiro_caido");
        w_.reino.magiaLiberta = true;
    }
    coletar();
    cuidarDaHorta();
    governar();
    construirEMelhorar();
    guilda();
    castelo();
    w_.tirarEventos();
    const int hordasAntes = w_.hordaNumero;
    w_.dormir(false);
    w_.reino.eraVisual = static_cast<float>(w_.reino.eraAlvo());  // a simulação não espera a animação
    Amostra a;
    for (const EventoJogo& e : w_.tirarEventos()) a.saques += e.tipo == EventoJogo::Tipo::Saque;
    a.hordas = (historico_.empty() ? 0 : historico_.back().hordas) + (w_.hordaNumero > hordasAntes ? 1 : 0);
    a.saques += historico_.empty() ? 0 : historico_.back().saques;
    for (const auto& [chave, v] : w_.ultimoRelatorio.variacoes)
        if (chave == "comida|estragou") a.estragou = -v;
    const ResumoReino r = w_.resumo();
    a.dia = dia;
    a.era = static_cast<int>(w_.reino.eraAlvo());
    a.desenvolvimento = w_.reino.desenvolvimento();
    a.populacao = w_.estado.populacao;
    a.populacaoMax = r.populacaoMax;
    a.felicidade = w_.estado.felicidade;
    a.ouro = w_.inventario.quantidade("ouro");
    a.madeira = w_.inventario.quantidade("madeira");
    a.pedra = w_.inventario.quantidade("pedra");
    a.comida = w_.inventario.quantidade("comida");
    a.cristal = w_.inventario.quantidade("cristal");
    a.diasDeComida = r.comidaConsumida > 0 ? static_cast<float>(a.comida) / r.comidaConsumida : 0.0f;
    a.obras = obras_;
    a.nivelCastelo = w_.nivelCastelo();
    a.alas = static_cast<int>(w_.alas.size());
    a.expedicoes = expedicoes_;
    for (const auto& m : w_.marcos) a.desejos += m.starts_with("desejo_"), a.partiram += m.starts_with("partiu:"), a.descobertos += m.starts_with("desvio:");
    a.anoes = w_.relacaoCom("anoes");
    a.ordem = w_.relacaoCom("ordem");
    a.culto = w_.relacaoCom("culto");
    a.fama = w_.reputacao;
    a.tratados = static_cast<int>(w_.tratados.size());
    a.comidaVendida = comidaVendida_;
    a.ouroVendas = w_.mercado.vendido;
    a.ouroCompras = w_.mercado.comprado;
    a.precoComida = w_.precoHoje("comida");
    for (const auto& [npc, q] : w_.desvios) a.sumido += q;
    historico_.push_back(a);
    return a;
}

std::vector<std::string> alarmes(const std::vector<Amostra>& h, const Ritmo& r) {
    std::vector<std::string> v;
    int diaEra2 = -1;
    for (const Amostra& a : h) {
        if (a.era >= 2 && diaEra2 < 0) diaEra2 = a.dia;
        for (const int q : {a.ouro, a.madeira, a.pedra, a.comida, a.cristal})
            if (q < 0) v.push_back(std::format("dia {}: recurso negativo", a.dia));
        // A comida tem de pesar: do dia 10 em diante, entre 1 e 20 dias de consumo guardados (sem fome e sem montanha)
        if (a.dia >= 10 && (a.diasDeComida < 1.0f || a.diasDeComida > 20.0f))
            v.push_back(std::format("dia {}: {:.1f} dias de comida guardados (fora de 1 a 20)", a.dia, a.diasDeComida));
        if (a.dia >= 20 && a.felicidade < 40.0f) v.push_back(std::format("dia {}: felicidade {:.0f} (abaixo de 40)", a.dia, a.felicidade));
    }
    // No ritmo médio (25 coletas), o Reino Renascido chega entre os dias 10 e 22
    if (r.coletasPorDia == 25 && !h.empty() && h.back().dia >= 22 && (diaEra2 < 10 || diaEra2 > 22))
        v.push_back(std::format("Reino Renascido no dia {} (fora de 10 a 22)", diaEra2));
    return v;
}

}
