#include <oswaps.hpp>
#include <../capi/eosio/action.h>

void oswaps::reset() {
  require_auth2(get_self().value, "owner"_n.value);
  assets tbl(get_self(), get_self().value);
  auto itr = tbl.begin();
  while (itr != tbl.end()) {
    itr = tbl.erase(itr);
  }
  // clear prep tables
}

void oswaps::init(name manager, uint64_t nonce_life_msec, string chain) {
};

void oswaps::freeze(name actor, uint64_t token_id, string symbol) {
};

void oswaps::unfreeze(name actor, uint64_t token_id, string symbol) {
};

void oswaps::createasseta(name actor, string chain, name contract, name symbol, string meta) {
};

void oswaps::withdrawprep(name account, uint64_t token_id, string amount, float weight_frac) {
}

uint64_t oswaps::addliqprep(name account, uint64_t token_id,
                            string amount, float weight_frac) {
  return 123;
}

std::vector<int64_t> oswaps::exchangeprep(
           name sender, uint64_t in_token_id, string in_amount,
           name recipient, uint64_t out_token_id, string out_amount,
           string memo) {
  std::vector<int64_t> rv{5, 4, 3, 2, 1};
  return rv;
}
  

