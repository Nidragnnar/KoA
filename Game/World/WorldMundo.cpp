// Parte do World: o mundo vivo longe do herdeiro (aldeias, caravanas, viajantes, a terra tomada que avança) (versão 1.14)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

void World::iniciarMundoVivo() {
    mundoVivo = {};
    for (const AldeiaDef& a : dados().mundo.aldeias) {
        EstadoAldeia e;
        e.povo = a.povo;
        e.prosperidade = a.base * 0.8f;
        e.proximaCaravana = a.caravana.aCada;
        mundoVivo.aldeias[a.id] = e;
    }
}

bool World::mundoVivoAtivo() const { return dados_ && (dados().mundo.requer.empty() || marcos.contains(dados().mundo.requer)); }

float World::corrupcaoPerto(const AldeiaDef& a) const {
    if (a.corrupcaoFixa >= 0.0f) return a.corrupcaoFixa;
    const auto it = estadoRegioes.find(a.perto);
    return it == estadoRegioes.end() ? 50.0f : it->second.corrupcao;
}

std::string World::textoPedido(const PedidoAberto& p) const {
    const DadosMundo& d = dados().mundo;
    if (!p.aldeia.empty()) {
        const AjudaDef* a = d.ajuda(p.def);
        const AldeiaDef* al = d.aldeia(p.aldeia);
        return a && al ? preencher(a->texto, {{"aldeia", al->nome}}) : p.def;
    }
    const ViajanteDef* v = d.viajante(p.def);
    return v ? v->texto : p.def;
}

Custo World::custoPedido(const PedidoAberto& p) const {
    const DadosMundo& d = dados().mundo;
    if (!p.aldeia.empty()) return d.ajuda(p.def) ? d.ajuda(p.def)->custo : Custo{};
    return d.viajante(p.def) ? d.viajante(p.def)->custo : Custo{};
}

std::string World::podeAtender(int indice) const {
    if (indice < 0 || indice >= static_cast<int>(mundoVivo.pedidos.size())) return "Pedido desconhecido.";
    if (!inventario.tem(custoPedido(mundoVivo.pedidos[indice]))) return "Faltam recursos.";
    return "";
}

bool World::atenderPedido(int indice) {
    const std::string falta = podeAtender(indice);
    if (!falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, falta});
        return false;
    }
    const DadosMundo& d = dados().mundo;
    const PedidoAberto p = mundoVivo.pedidos[indice];
    mundoVivo.pedidos.erase(mundoVivo.pedidos.begin() + indice);
    inventario.pagar(custoPedido(p));
    std::string texto;
    if (!p.aldeia.empty()) {
        const AjudaDef& a = *d.ajuda(p.def);
        const AldeiaDef& al = *d.aldeia(p.aldeia);
        EstadoAldeia& e = mundoVivo.aldeias[p.aldeia];
        e.prosperidade = std::min(100.0f, e.prosperidade + a.ajuda);
        ++e.gratidao;
        if (a.limpa > 0.0f)
            if (const auto it = estadoRegioes.find(al.perto); it != estadoRegioes.end()) it->second.corrupcao = std::max(0.0f, it->second.corrupcao - a.limpa);
        if (!al.faccao.empty()) mudarRelacao(al.faccao, 5.0f);
        texto = std::format("Valdren ajudou {}. Eles não vão esquecer.", al.nome);
        marcar("ajudou_aldeia:" + p.aldeia, al.nome);
        reagirAoMarco("ajudou_aldeia:" + p.aldeia);  // a fama conta toda vez (o marco só na primeira)
    } else {
        const ViajanteDef& v = *d.viajante(p.def);
        estado.felicidade = std::clamp(estado.felicidade + v.felicidade, 0.0f, 100.0f);
        if (v.generosidade != 0.0f) {
            reputacao.generosidade = std::clamp(reputacao.generosidade + v.generosidade, -100.0f, 100.0f);
            verificarFama();
        }
        guardaTreinada += v.soldados;
        for (const auto& [regiao, delta] : v.corrupcao)
            if (const auto it = estadoRegioes.find(regiao); it != estadoRegioes.end()) it->second.corrupcao = std::clamp(it->second.corrupcao + delta, 0.0f, 100.0f);
        texto = "Pedido atendido: " + v.texto;
    }
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0, texto});
    return true;
}

// Depois da virada do dia: a terra tomada avança, as aldeias vivem, as caravanas passam, os pedidos vencem e chegam viajantes.
// No relatório, só as notícias mais importantes (no máximo noticias_por_manha); as outras ficam no painel das Terras
void World::noiteDoMundo(std::vector<std::string>& notas) {
    mundoVivo.noticias.clear();
    if (!mundoVivoAtivo()) return;
    const DadosMundo& d = dados().mundo;
    std::vector<Noticia> n;
    // A terra tomada empurra as vizinhas e as aldeias perto
    for (const auto& [origem, vizinhas] : d.vizinhanca) {
        const auto it = estadoRegioes.find(origem);
        if (it == estadoRegioes.end() || it->second.corrupcao < regra::INVASAO) continue;
        for (const auto& v : vizinhas)
            if (const auto iv = estadoRegioes.find(v); iv != estadoRegioes.end())
                iv->second.corrupcao = std::min(100.0f, iv->second.corrupcao + d.avancoPressao);
        for (const AldeiaDef& a : d.aldeias)
            if (a.perto == origem) mundoVivo.aldeias[a.id].prosperidade = std::max(0.0f, mundoVivo.aldeias[a.id].prosperidade - d.avancoAldeia);
        if (marcar("avanco:" + origem, nomeRegiao(origem)))
            n.push_back({6, std::format("A terra tomada de {} começou a avançar: as terras e as aldeias ao redor sofrem a cada noite.", nomeRegiao(origem))});
    }
    // Pedidos que venceram sem resposta
    std::erase_if(mundoVivo.pedidos, [&](const PedidoAberto& p) {
        if (p.prazo >= relogio.dia) return false;
        if (const AldeiaDef* al = d.aldeia(p.aldeia)) {
            EstadoAldeia& e = mundoVivo.aldeias[p.aldeia];
            e.prosperidade = std::max(0.0f, e.prosperidade - d.abandono);
            e.povo = e.povo * 7 / 10;
            n.push_back({7, std::format("Ninguém ajudou {}: parte do povo foi embora.", al->nome)});
        }
        return true;
    });
    const int seguranca = resumo().seguranca;
    for (const AldeiaDef& a : d.aldeias) {
        EstadoAldeia& e = mundoVivo.aldeias[a.id];
        if (e.sumiu) continue;
        const float corr = corrupcaoPerto(a);
        noiteDaAldeia(d, e, a, corr, sorteMundo(sementeBase_, relogio.dia, a.id + "|povo"));
        // Sumiu: quem sobrou vem para Valdren, se houver casa
        if (e.povo < d.someAbaixo) {
            e.sumiu = true;
            const int vem = std::clamp(resumo().populacaoMax - estado.populacao, 0, e.povo);
            estado.populacao += vem;
            std::erase_if(mundoVivo.pedidos, [&](const PedidoAberto& p) { return p.aldeia == a.id; });
            if (!a.faccao.empty()) mudarRelacao(a.faccao, -5.0f);
            marcar("aldeia_sumiu:" + a.id, a.nome);
            n.push_back({9, vem > 0 ? std::format("{} foi abandonada. {} sobreviventes chegaram a Valdren.", a.nome, vem)
                                    : std::format("{} foi abandonada. Os sobreviventes seguiram para o sul.", a.nome)});
            continue;
        }
        // Tributo de quem foi ajudado
        if (e.gratidao >= 2 && relogio.dia - e.ultimoTributo >= 7) {
            e.ultimoTributo = relogio.dia;
            inventario.adicionar("ouro", d.tributo);
            ultimoRelatorio.variacoes.push_back({"ouro|" + a.nome, d.tributo});
            n.push_back({1, std::format("{} mandou {} de ouro em gratidão.", a.nome, d.tributo)});
        }
        // Pedido de ajuda (um por aldeia de cada vez)
        if (std::ranges::none_of(mundoVivo.pedidos, [&](const PedidoAberto& p) { return p.aldeia == a.id; }))
            if (const AjudaDef* aj = ajudaNecessaria(d, e, corr, sorteMundo(sementeBase_, relogio.dia, a.id + "|ajuda"))) {
                mundoVivo.pedidos.push_back({aj->id, a.id, relogio.dia + aj->prazo});
                n.push_back({8, preencher(aj->texto, {{"aldeia", a.nome}}) + std::format(" ({} dias; R: Terras)", aj->prazo)});
            }
        // A caravana: chega (preços caem, pedágio) ou é atacada na estrada (preços sobem)
        if (relogio.dia >= e.proximaCaravana) {
            e.proximaCaravana = relogio.dia + a.caravana.aCada;
            if (e.prosperidade < 20.0f) continue;  // aldeia pobre não manda caravana
            const bool atacada = sorteMundo(sementeBase_, relogio.dia, a.id + "|caravana") < riscoCaravana(corr, seguranca);
            for (const auto& [bem, v] : a.caravana.mercado) {
                float& p = mercado.pressao[bem];
                p = std::clamp(p + (atacada ? -v : v), dados().mercado.pressaoMin, dados().mercado.pressaoMax);
            }
            if (atacada) {
                e.prosperidade = std::max(0.0f, e.prosperidade - 5.0f);
                n.push_back({5, std::format("Atacaram {} na estrada perto de {}: o que ela trazia vai faltar no mercado.", a.caravana.nome,
                                            a.perto.empty() || !regioes.contains(a.perto) ? a.nome : nomeRegiao(a.perto))});
            } else {
                std::string pedagio;
                if (nivelMercado() > 0 && a.caravana.pedagio > 0) {
                    inventario.adicionar("ouro", a.caravana.pedagio);
                    ultimoRelatorio.variacoes.push_back({"ouro|pedágio", a.caravana.pedagio});
                    pedagio = std::format(" e pagou {} de pedágio", a.caravana.pedagio);
                }
                n.push_back({2, std::format("Chegou {}{}: o mercado ficou mais farto.", a.caravana.nome, pedagio)});
            }
        }
    }
    // Um viajante com um pedido curto (vale só até amanhã)
    if (std::ranges::none_of(mundoVivo.pedidos, [](const PedidoAberto& p) { return p.aldeia.empty(); }) &&
        sorteMundo(sementeBase_, relogio.dia, "viajante") < d.chanceViajante) {
        std::vector<const ViajanteDef*> possiveis;
        for (const ViajanteDef& v : d.viajantes)
            if (v.requer.empty() || marcos.contains(v.requer)) possiveis.push_back(&v);
        if (!possiveis.empty()) {
            const ViajanteDef* v = possiveis[static_cast<size_t>(sorteMundo(sementeBase_, relogio.dia, "qual") * possiveis.size()) % possiveis.size()];
            mundoVivo.pedidos.push_back({v->id, "", relogio.dia});
            n.push_back({3, v->texto + " (só hoje; R: Terras)"});
        }
    }
    mundoVivo.noticias = ordenarNoticias(std::move(n));
    for (size_t i = 0; i < mundoVivo.noticias.size() && static_cast<int>(i) < d.noticiasPorManha; ++i)
        notas.push_back("Notícia: " + mundoVivo.noticias[i]);
}

}
