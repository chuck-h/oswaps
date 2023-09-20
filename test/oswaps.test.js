console.log('oswaps.test.js')

const { describe } = require("riteway")
const { eos, names, getTableRows, isLocal, sleep, initContracts,
        httpEndpoint, getBalance, getBalanceFloat, asset } = require("../scripts/helper")
const { addActorPermission } = require("../scripts/deploy")
const { equals } = require("ramda")
const fetch = require("node-fetch");
const { oswaps, token, firstuser, seconduser, thirduser, fourthuser,
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
  //await contracts.oswaps.reset(true, 100, { authorization: `${oswaps}@active` })

  assert({
    given: 'reset all',
    should: 'clear table RAM',
    actual: await get_scope(oswaps),
    expected: { rows: [], more: '' }
  })


})


