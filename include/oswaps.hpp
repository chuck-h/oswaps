#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>
#include <algorithm>

using namespace eosio;
using std::string;
   /**
    * The `oswaps' contract implements a token conversion service ("currency exchange") based on a
    *   multilateral token pool. The contract uses a "single sided" liquidity investment model. These
    *   fundamental functions are supported :
    *   - add liquidity, e.g. insert some Token A into the pool
    *   - withdraw liquidity, e.g. extract some Token A from the pool
    *   - convert, e.g. change token A to Token B, delivered to a recipient
    *
    * The contract anticipates a future ability to operate across different Antelope chains
    *   using a trustless IBC mechanism; therefore tokens are identified by a triplet
    *   < chain, contract, symbol >
    *
    * The contract supports a generic per-token parameter table. Parameters are expected to include
    *   balancer weights, fee levels, liquidity metering limits, etc. (details TBD)
    *
    * Authorization model
    * The contract account should be a "cold" multisig which is used once for uploading the contract
    *   and once for specifying a manager account. It has no operational role after that. For
    *   any token having transfer rights restricted to whitelisted accounts, the contract
    *   account must be added to the token's whitelist.
    * The manager account will typically be under some sort type of governance control (e.g. DAO).
    *   This account can freeze and unfreeze transaction processing on a per-token basis.
    * By placing named authorizations in the generic parameter table, this facility may be used to
    *   assign per-token management powers, but the details are TBD.    
    */

CONTRACT oswaps : public contract {
  public:
      using contract::contract;

      /**
          * The `reset` action executed by the oswaps contract account deletes all table data
      */
      ACTION reset();

      /**
          * The one-time `init` action executed by the tokensmaster contract account records
          *  the manager account
          *
          * @param manager - an account empowered to execute freeze and setparameter actions
      */
      ACTION init(name manager);
      
      /**
          * The `freeze` action executed by the manager or other authorized actor suspends
          * transactions in a specified token
          *
          * @param actor - an account empowered to execute the freeze action
          * @param chain - the "home chain" of a token
          * @param extended-symbol - the contract and symbol of the affected token
      */
      ACTION freeze(name actor, string chain, extended_symbol);

      /**
          * The `unfreeze` action executed by the manager or other authorized actor enables
          * transactions in a specified token
          *
          * @param actor - an account empowered to execute the unfreeze action
          * @param chain - the "home chain" of a token
          * @param extended-symbol - the contract and symbol of the affected token
      */
      ACTION unfreeze(name actor, string chain, extended_symbol);
      

      /**
          * The `setparameter` action sets a specified parameter value for a specified token
          *
          * @param actor - an account empowered to set the specified parameter
          * @param chain - the "home chain" of a token
          * @param extended-symbol - the contract and symbol of the affected token
          * @param parameter - the parameter name
          * @param value - the new parameter value
      */
      ACTION setparameter(name actor, string chain, extended_symbol, name parameter, string value);

      /**
          * The `changeliq` action adds/removes liquidity to/from a token pool
          * Token transfers occur through a rate-throttling queue which may introduce delays
          * 
          * @param owner - the account sourcing or receiving the tokens
          * @param chain - the "home chain" of the asset
          * @param amount - the (signed) amount of asset to add to pool;
          *    negative amount represents withdrawal.
      */
      ACTION changeliq(name owner, string chain, extended_asset amount);
      
      /**
          * The `convert` action takes a quantity of tokens from the sender and delivers a
          *   corresponding quantity of a different token to the recipient.
          * The conversion ratio ("exchange rate") is computed according to a multilateral
          *   "balancer" type algorithm with an invariant V
          *     V = B1**W1 * B2**W2 * ... * Bn**Wn
          *   and therefore depends dynamically on the pool balances B. In the small-
          *   transaction limit (no "slippage"), and with zero fees, an input of
          *   Qi tokens of type i will emit Qj = Qi * (Bj/Bi)*(Wi/Wj) tokens of type j.
          * In the action call, both incoming and outgoing amounts are specified, but
          *   only one of them is the "exact" or "controlling" parameter. The other
          *   amount specifies a limit; the action will fail if the computed exchange value
          *   is beyond the limit. The sign of the incoming amount encodes the choice
          *   above : a positive incoming amount parameter indicates that the incoming
          *   amount is exact and requires that the computed outgoing value must be no less than
          *   the outgoing amount parameter. A negative incoming amount indicates that the
          *   outgoing amount is exact and the computed incoming value must be no more than
          *   the absolute value of the incoming amount parameter.
          * 
          * @param sender - the account sourcing tokens to the transaction
          * @param in_chain - the "home chain" of the incoming asset
          * @param in_amount - the incoming amount (contract, symbol, and quantity) 
          * @param recipient - the account receiving tokens from the transaction
          * @param out_chain - the "home chain" of the outgoing asset
          * @param out_amount - the outgoing amount (contract, symbol, and quantity)
          * @param memo - memo string
      */
      ACTION convert(name sender, string in_chain, extended_asset in_amount,
                      name recipient, string out_chain, extended_asset out_amount,
                      string memo);
      
      /**
          * The `dryconvert` action takes the same parameters as `convert` but does
          *   not transfer tokens or change any table values. It performs a "dry run"
          *   conversion computation and returns fee information and the computed
          *   quantity for the non-controlling token (this can be either the incoming
          *   or outgoing token, depending on the sign of the incoming amount parameter).
          * 
          * @param sender - the account sourcing tokens to the transaction
          * @param in_chain - the "home chain" of the incoming asset
          * @param in_amount - the incoming amount (contract, symbol, and quantity) 
          * @param recipient - the account receiving tokens from the transaction
          * @param out_chain - the "home chain" of the outgoing asset
          * @param out_amount - the outgoing amount (contract, symbol, and quantity)
          * @param memo - memo string
          *
          * @result a vector of 3 elements
          *     rv[0] - the quantity of incoming tokens assessed as fee
          *     rv[1] - the quantity of outgoing tokens assessed as fee
          *     rv[2] - the computed quantity field of the non-controlling asset
      */
      [[eosio::action]] std::vector<int64_t> dryconvert(
           name sender, string in_chain, extended_asset in_amount,
           name recipient, string out_chain, extended_asset out_amount,
           string memo);
      
  private:

      // tokens in pool
      TABLE token {  // single table, scoped by contract account name
        uint64_t token_id;
        string token_chain;
        extended_asset active_balance;
        uint64_t total_quantity; // including amounts in liquidity-add queue
        double in_rate;
        double out_rate;
        time_point rates_updated;
        
        uint64_t primary_key() const { return token_id; }
        uint64_t by_sym_code() const { return balance.symbol.code().raw(); }
      };
      
      // liquidity provider balances     
      TABLE balance { // single table, scoped by contract account name
        uint64_t id; // unique to <owner, token> pair
        name owner;
        uint64_t token_id;
        int64_t queued;
        int64_t pool_contrib;
        double cred;
        time_point cred_updated;

        uint64_t primary_key() const { return id; }
        uint64_t by_owner() const { return owner.value(); }
        uint64_t by_token() const { return token_id; }
      };
      
      // parameters
      TABLE parameter { // scoped by token index
        name param_name;
        string param_val;
        double param_float;
        
        uint64_t primary_key() const { return param_name.value(); }        
      };
      
      // chains
      TABLE chain { // single table, scoped by contract account name
        uint64_t hex_name;
        string common_name;
        
        uint64_t primary_key() const { return hex_name; }
        uint128_t by_common_name() const {
          uint128_t rv = 0;
          memcpy(&rv, common_name.c_str(), std::min(16, common_name.size()));
          return rv;
        }
      };

      typedef eosio::multi_index<"balances"_n, balance, indexed_by
               < "by_owner"_n,
                 const_mem_fun<balance, uint64_t, &balance::by_owner > >,
               < "by_token"_n,
                 const_mem_fun<balance, uint64_t, &balance::by_token > >
               > balances;
      typedef eosio::multi_index<"tokens"_n, token, indexed_by
               < "by_sym_code"_n,
                 const_mem_fun<token, uint64_t, &token::by_sym_code > >
               > tokens;
      typedef eosio::multi_index<"params"_n, parameter > params;
      typedef eosio::multi_index<"chains"_n, chain, indexed_by
               < "by_common_name"_n,
                 const_mem_fun<chain, uint128_t, &chain::by_common_name > >
               > chains;

 
