#include <oswaps.hpp>
#include <../capi/eosio/action.h>

const checksum256 telos_chain_id = checksum256::make_from_word_sequence<uint64_t>(
  0x4667b205c6838ef7u,
  0x0ff7988f6e8257e8u,
  0xbe0e1284a2f59699u,
  0x054a018f743b1d11u );

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
  configs configset(get_self(), get_self().value);
  bool reconfig = configset.exists();
  if(reconfig) {
    // TODO allow transfer to new manager account
    require_auth2(get_self().value, "owner"_n.value);
  } else {
    require_auth(manager);
  }
  // TODO parse chain into chain_name, chain_code
  string chain_name = "Telos";
  checksum256 chain_code = telos_chain_id;
  check(chain == chain_name, "currently only Telos chain supported");
  auto config_entry = configset.get_or_create(get_self(), config_row);
  check(chain.size() <= 100, "chain name too long");
  config_entry.chain_id = chain_code;
  config_entry.manager = manager;
  config_entry.nonce_life_msec = nonce_life_msec;
  configset.set(config_entry, get_self());
}

void oswaps::freeze(name actor, uint64_t token_id, string symbol) {
}

void oswaps::unfreeze(name actor, uint64_t token_id, string symbol) {
};

void oswaps::createasseta(name actor, string chain, name contract, symbol_code symbol, string meta) {
  require_auth(actor);
  assets assettable(get_self(), get_self().value);
  // TODO parse chain into chain_name, chain_code
  string chain_name = "Telos";
  checksum256 chain_code = telos_chain_id;
  check(chain == chain_name, "currently only Telos chain supported");
    
  assettable.emplace(actor, [&]( auto& s ) {
    s.token_id = assettable.available_primary_key();
    s.family = "antelope"_n;
    s.chain = chain_name;
    s.chain_code = chain_code;
    s.contract = contract.to_string();
    s.contract_code = contract.value;
    s.symbol = symbol.to_string();
    s.active = false;
    s.metadata = meta;
    s.weight = 0.0;
  });

}

void oswaps::withdraw(name account, uint64_t token_id, string amount, float weight_frac) {
}

uint64_t oswaps::addliqprep(name account, uint64_t token_id,
                            string amount, float weight_frac) {
  return 123;
}

std::vector<int64_t> oswaps::exchangeprep(
           name sender, uint64_t in_token_id, string in_amount,
           name recipient, uint64_t out_token_id, string out_amount,
           string mods, string memo) {
  // for controlling asset, compute LC = ln(new/old)
  // for noncontrolling asset compute LNC = - WC/WNC * LC
  // for noncontrolling compute new = old * exp(LNC)
        
  std::vector<int64_t> rv{5, 4, 3, 2, 1};
  return rv;
}

void oswaps::ontransfer(name from, name to, eosio::asset quantity, string memo) {
    if (from == get_self()) return;
    check(to == get_self(), "This transfer is not for oswaps");
    check(quantity.amount >= 0, "transfer quantity must be positive");
    // look up memo field in adprep and exprep tables
    uint64_t memo_nonce;
    memo_nonce = std::stol(memo, nullptr, 0); // could throw on bad memo
    // if in adprep table
    adpreps adpreptable(get_self(), get_self().value);
    auto adidx = adpreptable.get_index<"bynonce"_n>();
    auto itr = adidx.find( memo_nonce );
    if (itr == adidx.end()) {
      // do liquidity add
    } else {
      expreps expreptable(get_self(), get_self().value);
      auto exidx = expreptable.get_index<"bynonce"_n>();
      auto itr = exidx.find( memo_nonce );
      if (itr == exidx.end()) {
        check(false, "no matching transaction");
      } else {
        std::vector<int64_t> result = exchangeprep(
           itr->sender, itr->in_token_id, itr->in_amount,
           itr->recipient, itr->out_token_id, itr->out_amount,
           itr->mods, itr->memo);
      // test mods for which asset is controlling
      // test whether noncontrolling quantity is within limits
      // if ok, call outgoing transfer as inline action
      //  call surplus return transaction for incoming asset if needed
      }
    }
}



  

