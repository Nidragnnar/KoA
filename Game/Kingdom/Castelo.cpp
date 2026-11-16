#include "Game/Kingdom/Castelo.h"

#include <algorithm>
#include <cmath>
#include <format>

#include <nlohmann/json.hpp>

namespace koa {

namespace {

glm::vec3 corHex(const std::string& h) {
    if (h.size() != 7 || h[0] != '#') return glm::vec3(1.0f);
    const unsigned v = static_cast<unsigned>(std::stoul(h.substr(1), nullptr, 16));
    return glm::vec3((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF) / 255.0f;
}

// "Goles" → "goles" (os nomes das tinturas começam com letra comum ou acentuada só depois da primeira)
std::string minuscula(std::string s) {
    if (!s.empty() && s[0] >= 'A' && s[0] <= 'Z') s[0] = static_cast<char>(s[0] - 'A' + 'a');
    return s;
}

// A peça da linha do editor (vale para brasão constante ou não)
template <class B>
auto escolhido(B& b, int linha) -> decltype(&b.forma) {
    switch (linha) {
    case 0: return &b.forma;
    case 1: return &b.divisao;
    case 2: return &b.cor1;
    case 3: return &b.cor2;
    case 4: return &b.simbolo;
    case 5: return &b.corSimbolo;
    default: return nullptr;
    }
}

}

EfeitosAla& EfeitosAla::operator+=(const EfeitosAla& o) {
    felicidade += o.felicidade;
    impostos += o.impostos;
    comida += o.comida;
    seguranca += o.seguranca;
    viajantes += o.viajantes;
    ouroDia += o.ouroDia;
    soldados += o.soldados;
    expedicao += o.expedicao;
    return *this;
}

const AlaDef* DadosCastelo::ala(const std::string& id) const {
    const auto it = std::ranges::find(alas, id, &AlaDef::id);
    return it == alas.end() ? nullptr : &*it;
}

const Tintura* DadosCastelo::tintura(const std::string& id) const {
    const auto it = std::ranges::find(tinturas, id, &Tintura::id);
    return it == tinturas.end() ? nullptr : &*it;
}

int DadosCastelo::vagas(int nivelCastelo) const {
    if (vagasPorNivel.empty()) return 0;
    return vagasPorNivel[std::clamp<size_t>(static_cast<size_t>(std::max(0, nivelCastelo)), 0, vagasPorNivel.size() - 1)];
}

std::vector<std::string> DadosCastelo::opcoes(int linha) const {
    std::vector<std::string> v;
    auto ids = [&](const auto& lista) {
        for (const auto& p : lista) v.push_back(p.id);
    };
    switch (linha) {
    case 0: ids(formas); break;
    case 1: ids(divisoes); break;
    case 2:
    case 3:
    case 5: ids(tinturas); break;
    case 4: ids(simbolos); break;
    default: break;
    }
    return v;
}

std::string DadosCastelo::nomePeca(int linha, const std::string& id) const {
    auto nome = [&](const auto& lista) -> std::string {
        for (const auto& p : lista)
            if (p.id == id) return p.nome;
        return id;
    };
    switch (linha) {
    case 0: return nome(formas);
    case 1: return nome(divisoes);
    case 2:
    case 3:
    case 5: return nome(tinturas);
    case 4: return nome(simbolos);
    default: return id;
    }
}

Brasao DadosCastelo::validar(Brasao b) const {
    for (int linha = 0; linha < 6; ++linha) {
        const auto ops = opcoes(linha);
        std::string& atual = *escolhido(b, linha);
        if (std::ranges::find(ops, atual) == ops.end()) atual = *escolhido(inicial, linha);
    }
    if (b.lema < 0 || b.lema >= static_cast<int>(lemas.size())) b.lema = 0;
    return b;
}

void girarPeca(const DadosCastelo& d, Brasao& b, int linha, int passo) {
    if (linha == 6) {
        const int n = static_cast<int>(d.lemas.size());
        if (n > 0) b.lema = ((b.lema + passo) % n + n) % n;
        return;
    }
    const auto ops = d.opcoes(linha);
    if (ops.empty()) return;
    std::string& atual = *escolhido(b, linha);
    const auto it = std::ranges::find(ops, atual);
    const int i = it == ops.end() ? 0 : static_cast<int>(it - ops.begin());
    const int n = static_cast<int>(ops.size());
    atual = ops[((i + passo) % n + n) % n];
}

std::string textoBrasao(const DadosCastelo& d, const Brasao& b) {
    auto cor = [&](const std::string& id) { return minuscula(d.nomePeca(2, id)); };
    const std::string c1 = cor(b.cor1), c2 = cor(b.cor2);
    std::string campo;
    if (b.divisao == "partido") campo = std::format("Partido de {} e {}", c1, c2);
    else if (b.divisao == "cortado") campo = std::format("Cortado de {} e {}", c1, c2);
    else if (b.divisao == "esquartelado") campo = std::format("Esquartelado de {} e {}", c1, c2);
    else if (b.divisao == "banda") campo = std::format("De {} com uma banda de {}", c1, c2);
    else if (b.divisao == "chefe") campo = std::format("De {} com chefe de {}", c1, c2);
    else campo = "De " + c1;
    return std::format("{}, {} de {}", campo, d.nomePeca(4, b.simbolo), cor(b.corSimbolo));
}

std::string textoAla(const EfeitosAla& e) {
    std::string t;
    auto junta = [&](const std::string& s) { t += (t.empty() ? "" : ", ") + s; };
    if (e.felicidade != 0.0f) junta(std::format("+{:.0f} de felicidade", e.felicidade));
    if (e.seguranca) junta(std::format("+{} de segurança", e.seguranca));
    if (e.impostos != 0.0f) junta(std::format("+{:.0f}% de impostos", e.impostos * 100.0f));
    if (e.comida != 0.0f) junta(std::format("+{:.0f}% de comida", e.comida * 100.0f));
    if (e.ouroDia) junta(std::format("+{} de ouro por dia", e.ouroDia));
    if (e.viajantes) junta(std::format("+{} família chegando", e.viajantes));
    if (e.soldados) junta(std::format("+{} soldado no portão", e.soldados));
    if (e.expedicao) junta(std::format("+{} de força nas expedições", e.expedicao));
    return t.empty() ? "nada" : t;
}

bool lerCastelo(const nlohmann::json& j, DadosCastelo& d, std::string* erro) {
    try {
        d.vagasPorNivel = j.value("vagas_por_nivel", d.vagasPorNivel);
        for (const auto& ja : j.at("alas")) {
            AlaDef a;
            a.id = ja.at("id").get<std::string>();
            a.nome = ja.value("nome", a.id);
            a.descricao = ja.value("descricao", "");
            a.custo = ja.value("custo", Custo{});
            const auto& je = ja.value("efeitos", nlohmann::json::object());
            a.efeitos.felicidade = je.value("felicidade", 0.0f);
            a.efeitos.impostos = je.value("impostos", 0.0f);
            a.efeitos.comida = je.value("comida", 0.0f);
            a.efeitos.seguranca = je.value("seguranca", 0);
            a.efeitos.viajantes = je.value("viajantes", 0);
            a.efeitos.ouroDia = je.value("ouro_dia", 0);
            a.efeitos.soldados = je.value("soldados", 0);
            a.efeitos.expedicao = je.value("expedicao", 0);
            d.alas.push_back(std::move(a));
        }
        const auto& jb = j.at("brasao");
        auto pecas = [&](const char* nome, std::vector<PecaBrasao>& v) {
            for (const auto& p : jb.at(nome)) v.push_back({p.at("id").get<std::string>(), p.value("nome", p.at("id").get<std::string>())});
        };
        pecas("formas", d.formas);
        pecas("divisoes", d.divisoes);
        pecas("simbolos", d.simbolos);
        for (const auto& t : jb.at("tinturas")) d.tinturas.push_back({t.at("id").get<std::string>(), t.value("nome", ""), corHex(t.value("cor", ""))});
        d.lemas = jb.at("lemas").get<std::vector<std::string>>();
        const auto& ji = jb.at("inicial");
        d.inicial = {ji.value("forma", "classico"), ji.value("divisao", "pleno"), ji.value("cor1", "goles"), ji.value("cor2", "ouro"),
                     ji.value("simbolo", "coroa"),  ji.value("cor_simbolo", "ouro"), ji.value("lema", 0)};
        if (d.formas.empty() || d.divisoes.empty() || d.simbolos.empty() || d.tinturas.size() < 2 || d.lemas.empty()) {
            if (erro) *erro = "castelo.json: o brasão precisa de formas, divisões, símbolos, tinturas e lemas";
            return false;
        }
        if (d.validar(d.inicial) != d.inicial) {
            if (erro) *erro = "castelo.json: o brasão inicial usa uma peça que não existe";
            return false;
        }
    } catch (const std::exception& e) {
        if (erro) *erro = std::string("castelo.json: ") + e.what();
        return false;
    }
    return true;
}

}
