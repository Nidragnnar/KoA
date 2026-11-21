#include "Game/Lore/Lore.h"

#include <stdexcept>

#include <nlohmann/json.hpp>

namespace koa {

bool cumpre(const RequisitoTitulo& r, const SituacaoTitulos& s) {
    for (const auto& m : r.marcos)
        if (!s.marcos || !s.marcos->contains(m)) return false;
    return s.construcoes >= r.construcoesMin && s.tipos >= r.tiposMin && s.abates >= r.abatesMin && s.populacao >= r.populacaoMin &&
           s.era >= r.eraMin && s.pistas >= r.pistasMin;
}

const PistaDef* Lore::pista(const std::string& id) const {
    for (const auto& p : pistas)
        if (p.id == id) return &p;
    return nullptr;
}

const TituloDef* Lore::titulo(const std::string& id) const {
    for (const auto& t : titulos)
        if (t.id == id) return &t;
    return nullptr;
}

namespace {

Custo lerCusto(const nlohmann::json& j) {
    Custo c;
    for (const auto& [id, q] : j.items()) c[id] = q.get<int>();
    return c;
}

}

std::vector<ItemLoja> lerLoja(const nlohmann::json& j) {
    std::vector<ItemLoja> v;
    for (const auto& i : j) {
        ItemLoja item;
        item.id = i.at("id").get<std::string>();
        item.nome = i.value("nome", item.id);
        if (i.contains("custo")) item.custo = lerCusto(i["custo"]);
        if (i.contains("ganho")) item.ganho = lerCusto(i["ganho"]);
        item.pista = i.value("pista", "");
        v.push_back(std::move(item));
    }
    return v;
}

bool lerLore(const nlohmann::json& jc, const nlohmann::json& jt, const nlohmann::json& jp, Lore& saida, std::string* erro) {
    try {
        const nlohmann::json paginas = jc.at("paginas");  // variável: .items() de temporário quebra
        for (const auto& [marco, texto] : paginas.items()) saida.paginas[marco] = texto.get<std::string>();
        for (const auto& t : jt.at("titulos")) {
            TituloDef d;
            d.id = t.at("id").get<std::string>();
            d.nome = t.at("nome").get<std::string>();
            d.descricao = t.value("descricao", "");
            const nlohmann::json r = t.value("requer", nlohmann::json::object());
            d.requer.marcos = r.value("marcos", std::vector<std::string>{});
            d.requer.construcoesMin = r.value("construcoes_min", 0);
            d.requer.tiposMin = r.value("tipos_min", 0);
            d.requer.abatesMin = r.value("abates_min", 0);
            d.requer.populacaoMin = r.value("populacao_min", 0);
            d.requer.eraMin = r.value("era_min", 0);
            d.requer.pistasMin = r.value("pistas_min", 0);
            saida.titulos.push_back(std::move(d));
        }
        if (saida.titulos.empty()) throw std::runtime_error("nenhum título definido");
        for (const auto& p : jp.at("pistas")) {
            PistaDef d;
            d.id = p.at("id").get<std::string>();
            d.titulo = p.at("titulo").get<std::string>();
            d.regiao = p.value("regiao", "");
            if (p.contains("tile")) d.tile = {p["tile"][0].get<int>(), p["tile"][1].get<int>()};
            d.requer = p.value("requer", "");
            d.texto = p.at("texto").get<std::vector<std::string>>();
            d.ato = std::max(1, p.value("ato", 1));
            d.fonte = p.value("fonte", "");
            if (d.texto.empty()) throw std::runtime_error("pista sem texto: " + d.id);
            saida.pistas.push_back(std::move(d));
        }
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("Data/Lore: ") + ex.what();
        return false;
    }
    return true;
}

std::string preencher(std::string texto, const std::map<std::string, std::string>& valores) {
    for (const auto& [chave, valor] : valores) {
        const std::string marca = "{" + chave + "}";
        for (auto p = texto.find(marca); p != std::string::npos; p = texto.find(marca, p + valor.size()))
            texto.replace(p, marca.size(), valor);
    }
    return texto;
}

}
