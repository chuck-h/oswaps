#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>

using namespace eosio;
using std::string;
   /**
    * The `oswaps' contract implements a multilateral token exchange algorithm
    *
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
          * @param manager - an account empowered to execute    actions
      */
      ACTION init(name manager);
      
      /**
          * The `changeliq` action adds/removes liquidity to/from a token pool
          * Token transfers occur through a rate-throttling queue which may introduce delays
          * 
          * @param owner - an account sourcing or receiving the tokens
          * @param chain - the "home chain" of the asset
          * @param amount - the (signed) amount of asset to add to pool;
          *    negative amount represents withdrawal.
      */
      ACTION changeliq(name owner, string chain, extended_asset amount);
      
      /**
          * The `convert` action accepts a quantity of tokens from the sender and delivers a
          *  corresponding quantity of a different token to the recipient
          *
          * @param sender -
          * @param in_chain -
          * @param in_amount
          * @param recipient
          * @param out_chain
          * @param out_amount
      */
      ACTION convert(name sender, string in_chain, extended_asset in_amount,
                      name recipient, string out_chain, extended_asset out_amount);
      


