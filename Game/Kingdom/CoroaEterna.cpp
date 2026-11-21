#include "Game/Kingdom/CoroaEterna.h"

#include <nlohmann/json.hpp>

namespace koa {

bool lerCoroa(const nlohmann::json& j, DadosCoroa& d, std::string* erro) {
    try {
        d.requerEra = j.value("requer_era", d.requerEra);
        d.ecoACada = std::max(1, j.value("eco_a_cada", d.ecoACada));
        d.ecoForca = j.value("eco_forca", d.ecoForca);
        for (const auto& [id, jf] : j.at("finais").items()) {
            FinalCoroa f;
            f.nome = jf.value("nome", id);
            const auto& g = jf.at("grade");
            f.saturacao = g.value("saturacao", 1.0f);
            f.contraste = g.value("contraste", 1.0f);
            f.brilho = g.value("brilho", 0.0f);
            f.vinheta = g.value("vinheta", 0.1f);
            if (g.contains("tinta")) f.tinta = {g["tinta"][0].get<float>(), g["tinta"][1].get<float>(), g["tinta"][2].get<float>()};
            const auto& e = jf.value("efeitos", nlohmann::json::object());
            f.felicidade = e.value("felicidade", 0.0f);
            f.comida = e.value("comida", 0.0f);
            f.seguranca = e.value("seguranca", 0);
            f.ouroDia = e.value("ouro_dia", 0);
            f.cristais = jf.value("cristais", false);
            f.epilogo = jf.value("epilogo", std::vector<std::string>{});
            d.finais[id] = std::move(f);
        }
    } catch (const std::exception& e) {
        if (erro) *erro = std::string("coroa_eterna.json: ") + e.what();
        return false;
    }
    return true;
}

}