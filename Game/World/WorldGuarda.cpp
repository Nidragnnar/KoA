// Parte do World: a Guarda Real (esquadras do Quartel, postos fora do castelo, soldo e as lutas da noite) (versão 1.15)
#include "Game/World/WorldInterno.h"

namespace koa {

using namespace interno;

bool World::guardaDisponivel() const { return std::ranges::find(alas, dados().guarda.requerAla) != alas.end(); }

Esquadra* World::esquadra(int id) {
    const auto it = std::ranges::find(esquadras, id, &Esquadra::id);
    return it == esquadras.end() ? nullptr : &*it;
}

BonusGuarda World::bonusGuarda(const Esquadra& e) const {
    BonusGuarda b;
    b.nivelForja = nivelForja();
    if (const std::string m = ocupante("marechal"); !m.empty()) {
        const auto it = lealdade.find(m);
        const int l = it == lealdade.end() ? 50 : it->second;
        b.marechal = l >= 50 ? 0.15f : l < regra::LEALDADE_INSATISFEITO ? -0.15f : 0.0f;
    }
    // O herdeiro lutou lá ontem (o dia em que foi dormir): a esquadra se anima
    if (e.posto.starts_with("regiao:"))
        if (const auto it = estadoRegioes.find(e.posto.substr(7)); it != estadoRegioes.end()) b.heroiPresente = it->second.ultimaVisita >= relogio.dia - 1;
    return b;
}

float World::forcaDaEsquadra(const Esquadra& e) const { return forcaEsquadra(dados().guarda, e, bonusGuarda(e)); }

std::string World::podeFormar(const std::string& tipo) const {
    const DadosGuarda& d = dados().guarda;
    const TipoEsquadra* t = d.tipo(tipo);
    if (!t) return "Tipo desconhecido.";
    if (!guardaDisponivel()) return "Erga o Quartel Real no castelo.";
    if (static_cast<int>(esquadras.size()) >= d.esquadrasMax) return std::format("No máximo {} esquadras.", d.esquadrasMax);
    if (nivelForja() < t->requerForja) return std::format("Pede a forja no nível {}.", t->requerForja);
    if (!inventario.tem(t->custo)) return "Faltam recursos.";
    return "";
}

bool World::formarEsquadra(const std::string& tipo) {
    const std::string falta = podeFormar(tipo);
    if (!falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, falta});
        return false;
    }
    const DadosGuarda& d = dados().guarda;
    inventario.pagar(d.tipo(tipo)->custo);
    Esquadra e;
    e.id = proximaEsquadra++;
    e.tipo = tipo;
    e.nome = d.nomes[static_cast<size_t>(e.id - 1) % d.nomes.size()];
    e.soldados = d.soldadosMax;
    esquadras.push_back(e);
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0, std::format("{} formada: {} {}.", e.nome, e.soldados, d.tipo(tipo)->nome)});
    marcar("guarda_formada", e.nome);
    return true;
}

std::vector<std::string> World::postosPossiveis() const {
    std::vector<std::string> v{"reserva", "escolta"};
    for (const auto& id : regioesComVida())
        if (visitadas.contains(id)) v.push_back("regiao:" + id);
    if (mundoVivoAtivo())
        for (const AldeiaDef& a : dados().mundo.aldeias)
            if (const auto it = mundoVivo.aldeias.find(a.id); it != mundoVivo.aldeias.end() && !it->second.sumiu) v.push_back("aldeia:" + a.id);
    return v;
}

std::string World::nomePosto(const std::string& posto) const {
    if (posto == "reserva") return "reserva no portão";
    if (posto == "escolta") return "escolta de caravanas e expedições";
    if (posto.starts_with("regiao:")) return "guardando " + nomeRegiao(posto.substr(7));
    if (posto.starts_with("aldeia:"))
        if (const AldeiaDef* a = dados().mundo.aldeia(posto.substr(7))) return "protegendo " + a->nome;
    return posto;
}

bool World::proximoPosto(int id) {
    Esquadra* e = esquadra(id);
    if (!e) return false;
    const auto postos = postosPossiveis();
    const auto it = std::ranges::find(postos, e->posto);
    e->posto = postos[it == postos.end() ? 0 : (static_cast<size_t>(it - postos.begin()) + 1) % postos.size()];
    return true;
}

bool World::definirPosto(int id, const std::string& posto) {
    Esquadra* e = esquadra(id);
    const auto postos = postosPossiveis();
    if (!e || std::ranges::find(postos, posto) == postos.end()) return false;
    e->posto = posto;
    return true;
}

Custo World::custoTreino(const Esquadra& e) const {
    Custo c = dados().guarda.treino;
    for (auto& [r, q] : c) q *= e.nivel + 1;
    return c;
}

Custo World::custoReforco(const Esquadra& e) const {
    Custo c = dados().guarda.reforco;
    const int faltam = dados().guarda.soldadosMax - e.soldados;
    for (auto& [r, q] : c) q *= std::max(0, faltam);
    return c;
}

bool World::treinarEsquadra(int id) {
    Esquadra* e = esquadra(id);
    if (!e) return false;
    std::string falta = e->nivel >= dados().guarda.nivelMax ? "Já treinou tudo o que podia." : !inventario.tem(custoTreino(*e)) ? "Faltam recursos." : "";
    if (!falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, falta});
        return false;
    }
    inventario.pagar(custoTreino(*e));
    ++e->nivel;
    eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "bom", "", 0, std::format("{} treinou: nível {}.", e->nome, e->nivel)});
    return true;
}

bool World::reforcarEsquadra(int id) {
    Esquadra* e = esquadra(id);
    if (!e) return false;
    const std::string falta = e->soldados >= dados().guarda.soldadosMax ? "A esquadra está completa." : !inventario.tem(custoReforco(*e)) ? "Faltam recursos." : "";
    if (!falta.empty()) {
        eventos_.push_back({EventoJogo::Tipo::Aviso, jogador.pos, "", "", 0, falta});
        return false;
    }
    inventario.pagar(custoReforco(*e));
    e->soldados = dados().guarda.soldadosMax;
    return true;
}

bool World::dispensarEsquadra(int id) { return std::erase_if(esquadras, [&](const Esquadra& e) { return e.id == id; }) > 0; }

bool World::regiaoGuardada(const std::string& regiao) const {
    return std::ranges::any_of(esquadras, [&](const Esquadra& e) { return e.posto == "regiao:" + regiao && e.soldados > 0; });
}

bool World::aldeiaProtegida(const std::string& aldeia) const {
    return std::ranges::any_of(esquadras, [&](const Esquadra& e) { return e.posto == "aldeia:" + aldeia && e.soldados > 0; });
}

float World::forcaNoPosto(const std::string& posto) const {
    float f = 0.0f;
    for (const Esquadra& e : esquadras)
        if (e.posto == posto) f += forcaDaEsquadra(e);
    return f;
}

// Depois da virada do dia: o soldo (ou a deserção), o que cada posto faz e as lutas nas regiões guardadas
void World::noiteDaGuarda(std::vector<Noticia>& noticias) {
    if (esquadras.empty()) return;
    const DadosGuarda& d = dados().guarda;
    const Custo soldo = soldoDaNoite(d, esquadras);
    if (inventario.tem(soldo)) {
        inventario.pagar(soldo);
        for (const auto& [r, q] : soldo) ultimoRelatorio.variacoes.push_back({r + "|soldo da guarda", -q});
    } else {
        for (Esquadra& e : esquadras) e.soldados = std::max(0, e.soldados - 1);
        noticias.push_back({8, "Sem soldo, um soldado de cada esquadra desertou da Guarda Real."});
    }
    for (Esquadra& e : esquadras) {
        if (e.soldados <= 0) continue;
        const float f = forcaDaEsquadra(e);
        if (e.posto.starts_with("regiao:")) {
            const std::string regiao = e.posto.substr(7);
            const auto it = estadoRegioes.find(regiao);
            if (it == estadoRegioes.end()) continue;
            EstadoRegiao& er = it->second;
            er.corrupcao = std::max(transformacao(regiao) ? transformacao(regiao)->pisoCorrupcao : 0.0f, er.corrupcao - f * d.limpeza);
            er.seguranca = std::min(100.0f, er.seguranca + f * d.seguranca);
            // Com a terra ainda escura, às vezes há luta: a conta vai para as notícias
            if (sorteMundo(sementeBase_, relogio.dia, "guarda" + std::to_string(e.id)) < d.chanceLuta * er.corrupcao / 100.0f) {
                const ResultadoLuta r = lutaNaRegiao(d, f, er.corrupcao);
                e.soldados = std::max(0, e.soldados - r.perdas);
                if (r.vitoria) er.corrupcao = std::max(0.0f, er.corrupcao - 3.0f);
                noticias.push_back({4, std::format("{} lutou em {}: força {:.0f} contra {:.0f}, {}{}.", e.nome, nomeRegiao(regiao), r.forca, r.ameaca,
                                                   r.vitoria ? "vitória" : "derrota",
                                                   r.perdas ? std::format(" ({} baixa{})", r.perdas, r.perdas > 1 ? "s" : "") : "")});
            }
        } else if (e.posto.starts_with("aldeia:")) {
            if (const auto it = mundoVivo.aldeias.find(e.posto.substr(7)); it != mundoVivo.aldeias.end() && !it->second.sumiu)
                it->second.prosperidade = std::min(100.0f, it->second.prosperidade + d.protecaoProsperidade);
        }
    }
    // Esquadra sem ninguém se desfaz; posto que deixou de existir (aldeia abandonada) volta para a reserva
    for (const Esquadra& e : esquadras)
        if (e.soldados <= 0) noticias.push_back({7, e.nome + " foi desfeita: não sobrou ninguém."});
    std::erase_if(esquadras, [](const Esquadra& e) { return e.soldados <= 0; });
    const auto postos = postosPossiveis();
    for (Esquadra& e : esquadras)
        if (std::ranges::find(postos, e.posto) == postos.end()) e.posto = "reserva";
}

}
