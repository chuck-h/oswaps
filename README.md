# oSwaps

**oSwaps** is a multilateral cryptocurrency exchange intended to support an ecosystem of local currencies. This repository contains smart contracts supporting oSwaps features on the Telos blockchain.

A design paper is located at https://docs.google.com/document/d/1oHhCGfU-CEjdKmq8iV9gaumID55fdgRxv0nb9O6i3so/edit?usp=sharing

## Status

Under development. This contract is currently a proof-of-concept for the swapping functions. It does not implement important features from the design paper such as swap fees, authorizations for weight changes and liquidity withdrawal, and liquidity movement rate controls.

Development and testing is being migrated to https://github.com/nsjames/fuckyea


A proof-of-concept web interface for swaps in under development at https://github.com/chuck-h/oswapweb .

# Setup

### Note

Refer to https://github.com/nsjames/fuckyea/blob/main/README.md for use of the new development platform .
![image](https://github.com/chuck-h/oswaps-smart-contracts/assets/2141014/99a0fb7d-d0d2-4482-9b82-09e46bb7024b)

# Install

The compiled contract takes about 840kB of RAM. Additional table RAM will be required for each token. 

The contract `active` auth must include `<contract>@eosio.code`

# Initialize

Call the `init` action to set the blockchain ID and a manager account

# Setup (old)
The description below refers to an earlier development platform which is deprecated.

The development scripts were ported from https://github.com/JoinSEEDS/seeds-smart-contracts and may not function well.

### Environment (deprecated)

The .env file contains the all-important keys for local, testnet, and potentially mainnet

It also contains a compiler setting - use local compiler 

Copy the example to .env

```
cp .env.example .env
```

### Compiler Setup in .env file

Use COMPILER=local if using https://github.com/AntelopeIO/DUNES for local unit tests

### Tools Setup

```
npm install
```

### Start local test network with DUNES

get DUNES from `https://github.com/AntelopeIO/DUNES` and follow install process in README

Upgrade nodejs version in the DUNES container `https://docs.npmjs.com/downloading-and-installing-node-js-and-npm`. (The js test scripts need a newer version of nodejs than is shipped with the standard DUNES container.) The below sequence gives a deprecation warning but works (Oct 2023).
```
 dune -- cp /host`pwd`/nodesource_setup.sh .
 dune -- bash nodesource_setup.sh
 dune -- apt-get install nodejs
```

```
dune --start mynode
dune --bootstrap-system
dune --activate-feature ACTION_RETURN_VALUE
dune --create-account owner eosio EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3
```
You may want to export this "fresh" test node container for reimport as a clean test environment
```
dune --export-node mynode <path>
...
dune --import-node <path> mynode
```

When using the `ops.js` test script (see below) in the DUNES container, use this convention
```
dune -- /host`pwd`/scripts/ops.js init
```
instead of
```
./scripts/ops.js init
```

### Start single-node local test network (alternative without DUNES)

The local testnet is required for unit tests.

```
nodeos -e -p eosio --plugin eosio::producer_plugin --plugin eosio::producer_api_plugin --plugin eosio::chain_api_plugin --plugin eosio::state_history_plugin --disable-replay-opts --plugin eosio::http_plugin --access-control-allow-origin='*' --access-control-allow-headers "*" --contracts-console --http-validate-host=false --delete-all-blocks --delete-state-history --verbose-http-errors >> nodeos.log 2>&1
```

### Create local testnet owner account

This requires a wallet capable of signing the "create account" action, for example `cleos`.

```
cleos wallet create --to-console
cleos wallet import --private-key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3 # LOCAL_PRIVATE_KEY in .env file
cleos create account eosio owner EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV # Public key matching above
```

# Deploy Tools

Use the ops.js script to 

### init all contracts and deploy them on local network

```
./scripts/ops.js init
```

### update contract permissions

This command will update all permissions on all contracts

It will check if a permission is already set and only set permissions that
have been added or have been changed.

```
./scripts/ops.js updatePermissions
```

### Compile, deploy, or test a contract

```
./scripts/ops.js compile harvest => compiles seeds.harvest.cpp
```
```
./scripts/ops.js deploy accounts => deploys accounts contract
```
```
./scripts/ops.js test accounts => run unit tests on accounts contract
```
```
./scripts/ops.js run accounts => compile, deploy, and run unit tests
```
### Specify more than one contract - 

Contract is a varadic parameter

```
./scripts/ops.js run accounts onboarding organization
```

### Deploy on testnet
```
EOSIO_NETWORK=telosTestnet ./scripts/ops.js deploy accounts
```
### Deploy on mainnet
```
EOSIO_NETWORK=telosMainnet ./scripts/ops.js deploy accounts
```

### usage ops.js 
```
./scripts/ops.js <command> <contract name> [additional contract names...]
command = compile | deploy | test | run
```


### run a contract - compile, then deploy, then test 

```
example: 
./scripts/ops.js run harvest => compiles seeds.harvest.cpp, deploys it, runs unit tests
```


