#include "Game/Kingdom/Linhagem.h"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "Game/Kingdom/Povo.h"

namespace koa {

EfeitosTracos& EfeitosTracos::operator+=(const EfeitosTracos& o) {
    vida += o.vida;
    dano += o.dano;
    expedicao += o.expedicao;
    descobrir += o.descobrir;
    felicidade += o.felicidade;
    taxaMercado += o.taxaMercado;
    consumo += o.consumo;
    devocaoNoite += o.devocaoNoite;
    return *this;
}

const TracoLinhagem* DadosLinhagem::traco(const std::string& id) const {
    const auto it = std::ranges::find(tracos, id, &TracoLinhagem::id);
    return it == tracos.end() ? nullptr : &*it;
}

const ConjugeDef* DadosLinhagem::conjuge(const std::string& id) const {
    const auto it = std::ranges::find_if(conjuges, [&](const ConjugeDef& c) { return c.id() == id; });
    return it == conjuges.end() ? nullptr : &*it;
}

bool lerLinhagem(const nlohmann::json& j, DadosLinhagem& d, std::string* erro) {
    try {
        const auto& f = j.at("fundador");
        d.fundadorNome = f.value("nome", d.fundadorNome);
        d.fundadorTracos = f.value("tracos", std::vector<std::string>{});
        d.amizadeMin = j.value("amizade_min", d.amizadeMin);
        d.relacaoMin = j.value("relacao_min", d.relacaoMin);
        d.festa = j.value("festa", Custo{});
        d.gestacao = std::max(1, j.value("gestacao", d.gestacao));
        d.entreFilhos = std::max(1, j.value("entre_filhos", d.entreFilhos));
        d.filhosMax = std::max(1, j.value("filhos_max", d.filhosMax));
        d.maioridade = std::max(1, j.value("maioridade", d.maioridade));
        d.tutorLealdade = j.value("tutor_lealdade", d.tutorLealdade);
        d.saltoAnos = std::max(1, j.value("salto_anos", d.saltoAnos));
        d.saltoMemoria = std::clamp(j.value("salto_memoria", d.saltoMemoria), 0.0f, 1.0f);
        d.reinadoVelho = j.value("reinado_velho", d.reinadoVelho);
        d.reinadoMax = std::max(d.reinadoVelho, j.value("reinado_max", d.reinadoMax));
        for (const auto& t : j.at("tracos")) {
            TracoLinhagem x;
            x.id = t.at("id").get<std::string>();
            x.nome = t.value("nome", x.id);
            x.descricao = t.value("descricao", "");
            x.ruim = t.value("ruim", false);
            x.efeitos = {t.value("vida", 0), t.value("dano", 0), t.value("expedicao", 0), t.value("descobrir", 0),
                         t.value("felicidade", 0.0f), t.value("taxa_mercado", 0.0f), t.value("consumo", 0.0f), t.value("devocao_noite", 0.0f)};
            d.tracos.push_back(std::move(x));
        }
        for (const auto& c : j.at("conjuges"))
            d.conjuges.push_back({c.value("npc", ""), c.value("faccao", ""), c.value("nome", ""), c.at("traco").get<std::string>(), c.value("dote", Custo{})});
        d.tutores = j.value("tutores", std::map<std::string, std::string>{});
        for (const auto& r : j.value("reacoes_traco", nlohmann::json::array()))
            for (const auto& [k, v] : r.items())
                if (k != "traco") d.reacoesTraco[r.at("traco").get<std::string>()][k] = v.get<float>();
    } catch (const std::exception& e) {
        if (erro) *erro = std::string("linhagem.json: ") + e.what();
        return false;
    }
    // Todo traço citado precisa existir
    std::vector<std::string> citados = d.fundadorTracos;
    for (const auto& c : d.conjuges) citados.push_back(c.traco);
    for (const auto& [cargo, t] : d.tutores) citados.push_back(t);
    for (const auto& c : citados)
        if (!d.traco(c)) {
            if (erro) *erro = "linhagem.json: traço desconhecido: " + c;
            return false;
        }
    return true;
}

Membro* Linhagem::membro(int id) {
    const auto it = std::ranges::find(membros, id, &Membro::id);
    return it == membros.end() ? nullptr : &*it;
}

const Membro* Linhagem::membro(int id) const {
    const auto it = std::ranges::find(membros, id, &Membro::id);
    return it == membros.end() ? nullptr : &*it;
}

std::vector<const Membro*> Linhagem::filhosDe(int id) const {
    std::vector<const Membro*> v;
    for (const Membro& m : membros)
        if (id != 0 && (m.genitor == id || m.outroGenitor == id)) v.push_back(&m);
    return v;
}

EfeitosTracos efeitosDe(const DadosLinhagem& d, const std::vector<std::string>& tracos) {
    EfeitosTracos e;
    for (const auto& t : tracos)
        if (const TracoLinhagem* x = d.traco(t)) e += x->efeitos;
    return e;
}

std::vector<std::string> tracosDaMaioridade(const DadosLinhagem& d, const std::vector<std::string>& genitor, const std::vector<std::string>& outro,
                                            const std::string& tracoDoTutor, uint32_t sorte) {
    std::vector<std::string> v;
    // Herdado: um traço bom de um dos genitores (sem nenhum, um sorteado entre os bons)
    std::vector<std::string> herdaveis;
    for (const auto* lado : {&genitor, &outro})
        for (const auto& t : *lado)
            if (const TracoLinhagem* x = d.traco(t); x && !x->ruim) herdaveis.push_back(t);
    if (herdaveis.empty())
        for (const TracoLinhagem& t : d.tracos)
            if (!t.ruim) herdaveis.push_back(t.id);
    if (!herdaveis.empty()) v.push_back(herdaveis[misturar(sorte, 1u) % herdaveis.size()]);
    // Do tutor (se leal) ou da sorte, que pode trazer um traço ruim; nunca repete o herdado
    std::string segundo = tracoDoTutor;
    if (segundo.empty() || std::ranges::find(v, segundo) != v.end()) {
        std::vector<std::string> todos;
        for (const TracoLinhagem& t : d.tracos)
            if (std::ranges::find(v, t.id) == v.end()) todos.push_back(t.id);
        segundo = todos.empty() ? "" : todos[misturar(sorte, 2u) % todos.size()];
    }
    if (!segundo.empty()) v.push_back(segundo);
    return v;
}

std::string textoTracos(const DadosLinhagem& d, const std::vector<std::string>& tracos) {
    std::string s;
    for (const auto& t : tracos) {
        const TracoLinhagem* x = d.traco(t);
        s += (s.empty() ? "" : ", ") + (x ? x->nome : t);
    }
    return s.empty() ? "sem traços ainda" : s;
}

}
