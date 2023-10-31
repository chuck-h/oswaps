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

void oswaps::resetacct( const name& account )
{
  require_auth2( get_self().value, "owner"_n.value );
    accounts tbl(get_self(),account.value);
    auto itr = tbl.begin();
    while (itr != tbl.end()) {
      itr = tbl.erase(itr);
    }
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
  check(contract != get_self(), "asset contract cannot be oswaps");
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
    s.contract_name = contract;
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
  //check( existing == lstattable.end(), "liquidity token already exists");
  if (existing != lstattable.end()) { // corner case from clumsy reset
    lstattable.modify( existing, get_self(), [&]( auto& s ) {
      s.supply      = asset(0, liq_sym);
      s.max_supply  = asset(asset::max_amount, liq_sym);
      s.issuer      = get_self();
    });
  } else {
    lstattable.emplace( get_self(), [&]( auto& s ) {
      s.supply.symbol = liq_sym;
      s.max_supply    = asset(asset::max_amount, liq_sym);
      s.issuer        = get_self();
    }); 
  } 
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
  stats stattable(a->contract_name, a->symbol.raw());
  auto st = stattable.require_find(a->symbol.raw(), "can't stat symbol");
  uint64_t amount64 = amount_from(st->supply.symbol, amount);
  asset qty = asset(amount64, st->supply.symbol);
  accounts accttable(a->contract_name, get_self().value);
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
  // burn LIQ tokens
  auto liq_sym_code = symbol_code(sym_from_id(token_id, "LIQ"));
  stats lstatstable( get_self(), liq_sym_code.raw() );
  const auto& lst = lstatstable.get( liq_sym_code.raw() );
  asset lqty = qty;
  lqty.symbol = symbol(liq_sym_code, qty.symbol.precision());
  sub_balance( account, lqty );
  add_balance( get_self(), lqty, get_self()); 
  action (
    permission_level{get_self(), "active"_n},
    get_self(),
    "retire"_n,
    std::make_tuple(lqty, std::string("oswaps withdrawal for "+account.to_string()))
  ).send(); 

  action (
    permission_level{get_self(), "active"_n},
    a->contract_name,
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
  stats stattable(a->contract_name, a->symbol.raw());
  auto st = stattable.require_find(a->symbol.raw(), "can't stat symbol");
  uint64_t amount64 = amount_from(st->supply.symbol, amount);
  accounts accttable(a->contract_name, get_self().value);
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
  stats in_stattable(ain->contract_name, ain->symbol.raw());
  auto stin = in_stattable.require_find(ain->symbol.raw(), "can't stat symbol");
  uint64_t in_amount64 = amount_from(stin->supply.symbol, in_amount);
  accounts in_accttable(ain->contract_name, get_self().value);
  auto acin = in_accttable.find(ain->symbol.raw());
  uint64_t in_bal_before = 0;
  if(acin != in_accttable.end()) {
    in_bal_before = acin->balance.amount;
  }
  check(in_bal_before > 0, "zero input balance");
  
  auto aout = assettable.require_find(out_token_id, "unrecog output token id");
  stats out_stattable(aout->contract_name, aout->symbol.raw());
  auto stout = out_stattable.require_find(aout->symbol.raw(), "can't stat symbol");
  uint64_t out_amount64 = amount_from(stout->supply.symbol, out_amount);
  accounts out_accttable(aout->contract_name, get_self().value);
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
  // implement eosio.token transfer action for LIQ tokens, but restrict p2p trading
    check( from != to, "cannot transfer to self" );
    // should this no-p2p restriction be under manager config control?
    check( from == get_self() || to == get_self(), "oswaps token transfers must be to/from contract");
    require_auth( from ); // TODO: allow manager to authorize LIQxx transfers for withdrawals
    check( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    // note that require_recipient does not self-notify the oswaps contract
    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount >= 0, "transfer quantity is negative" ); // 0 qty => open account
    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}
 
void oswaps::ontransfer(name from, name to, eosio::asset quantity, string memo) {
    if (from == get_self()) return;
    check(to == get_self(), "This transfer is not for oswaps");
    check(quantity.amount >= 0, "transfer quantity must be positive");
    name tkcontract = get_first_receiver();
    //print("oswaps_ontransfer|");
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
        //printf("expiring adprep %llu|", adx->nonce);
        check(adx->nonce != memo_nonce, "prep "+std::to_string(memo_nonce)+" has expired.");
        adx = adpreps_byexpiration.erase(adx);
      } else {
        break;
      }
    }
    // if in adprep table, accept addition of liquidity
    auto itr = adpreptable.find( memo_nonce );
    if (itr != adpreptable.end()) {
      assetsa assettable(get_self(), get_self().value);
      auto a = assettable.require_find(itr->token_id, "unrecog token id");
      // TODO verify chain & family
      check(a->contract_name == tkcontract, "wrong token contract");
      uint64_t amt = amount_from(quantity.symbol, itr->amount);
      check(amt == quantity.amount, "transfer qty mismatched to prep");
      assettable.modify(a, same_payer, [&](auto& s) {
        s.weight = itr->weight;
      });
      adpreptable.erase(itr);
      // issue LIQ tokens to self & transfer to `from` account
      auto liq_sym_code = symbol_code(sym_from_id(itr->token_id, "LIQ"));
      stats lstatstable( get_self(), liq_sym_code.raw() );
      const auto& lst = lstatstable.get( liq_sym_code.raw() );
      asset lqty = quantity;
      lqty.symbol = symbol(liq_sym_code, quantity.symbol.precision());
      add_balance( get_self(), lqty, get_self() );
      lstatstable.modify( lst, same_payer, [&]( auto& s ) {
        s.supply += lqty;
      });
      action (
        permission_level{get_self(), "active"_n},
        get_self(),
        "transfer"_n,
        std::make_tuple(get_self(), from, lqty,
           std::string("oswaps liquidity receipt ")+std::to_string(memo_nonce))
      ).send();

      
    } else {
      expreps expreptable(get_self(), get_self().value);
      // purge stale exprep table entries
      auto expreps_byexpiration = expreptable.get_index<"byexpiration"_n>();
      for (auto adx = expreps_byexpiration.begin();
                adx != expreps_byexpiration.end();) {
        if(adx->expires.time_since_epoch().count() < usec_now) {
          check(adx->nonce != memo_nonce, "exch "+std::to_string(memo_nonce)+" has expired.");
          adx = expreps_byexpiration.erase(adx);
        } else {
          break;
        }
      }
      auto ex = expreptable.find( memo_nonce );
      if (ex == expreptable.end()) {
        check(false, "no matching transaction for "+std::to_string(memo_nonce));
      }
      assetsa assettable(get_self(), get_self().value);
      auto ain = assettable.require_find(ex->in_token_id, "unrecog input token id");
      check(ain->contract_name == tkcontract, "wrong token contract");
      stats in_stattable(ain->contract_name, ain->symbol.raw());
      auto stin = in_stattable.require_find(ain->symbol.raw(), "can't stat symbol");
      uint64_t in_amount64 = amount_from(stin->supply.symbol, ex->in_amount);
      accounts in_accttable(ain->contract_name, get_self().value);
      auto acin = in_accttable.find(ain->symbol.raw());
      uint64_t in_bal_before = 0;
      if(acin != in_accttable.end()) {
        // must back out transfer which just occurred
        in_bal_before = acin->balance.amount - quantity.amount;
      }
      check(in_bal_before > 0, "zero input balance");

      auto aout = assettable.require_find(ex->out_token_id, "unrecog output token id");
      stats out_stattable(aout->contract_name, aout->symbol.raw());
      auto stout = out_stattable.require_find(aout->symbol.raw(), "can't stat symbol");
      uint64_t out_amount64 = amount_from(stout->supply.symbol, ex->out_amount);
      accounts out_accttable(aout->contract_name, get_self().value);
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
        aout->contract_name,
        "transfer"_n,
        std::make_tuple(get_self(), ex->recipient, out_qty, std::string("oswaps exchange"))
      ).send();
      // refund surplus to sender
      if(in_surplus > 0) {
        asset overpayment = asset(in_surplus, stin->supply.symbol);
        action (
          permission_level{get_self(), "active"_n},
          ain->contract_name,
          "transfer"_n,
          std::make_tuple(get_self(), ex->sender, overpayment, std::string("oswaps exchange refund overpayment"))
        ).send();
      }
      expreptable.erase(ex); 
    }
}

void oswaps::sub_balance( const name& owner, const asset& value ) {
   accounts from_acnts( get_self(), owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   check( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, same_payer, [&]( auto& a ) {
         a.balance -= value;
      });
}

void oswaps::add_balance( const name& owner, const asset& value, const name& ram_payer )
{
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void oswaps::retire( const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must retire positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( st.issuer, quantity );
}


