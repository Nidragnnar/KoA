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
            d.construivel = c.value("construivel", false);
            d.limite = c.value("limite", 0);
            if (c.contains("tamanho")) d.tamanho = {c["tamanho"][0].get<int>(), c["tamanho"][1].get<int>()};
            d.requer = c.value("requer", "");
            d.requerTexto = c.value("requer_texto", "");
            d.monumento = c.value("monumento", false);
            for (const auto& n : c.at("niveis")) {
                NivelConstrucao nv;
                nv.nivel = n.at("nivel").get<int>();
                nv.nome = n.value("nome", d.id);
                nv.descricao = n.value("descricao", "");
                if (n.contains("custo"))
                    for (const auto& [rid, q] : n["custo"].items()) nv.custo[rid] = q.get<int>();
                if (n.contains("efeitos"))
                    for (const auto& [ef, v] : n["efeitos"].items()) nv.efeitos[ef] = v.get<float>();
                d.niveis.push_back(std::move(nv));
            }
            std::ranges::sort(d.niveis, {}, &NivelConstrucao::nivel);
            if (d.niveis.empty()) throw std::runtime_error("construção sem níveis: " + d.id);
            ordemConstrucoes.push_back(d.id);
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
    if (!lerJson(jr, jc, erro)) return false;
    const auto n = ke::fs::lerTexto(pasta / "NPCs/npcs.json");
    const auto d = ke::fs::lerTexto(pasta / "Dialogues/dialogos.json");
    if (!n || !d) {
        if (erro) *erro = "Data/NPCs/npcs.json ou Data/Dialogues/dialogos.json ausente";
        return false;
    }
    const auto jn = nlohmann::json::parse(*n, nullptr, false);
    const auto jd = nlohmann::json::parse(*d, nullptr, false);
    if (jn.is_discarded() || jd.is_discarded()) {
        if (erro) *erro = "JSON inválido em Data/NPCs ou Data/Dialogues";
        return false;
    }
    if (!lerNpcs(jn, jd, npcs, erro)) return false;
    nlohmann::json jc3[3];
    const char* arquivos[3] = {"Enemies/inimigos.json", "Items/equipamentos.json", "Quests/missoes.json"};
    for (int i = 0; i < 3; ++i) {
        const auto t = ke::fs::lerTexto(pasta / arquivos[i]);
        jc3[i] = t ? nlohmann::json::parse(*t, nullptr, false) : nlohmann::json();
        if (!t || jc3[i].is_discarded()) {
            if (erro) *erro = std::string("Data/") + arquivos[i] + " ausente ou inválido";
            return false;
        }
    }
    if (!lerCombate(jc3[0], jc3[1], jc3[2], inimigos, equipamentos, ordemEquipamentos, missoes, erro)) return false;
    // Fase 9: Crônicas, títulos e pistas
    nlohmann::json jl[3];
    const char* arquivosLore[3] = {"Lore/cronicas.json", "Lore/titulos.json", "Lore/pistas.json"};
    for (int i = 0; i < 3; ++i) {
        const auto t = ke::fs::lerTexto(pasta / arquivosLore[i]);
        jl[i] = t ? nlohmann::json::parse(*t, nullptr, false) : nlohmann::json();
        if (!t || jl[i].is_discarded()) {
            if (erro) *erro = std::string("Data/") + arquivosLore[i] + " ausente ou inválido";
            return false;
        }
    }
    if (!lerLore(jl[0], jl[1], jl[2], lore, erro)) return false;
    // Fase 10: estações, acontecimentos e festival
    const auto te = ke::fs::lerTexto(pasta / "World/estacoes.json");
    const auto je = te ? nlohmann::json::parse(*te, nullptr, false) : nlohmann::json();
    if (!te || je.is_discarded()) {
        if (erro) *erro = "Data/World/estacoes.json ausente ou inválido";
        return false;
    }
    return lerEstacoes(je, estacoes, erro);
}

}
