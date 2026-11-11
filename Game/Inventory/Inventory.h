#pragma once
#include <map>
#include <string>

namespace koa {

using Custo = std::map<std::string, int>;  // recurso → quantidade

// Itens e recursos do herdeiro (no 0.1: madeira, pedra e ouro)
class Inventory {
public:
    int quantidade(const std::string& id) const;
    void adicionar(const std::string& id, int n);
    bool remover(const std::string& id, int n);  // falha sem mexer se não houver o bastante

    bool tem(const Custo& c) const;
    Custo faltando(const Custo& c) const;  // só o que falta, por recurso
    bool pagar(const Custo& c);            // tudo ou nada

    const std::map<std::string, int>& todos() const { return itens_; }
    void definir(const std::string& id, int n) { itens_[id] = n < 0 ? 0 : n; }

private:
    std::map<std::string, int> itens_;
};

}
