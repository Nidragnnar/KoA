#include "Game/Kingdom/Conselho.h"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace koa {

const OpcaoDecisao* DecisaoDef::opcao(const std::string& qual) const {
    for (const auto& o : opcoes)
        if (o.id == qual) return &o;
    return nullptr;
}

const CargoDef* DadosConselho::cargo(const std::string& id) const {
    for (const auto& c : cargos)
        if (c.id == id) return &c;
    return nullptr;
}

const DecisaoDef* DadosConselho::decisao(const std::string& id) const {
    for (const auto& d : decisoes)
        if (d.id == id) return &d;
    return nullptr;
}

float fatorLealdade(int l) { return l >= 50 ? 1.0f : l >= regra::LEALDADE_INSATISFEITO ? 0.6f : 0.0f; }

const char* palavraLealdade(int l) {
    if (l >= 70) return "Leal";
    if (l >= 50) return "Satisfeito";
    if (l >= regra::LEALDADE_INSATISFEITO) return "Desconfiado";
    return "Insatisfeito";
}

int lealdadeInicial(int afinidade) { return std::clamp(40 + afinidade / 20, 0, 90); }

float efeitoNoCargo(const CargoDef& c, const std::string& npc, int l) { return c.forca * c.aptidaoDe(npc) * fatorLealdade(l); }

namespace {

EfeitosDecisao lerEfeitos(const nlohmann::json& j) {
    EfeitosDecisao e;
    for (const auto& [chave, v] : j.items()) {
        if (chave == "felicidade") e.felicidade = v.get<float>();
        else if (chave == "populacao") e.populacao = v.get<int>();
        else if (chave == "lei") e.lei = v.get<std::string>();
        else if (chave == "revogar") e.revogar = v.get<std::string>();
        else if (chave == "imposto") e.imposto = std::clamp(v.get<int>(), 0, 2);
        else if (chave == "lealdade")
            for (const auto& [c, n] : v.items()) e.lealdade[c] = n.get<int>();
        else if (chave == "amizade")
            for (const auto& [m, n] : v.items()) e.amizade[m] = n.get<int>();
        else if (chave == "corrupcao")
            for (const auto& [r, n] : v.items()) e.corrupcao[r] = n.get<float>();
        else e.recursos[chave] = v.get<int>();  // madeira, pedra, ouro, comida, cristal
    }
    return e;
}

}

bool lerConselho(const nlohmann::json& j, DadosConselho& d, std::string* erro) {
    try {
        d.obrasParaFormar = j.value("obras_para_formar", 4);
        d.diasEntreDecisoes = j.value("dias_entre_decisoes", 3);
        for (const auto& jc : j.at("cargos")) {
            CargoDef c;
            c.id = jc.at("id").get<std::string>();
            c.nome = jc.at("nome").get<std::string>();
            c.descricao = jc.value("descricao", "");
            c.efeito = jc.at("efeito").get<std::string>();
            if (c.efeito != "seguranca" && c.efeito != "impostos" && c.efeito != "comida" && c.efeito != "obras")
                throw std::runtime_error("cargo " + c.id + " com efeito desconhecido: " + c.efeito);
            c.forca = jc.at("forca").get<float>();
            const nlohmann::json apt = jc.value("aptidao", nlohmann::json::object());  // variável: .items() de temporário quebra
            for (const auto& [npc, a] : apt.items()) c.aptidao[npc] = a.get<float>();
            d.cargos.push_back(std::move(c));
        }
        const nlohmann::json leis = j.value("leis", nlohmann::json::object());
        for (const auto& [id, jl] : leis.items()) {
            LeiDef l;
            l.id = id;
            l.nome = jl.at("nome").get<std::string>();
            l.felicidade = jl.value("felicidade", 0.0f);
            l.ouroDia = jl.value("ouro_dia", 0);
            l.seguranca = jl.value("seguranca", 0);
            d.leis[id] = l;
        }
        for (const auto& jd : j.at("decisoes")) {
            DecisaoDef dd;
            dd.id = jd.at("id").get<std::string>();
            dd.titulo = jd.at("titulo").get<std::string>();
            dd.texto = jd.value("texto", std::vector<std::string>{});
            dd.repetir = jd.value("repetir", false);
            dd.manual = jd.value("manual", false);
            if (jd.contains("requer")) {
                const auto& r = jd["requer"];
                dd.requer.populacaoMin = r.value("populacao_min", 0);
                dd.requer.eraMin = r.value("era_min", 0);
                dd.requer.impostoMin = r.value("imposto_min", -1);
                dd.requer.marco = r.value("marco", "");
                dd.requer.semMarco = r.value("sem_marco", "");
                dd.requer.semLei = r.value("sem_lei", "");
            }
            for (const auto& jo : jd.at("opcoes")) {
                OpcaoDecisao o;
                o.id = jo.at("id").get<std::string>();
                o.nome = jo.at("nome").get<std::string>();
                o.descricao = jo.value("descricao", "");
                const nlohmann::json efeitos = jo.value("efeitos", nlohmann::json::object());
                o.efeitos = lerEfeitos(efeitos);
                for (const std::string* lei : {&o.efeitos.lei, &o.efeitos.revogar})
                    if (!lei->empty() && !d.leis.contains(*lei)) throw std::runtime_error("decisão " + dd.id + " usa lei inexistente: " + *lei);
                for (const auto& [c, n] : o.efeitos.lealdade)
                    if (!d.cargo(c)) throw std::runtime_error("decisão " + dd.id + " mexe na lealdade de cargo inexistente: " + c);
                dd.opcoes.push_back(std::move(o));
            }
            const nlohmann::json opinioes = jd.value("conselho", nlohmann::json::object());
            for (const auto& [cargo, jc] : opinioes.items()) {
                if (!d.cargo(cargo)) throw std::runtime_error("decisão " + dd.id + ": cargo inexistente no conselho: " + cargo);
                const std::string apoia = jc.at("apoia").get<std::string>();
                if (!dd.opcao(apoia)) throw std::runtime_error("decisão " + dd.id + ": " + cargo + " apoia opção inexistente: " + apoia);
                dd.conselho[cargo] = {apoia, jc.value("fala", "")};
            }
            if (dd.opcoes.empty()) throw std::runtime_error("decisão sem opções: " + dd.id);
            d.decisoes.push_back(std::move(dd));
        }
    } catch (const std::exception& ex) {
        if (erro) *erro = std::string("Data/Kingdom/conselho.json: ") + ex.what();
        return false;
    }
    return true;
}

}
