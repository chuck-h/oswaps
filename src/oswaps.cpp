#include <oswaps.hpp>
#include <../capi/eosio/action.h>

const checksum256 telos_chain_id = checksum256::make_from_word_sequence<uint64_t>(
  0x4667b205c6838ef7u,
  0x0ff7988f6e8257e8u,
  0xbe0e1284a2f59699u,
  0x054a018f743b1d11u );

uint64_t amount_from(symbol sym, string qty) { 
  int sp = qty.find(' ');
  check(sym.code().to_string() == qty.substr(sp+1), "mismatched symbol");
  size_t dp;
  uint64_t rv = std::stol(qty, &dp)*pow(10,sym.precision());
  if(qty.at(dp)=='.') {
    uint64_t f = std::stol(qty.substr(dp+1));
    int decimals = sp-dp-1;
    check(decimals <= sym.precision(), "too many decimals");
    for(int i=decimals; i<sym.precision(); ++i) {
      f *= 10;
    }
    rv += f;
  }
  return rv;
}

/** hack ** hack ** hack **/
bool parse_mod_in(string s) {
  // strip whitespace
  s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char x) { return std::isspace(x); }), s.end());
  check(s[0] == '{' && s.back() == '}', "mod: missing braces");
  size_t f = s.find("\"exact\"", 1);
  check(f != string::npos, "mod: missing exact field");
  bool exact_in = s.substr(f+7, 5) == ":\"in\"";
  if (!exact_in) {
    check(s.substr(f+7, 6) == ":\"out\"", "mod: exact fld must be in or out");
  }
  return exact_in;
} 
  

void oswaps::reset() {
  require_auth2(get_self().value, "owner"_n.value);
  {
    assets tbl(get_self(), get_self().value);
    auto itr = tbl.begin();
    while (itr != tbl.end()) {
      itr = tbl.erase(itr);
    }
  }
  {
    adpreps tbl(get_self(), get_self().value);
    auto itr = tbl.begin();
    while (itr != tbl.end()) {
      itr = tbl.erase(itr);
    }
  }
  {
    expreps tbl(get_self(), get_self().value);
    auto itr = tbl.begin();
    while (itr != tbl.end()) {
      itr = tbl.erase(itr);
    }
  }
  configs configset(get_self(), get_self().value);
  if(configset.exists()) { configset.remove(); }
  // clear prep tables
}

void oswaps::init(name manager, uint64_t nonce_life_msec, string chain) {
  configs configset(get_self(), get_self().value);
  bool reconfig = configset.exists();
  auto cfg = configset.get_or_create(get_self(), config_row);
  if(reconfig) {
    require_auth(cfg.manager);
  } else {
    require_auth2(get_self().value, "owner"_n.value);
  }
  // TODO parse chain into chain_name, chain_code
  string chain_name = "Telos";
  checksum256 chain_code = telos_chain_id;
  check(chain == chain_name, "currently only Telos chain supported");
  check(chain.size() <= 100, "chain name too long");
  cfg.chain_id = chain_code;
  cfg.manager = manager;
  cfg.nonce_life_msec = nonce_life_msec;
  if (!reconfig) {
    cfg.last_nonce = 1111;
  }
  configset.set(cfg, get_self());
}

void oswaps::freeze(name actor, uint64_t token_id, string symbol) {
  check(false, "freeze not supported");
}

void oswaps::unfreeze(name actor, uint64_t token_id, string symbol) {
  check(false, "unfreeze not supported");
};

uint64_t oswaps::createasseta(name actor, string chain, name contract, symbol_code symbol, string meta) {
  require_auth(actor);
  assets assettable(get_self(), get_self().value);
  // TODO parse chain into chain_name, chain_code
  string chain_name = "Telos";
  checksum256 chain_code = telos_chain_id;
  check(chain == chain_name, "currently only Telos chain supported");
  uint64_t token_id = assettable.available_primary_key();
  assettable.emplace(actor, [&]( auto& s ) {
    s.token_id = token_id;
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
  return token_id;
}

void oswaps::forgetasset(name actor, uint64_t token_id, string memo) {
  configs configset(get_self(), get_self().value);
  check(configset.exists(), "not configured.");
  auto cfg = configset.get();
  check(actor == cfg.manager, "must be manager");
  require_auth(actor);
  assets assettable(get_self(), get_self().value);
  auto a = assettable.require_find(token_id, "unrecog token id");
  assettable.erase(a);
}  

void oswaps::withdraw(name account, uint64_t token_id, string amount, float weight) {
  configs configset(get_self(), get_self().value);
  check(configset.exists(), "not configured.");
  auto cfg = configset.get();
  require_auth(cfg.manager);
  assets assettable(get_self(), get_self().value);
  auto a = assettable.require_find(token_id, "unrecog token id");
  // TODO verify chain, family, and contract
  symbol_code asset_symbol_code = symbol_code(a->symbol);
  stats stattable(name(a->contract_code), asset_symbol_code.raw());
  auto st = stattable.require_find(asset_symbol_code.raw(), "can't stat symbol");
  uint64_t amount64 = amount_from(st->supply.symbol, amount);
  asset qty = asset(amount64, st->supply.symbol);
  accounts accttable(name(a->contract_code), get_self().value);
  auto ac = accttable.find(symbol_code(a->symbol).raw());
  uint64_t bal_before = 0;
  if(ac != accttable.end()) {
    bal_before = ac->balance.amount;
  }
  check(bal_before > amount64, "withdraw: insufficient balance");
  float new_weight = weight;
  if(weight == 0.0) {
    new_weight = a->weight * (1.0 - float(amount64)/bal_before);
  }
  assettable.modify(a, same_payer, [&](auto& s) {
    s.weight = new_weight;
  });

  action (
    permission_level{get_self(), "active"_n},
    name(a->contract_code),
    "transfer"_n,
    std::make_tuple(get_self(), account, qty, std::string("oswaps withdrawal"))
  ).send(); 
}

uint32_t oswaps::addliqprep(name account, uint64_t token_id,
                            string amount, float weight) {
  configs configset(get_self(), get_self().value);
  auto cfg = configset.get();
  cfg.last_nonce += 1;
  configset.set(cfg, get_self());
  assets assettable(get_self(), get_self().value);
  auto a = assettable.require_find(token_id, "unrecog token id");
  // TODO verify chain & family
  symbol_code asset_symbol_code = symbol_code(a->symbol);
  stats stattable(name(a->contract_code), asset_symbol_code.raw());
  auto st = stattable.require_find(asset_symbol_code.raw(), "can't stat symbol");
  uint64_t amount64 = amount_from(st->supply.symbol, amount);
  accounts accttable(name(a->contract_code), get_self().value);
  auto ac = accttable.find(symbol_code(a->symbol).raw());
  uint64_t bal_before = 0;
  if(ac != accttable.end()) {
    bal_before = ac->balance.amount;
  }
  float new_weight = weight;
  if(weight == 0.0) {
    check(bal_before > 0, "zero weight requires existing balance");
    new_weight = a->weight * (1.0 + float(amount64)/bal_before);
  }
  adpreps adpreptable(get_self(), get_self().value);
  adprep ap;
  ap.nonce = cfg.last_nonce;
  ap.expires = time_point(microseconds(
    current_time_point().time_since_epoch().count()
    + 1000*cfg.nonce_life_msec));
  ap.account = account;
  ap.token_id = token_id;
  ap.amount = amount;
  ap.weight = new_weight;
  adpreptable.emplace(get_self(), [&]( auto& s ) {
    s = ap;
  });
  return ap.nonce;
}

std::vector<int64_t> oswaps::exchangeprep(
           name sender, uint64_t in_token_id, string in_amount,
           name recipient, uint64_t out_token_id, string out_amount,
           string mods, string memo) {
  configs configset(get_self(), get_self().value);
  auto cfg = configset.get();
  cfg.last_nonce += 1;
  configset.set(cfg, get_self());
  assets assettable(get_self(), get_self().value);
  
  auto ain = assettable.require_find(in_token_id, "unrecog input token id");
  symbol_code in_asset_symbol_code = symbol_code(ain->symbol);
  stats in_stattable(name(ain->contract_code), in_asset_symbol_code.raw());
  auto stin = in_stattable.require_find(in_asset_symbol_code.raw(), "can't stat symbol");
  uint64_t in_amount64 = amount_from(stin->supply.symbol, in_amount);
  accounts in_accttable(name(ain->contract_code), get_self().value);
  auto acin = in_accttable.find(symbol_code(ain->symbol).raw());
  uint64_t in_bal_before = 0;
  if(acin != in_accttable.end()) {
    in_bal_before = acin->balance.amount;
  }
  check(in_bal_before > 0, "zero input balance");
  
  auto aout = assettable.require_find(out_token_id, "unrecog output token id");
  symbol_code out_asset_symbol_code = symbol_code(aout->symbol);
  stats out_stattable(name(aout->contract_code), out_asset_symbol_code.raw());
  auto stout = out_stattable.require_find(out_asset_symbol_code.raw(), "can't stat symbol");
  uint64_t out_amount64 = amount_from(stout->supply.symbol, out_amount);
  accounts out_accttable(name(aout->contract_code), get_self().value);
  auto acout = out_accttable.find(symbol_code(aout->symbol).raw());
  uint64_t out_bal_before = 0;
  if(acout != out_accttable.end()) {
    out_bal_before = acout->balance.amount;
  }

  // balancer computation 
  // for controlling asset, compute LC = ln(new/old)
  // for noncontrolling asset compute LNC = - WC/WNC * LC
  // for noncontrolling compute new = old * exp(LNC)
  double lc, lnc;
  bool input_is_exact = parse_mod_in(mods);
  int64_t in_bal_after, out_bal_after, computed_amt;
  if (input_is_exact) {
    in_bal_after = in_bal_before + in_amount64;
    lc = log((double)in_bal_after/in_bal_before);
    lnc = -(ain->weight/aout->weight * lc);
    out_bal_after = llround(out_bal_before * exp(lnc));
    computed_amt = out_bal_before - out_bal_after;
  } else {
    out_bal_after = out_bal_before - out_amount64;
    check(out_bal_after > 0, "insufficient pool bal output token");
    lc = log((double)out_bal_after/out_bal_before);
    lnc = -(aout->weight/ain->weight * lc);
    in_bal_after = llround(in_bal_before * exp(lnc));
    computed_amt = in_bal_after - in_bal_before;
  }
  printf("balancer lc %f, lnc %f", lc, lnc);

  expreps expreptable(get_self(), get_self().value);
  exprep ex;
  ex.nonce = cfg.last_nonce;
  ex.expires = time_point(microseconds(
    current_time_point().time_since_epoch().count()
    + 1000*cfg.nonce_life_msec));
  ex.sender = sender;
  ex.in_token_id = in_token_id;
  ex.in_amount = in_amount;
  ex.recipient = recipient;
  ex.out_token_id = out_token_id;
  ex.out_amount = out_amount;
  ex.mods = mods;
  ex.memo = memo;
  expreptable.emplace(get_self(), [&]( auto& s ) {
    s = ex;
  });

  std::vector<int64_t> rv{static_cast<int64_t>(ex.nonce), 0, 0, computed_amt };
  return rv;
}

void oswaps::ontransfer(name from, name to, eosio::asset quantity, string memo) {
    if (from == get_self()) return;
    check(to == get_self(), "This transfer is not for oswaps");
    check(quantity.amount >= 0, "transfer quantity must be positive");
    name tkcontract = get_first_receiver();
    // get rightmost numerical token of memo as nonce
    size_t mp = memo.find_last_not_of("0123456789");
    check(mp+1 != memo.length(), "no nonce at end of memo");
    string nonce_string;
    if (mp == string::npos) {
      nonce_string = memo;
    } else {
      nonce_string = memo.substr(mp+1);
    }
    uint64_t memo_nonce = std::stol(nonce_string, nullptr);
    adpreps adpreptable(get_self(), get_self().value);
    // TODO: purge stale adprep table entries
    // if in adprep table
    auto itr = adpreptable.find( memo_nonce );
    if (itr != adpreptable.end()) {
      // accept liquidity addition if valid
      assets assettable(get_self(), get_self().value);
      auto a = assettable.require_find(itr->token_id, "unrecog token id");
      // TODO verify chain & family
      check(a->contract == tkcontract.to_string(), "wrong token contract");
      uint64_t amt = amount_from(quantity.symbol, itr->amount);
      check(amt == quantity.amount, "transfer qty mismatched to prep");
      assettable.modify(a, same_payer, [&](auto& s) {
        s.weight = itr->weight;
      });
      adpreptable.erase(itr);
    } else {
      expreps expreptable(get_self(), get_self().value);
      // TODO: purge stale exprep table entries
      auto ex = expreptable.find( memo_nonce );
      if (ex == expreptable.end()) {
        check(false, "no matching transaction");
      }
      assets assettable(get_self(), get_self().value);
      auto ain = assettable.require_find(ex->in_token_id, "unrecog input token id");
      check(ain->contract == tkcontract.to_string(), "wrong token contract");
      symbol_code in_asset_symbol_code = symbol_code(ain->symbol);
      stats in_stattable(name(ain->contract_code), in_asset_symbol_code.raw());
      auto stin = in_stattable.require_find(in_asset_symbol_code.raw(), "can't stat symbol");
      uint64_t in_amount64 = amount_from(stin->supply.symbol, ex->in_amount);
      accounts in_accttable(name(ain->contract_code), get_self().value);
      auto acin = in_accttable.find(symbol_code(ain->symbol).raw());
      uint64_t in_bal_before = 0;
      if(acin != in_accttable.end()) {
        // must back out transfer which just occurred
        in_bal_before = acin->balance.amount - quantity.amount;
      }
      check(in_bal_before > 0, "zero input balance");

      auto aout = assettable.require_find(ex->out_token_id, "unrecog output token id");
      symbol_code out_asset_symbol_code = symbol_code(aout->symbol);
      stats out_stattable(name(aout->contract_code), out_asset_symbol_code.raw());
      auto stout = out_stattable.require_find(out_asset_symbol_code.raw(), "can't stat symbol");
      uint64_t out_amount64 = amount_from(stout->supply.symbol, ex->out_amount);
      accounts out_accttable(name(aout->contract_code), get_self().value);
      auto acout = out_accttable.find(symbol_code(aout->symbol).raw());
      uint64_t out_bal_before = 0;
      if(acout != out_accttable.end()) {
        out_bal_before = acout->balance.amount;
      }

      // do balancer computation again (balances may have changed)
      double lc, lnc;
      bool input_is_exact = parse_mod_in(ex->mods);
      int64_t in_bal_after, out_bal_after, computed_amt;
      if (input_is_exact) {
        in_bal_after = in_bal_before + in_amount64;
        lc = log((double)in_bal_after/in_bal_before);
        lnc = -(ain->weight/aout->weight * lc);
        out_bal_after = llround(out_bal_before * exp(lnc));
        computed_amt = out_bal_before - out_bal_after;
        check(computed_amt >= out_amount64, "output below limit");
      } else {
        out_bal_after = out_bal_before - out_amount64;
        check(out_bal_after > 0, "insufficient pool bal output token");
        lc = log((double)out_bal_after/out_bal_before);
        lnc = -(aout->weight/ain->weight * lc);
        in_bal_after = llround(in_bal_before * exp(lnc));
        computed_amt = in_bal_after - in_bal_before;
        check(computed_amt <= in_amount64, "input over limit");
      }
      int64_t in_surplus = 0;
      asset out_qty;
      if(input_is_exact) {
        check(in_amount64 == quantity.amount, "transfer qty mismatched to prep");
        out_qty = asset(computed_amt, stout->supply.symbol);
      } else {
        in_surplus = quantity.amount - computed_amt;
        check(in_surplus >= 0, "insufficient amount transferred");
        out_qty = asset(out_amount64, stout->supply.symbol);
      }
      // send exchange output to recipient 
      action (
        permission_level{get_self(), "active"_n},
        name(aout->contract_code),
        "transfer"_n,
        std::make_tuple(get_self(), ex->recipient, out_qty, std::string("oswaps exchange"))
      ).send();
      // refund surplus to sender
      if(in_surplus > 0) {
        asset overpayment = asset(in_surplus, stin->supply.symbol);
        action (
          permission_level{get_self(), "active"_n},
          name(ain->contract_code),
          "transfer"_n,
          std::make_tuple(get_self(), ex->sender, overpayment, std::string("oswaps exchange refund overpayment"))
        ).send();
      }
      expreptable.erase(ex); 
    }
}


  

