#pragma once
#include <functional>
#include "intrusive_list.h"

namespace signals {

template <typename T>
struct signal;

template <typename... Args>
struct signal<void(Args...)> {
  struct connection;
  using slot_t = std::function<void(Args...)>;
  using list_t = typename intrusive::list<connection>;
  using const_iterator = typename list_t::const_iterator;

  struct connection : intrusive::list_element<> {
    connection() = default;

    connection(signal* sig, slot_t slot) : slot(std::move(slot)), sig(sig) {
      sig->slots.push_back(*this);
    }

    connection(connection&& other) {
      move_from(std::move(other));
    }

    connection& operator=(connection&& other) {
      if (&other != this) {
        move_from(std::move(other));
      }
      return *this;
    }

    void disconnect() noexcept {
      if (!in_list() || !sig) {
        return;
      }

      for (auto* token = sig->deepest_token; token != nullptr; token = token->prev) {
        if (&*token->it == this) {
          ++token->it;
        }
      }

      unlink();
      slot = {};
      sig = nullptr;
    }

    ~connection() {
      disconnect();
    }

  private:
    slot_t slot{};
    signal* sig{nullptr};

    void move_from(connection&& other) {
      slot = std::move(other.slot);
      sig = other.sig;

      if (!sig) {
        return;
      }

      auto other_old_pos = sig->slots.as_iterator(other);
      auto this_cur_pos = sig->slots.insert(other_old_pos, *this);

      for (auto* token = sig->deepest_token; token != nullptr; token = token->prev) {
        if (token->it == other_old_pos) {
          token->it = this_cur_pos;
        }
      }

      sig->slots.erase(other_old_pos);
    }

    friend struct signal;
  };

  signal() = default;

  signal(signal const&) = delete;
  signal& operator=(signal const&) = delete;

  connection connect(slot_t slot) noexcept {
    return connection(this, std::move(slot));
  }

  struct iteration_token {
    explicit iteration_token(signal const* sig, const_iterator it) : it(it), prev(sig->deepest_token), sig(sig) {
      sig->deepest_token = this;
    }

    ~iteration_token() {
      if (sig) {
        sig->deepest_token = prev;
      }
    }

  private:
    const_iterator it;
    iteration_token* prev;
    signal const* sig = nullptr;

    friend signal;
  };

  ~signal() {
    for (auto* token = deepest_token; token != nullptr; token = token->prev) {
      token->sig = nullptr;
    }

    while (!slots.empty()) {
      auto& con = slots.back();
      con.unlink();
      con.sig = nullptr;
      con.slot = {};
    }
  }

  void operator()(Args... args) const {
    iteration_token token(this, slots.begin());

    while (token.sig && token.it != slots.end()) {
      auto cur = token.it;
      ++token.it;
      cur->slot(args...);
    }
  }

private:
  list_t slots{};
  mutable iteration_token* deepest_token{nullptr};
};

} // namespace signals
