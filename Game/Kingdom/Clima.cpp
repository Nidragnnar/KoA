#include "Game/Kingdom/Clima.h"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "Game/Kingdom/Povo.h"

namespace koa {

const TempoDef* DadosClima::tempo(const std::string& id) const {
    const auto it = std::ranges::find(tempos, id, &TempoDef::id);
    return it == tempos.end() ? nullptr : &*it;
}

const TempoDef& tempoDoDia(const DadosClima& d, int dia, uint32_t semente) {
    static const TempoDef limpo{"limpo", "céu limpo", ""};
    const Estacao e = Relogio{dia}.estacao();
    const auto it = d.pesos.find(e);
    if (it == d.pesos.end() || it->second.empty()) return limpo;
    int total = 0;
    for (const auto& [id, p] : it->second) total += std::max(0, p);
    if (total <= 0) return limpo;
    int alvo = static_cast<int>(misturar(static_cast<uint32_t>(dia), semente ^ 0xC11A7Au) % static_cast<uint32_t>(total));
    for (const auto& [id, p] : it->second) {
        alvo -= std::max(0, p);
        if (alvo < 0) {
            const TempoDef* t = d.tempo(id);
            return t ? *t : limpo;
        }
    }
    return limpo;
}

bool lerClima(const nlohmann::json& j, DadosClima& d, std::string* erro) {
    try {
        static const std::pair<const char*, Estacao> nomes[] = {
            {"primavera", Estacao::Primavera}, {"verao", Estacao::Verao}, {"outono", Estacao::Outono}, {"inverno", Estacao::Inverno}};
        for (const auto& jt : j.at("tempos")) {
            TempoDef t;
            t.id = jt.at("id").get<std::string>();
            t.nome = jt.value("nome", t.id);
            t.aviso = jt.value("aviso", "");
            t.rega = jt.value("rega", false);
            t.raios = jt.value("raios", false);
            t.felicidade = jt.value("felicidade", 0.0f);
            t.velocidade = jt.value("velocidade", 1.0f);
            t.forcaHorda = jt.value("forca_horda", 1.0f);
            t.nevoa = jt.value("nevoa", 0.0f);
            t.particulas = jt.value("particulas", 0);
            if (jt.contains("luz")) t.luz = {jt["luz"][0].get<float>(), jt["luz"][1].get<float>(), jt["luz"][2].get<float>()};
            d.tempos.push_back(std::move(t));
        }
        for (const auto& [nome, e] : nomes)
            for (const auto& [id, peso] : j.at("estacoes").at(nome).items()) {
                if (!d.tempo(id)) {
                    if (erro) *erro = std::string("clima.json: tempo desconhecido em ") + nome + ": " + id;
                    return false;
                }
                d.pesos[e].push_back({id, peso.get<int>()});
            }
    } catch (const std::exception& e) {
        if (erro) *erro = std::string("clima.json: ") + e.what();
        return false;
    }
    return true;
}

}
