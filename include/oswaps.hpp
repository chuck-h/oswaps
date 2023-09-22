#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>
#include <eosio/singleton.hpp>
#include <algorithm>

using namespace eosio;
using std::string;
   /**
    * The `oswaps` contract implements a token conversion service ("currency exchange") based on a
    *   multilateral token pool using the "balancer" invariant. The contract uses a "single sided"
    *   liquidity investment model in which balancer weights are adjusted during liquidity
    *   changes. These fundamental functions are supported :
    *   - add liquidity, e.g. insert some Token A into the pool
    *   - withdraw liquidity, e.g. extract some Token A from the pool
    *   - convert, e.g. change token A to Token B, delivered to a recipient
    * The initial `oswaps` implementation is a Proof of Concept and lacks some functions
    *   including
    *   - exchange fees
    *   - liquidity metering
    *   - multichain operation
    *
    * Transfers into the oswaps contract proceed via two steps :
    *   First, the originator submits transaction details in a "prep" action, which returns data
    *     including a nonce, i.e. an ephemeral transaction code.
    *   Second, the originator sends an ordinary token transfer action to the contract, with a memo
    *     field containing the nonce; the action sent earlier is executed.
    *
    * The contract anticipates a future ability to operate across different chains, with
    *   varying conventions for token identification. Therefore token identities are
    *   recorded in a table and a numerical code from that table is used in action fields.
    *   (Typically a redundant action field with the symbol string is also present,
    *   to minimize errors in manually-entered transaction data.)
    *   Initially the contract supports Antelope chains and registers tokens
    *   referencing a 4-tuple < family, chain, contract, symbol > where family is
    *   "antelope".
    *
    * The contract supports an asset table with one row for each recognized token.
    *   The parameter set serves the Proof of Concept.
    *   Future expansion TBD, may involve adding new fields to the asset table or
    *   adding a supplementary table.
    *
    * Authorization model
    * The contract account owner permission should be a "cold" multisig which is used once for
    *   uploading the contract and once for specifying a manager account. It has no
    *   operational role after that, however for test purposes the `reset` action is
    *   implemented.
    *   For any token having transfer rights restricted to whitelisted accounts, the contract
    *   account must be added to the token's whitelist.
    * The manager account will typically be under some sort type of governance control (e.g. DAO).
    *   This account can freeze and unfreeze transaction processing on a per-token basis. The
    *   manager account may transfer authority to a replacement manager account.
    * By placing named authorizations in the asset table, this facility may be used to
    *   assign per-token management powers, but the details are TBD and beyond the PoC.    
    */

CONTRACT oswaps : public contract {
  public:
      using contract::contract;

      /**
          * The `reset` action executed by the oswaps contract account deletes all table data
      */
      ACTION reset();

      /**
          * The one-time `init` action executed by the oswaps contract account records
          *  the manager account, nonce validity time, and chain identifier
          *
          * @param manager - an account empowered to execute freeze and setparameter actions
          * @param nonce_life_msec - the time validity window for a "prep" action nonce
          * @param chain - a well-known name or hex-encoded chain_id
      */
      ACTION init(name manager, uint64_t nonce_life_msec, string chain);
      
      /**
          * The `freeze` action executed by the manager or other authorized actor suspends
          * transactions in a specified token
          *
          * @param actor - an account empowered to execute the freeze action
          * @param token_id - a numerical token identifier in the asset table
          * @param symbol - the symbol of the affected token
      */
      ACTION freeze(name actor, uint64_t token_id, string symbol);

      /**
          * The `unfreeze` action executed by the manager or other authorized actor enables
          * transactions in a specified token
          *
          * @param actor - an account empowered to execute the freeze action
          * @param token_id - a numerical token identifier in the asset table
          * @param symbol - the symbol of the affected token
      */
      ACTION unfreeze(name actor, uint64_t token_id, string symbol);
      

      /**
          * The `createasseta` creates an entry in the asset table for an
          *   antelope family token. If a table entry exists, it is overwritten
          *   TBD: how to record IBC wrapped token contracts
          *
          * @param actor - an account empowered to set the specified parameter
          * @param chain - the "home chain" of a token, expressed as a common name
          *                  followed by ';' and a hex chain id
          * @param contract - the contract name
          * @param symbol - the symbol of the affected token
          * @param meta - metadata (JSON) 
      */
      ACTION createasseta(name actor, string chain, name contract, symbol_code symbol, string meta);

      /**
          * The `withdraw` action withdraws liquidity while simultaneously
          *   adjusting weight-fractions in the balancer invariant formula
          * [future: Token transfers occur through a rate-throttling queue which may
          *    introduce delays]
          * 
          * @param account - the account receiving the tokens
          * @param token_id - a numerical token identifier in the asset table
          * @param amount - the amount of asset (quantity, symbol) to withdraw from pool;
          * @param weight - the new balancer weight (or zero)
      */
      ACTION withdraw(name account, uint64_t token_id, string amount, float weight);

      /**
          * The `addliqprep` action adds liquidity while simultaneously
          *   adjusting weight-fractions in the balancer invariant formula
          * [future: Token transfers occur through a rate-throttling queue which may
          *    introduce delays]
          * 
          * @param account - the account sourcing the tokens
          * @param token_id - a numerical token identifier in the asset table
          * @param amount - the amount of asset (quantity, symbol) to add to pool;
          * @param weight - the new balancer weight (or zero)
          *
          * @result - a nonce identifying this transaction
      */
      [[eosio::action]] uint32_t addliqprep(name account, uint64_t token_id,
                                            string amount, float weight);

      /**
          * The `exchangeprep` action is a function describing a conversion
          *   ("currency exchange") transaction, taking a quantity of tokens from the sender
          *   and delivering a corresponding quantity of a different token to the recipient. 
          * The conversion ratio ("exchange rate") is computed according to a multilateral
          *   "balancer" type algorithm with an invariant V
          *     V = B1**W1 * B2**W2 * ... * Bn**Wn
          *   and therefore depends dynamically on the pool balances B. In the small-
          *   transaction limit (no "slippage"), and with zero fees, an input of
          *   Qi tokens of type i will emit Qj = Qi * (Bj/Bi)*(Wi/Wj) tokens of type j.
          * In the action call, both incoming and outgoing amounts are specified, but
          *   only one of them is the "exact" or "controlling" parameter. The other
          *   amount specifies a limit; the action will fail if the computed exchange value
          *   is beyond the limit. The `mods` parameter encodes the choice of exact field.
          *   If the incoming amount is exact, the computed outgoing value must be no less than
          *   the outgoing amount parameter. If the outgoing amount is exact, the computed
          *   incoming value must be no more than the value of the incoming amount parameter.
          * 
          * 
          * @param sender - the account sourcing tokens to the transaction
          * @param in_token_id - a numerical token identifierfor the incoming asset
          * @param in_amount - the incoming amount (quantity, symbol) 
          * @param recipient - the account receiving tokens from the transaction
          * @param out_token_id - a numerical token identifier for the outgoing asset
          * @param out_amount - the outgoing amount (quantity, symbol)
          * @param mods - a simple json object
          * @param memo
          *
          * @result a vector of 4 uint64_t elements
          *     rv[0] - a nonce identifying this transaction
          *     rv[1] - the quantity of incoming tokens assessed as fee
          *     rv[2] - the quantity of outgoing tokens assessed as fee
          *     rv[3] - the computed quantity field of the non-controlling asset
          *
      */
      [[eosio::action]] std::vector<int64_t> exchangeprep(
           name sender, uint64_t in_token_id, string in_amount,
           name recipient, uint64_t out_token_id, string out_amount,
           string mods, string memo);

      [[eosio::on_notify("*::transfer")]]
      void ontransfer(name from, name to, eosio::asset quantity, string memo);

      
  private:

      /********** standard token tables ***********/
      TABLE account { // scoped on account name
        asset    balance;
        uint64_t primary_key()const { return balance.symbol.code().raw(); }
      };
      TABLE currency_stats {  // scoped on token symbol code
        asset    supply;
        asset    max_supply;
        name     issuer;
        uint64_t primary_key()const { return supply.symbol.code().raw(); }
      };
      typedef eosio::multi_index< "accounts"_n, account > accounts;
      typedef eosio::multi_index< "stat"_n, currency_stats > stats;
      /**************/

      
      // config
      TABLE config { // singleton, scoped by contract account name
        name manager;
        uint64_t nonce_life_msec;
        checksum256 chain_id;
        uint32_t last_nonce;
      } config_row;

      // types of tokens
      TABLE assettype { // single table, scoped by contract account name
        uint64_t token_id;
        name family;
        string chain;
        checksum256 chain_code;
        string contract;
        uint64_t contract_code;
        string symbol;
        bool active;
        string metadata;
        float weight;
        
        uint64_t primary_key() const { return token_id; }
        uint64_t by_family() const { return family.value; }
      };
     
     // prepped liquidity additions
     TABLE adprep { // single table, scoped by contract account name
       uint64_t nonce;
       time_point expires;
       name account;
       uint64_t token_id;
       string amount;
       float weight;
       
       uint64_t primary_key() const { return expires.elapsed._count; }
       uint64_t by_nonce() const { return nonce; }
     };

     // prepped exchanges
     TABLE exprep { // single table, scoped by contract account name
       uint64_t nonce;
       time_point expires;
       name sender;
       uint64_t in_token_id;
       string in_amount;
       name recipient;
       uint64_t out_token_id;
       string out_amount;
       string mods;
       string memo;
       
       uint64_t primary_key() const { return expires.elapsed._count; }
       uint64_t by_nonce() const { return nonce; }
     };
       
      typedef eosio::singleton< "configs"_n, config > configs;
      typedef eosio::multi_index< "configs"_n, config >  dump_for_config;
      typedef eosio::multi_index<"assets"_n, assettype, indexed_by
               < "byfamily"_n,
                 const_mem_fun<assettype, uint64_t, &assettype::by_family > >
               > assets;
      typedef eosio::multi_index<"adpreps"_n, adprep, indexed_by
               < "bynonce"_n,
                 const_mem_fun<adprep, uint64_t, &adprep::by_nonce > >
               > adpreps;
      typedef eosio::multi_index<"expreps"_n, exprep, indexed_by
               < "bynonce"_n,
                 const_mem_fun<exprep, uint64_t, &exprep::by_nonce > >
               > expreps;
};


