#include "Game/Kingdom/Mercado.h"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

#include "Game/Kingdom/Povo.h"

namespace koa {

float BemDef::fatorEstacao(const std::string& estacao) const {
    const auto it = estacoes.find(estacao);
    return it == estacoes.end() ? 1.0f : it->second;
}

const BemDef* DadosMercado::bem(const std::string& id) const {
    const auto it = std::ranges::find(bens, id, &BemDef::id);
    return it == bens.end() ? nullptr : &*it;
}

const EncomendaDef* DadosMercado::encomenda(const std::string& id) const {
    const auto it = std::ranges::find(encomendas, id, &EncomendaDef::id);
    return it == encomendas.end() ? nullptr : &*it;
}

float DadosMercado::taxa(int nivel) const {
    if (nivel <= 0 || taxas.empty()) return taxaCorvin;
    return taxas[std::min<size_t>(nivel, taxas.size()) - 1];
}

int DadosMercado::limite(int nivel) const {
    if (limites.empty()) return 0;
    return limites[std::clamp<size_t>(nivel, 1, limites.size()) - 1];
}

std::map<std::string, float> DadosMercado::reacaoA(const std::string& marco) const {
    std::map<std::string, float> r;
    for (const ReacaoMercado& x : reacoes) {
        const bool casa = x.marco.ends_with('*') ? marco.starts_with(x.marco.substr(0, x.marco.size() - 1)) : marco == x.marco;
        if (casa)
            for (const auto& [b, v] : x.pressao) r[b] += v;
    }
    return r;
}

bool lerMercado(const nlohmann::json& j, DadosMercado& d, std::string* erro) {
    try {
        d.lote = j.value("lote", 10);
        d.taxas = j.value("taxa", d.taxas);
        d.limites = j.value("limite_dia", d.limites);
        d.impacto = j.value("impacto", d.impacto);
        d.recupera = j.value("recupera", d.recupera);
        d.sorte = j.value("sorte", d.sorte);
        d.pressaoMin = j.value("pressao_min", d.pressaoMin);
        d.pressaoMax = j.value("pressao_max", d.pressaoMax);
        d.diasHistorico = j.value("dias_historico", d.diasHistorico);
        d.taxaCorvin = j.value("taxa_corvin", d.taxaCorvin);
        for (const auto& b : j.at("bens")) {
            BemDef x;
            x.id = b.at("id").get<std::string>();
            x.base = b.at("base").get<float>();
            x.lote = b.value("lote", d.lote);
            x.soVenda = b.value("so_venda", false);
            x.requer = b.value("requer", "");
            x.estacoes = b.value("estacoes", std::map<std::string, float>{});
            if (x.base <= 0.0f || x.lote <= 0) {
                if (erro) *erro = "mercado.json: preço ou lote inválido em " + x.id;
                return false;
            }
            d.bens.push_back(std::move(x));
        }
        for (const auto& r : j.value("reacoes", nlohmann::json::array())) {
            ReacaoMercado x;
            x.marco = r.at("marco").get<std::string>();
            for (const auto& [k, v] : r.items())
                if (k != "marco") x.pressao[k] = v.get<float>();
            d.reacoes.push_back(std::move(x));
        }
        for (const auto& e : j.value("encomendas", nlohmann::json::array()))
            d.encomendas.push_back({e.at("id").get<std::string>(), e.value("nome", ""), e.value("custo", Custo{}), e.value("ganho", Custo{}), e.value("dias", 3)});
    } catch (const std::exception& e) {
        if (erro) *erro = std::string("mercado.json: ") + e.what();
        return false;
    }
    // A taxa precisa vencer o impacto de um lote: senão comprar e vender em seguida daria lucro
    for (const float t : d.taxas)
        if (d.impacto * (1.0f + t) >= 2.0f * t) {
            if (erro) *erro = "mercado.json: impacto grande demais para a taxa (dá lucro comprando e vendendo)";
            return false;
        }
    return true;
}

float sorteDoDia(const DadosMercado& d, uint32_t semente, int dia, const std::string& bem) {
    uint32_t h = 2166136261u;  // FNV-1a do nome do bem
    for (const char c : bem) h = (h ^ static_cast<unsigned char>(c)) * 16777619u;
    const uint32_t x = misturar(misturar(semente, static_cast<uint32_t>(dia) * 2654435761u), h);
    return d.sorte * ((x % 2001u) / 1000.0f - 1.0f);
}

float precoUnitario(const BemDef& b, const std::string& estacao, float pressao, float sorte) {
    return b.base * b.fatorEstacao(estacao) * (1.0f + pressao) * (1.0f + sorte);
}

int precoCompra(float unitario, int lote, float taxa) {
    return std::max(1, static_cast<int>(std::ceil(unitario * lote * (1.0f + taxa) - 1e-4f)));
}

int precoVenda(float unitario, int lote, float taxa) {
    return std::max(0, static_cast<int>(std::floor(unitario * lote * (1.0f - taxa) + 1e-4f)));
}

void negociar(const DadosMercado& d, EstadoMercado& e, const std::string& bem, bool comprou) {
    float& p = e.pressao[bem];
    p = std::clamp(p + (comprou ? d.impacto : -d.impacto), d.pressaoMin, d.pressaoMax);
    ++e.negociadosHoje;
}

void noiteDoMercado(const DadosMercado& d, EstadoMercado& e) {
    for (auto& [bem, p] : e.pressao) p *= 1.0f - d.recupera;
    std::erase_if(e.pressao, [](const auto& x) { return std::abs(x.second) < 0.005f; });
    e.negociadosHoje = 0;
}

}
