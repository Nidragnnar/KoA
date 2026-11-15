#include "Game/Kingdom/Estacoes.h"

#include <stdexcept>

#include <nlohmann/json.hpp>

#include "Game/NPC/Npc.h"

namespace koa {

namespace {

std::vector<glm::ivec2> lerTiles(const nlohmann::json& j) {
    std::vector<glm::ivec2> v;
    for (const auto& t : j) v.push_back({t[0].get<int>(), t[1].get<int>()});
    return v;
}

}

bool lerEstacoes(const nlohmann::json& j, DadosEstacoes& d, std::string* erro) {
    try {
        const char* ids[4] = {"primavera", "verao", "outono", "inverno"};
        const auto& je = j.at("estacoes");
        for (int i = 0; i < 4; ++i) {
            const auto& e = je.at(ids[i]);
            EstacaoDef& s = d.estacoes[i];
            s.id = ids[i];
            s.nome = nomeEstacao(static_cast<Estacao>(i));
            s.mod.comida = e.value("comida", 1.0f);
            s.mod.consumo = e.value("consumo", 1.0f);
            s.mod.ouro = e.value("ouro", 1.0f);
            s.mod.felicidade = e.value("felicidade", 0.0f);
            s.mod.viajantes = e.value("viajantes", 0);
            s.inimigos = e.value("inimigos", 1.0f);
            s.mercadorACada = e.value("mercador_a_cada", 0);
            s.resumo = e.value("resumo", "");
            s.chegada = e.value("chegada", "");
        }
        for (const auto& e : j.value("eventos", nlohmann::json::array())) {
            EventoDef ev;
            ev.id = e.at("id").get<std::string>();
            ev.nome = e.at("nome").get<std::string>();
            ev.anuncio = e.value("anuncio", "");
            ev.aviso = e.value("aviso", "");
            ev.chance = e.value("chance", 0.0f);
            ev.requer = e.value("requer", "");
            ev.eraMin = e.value("era_min", 0);
            ev.inimigos = e.value("inimigos", 1.0f);
            ev.ouroInimigos = e.value("ouro_inimigos", 1.0f);
            ev.felicidade = e.value("felicidade", 0.0f);
            ev.ouroMeteorito = e.value("ouro_meteorito", 0);
            d.eventos.push_back(std::move(ev));
        }
        if (j.contains("festival")) {
            const auto& f = j["festival"];
            FestivalDef& fd = d.festival;
            fd.nome = f.value("nome", fd.nome);
            const std::string e = f.value("estacao", "outono");
            for (int i = 0; i < 4; ++i)
                if (e == ids[i]) fd.estacao = static_cast<Estacao>(i);
            fd.diaDaEstacao = f.value("dia_da_estacao", fd.diaDaEstacao);
            fd.inicio = lerHora(f.value("inicio", "17:00"));
            fd.fim = lerHora(f.value("fim", "23:00"));
            fd.felicidade = f.value("felicidade", fd.felicidade);
            fd.lugares = lerTiles(f.value("lugares", nlohmann::json::array()));
            if (fd.lugares.empty()) throw std::runtime_error("festival sem lugares");
        }
        d.meteoritos = lerTiles(j.value("meteoritos", nlohmann::json::array()));
        d.diasSemEventos = j.value("dias_sem_eventos", 3);
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("Data/World/estacoes.json: ") + ex.what();
        return false;
    }
    return true;
}

}
