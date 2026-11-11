#include "Game/Save/SaveSystem.h"

#include <nlohmann/json.hpp>

#include "Engine/Platform/FileSystem.h"

namespace koa {

using nlohmann::json;

bool SaveSystem::existe(const std::string& slot) const {
    std::error_code ec;
    return std::filesystem::exists(pasta_ / slot / "metadata.json", ec);
}

bool SaveSystem::salvar(const DadosSave& d, const std::string& slot, std::string* erro) {
    const auto dir = pasta_ / slot;
    std::error_code ec;
    // Backup do save anterior antes de sobrescrever
    if (std::filesystem::exists(dir / "metadata.json", ec)) {
        auto bak = dir;
        bak += ".bak";
        ke::fs::copiarPasta(dir, bak);
    }

    json meta = {{"versao_formato", VERSAO_SAVE}, {"versao_jogo", d.versaoJogo}, {"data_hora", d.dataHora}, {"tempo_jogado", d.tempoJogado}};
    json player = {{"x", d.posJogador.x}, {"y", d.posJogador.y}, {"direcao", d.direcao}};
    json inv = d.inventario;
    json reino = {{"construcoes", d.niveis}, {"era_visual", d.eraVisual}};
    json objs = json::array();
    for (const auto& o : d.objetos) objs.push_back({{"id", o.id}, {"hp", o.hp}, {"esgotado", o.esgotado}, {"aberto", o.aberto}});
    json mundo = {{"objetos", objs}};

    const std::pair<const char*, const json*> arquivos[] = {
        {"player.json", &player}, {"inventory.json", &inv}, {"kingdom.json", &reino}, {"world.json", &mundo},
        {"metadata.json", &meta}};  // metadata por último: só existe save "válido" quando tudo foi gravado
    for (const auto& [nome, conteudo] : arquivos) {
        if (!ke::fs::gravarTextoSeguro(dir / nome, conteudo->dump(2))) {
            if (erro) *erro = std::string("não consegui gravar ") + nome;
            return false;
        }
    }
    return true;
}

json SaveSystem::migrar(json tudo, int deVersao) {
    // Exemplo de como ficará: if (deVersao < 2) { ...converte campos...; deVersao = 2; }
    (void)deVersao;
    tudo["metadata"]["versao_formato"] = VERSAO_SAVE;
    return tudo;
}

std::optional<DadosSave> SaveSystem::lerPasta(const std::filesystem::path& dir, std::string* erro) const {
    json tudo;
    for (const char* nome : {"metadata", "player", "inventory", "kingdom", "world"}) {
        const auto texto = ke::fs::lerTexto(dir / (std::string(nome) + ".json"));
        if (!texto) {
            if (erro) *erro = std::string("arquivo ausente: ") + nome + ".json";
            return std::nullopt;
        }
        tudo[nome] = json::parse(*texto, nullptr, false);
        if (tudo[nome].is_discarded()) {
            if (erro) *erro = std::string("arquivo corrompido: ") + nome + ".json";
            return std::nullopt;
        }
    }
    try {
        const int versao = tudo["metadata"].value("versao_formato", 0);
        if (versao <= 0 || versao > VERSAO_SAVE) {
            if (erro) *erro = "versão de save desconhecida: " + std::to_string(versao);
            return std::nullopt;
        }
        if (versao < VERSAO_SAVE) tudo = migrar(std::move(tudo), versao);

        DadosSave d;
        const json& m = tudo["metadata"];
        d.versao = VERSAO_SAVE;
        d.versaoJogo = m.value("versao_jogo", "");
        d.dataHora = m.value("data_hora", "");
        d.tempoJogado = std::max(0.0, m.value("tempo_jogado", 0.0));
        const json& p = tudo["player"];
        d.posJogador = {p.value("x", 0.0f), p.value("y", 0.0f)};
        d.direcao = std::clamp(p.value("direcao", 0), 0, 3);
        for (const auto& [id, q] : tudo["inventory"].items())
            if (q.is_number_integer()) d.inventario[id] = std::max(0, q.get<int>());
        const json& r = tudo["kingdom"];
        if (r.contains("construcoes"))
            for (const auto& [id, n] : r["construcoes"].items()) d.niveis[id] = n.get<int>();
        d.eraVisual = std::clamp(r.value("era_visual", 0.0f), 0.0f, 3.0f);
        for (const auto& o : tudo["world"].value("objetos", json::array()))
            d.objetos.push_back({o.value("id", ""), o.value("hp", 0), o.value("esgotado", false), o.value("aberto", false)});
        return d;
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("save inválido: ") + ex.what();
        return std::nullopt;
    }
}

std::optional<DadosSave> SaveSystem::carregar(const std::string& slot, std::string* erro, std::string* aviso) {
    std::string e1;
    if (auto d = lerPasta(pasta_ / slot, &e1)) return d;
    auto bak = pasta_ / slot;
    bak += ".bak";
    std::string e2;
    if (auto d = lerPasta(bak, &e2)) {
        if (aviso) *aviso = "Save principal com problema (" + e1 + "); carreguei o backup.";
        return d;
    }
    if (erro) *erro = e1;
    return std::nullopt;
}

}
