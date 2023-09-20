console.log('oswaps.test.js')

const { describe } = require("riteway")
const { eos, names, getTableRows, isLocal, sleep, initContracts,
        httpEndpoint, getBalance, getBalanceFloat, asset } = require("../scripts/helper")
const { addActorPermission } = require("../scripts/deploy")
const { equals } = require("ramda")
const fetch = require("node-fetch");
const { oswaps, token, testtoken, firstuser, seconduser, thirduser, fourthuser,
        fifthuser, owner } = names
const moment = require('moment')

const get_scope = async ( code ) => {
  const url = httpEndpoint + '/v1/chain/get_table_by_scope'
  const params = {
    json: "true",
    limit: 20,
    code: code
  }
  const rawResponse = await fetch(url, {
        method: 'POST',
        headers: {
            'Accept': 'application/json',
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(params)
  });

  const res = await rawResponse.json();
  return res
}

const get_supply = async( code, symbol ) => {
  const resp = await getTableRows({
    code: code,
    scope: symbol,
    table: 'stat',
    json: true
  })
  const res = await resp;
  return res.rows[0].supply;
}

describe('oswaps', async assert => {

  if (!isLocal()) {
    console.log("only run unit tests on local - don't reset accounts on mainnet or testnet")
    return
  }

  console.log('installed at '+oswaps)
  const contracts = await Promise.all([
    eos.contract(oswaps),
    eos.contract(token)
  ]).then(([oswaps, token]) => ({
    oswaps, token
  }))

  const starttime = new Date()

  console.log('--Normal operations--')

  console.log('reset')
  await contracts.oswaps.reset( { authorization: `${oswaps}@owner` })

  assert({
    given: 'reset all',
    should: 'clear table RAM',
    actual: await get_scope(oswaps),
    expected: { rows: [], more: '' }
  })

  console.log('first init')
  await contracts.oswaps.init( firstuser, 10000, "Telos", { authorization: `${firstuser}@active` })

  assert({
    given: 'init',
    should: 'initialize config',
    actual: await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'configs',
      json: true
    }),
    expected: { rows: [ { manager: 'seedsuseraaa', nonce_life_msec: 10000, chain_id: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11' } ], more: false, next_key: '' }
  })

  console.log('reconfigure')
  await contracts.oswaps.init( seconduser, 20000, "Telos", { authorization: `${oswaps}@owner` })

  assert({
    given: 'init',
    should: 'initialize config',
    actual: await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'configs',
      json: true
    }),
    expected: { rows: [ { manager: 'seedsuserbbb', nonce_life_msec: 20000, chain_id: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11' } ], more: false, next_key: '' }
  })

  console.log('create SEEDS & TESTS assets')
  await contracts.oswaps.createasseta( firstuser, "Telos", token, 'SEEDS', "",{ authorization: `${firstuser}@active` })
  await contracts.oswaps.createasseta( firstuser, "Telos", token, 'TESTS', "",{ authorization: `${firstuser}@active` })

  assert({
    given: 'create',
    should: 'create SEEDS & TESTS assets',
    actual: await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'assets',
      json: true
    }),
    expected: { rows: [ { token_id: 0, family: 'antelope', chain: 'Telos', chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11', contract: 'token.seeds', contract_code: '14781000357308952576', symbol: 'SEEDS', active: 0, metadata: '', weight: '0.00000000000000000' }, { token_id: 1, family: 'antelope', chain: 'Telos', chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11', contract: 'token.seeds', contract_code: '14781000357308952576', symbol: 'TESTS', active: 0, metadata: '', weight: '0.00000000000000000' } ], more: false, next_key: '' }
    
  })


})


