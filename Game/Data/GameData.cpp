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
    if (const auto it = recursos.find(id); it != recursos.end()) return it->second.nome;
    if (const auto it = cotidiano.itens.find(id); it != cotidiano.itens.end()) return it->second;  // fase 15: sementes, peixes, pratos
    return id;
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
    if (!lerEstacoes(je, estacoes, erro)) return false;
    // Fase 11: o que o herdeiro decide quando um chefe se rende
    const auto tx = ke::fs::lerTexto(pasta / "Lore/escolhas.json");
    const auto jx = tx ? nlohmann::json::parse(*tx, nullptr, false) : nlohmann::json();
    if (!tx || jx.is_discarded()) {
        if (erro) *erro = "Data/Lore/escolhas.json ausente ou inválido";
        return false;
    }
    if (!lerEscolhas(jx, escolhas, erro)) return false;
    for (const auto& [id, ini] : inimigos)
        if (!ini.escolha.empty() && !escolhas.contains(ini.escolha)) {
            if (erro) *erro = "inimigo " + id + " usa escolha inexistente: " + ini.escolha;
            return false;
        }
    for (const auto& [id, e] : escolhas)
        for (const auto& o : e.opcoes)
            if (!o.item.empty() && !equipamentos.contains(o.item)) {
                if (erro) *erro = "escolha " + id + " dá item inexistente: " + o.item;
                return false;
            }
    // Fase 12: o Conselho Real
    const auto tc = ke::fs::lerTexto(pasta / "Kingdom/conselho.json");
    const auto jcs = tc ? nlohmann::json::parse(*tc, nullptr, false) : nlohmann::json();
    if (!tc || jcs.is_discarded()) {
        if (erro) *erro = "Data/Kingdom/conselho.json ausente ou inválido";
        return false;
    }
    if (!lerConselho(jcs, conselho, erro)) return false;
    for (const auto& cargo : conselho.cargos)
        for (const auto& [npc, apt] : cargo.aptidao)
            if (!npcs.contains(npc)) {
                if (erro) *erro = "cargo " + cargo.id + " cita morador inexistente: " + npc;
                return false;
            }
    for (const auto& dec : conselho.decisoes)
        for (const auto& o : dec.opcoes)
            for (const auto& [npc, valor] : o.efeitos.amizade)
                if (!npcs.contains(npc)) {
                    if (erro) *erro = "decisão " + dec.id + " cita morador inexistente: " + npc;
                    return false;
                }
    // Fase 13: hordas e defesa
    const auto tdef = ke::fs::lerTexto(pasta / "Kingdom/defesa.json");
    const auto jdef = tdef ? nlohmann::json::parse(*tdef, nullptr, false) : nlohmann::json();
    if (!tdef || jdef.is_discarded()) {
        if (erro) *erro = "Data/Kingdom/defesa.json ausente ou inválido";
        return false;
    }
    if (!lerDefesa(jdef, defesa, erro)) return false;
    for (const auto& [tipo, pontos] : defesa.custo)
        if (!inimigos.contains(tipo)) {
            if (erro) *erro = "horda usa criatura inexistente: " + tipo;
            return false;
        }
    // Fase 14: guilda, expedições e relíquias
    const auto tg = ke::fs::lerTexto(pasta / "Kingdom/guilda.json");
    const auto jg = tg ? nlohmann::json::parse(*tg, nullptr, false) : nlohmann::json();
    if (!tg || jg.is_discarded()) {
        if (erro) *erro = "Data/Kingdom/guilda.json ausente ou inválido";
        return false;
    }
    if (!lerGuilda(jg, guilda, erro)) return false;
    for (const auto& dest : guilda.destinos)
        for (const auto& [rec, faixa] : dest.ganhos)
            if (!recursos.contains(rec)) {
                if (erro) *erro = "destino " + dest.id + " rende recurso inexistente: " + rec;
                return false;
            }
    // Fase 15: vida cotidiana
    const auto tv = ke::fs::lerTexto(pasta / "World/vida.json");
    const auto jv = tv ? nlohmann::json::parse(*tv, nullptr, false) : nlohmann::json();
    if (!tv || jv.is_discarded()) {
        if (erro) *erro = "Data/World/vida.json ausente ou inválido";
        return false;
    }
    if (!lerCotidiano(jv, cotidiano, erro)) return false;
    // Todo item citado precisa ter nome (é o que a interface mostra)
    auto conhecido = [&](const std::string& id) { return recursos.contains(id) || cotidiano.itens.contains(id); };
    std::vector<std::string> citados;
    for (const auto& cul : cotidiano.culturas) citados.push_back(cul.semente), citados.push_back(cul.id);
    for (const auto& p : cotidiano.peixes) citados.push_back(p.id);
    for (const auto& [b, a] : cotidiano.animais) citados.push_back(a.produto);
    for (const auto& rec : cotidiano.receitas) {
        citados.push_back(rec.id);
        for (const auto& [item, q] : rec.ingredientes) citados.push_back(item);
    }
    for (const auto& [npc, lista] : cotidiano.presentes) {
        if (!npcs.contains(npc)) {
            if (erro) *erro = "presentes para morador inexistente: " + npc;
            return false;
        }
        for (const auto& item : lista) citados.push_back(item);
    }
    for (const auto& id : citados)
        if (!conhecido(id)) {
            if (erro) *erro = "vida.json cita item sem nome: " + id;
            return false;
        }
    return true;
}

}
