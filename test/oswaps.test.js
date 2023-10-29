console.log('oswaps.test.js')

const { describe } = require("riteway")
const { eos, names, accounts, getTableRows, isLocal, sleep, initContracts,
        httpEndpoint, getBalance, getBalanceFloat, asset, allContracts } = require("../scripts/helper")
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
    eos.contract(token),
    eos.contract(testtoken)
  ]).then(([oswaps, token, testtoken]) => ({
    oswaps, token, testtoken
  }))

const empty = async( account, tokenaccount) => {
  const resp = await getTableRows({
    code: tokenaccount.account,
    scope: account,
    table: 'accounts',
    json: true
  })
  const res = await resp;
  var bal = res.rows.filter((x)=>x.balance.split(' ')[1] == tokenaccount.supply.split(' ')[1])
  if (bal.length == 0 || bal[0].balance.split(' ')[0] == 0) {
    return
  }
  await contracts[tokenaccount.name].transfer( account, owner, bal[0].balance,
     "empty", { authorization: `${account}@active` })
} 

  const starttime = new Date()

  console.log('--Normal operations--')

  console.log('add eosio.code permissions')
  await addActorPermission(oswaps, 'active', oswaps, 'eosio.code')

  console.log('reset')
  await contracts.oswaps.reset( { authorization: `${oswaps}@owner` })
  console.log('reset liq token accounts')
  await contracts.oswaps.resetacct( owner, { authorization: `${oswaps}@owner` })
  await contracts.oswaps.resetacct( firstuser, { authorization: `${oswaps}@owner` })
  console.log('sending oswaps token balances back to owner')
  await empty(oswaps, accounts.token)
  await empty(oswaps, accounts.testtoken)
  await empty(seconduser, accounts.token)

  assert({
    given: 'reset all',
    should: 'clear table RAM',
    actual: await get_scope(oswaps),
    expected: { rows: [], more: '' }
  })

  console.log('first init')
  await contracts.oswaps.init( firstuser, 10000, "Telos", { authorization: `${oswaps}@owner` })

  assert({
    given: 'init',
    should: 'initialize config',
    actual: await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'configs',
      json: true
    }),
    expected: { rows: [ { manager: 'seedsuseraaa', nonce_life_msec: 10000, chain_id: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11', last_token_id: 0, last_nonce: 1111 } ], more: false, next_key: '' }
  })

  console.log('reconfigure')
  const expirems = 1000;
  await contracts.oswaps.init( seconduser, expirems, "Telos", { authorization: `${firstuser}@active` })

  assert({
    given: 'init',
    should: 'initialize config',
    actual: await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'configs',
      json: true
    }),
    expected: { rows: [ { manager: 'seedsuserbbb', nonce_life_msec: 1000, chain_id: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11', last_token_id: 0, last_nonce: 1111 } ], more: false, next_key: '' }
    
  })

  console.log('create SEEDS & TESTS assets')
  await contracts.oswaps.createasseta( firstuser, "Telos", token, 'SEEDS', "",{ authorization: `${firstuser}@active` })
  var res = await contracts.oswaps.createasseta( firstuser, "Telos", token, 'TESTS', "",{ authorization: `${firstuser}@active` })
  var rvbuf = Buffer.from(
       res.processed.action_traces[0].return_value_hex_data, 'hex'
     )
  var rv = new Int32Array(rvbuf.buffer, rvbuf.byteOffset, 1)[0]
  console.log(`action returned ${rv}`)

  assert({
    given: 'create',
    should: 'create SEEDS & TESTS assets',
    actual: await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'assetsa',
      json: true
    }),
    expected: { rows: [ { token_id: 1, chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11', contract_name: 'token.seeds', symbol: 'SEEDS', active: 1, metadata: '', weight: '0.00000000000000000' }, { token_id: 2, chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11', contract_name: 'token.seeds', symbol: 'TESTS', active: 1, metadata: '', weight: '0.00000000000000000' } ], more: false, next_key: '' }
    
  })

  console.log('add liquidity 1 - prep')
  // TBD this expiration computation doesn't make sense, but works.
  var exptimestamp = (new Date(500*Math.trunc(Date.now()/500) + expirems + 500)).toISOString().slice(0,-1);
  res = await contracts.oswaps.addliqprep( firstuser, 1, "10.0000 SEEDS", 1.00, { authorization: `${firstuser}@active` })
  rvbuf = Buffer.from(
       res.processed.action_traces[0].return_value_hex_data, 'hex'
     )
  rv = new Int32Array(rvbuf.buffer, rvbuf.byteOffset, 1)[0]
  console.log(`action returned ${rv}`)
  
  assert({
    given: 'addliqprep',
    should: 'create table entry',
    actual: (await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'adpreps',
      json: true
    })),
    expected: { rows: [ { nonce: rv, expires: exptimestamp, account: 'seedsuseraaa', token_id: 1, amount: '10.0000 SEEDS', weight: '1.00000000000000000' } ], more: false, next_key: '' }
  })

  console.log('add liquidity 2 - transfer')

  await contracts.token.transfer( firstuser, oswaps, "10.0000 SEEDS", "nonce 1112", { authorization: `${firstuser}@active` })

  assert({
    given: 'send tokens',
    should: 'add liquidity & issue liq tokens',
    actual: [ (await getTableRows({
      code: token,
      scope: oswaps,
      table: 'accounts',
      json: true
    })).rows[0],
    (await getTableRows({
      code: oswaps,
      scope: firstuser,
      table: 'accounts',
      json: true
    })) ],
    expected: [ { balance: '10.0000 SEEDS' }, { rows: [ { balance: '10.0000 LIQB' } ], more: false, next_key: '' } ]
  })

  console.log('withdraw liquidity')

  await contracts.oswaps.withdraw( firstuser, 1, "5.0000 SEEDS", 0.00, { authorization: `${seconduser}@active` })

  assert({
    given: 'withdraw tokens',
    should: 'reduce liquidity',
    actual: [(await getTableRows({
        code: oswaps,
        scope: oswaps,
        table: 'assetsa',
        json: true
      })),
      (await getTableRows({
        code: token,
        scope: oswaps,
        table: 'accounts',
        json: true
      })).rows[0]
    ],
    expected: [ { rows: [ { token_id: 1, chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11', contract_name: 'token.seeds', symbol: 'SEEDS', active: 1, metadata: '', weight: '0.50000000000000000' }, { token_id: 2, chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11', contract_name: 'token.seeds', symbol: 'TESTS', active: 1, metadata: '', weight: '0.00000000000000000' } ], more: false, next_key: '' }, { balance: '5.0000 SEEDS' } ]
  })

  console.log('add TESTS liquidity')
  // TBD this expiration computation doesn't make sense, but works.
  exptimestamp = (new Date(500*Math.trunc(Date.now()/500) + expirems + 500)).toISOString().slice(0,-1);
  res = await contracts.oswaps.addliqprep( firstuser, 2, "10.0000 TESTS", 1.00, { authorization: `${firstuser}@active` })
  rvbuf = Buffer.from(
       res.processed.action_traces[0].return_value_hex_data, 'hex'
     )
  rv = new Int32Array(rvbuf.buffer, rvbuf.byteOffset, 1)[0]
  console.log(`action returned ${rv}`)
  
  await contracts.token.transfer( owner, oswaps, "10.0000 TESTS", "1113", { authorization: `${owner}@active` })

  assert({
    given: 'send TESTS tokens',
    should: 'add liquidity',
    actual: (await getTableRows({
      code: token,
      scope: oswaps,
      table: 'accounts',
      json: true
    })),
    expected: { rows: [ { balance: '5.0000 SEEDS' }, { balance: '10.0000 TESTS' } ], more: false, next_key: '' }
  })

  console.log('exchange 1 - prep')
  // TBD this expiration computation doesn't make sense, but works.
  exptimestamp = (new Date(500*Math.trunc(Date.now()/500) + expirems + 500)).toISOString().slice(0,-1);
  res = await contracts.oswaps.exchangeprep( owner, 2, "0.2500 TESTS",
       seconduser, 1, "0.2000 SEEDS", '{"exact":"out"}', "my memo", { authorization: `${owner}@active` })
  rvbuf = Buffer.from(
       res.processed.action_traces[0].return_value_hex_data, 'hex'
     )
  //console.log(rvbuf);
  rv = [1,9,17,25].map((x)=>rvbuf.readInt32LE(x));
  console.log(`action returned ${rv}`)
  
  assert({
    given: 'exchangeprep, exact out',
    should: 'create table entry',
    actual: (await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'expreps',
      json: true
    })),
    expected: { rows: [ { nonce: 1114, expires: exptimestamp, sender: 'owner', in_token_id: 2, in_amount: '0.2500 TESTS', recipient: 'seedsuserbbb', out_token_id: 1, out_amount: '0.2000 SEEDS', mods: '{"exact":"out"}', memo: 'my memo' } ], more: false, next_key: '' }
  })
  
  console.log('exchange 2 - transfer')

  await contracts.token.transfer( owner, oswaps, "0.3000 TESTS", "1114", { authorization: `${owner}@active` })

  assert({
    given: 'send tokens',
    should: 'execute exchange',
    actual: [ await getTableRows({
      code: token,
      scope: oswaps,
      table: 'accounts',
      json: true
    }),
    await getTableRows({
      code: token,
      scope: seconduser,
      table: 'accounts',
      json: true
    }) ],
    expected: [ { rows: [ { balance: '4.8000 SEEDS' }, { balance: '10.2062 TESTS' } ], more: false, next_key: '' }, { rows: [ { balance: '0.2000 SEEDS' } ], more: false, next_key: '' } ]
  })

  console.log('exchange 3 - prep')
  // TBD this expiration computation doesn't make sense, but works.
  exptimestamp = (new Date(500*Math.trunc(Date.now()/500) + expirems + 500)).toISOString().slice(0,-1);
  res = await contracts.oswaps.exchangeprep( owner, 2, "0.2500 TESTS",
       seconduser, 1, "0.2000 SEEDS", '{"exact":"in"}', "my memo", { authorization: `${owner}@active` })
  rvbuf = Buffer.from(
       res.processed.action_traces[0].return_value_hex_data, 'hex'
     )
  //console.log(rvbuf);
  rv = [1,9,17,25].map((x)=>rvbuf.readInt32LE(x));
  console.log(`action returned ${rv}`)
  
  assert({
    given: 'exchangeprep, exact in',
    should: 'create table entry',
    actual: (await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'expreps',
      json: true
    })),
    expected: { rows: [ { nonce: 1115, expires: exptimestamp, sender: 'owner', in_token_id: 2, in_amount: '0.2500 TESTS', recipient: 'seedsuserbbb', out_token_id: 1, out_amount: '0.2000 SEEDS', mods: '{"exact":"in"}', memo: 'my memo' } ], more: false, next_key: '' }
  })
  
  console.log('exchange 4 - transfer')

  await contracts.token.transfer( owner, oswaps, "0.2500 TESTS", "1115", { authorization: `${owner}@active` })

  assert({
    given: 'send tokens',
    should: 'execute exchange',
    actual: [ await getTableRows({
      code: token,
      scope: oswaps,
      table: 'accounts',
      json: true
    }),
    await getTableRows({
      code: token,
      scope: seconduser,
      table: 'accounts',
      json: true
    }) ],
    expected: [ { rows: [ { balance: '4.5732 SEEDS' }, { balance: '10.4562 TESTS' } ], more: false, next_key: '' }, { rows: [ { balance: '0.4268 SEEDS' } ], more: false, next_key: '' } ]
  })

  console.log("check for clean prep tables")
  assert({
    given: 'read prep tables',
    should: 'be no entries',
    actual: [ await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'adpreps',
      json: true
    }),
    await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'expreps',
      json: true
    }) ],
    expected: [ { rows: [], more: false, next_key: '' }, { rows: [], more: false, next_key: '' } ]
  })

  console.log("check for liquidity tokens")
  assert({
    given: 'check balances',
    should: 'see tokens',
    actual: [ (await getTableRows({
      code: oswaps,
      scope: owner,
      table: 'accounts',
      json: true
    })),
    (await getTableRows({
      code: oswaps,
      scope: firstuser,
      table: 'accounts',
      json: true
    })) ],
    expected: [ { rows: [ { balance: '10.0000 LIQC' } ], more: false, next_key: '' }, { rows: [ { balance: '5.0000 LIQB' } ], more: false, next_key: '' } ]
  })

  var stalenonce
  var freshnonce
  console.log('add TESTS liquidity 2x and expire')
  let actionProperlyBlocked = true
  try {
    // TBD this expiration computation doesn't make sense, but works.
    exptimestamp = (new Date(500*Math.trunc(Date.now()/500) + expirems + 500)).toISOString().slice(0,-1);
    res = await contracts.oswaps.addliqprep( firstuser, 2, "10.0000 TESTS", 1.00, { authorization: `${firstuser}@active` })
    rvbuf = Buffer.from(
         res.processed.action_traces[0].return_value_hex_data, 'hex'
       )
    rv = new Int32Array(rvbuf.buffer, rvbuf.byteOffset, 1)[0]
    console.log(`action returned ${rv}`)
    stalenonce = rv
    
    exptimestamp = (new Date(500*Math.trunc(Date.now()/500) + expirems + 500)).toISOString().slice(0,-1);
    res = await contracts.oswaps.addliqprep( firstuser, 2, "11.0000 TESTS", 1.00, { authorization: `${firstuser}@active` })
    rvbuf = Buffer.from(
         res.processed.action_traces[0].return_value_hex_data, 'hex'
       )
    rv = new Int32Array(rvbuf.buffer, rvbuf.byteOffset, 1)[0]
    console.log(`action returned ${rv}`)

    console.log("waiting...")
    await sleep(expirems + 500);

    exptimestamp = (new Date(500*Math.trunc(Date.now()/500) + expirems + 500)).toISOString().slice(0,-1);
    res = await contracts.oswaps.addliqprep( firstuser, 2, "12.0000 TESTS", 1.00, { authorization: `${firstuser}@active` })
    rvbuf = Buffer.from(
         res.processed.action_traces[0].return_value_hex_data, 'hex'
       )
    rv = new Int32Array(rvbuf.buffer, rvbuf.byteOffset, 1)[0]
    console.log(`action returned ${rv}`)
    freshnonce = rv
    
    await contracts.token.transfer( owner, oswaps, "10.0000 TESTS", stalenonce, { authorization: `${owner}@active` })
    actionProperlyBlocked = false
  } catch (err) {
    actionProperlyBlocked &&= err.toString().includes('no matching transaction')
    console.log( (actionProperlyBlocked ? "" : "un") + "expected error "+err)
  }
  
  assert({
    given: 'wait, then send TESTS tokens',
    should: 'fail',
    actual: actionProperlyBlocked,
    expected: true
  })

  console.log('send TEST tokens for liquidity on time')
  await contracts.token.transfer( owner, oswaps, "12.0000 TESTS", freshnonce, { authorization: `${owner}@active` })
  
  assert({
    given: 'send TESTS tokens',
    should: 'succeed and clear adprep queue',
    actual: (await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'adpreps',
      json: true
    })),
    expected: { rows: [], more: false, next_key: '' }
  })
    
  console.log('prepare exchange and expire')
  actionProperlyBlocked = true
  try {
    // TBD this expiration computation doesn't make sense, but works.
    exptimestamp = (new Date(500*Math.trunc(Date.now()/500) + expirems + 500)).toISOString().slice(0,-1);
    res = await contracts.oswaps.exchangeprep( owner, 2, "0.2500 TESTS",
         seconduser, 1, "0.2000 SEEDS", '{"exact":"out"}', "my memo", { authorization: `${owner}@active` })
    rvbuf = Buffer.from(
         res.processed.action_traces[0].return_value_hex_data, 'hex'
       )
    rv = [1,9,17,25].map((x)=>rvbuf.readInt32LE(x));
    console.log(`action returned ${rv}`)
    stalenonce = rv[0]
    
    await sleep(expirems + 500);
    
    await contracts.token.transfer( owner, oswaps, "0.3000 TESTS", stalenonce, { authorization: `${owner}@active` })

    actionProperlyBlocked = false
  } catch (err) {
    actionProperlyBlocked &&= err.toString().includes('no matching transaction')
    console.log( (actionProperlyBlocked ? "" : "un") + "expected error "+err)
  }
  
  assert({
    given: 'wait, then send TESTS tokens',
    should: 'fail',
    actual: actionProperlyBlocked,
    expected: true
  })

  console.log("use magic nonce")
  
  await contracts.token.transfer( owner, oswaps, "10.0000 TESTS", "42", { authorization: `${owner}@active` })

  assert({
    given: 'send TESTS tokens',
    should: 'add liquidity',
    actual: (await getTableRows({
      code: token,
      scope: oswaps,
      table: 'accounts',
      json: true
    })),
    expected: { rows: [ { balance: '4.5732 SEEDS' }, { balance: '32.4562 TESTS' } ], more: false, next_key: '' }
  })

  
  
  console.log("forget SEEDS asset")
  await contracts.oswaps.forgetasset( seconduser, 1, "removing SEEDS",
       { authorization: `${seconduser}@active` })  
  assert({
    given: 'read asset table',
    should: 'be no SEEDS entry',
    actual: await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'assetsa',
      json: true
    }),
    expected: { rows: [ { token_id: 2, chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11', contract_name: 'token.seeds', symbol: 'TESTS', active: 1, metadata: '', weight: '1.00000000000000000' } ], more: false, next_key: '' }
  })

  
  console.log("forget TESTS asset")
  await contracts.oswaps.forgetasset( seconduser, 2, "removing TESTS",
       { authorization: `${seconduser}@active` })  
  assert({
    given: 'read asset table',
    should: 'be no entries',
    actual: await getTableRows({
      code: oswaps,
      scope: oswaps,
      table: 'assetsa',
      json: true
    }),
    expected: { rows: [ ], more: false, next_key: '' }
  })

 
  console.log('reset')
  await contracts.oswaps.reset( { authorization: `${oswaps}@owner` })


})


