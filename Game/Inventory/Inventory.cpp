#include "Game/Inventory/Inventory.h"

namespace koa {

int Inventory::quantidade(const std::string& id) const {
    const auto it = itens_.find(id);
    return it == itens_.end() ? 0 : it->second;
}

void Inventory::adicionar(const std::string& id, int n) {
    if (n > 0) itens_[id] += n;
}

bool Inventory::remover(const std::string& id, int n) {
    if (n <= 0) return true;
    if (quantidade(id) < n) return false;
    itens_[id] -= n;
    return true;
}

bool Inventory::tem(const Custo& c) const { return faltando(c).empty(); }

Custo Inventory::faltando(const Custo& c) const {
    Custo f;
    for (const auto& [id, n] : c)
        if (quantidade(id) < n) f[id] = n - quantidade(id);
    return f;
}

bool Inventory::pagar(const Custo& c) {
    if (!tem(c)) return false;
    for (const auto& [id, n] : c) itens_[id] -= n;
    return true;
}

}
