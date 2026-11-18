// Parte do World: o povo com nome (quem chega, quem parte, os desejos) e o clima de cada dia (fase 18)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

namespace {

std::string nomeProfissao(const DadosPovo& d, const Habitante& h) {
    const ProfissaoDef* p = d.profissao(h.profissao);
    return p ? p->nome : "sem ofício";
}

// "Aldo (lenhador), Rosa (lavrador) e Ivo (pescador)"
std::string listaDePessoas(const DadosPovo& d, const std::vector<const Habitante*>& v) {
    std::string t;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) t += i + 1 == v.size() ? " e " : ", ";
        t += std::format("{} ({})", v[i]->nome, nomeProfissao(d, *v[i]));
    }
    return t;
}

}

const Habitante* World::habitante(int id) const {
    const auto it = std::ranges::find(povo, id, &Habitante::id);
    return it == povo.end() ? nullptr : &*it;
}

std::vector<const Habitante*> World::parentes(const Habitante& h) const {
    std::vector<const Habitante*> v;
    for (const Habitante& o : povo)
        if (o.familia == h.familia && o.id != h.id) v.push_back(&o);
    return v;
}

int World::moradoresComNome() const {
    return static_cast<int>(std::ranges::count_if(dados().npcs, [](const auto& n) { return !n.second.visitante(); }));
}

const TempoDef& World::tempoDoDia(int dia) const { return koa::tempoDoDia(dados().clima, dia, sementeBase_); }

const TempoDef& World::tempoAqui() const {
    static const TempoDef limpo{"limpo", "céu limpo", ""};
    return infoAtual().subterranea ? limpo : tempoHoje();
}

bool World::desejoCumprido(const DesejoDef& d) const {
    const CondicaoDesejo& c = d.condicao;
    if (c.tipo == "obra")
        return std::ranges::any_of(reino.construcoes, [&](const BuildingData& b) { return b.defId == c.alvo && b.constructed && b.level >= c.nivel; });
    if (c.tipo == "marco") return marcos.contains(c.alvo);
    if (c.tipo == "felicidade") return estado.felicidade >= c.valor;
    if (c.tipo == "seguranca") return resumo().seguranca >= estado.populacao;
    if (c.tipo == "comida_dias") return inventario.quantidade("comida") >= c.valor * std::max(1, resumo().comidaConsumida);
    if (c.tipo == "castelo") return nivelCastelo() >= static_cast<int>(c.valor);
    if (c.tipo == "brasao") return brasaoCriado;
    return false;
}

void World::gerarPovoInicial() {
    povo.clear();
    proximoHabitante = 1;
    std::vector<std::string> notas;
    ajustarPovo(false, notas);
}

// O povo com nome acompanha a população: quem chega forma uma família; quem sai é quem mais esperava
void World::ajustarPovo(bool fome, std::vector<std::string>& notas) {
    const DadosPovo& dp = dados().povo;
    const int alvo = std::max(0, estado.populacao - moradoresComNome());
    if (static_cast<int>(povo.size()) < alvo) {
        std::set<std::string> obras;
        for (const auto& b : reino.construcoes)
            if (b.constructed) obras.insert(b.defId);
        std::string familia;
        int restamNaFamilia = 0;
        std::vector<const Habitante*> chegaram;
        const size_t antes = povo.size();
        std::vector<std::string> familias;
        while (static_cast<int>(povo.size()) < alvo) {
            const int id = proximoHabitante++;
            // Quem chega junto vem em famílias de 2 a 4 pessoas
            if (restamNaFamilia-- <= 0) {
                familia = sobrenomeSorteado(dp, misturar(sementeBase_, static_cast<uint32_t>(id * 131 + relogio.dia)));
                restamNaFamilia = 1 + static_cast<int>(misturar(sementeBase_, static_cast<uint32_t>(id * 17)) % 3u);
                familias.push_back(familia);
            }
            Habitante h = gerarHabitante(dp, id, familia, relogio.dia, misturar(sementeBase_, static_cast<uint32_t>(id)), obras);
            // Ninguém chega desejando o que o reino já tem: sorteia de novo entre os desejos que faltam
            for (uint32_t k = 0; k < 12; ++k) {
                const DesejoDef* d = dp.desejo(h.desejo);
                if (!d || !desejoCumprido(*d)) break;
                h.desejo = dp.desejos[misturar(sementeBase_ ^ static_cast<uint32_t>(id), 100u + k) % dp.desejos.size()].id;
            }
            povo.push_back(std::move(h));
        }
        for (size_t i = antes; i < povo.size(); ++i) chegaram.push_back(&povo[i]);
        // As notas genéricas de chegada dão lugar aos nomes
        std::erase_if(notas, [](const std::string& n) { return n.ends_with("chegou ao vilarejo.") || n.ends_with("chegaram ao vilarejo."); });
        if (familias.size() == 1 && chegaram.size() <= 4) notas.push_back(std::format("Chegou a família {}: {}.", familia, listaDePessoas(dp, chegaram)));
        else notas.push_back(std::format("Chegaram {} pessoas de {} famílias ({}...).", chegaram.size(), familias.size(), familias.front()));
        return;
    }
    if (static_cast<int>(povo.size()) <= alvo) return;
    // Quem vai embora: quem mais esperou o desejo (pela paciência), e entre iguais quem chegou por último
    auto vontadeDeIr = [&](const Habitante& h) {
        const TracoDef* t = dp.traco(h.traco);
        return h.cumprido ? 0.0f : static_cast<float>(h.esperando) / std::max(1, t ? t->paciencia : 10);
    };
    std::vector<int> ordem(povo.size());
    for (size_t i = 0; i < povo.size(); ++i) ordem[i] = static_cast<int>(i);
    std::ranges::stable_sort(ordem, [&](int a, int b) {
        const float va = vontadeDeIr(povo[a]), vb = vontadeDeIr(povo[b]);
        return va != vb ? va > vb : povo[a].chegou > povo[b].chegou;
    });
    const size_t saem = povo.size() - static_cast<size_t>(alvo);
    const std::string motivo = fome ? "faltou comida" : estado.felicidade < 25.0f ? "estava descontente" : "não havia casa para todos";
    std::vector<int> ids;
    for (size_t k = 0; k < saem; ++k) ids.push_back(povo[ordem[k]].id);
    for (size_t k = 0; k < ids.size() && k < 3; ++k)
        if (const Habitante* h = habitante(ids[k])) notas.push_back(std::format("{} ({}) foi embora: {}.", h->nomeCompleto(), nomeProfissao(dp, *h), motivo));
    if (ids.size() > 3) notas.push_back(std::format("E mais {} pessoas foram embora.", ids.size() - 3));
    std::erase_if(povo, [&](const Habitante& h) { return std::ranges::find(ids, h.id) != ids.end(); });
}

void World::manhaDoPovo(std::vector<std::string>& notas) {
    const DadosPovo& dp = dados().povo;
    // O tempo de hoje: chuva e tempestade regam a horta
    const TempoDef& t = tempoHoje();
    if (t.rega) {
        for (auto& c : canteiros)
            if (!c.cultura.empty() && !c.maduro) c.regadoDia = relogio.dia;
        atualizarCanteiros();
    }
    if (!t.aviso.empty()) notas.push_back(t.aviso);
    // Desejos que se realizaram durante a noite
    int contados = 0;
    for (Habitante& h : povo) {
        if (h.cumprido) continue;
        const DesejoDef* d = dp.desejo(h.desejo);
        if (d && desejoCumprido(*d)) {
            h.cumprido = true;
            marcar(std::format("desejo_{}:{}", d->id, h.id), h.nomeCompleto());
            if (contados++ < 2) notas.push_back(std::format("{} realizou um desejo: {}.", h.nomeCompleto(), d->texto));
        } else {
            ++h.esperando;
        }
    }
    if (contados > 2) notas.push_back(std::format("E mais {} desejos se realizaram.", contados - 2));
    // Com o povo infeliz, quem esperou mais que a paciência desiste (uma pessoa por noite)
    if (estado.felicidade >= 50.0f) return;
    const Habitante* sai = nullptr;
    int excesso = 0;
    for (const Habitante& h : povo) {
        const TracoDef* tr = dp.traco(h.traco);
        const int passou = h.esperando - (tr ? tr->paciencia : 10);
        if (!h.cumprido && passou > excesso) excesso = passou, sai = &h;
    }
    if (!sai) return;
    const DesejoDef* d = dp.desejo(sai->desejo);
    notas.push_back(std::format("{} ({}) foi embora: cansou de esperar {}.", sai->nomeCompleto(), nomeProfissao(dp, *sai), d ? d->texto : "dias melhores"));
    marcar(std::format("partiu:{}", sai->id), sai->nomeCompleto());
    const int id = sai->id;
    std::erase_if(povo, [&](const Habitante& h) { return h.id == id; });
    estado.populacao = std::max(3, estado.populacao - 1);
}

}
