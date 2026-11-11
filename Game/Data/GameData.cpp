#include "Game/Data/GameData.h"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "Engine/Platform/FileSystem.h"

namespace koa {

const NivelConstrucao* BuildingDef::nivel(int n) const {
    for (const auto& nv : niveis)
        if (nv.nivel == n) return &nv;
    return nullptr;
}

std::string GameData::nomeRecurso(const std::string& id) const {
    const auto it = recursos.find(id);
    return it == recursos.end() ? id : it->second.nome;
}

bool GameData::lerJson(const nlohmann::json& jr, const nlohmann::json& jc, std::string* erro) {
    try {
        for (const auto& [id, r] : jr.at("recursos").items())
            recursos[id] = {id, r.value("nome", id), r.value("icone", "icone_" + id)};
        for (const auto& c : jc.at("construcoes")) {
            BuildingDef d;
            d.id = c.at("id").get<std::string>();
            d.aparencia = c.value("aparencia", d.id);
            if (c.contains("tamanho")) d.tamanho = {c["tamanho"][0].get<int>(), c["tamanho"][1].get<int>()};
            for (const auto& n : c.at("niveis")) {
                NivelConstrucao nv;
                nv.nivel = n.at("nivel").get<int>();
                nv.nome = n.value("nome", d.id);
                nv.descricao = n.value("descricao", "");
                if (n.contains("custo"))
                    for (const auto& [rid, q] : n["custo"].items()) nv.custo[rid] = q.get<int>();
                d.niveis.push_back(std::move(nv));
            }
            std::ranges::sort(d.niveis, {}, &NivelConstrucao::nivel);
            if (d.niveis.empty()) throw std::runtime_error("construção sem níveis: " + d.id);
            construcoes[d.id] = std::move(d);
        }
    } catch (const std::exception& ex) {
        if (erro) *erro = ex.what();
        return false;
    }
    return true;
}

bool GameData::carregar(const std::filesystem::path& pasta, std::string* erro) {
    const auto r = ke::fs::lerTexto(pasta / "Items/recursos.json");
    const auto c = ke::fs::lerTexto(pasta / "Buildings/construcoes.json");
    if (!r || !c) {
        if (erro) *erro = "arquivos de dados ausentes em " + pasta.string();
        return false;
    }
    const auto jr = nlohmann::json::parse(*r, nullptr, false);
    const auto jc = nlohmann::json::parse(*c, nullptr, false);
    if (jr.is_discarded() || jc.is_discarded()) {
        if (erro) *erro = "JSON inválido em Data/";
        return false;
    }
    return lerJson(jr, jc, erro);
}

}
