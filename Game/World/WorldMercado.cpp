// Parte do World: o mercado vivo (preços por oferta, estação e sorte do dia) e as encomendas do Corvin (fase 20)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

bool World::corvinNaVila() const {
    if (!noVilarejo()) return false;
    for (const auto& [id, d] : dados().npcs)
        if (d.visitante() && visitanteHoje(d)) return true;
    return false;
}

std::vector<const BemDef*> World::bensNoMercado(bool corvin) const {
    std::vector<const BemDef*> v;
    for (const BemDef& b : dados().mercado.bens) {
        if (!b.requer.empty() && !marcos.contains(b.requer)) continue;
        if (corvin && !b.soVenda) continue;
        // Itens da vida cotidiana só aparecem quando o herdeiro tem algum para vender
        if (b.soVenda && inventario.quantidade(b.id) <= 0) continue;
        v.push_back(&b);
    }
    return v;
}

float World::precoHoje(const std::string& bem) const {
    const DadosMercado& dm = dados().mercado;
    const BemDef* b = dm.bem(bem);
    if (!b) return 0.0f;
    const auto it = mercado.pressao.find(bem);
    return precoUnitario(*b, estacaoAtual().id, it == mercado.pressao.end() ? 0.0f : it->second, sorteDoDia(dm, sementeBase_, relogio.dia, bem));
}

// A taxa do dia: a do nível do mercado (ou a do Corvin), menos o desconto do herdeiro Astuto (nunca abaixo de 5%)
float World::taxaMercado(bool corvin) const {
    return std::max(0.05f, dados().mercado.taxa(corvin ? 0 : nivelMercado()) + efeitosHerdeiro().taxaMercado);
}

int World::precoCompraLote(const std::string& bem) const {
    const BemDef* b = dados().mercado.bem(bem);
    return b ? precoCompra(precoHoje(bem), b->lote, taxaMercado(false)) : 0;
}

int World::precoVendaLote(const std::string& bem, bool corvin) const {
    const BemDef* b = dados().mercado.bem(bem);
    return b ? precoVenda(precoHoje(bem), b->lote, taxaMercado(corvin)) : 0;
}

std::string World::podeNegociar(const std::string& bem, bool comprar, bool corvin) const {
    const DadosMercado& dm = dados().mercado;
    const BemDef* b = dm.bem(bem);
    if (!b || (!b->requer.empty() && !marcos.contains(b->requer))) return "Ninguém negocia isso aqui.";
    if (corvin) {
        if (!corvinNaVila()) return "O Corvin não está na vila hoje.";
        if (comprar || !b->soVenda) return "O Corvin só compra colheita, peixe e pratos.";
    } else if (nivelMercado() == 0) {
        return "Construa o mercado para negociar.";
    }
    if (comprar && b->soVenda) return "O mercado só compra isso.";
    if (mercado.negociadosHoje >= dm.limite(corvin ? 1 : nivelMercado())) return "Os mercadores já fecharam as contas de hoje.";
    if (comprar && inventario.quantidade("ouro") < precoCompraLote(bem)) return std::format("Precisa de {} de ouro.", precoCompraLote(bem));
    if (!comprar && inventario.quantidade(bem) < b->lote) return std::format("Precisa de {} para vender.", b->lote);
    if (!comprar && precoVendaLote(bem, corvin) <= 0) return "Ninguém paga nada por isso hoje.";
    return "";
}

bool World::comprarNoMercado(const std::string& bem) {
    const std::string falta = podeNegociar(bem, true);
    if (!falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, falta});
        return false;
    }
    const BemDef* b = dados().mercado.bem(bem);
    const int preco = precoCompraLote(bem);
    inventario.remover("ouro", preco);
    inventario.adicionar(bem, b->lote);
    mercado.comprado += preco;
    negociar(dados().mercado, mercado, bem, true);
    eventos_.push_back({EventoJogo::Tipo::Ganhou, jogador.pos - glm::vec2(0, 10), "mercado", bem, b->lote});
    return true;
}

bool World::venderNoMercado(const std::string& bem, bool corvin) {
    const std::string falta = podeNegociar(bem, false, corvin);
    if (!falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, falta});
        return false;
    }
    const BemDef* b = dados().mercado.bem(bem);
    const int preco = precoVendaLote(bem, corvin);
    inventario.remover(bem, b->lote);
    inventario.adicionar("ouro", preco);
    mercado.vendido += preco;
    negociar(dados().mercado, mercado, bem, false);
    eventos_.push_back({EventoJogo::Tipo::Ganhou, jogador.pos - glm::vec2(0, 10), "mercado", "ouro", preco});
    return true;
}

std::string World::podeEncomendar(const std::string& id) const {
    const EncomendaDef* e = dados().mercado.encomenda(id);
    if (!e) return "Encomenda desconhecida.";
    if (!corvinNaVila()) return "Só dá para encomendar com o Corvin na vila.";
    if (std::ranges::any_of(mercado.encomendas, [&](const EncomendaFeita& f) { return f.id == id; })) return "Já encomendada.";
    if (!inventario.tem(e->custo)) return "Faltam recursos.";
    return "";
}

bool World::encomendar(const std::string& id) {
    const std::string falta = podeEncomendar(id);
    if (!falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, falta});
        return false;
    }
    const EncomendaDef* e = dados().mercado.encomenda(id);
    inventario.pagar(e->custo);
    mercado.comprado += e->custo.contains("ouro") ? e->custo.at("ouro") : 0;
    mercado.encomendas.push_back({id, relogio.dia});
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0,
                        std::format("Encomenda feita: {}. Chega numa visita do Corvin daqui a {} dias ou mais.", e->nome, e->dias)});
    return true;
}

int World::entregaEncomenda(const EncomendaFeita& f) const {
    const EncomendaDef* e = dados().mercado.encomenda(f.id);
    return f.dia + (e ? e->dias : 3);
}

float World::tendencia(const std::string& bem, int dias) const {
    const auto it = mercado.historico.find(bem);
    if (it == mercado.historico.end() || it->second.size() < 2) return 0.0f;
    const auto& h = it->second;
    const float antes = h[h.size() - 1 - std::min<size_t>(static_cast<size_t>(dias), h.size() - 1)];
    return antes > 0.0f ? (h.back() - antes) / antes : 0.0f;
}

void World::reagirMercado(const std::string& marco) {
    const DadosMercado& dm = dados().mercado;
    for (const auto& [bem, v] : dm.reacaoA(marco)) {
        float& p = mercado.pressao[bem];
        p = std::clamp(p + v, dm.pressaoMin, dm.pressaoMax);
    }
}

// Depois da virada do dia: a pressão volta para o normal, o preço da manhã entra no histórico e o Corvin entrega
void World::manhaDoMercado(std::vector<std::string>& notas) {
    const DadosMercado& dm = dados().mercado;
    noiteDoMercado(dm, mercado);
    for (const BemDef& b : dm.bens) {
        if (b.soVenda || (!b.requer.empty() && !marcos.contains(b.requer))) continue;
        auto& h = mercado.historico[b.id];
        h.push_back(precoHoje(b.id));
        if (static_cast<int>(h.size()) > dm.diasHistorico) h.erase(h.begin(), h.end() - dm.diasHistorico);
    }
    bool visita = false;
    for (const auto& [id, d] : dados().npcs) visita |= d.visitante() && visitanteHoje(d);
    if (!visita) return;
    std::vector<EncomendaFeita> ficam;
    for (const EncomendaFeita& f : mercado.encomendas) {
        const EncomendaDef* e = dm.encomenda(f.id);
        if (!e) continue;  // encomenda que saiu dos dados: some
        if (relogio.dia < entregaEncomenda(f)) {
            ficam.push_back(f);
            continue;
        }
        for (const auto& [r, q] : e->ganho) inventario.adicionar(r, q);
        notas.push_back("O Corvin trouxe a encomenda: " + e->nome + ".");
        marcar("encomenda:" + f.id, e->nome);
    }
    mercado.encomendas = std::move(ficam);
}

}
