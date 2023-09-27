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
  
string sym_from_id(uint64_t token_id, string prefix) {
  if (token_id < 26) {
    return prefix + string(1, 'A' + token_id);
  } else {
    prefix = sym_from_id(token_id/26 - 1, prefix);
    return sym_from_id(token_id%26, prefix);
  }
}
  
void oswaps::reset() {
  require_auth2(get_self().value, "owner"_n.value);
  {
    assetsa tbl(get_self(), get_self().value);
    auto itr = tbl.begin();
    while (itr != tbl.end()) {
      itr = tbl.erase(itr);
    }
    // TODO destroy LIQ tokens
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
    cfg.last_nonce = 1111; // >42 (magic #)
    cfg.last_token_id = 0;
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
  assetsa assettable(get_self(), get_self().value);
  // TODO parse chain into chain_name, chain_code
  string chain_name = "Telos";
  checksum256 chain_code = telos_chain_id;
  check(chain == chain_name, "currently only Telos chain supported");
  configs configset(get_self(), get_self().value);
  auto cfg = configset.get();
  cfg.last_token_id += 1;
  configset.set(cfg, get_self());
  assettable.emplace(actor, [&]( auto& s ) {
    s.token_id = cfg.last_token_id;
    s.chain_code = chain_code;
    s.contract_code = contract.value;
    s.symbol = symbol;
    s.active = true;
    s.metadata = meta;
    s.weight = 0.0;
  });
  // create LIQ token with correct precision
  stats astattable(contract, symbol.raw());
  auto ast = astattable.require_find(symbol.raw(), "can't stat symbol");
  auto liq_sym_code = symbol_code(sym_from_id(cfg.last_token_id, "LIQ"));
  auto liq_sym = eosio::symbol(liq_sym_code, ast->supply.symbol.precision());
  printf("liq sym code id %llu %s %s", cfg.last_token_id,
            liq_sym_code.to_string().c_str(),
            name(liq_sym_code.raw()).to_string().c_str());
  stats lstattable(get_self(), liq_sym_code.raw());
  auto existing = lstattable.find(liq_sym_code.raw());
  check( existing == lstattable.end(), "liquidity token already exists");
  lstattable.emplace( get_self(), [&]( auto& s ) {
    s.supply.symbol = liq_sym;
    s.max_supply    = asset(asset::max_amount, liq_sym);
    s.issuer        = get_self();
  });  
  return cfg.last_token_id;
}

void oswaps::forgetasset(name actor, uint64_t token_id, string memo) {
  configs configset(get_self(), get_self().value);
  check(configset.exists(), "not configured.");
  auto cfg = configset.get();
  check(actor == cfg.manager, "must be manager");
  require_auth(actor);
  assetsa assettable(get_self(), get_self().value);
  auto a = assettable.require_find(token_id, "unrecog token id");
  assettable.erase(a);
  // should we check for zero balance before destroying LIQ token?
  auto liq_sym_code = symbol_code(sym_from_id(token_id, "LIQ"));
  stats lstattable(get_self(), liq_sym_code.raw());
  auto lst = lstattable.begin();
  while ( lst != lstattable.end()) {
    lst = lstattable.erase(lst);
  }
  // accounts table has stranded ram & data which could create weirdness
}  

void oswaps::withdraw(name account, uint64_t token_id, string amount, float weight) {
  configs configset(get_self(), get_self().value);
  check(configset.exists(), "not configured.");
  auto cfg = configset.get();
  require_auth(cfg.manager);
  assetsa assettable(get_self(), get_self().value);
  auto a = assettable.require_find(token_id, "unrecog token id");
  // TODO verify chain, family, and contract
  stats stattable(name(a->contract_code), a->symbol.raw());
  auto st = stattable.require_find(a->symbol.raw(), "can't stat symbol");
  uint64_t amount64 = amount_from(st->supply.symbol, amount);
  asset qty = asset(amount64, st->supply.symbol);
  accounts accttable(name(a->contract_code), get_self().value);
  auto ac = accttable.find(a->symbol.raw());
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
  // TODO burn LIQ tokens
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
  assetsa assettable(get_self(), get_self().value);
  auto a = assettable.require_find(token_id, "unrecog token id");
  // TODO verify chain & family
  stats stattable(name(a->contract_code), a->symbol.raw());
  auto st = stattable.require_find(a->symbol.raw(), "can't stat symbol");
  uint64_t amount64 = amount_from(st->supply.symbol, amount);
  accounts accttable(name(a->contract_code), get_self().value);
  auto ac = accttable.find(a->symbol.raw());
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
  assetsa assettable(get_self(), get_self().value);
  
  auto ain = assettable.require_find(in_token_id, "unrecog input token id");
  stats in_stattable(name(ain->contract_code), ain->symbol.raw());
  auto stin = in_stattable.require_find(ain->symbol.raw(), "can't stat symbol");
  uint64_t in_amount64 = amount_from(stin->supply.symbol, in_amount);
  accounts in_accttable(name(ain->contract_code), get_self().value);
  auto acin = in_accttable.find(ain->symbol.raw());
  uint64_t in_bal_before = 0;
  if(acin != in_accttable.end()) {
    in_bal_before = acin->balance.amount;
  }
  check(in_bal_before > 0, "zero input balance");
  
  auto aout = assettable.require_find(out_token_id, "unrecog output token id");
  stats out_stattable(name(aout->contract_code), aout->symbol.raw());
  auto stout = out_stattable.require_find(aout->symbol.raw(), "can't stat symbol");
  uint64_t out_amount64 = amount_from(stout->supply.symbol, out_amount);
  accounts out_accttable(name(aout->contract_code), get_self().value);
  auto acout = out_accttable.find(aout->symbol.raw());
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

void oswaps::transfer( const name& from, const name& to, const asset& quantity,
                       const string&  memo ) {
  // TODO impement standard transfer action for LIQ tokens
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
    uint64_t bypass_code = 42;
    if (memo_nonce == bypass_code) {
      return;
    }
    int64_t usec_now = current_time_point().time_since_epoch().count();
    adpreps adpreptable(get_self(), get_self().value);
    // purge stale adprep table entries
    auto adpreps_byexpiration = adpreptable.get_index<"byexpiration"_n>();
    for (auto adx = adpreps_byexpiration.begin();
              adx != adpreps_byexpiration.end();) {
      if(adx->expires.time_since_epoch().count() < usec_now) {
        adx = adpreps_byexpiration.erase(adx);
      } else {
        break;
      }
    }
    // if in adprep table
    auto itr = adpreptable.find( memo_nonce );
    if (itr != adpreptable.end()) {
      // accept liquidity addition if valid
      assetsa assettable(get_self(), get_self().value);
      auto a = assettable.require_find(itr->token_id, "unrecog token id");
      // TODO verify chain & family
      check(a->contract_code == tkcontract.value, "wrong token contract");
      uint64_t amt = amount_from(quantity.symbol, itr->amount);
      check(amt == quantity.amount, "transfer qty mismatched to prep");
      assettable.modify(a, same_payer, [&](auto& s) {
        s.weight = itr->weight;
      });
      adpreptable.erase(itr);
      // TODO issue LIQ tokens to `from` account
    } else {
      expreps expreptable(get_self(), get_self().value);
      // purge stale exprep table entries
      auto expreps_byexpiration = expreptable.get_index<"byexpiration"_n>();
      for (auto adx = expreps_byexpiration.begin();
                adx != expreps_byexpiration.end();) {
        if(adx->expires.time_since_epoch().count() < usec_now) {
          adx = expreps_byexpiration.erase(adx);
        } else {
          break;
        }
      }
      auto ex = expreptable.find( memo_nonce );
      if (ex == expreptable.end()) {
        check(false, "no matching transaction");
      }
      assetsa assettable(get_self(), get_self().value);
      auto ain = assettable.require_find(ex->in_token_id, "unrecog input token id");
      check(ain->contract_code == tkcontract.value, "wrong token contract");
      stats in_stattable(name(ain->contract_code), ain->symbol.raw());
      auto stin = in_stattable.require_find(ain->symbol.raw(), "can't stat symbol");
      uint64_t in_amount64 = amount_from(stin->supply.symbol, ex->in_amount);
      accounts in_accttable(name(ain->contract_code), get_self().value);
      auto acin = in_accttable.find(ain->symbol.raw());
      uint64_t in_bal_before = 0;
      if(acin != in_accttable.end()) {
        // must back out transfer which just occurred
        in_bal_before = acin->balance.amount - quantity.amount;
      }
      check(in_bal_before > 0, "zero input balance");

      auto aout = assettable.require_find(ex->out_token_id, "unrecog output token id");
      stats out_stattable(name(aout->contract_code), aout->symbol.raw());
      auto stout = out_stattable.require_find(aout->symbol.raw(), "can't stat symbol");
      uint64_t out_amount64 = amount_from(stout->supply.symbol, ex->out_amount);
      accounts out_accttable(name(aout->contract_code), get_self().value);
      auto acout = out_accttable.find(aout->symbol.raw());
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


  

