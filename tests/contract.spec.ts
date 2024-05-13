const { Blockchain, nameToBigInt, symbolCodeToBigInt, addInlinePermission,
        expectToThrow, } = require("@proton/vert");
const { Asset, TimePoint, Transaction, Action, Name, Serializer, PermissionLevel } = require("@greymass/eosio");
const { assert, expect } = require("chai");
const blockchain = new Blockchain()

// Load contract (use paths relative to the root of the project)
const oswaps = blockchain.createContract('oswaps', 'build/oswaps')
const token = blockchain.createContract('token', 'build/token')
const token2 = blockchain.createContract('token2', 'build/token')
const symAZURES = Asset.SymbolCode.from('AZURES')
const symBURGS = Asset.SymbolCode.from('BURGS')

var accounts
var rows

async function initTokens() {
    console.log('issue AZURES and BURGS')
    await token.actions.create(['issuera', '1000000.0000 AZURES']).send('token@active')
    await token.actions.issue(['issuera', '1000000.0000 AZURES', 'issue some']).send('issuera@active')
    await token.actions.create(['issuerb', '1000000.0000 BURGS']).send('token@active')
    await token.actions.issue(['issuerb', '1000000.0000 BURGS', 'issue some']).send('issuerb@active')
}

function transferAction( contract, from, to, quantity, memo ) {
    return Action.from({
      account: contract.name,
      name: 'transfer',
      authorization: [PermissionLevel.from({
        actor: from,
        permission: 'active'
      })],
      data: Serializer.encode({
        abi: contract.abi,
        type: 'transfer',
        object: {
          from: from,
          to: to,
          quantity: quantity,
          memo: memo
        }
      }).array,
      permission: 'active'
    })
}

function addliqprepAction( contract, account, token_id, amount, weight) {
    return Action.from({
      authorization: [{
        actor: account,
        permission: 'active',
      }],
      account: contract.name,
      name: 'addliqprep',
      data: Serializer.encode({
        abi: contract.abi,
        type: 'addliqprep',
        object: { account: account, token_id: token_id, amount: amount, weight: weight },
      }).array,
    })
}

function exprepfromAction(contract, sender, recipient, in_token_id, out_token_id,
           in_amount, memo) {
    return Action.from({
      authorization: [{
        actor: sender,
        permission: 'active',
      }],
      account: contract.name,
      name: 'exprepfrom',
      data: Serializer.encode({
        abi: contract.abi,
        type: 'exprepfrom',
        object: { sender: sender, recipient: recipient, in_token_id: in_token_id,
          out_token_id: out_token_id, in_amount: in_amount, memo: memo },
      }).array,
    })
}
function expreptoAction(contract, sender, recipient, in_token_id, out_token_id,
           out_amount, memo) {
    return Action.from({
      authorization: [{
        actor: sender,
        permission: 'active',
      }],
      account: contract.name,
      name: 'exprepto',
      data: Serializer.encode({
        abi: contract.abi,
        type: 'exprepto',
        object: { sender: sender, recipient: recipient, in_token_id: in_token_id,
          out_token_id: out_token_id, out_amount: out_amount, memo: memo },
      }).array,
    })
}

/* Runs before each test */
beforeEach(async () => {
    blockchain.resetTables()
    accounts = await blockchain.createAccounts('manager', 'issuera', 'issuerb', 'alice', 'bob', 'user1', 'user2',
       'user3', 'user4', 'user5')
    await initTokens()
})

/* Tests */
describe('Oswaps', () => {
    it('did create tokens', async () => {
        rows = token.tables.stat(symbolCodeToBigInt(symAZURES)).getTableRows()
        assert.deepEqual(rows, [ { supply: '1000000.0000 AZURES', max_supply: '1000000.0000 AZURES', issuer: 'issuera' } ] )
        rows = token.tables.stat(symbolCodeToBigInt(symBURGS)).getTableRows()
        assert.deepEqual(rows, [ { supply: '1000000.0000 BURGS', max_supply: '1000000.0000 BURGS', issuer: 'issuerb' } ] )
    });
    it('did basic tests', async () => {

        console.log('configure')
    	await oswaps.actions.init(['user2', 'Telos']).send('oswaps@owner')
        const cfg = oswaps.tables.configs(nameToBigInt('oswaps')).getTableRows()
        assert.deepEqual(cfg, [ {chain_id: "4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11",
            last_token_id: 0, manager: "user2"} ] )
        console.log('reconfigure')
    	await oswaps.actions.init(['manager', 'Telos']).send('user2@active')
        const cfg2 = oswaps.tables.configs(nameToBigInt('oswaps')).getTableRows()
        assert.deepEqual(cfg2, [ {chain_id: "4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11",
            last_token_id: 0, manager: "manager" } ] )

        console.log('create assets')
        await oswaps.actions.createasseta(['issuera', 'Telos', 'token', 'AZURES', '']).send('issuera@active')
        await oswaps.actions.createasseta(['issuerb', 'Telos', 'token', 'BURGS', '']).send('issuerb@active')
        rows = oswaps.tables.assetsa(nameToBigInt('oswaps')).getTableRows()
        assert.deepEqual(rows, [ 
            { token_id: 1, chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11',
              contract_name: 'token', symbol: 'AZURES', active: true, metadata: '', weight: '0.0000000' },
            { token_id: 2, chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11',
              contract_name: 'token', symbol: 'BURGS', active: true, metadata: '', weight: '0.0000000' } ] )

        console.log('add AZURES liquidity')
        await blockchain.applyTransaction(Transaction.from({
          expiration: 0, ref_block_num: 0, ref_block_prefix: 0,
          actions: [ addliqprepAction( oswaps, 'issuera', 1, '10.0000 AZURES', 1.00),
                     transferAction(token, 'issuera', 'oswaps', '10.0000 AZURES', 'yep') ] 
        }))
        //console.log(blockchain.console)
        balances = [ token.tables.accounts([nameToBigInt('oswaps')]).getTableRows(),
            oswaps.tables.accounts([nameToBigInt('issuera')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'10.0000 AZURES'}], [{balance:'10.0000 LIQB'}] ])
        
        console.log('withdraw liquidity')
        await oswaps.actions.withdraw(['issuera', 1, '5.0000 AZURES', 0.00]).send('manager')
        balances = [ token.tables.accounts([nameToBigInt('oswaps')]).getTableRows(),
            oswaps.tables.accounts([nameToBigInt('issuera')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'5.0000 AZURES'}], [{balance:'5.0000 LIQB'}] ])
        console.log('add BURGS liquidity')   
        await blockchain.applyTransaction(Transaction.from({
          expiration: 0, ref_block_num: 0, ref_block_prefix: 0,
          actions: [ addliqprepAction( oswaps, 'issuerb', 2, '10.0000 BURGS', 1.00),
                     transferAction(token, 'issuerb', 'oswaps', '10.0000 BURGS', 'yep') ] 
        }))
        //console.log(blockchain.console)
        balances = [ token.tables.accounts([nameToBigInt('oswaps')]).getTableRows(),
            oswaps.tables.accounts([nameToBigInt('issuerb')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'10.0000 BURGS'}, {balance:'5.0000 AZURES'}], [{balance:'10.0000 LIQC'}] ])
         
        console.log('read pool status')
        await oswaps.actions.querypool([[1,2]]).send('bob')
        rvbuf = Buffer.from(blockchain.actionTraces[0].returnValue)
        rv = Serializer.decode({data: rvbuf, type: 'poolStatus', abi: oswaps.abi})
        rvstruct = JSON.parse(JSON.stringify(rv))
        assert.deepEqual(rvstruct,
          { status_entries: [
            { token_id: 1, balance: '5.0000 AZURES', weight: '0.5000000' },
            { token_id: 2, balance: '10.0000 BURGS', weight: '1.0000000' }
        ]})
        console.log("balancer computation, exact out")
        {
          // floating point calculation
          in_token = 2
          out_token = 1
          out_bal_before = parseFloat(rvstruct.status_entries.filter((e)=>e.token_id==out_token)[0]
            .balance.split(' ')[0])
          out_amount = 0.2000
          out_bal_after = out_bal_before - out_amount
          lc = Math.log(out_bal_after/out_bal_before)
          out_weight = parseFloat(rvstruct.status_entries.filter((e)=>e.token_id==out_token)[0]
            .weight.split(' ')[0])
          in_weight = parseFloat(rvstruct.status_entries.filter((e)=>e.token_id==in_token)[0]
            .weight.split(' ')[0])
          lnc = -out_weight/in_weight * lc
          in_bal_before = parseFloat(rvstruct.status_entries.filter((e)=>e.token_id==in_token)[0]
            .balance.split(' ')[0])
          in_bal_after = in_bal_before * Math.exp(lnc)
          computed_amt = in_bal_after - in_bal_before
          console.log(`computed input ${in_bal_before} + ${computed_amt} = ${in_bal_after}`)
        }
        console.log('exchange 1 - exact out')
        await token.actions.transfer(['issuerb', 'bob', '100.0000 BURGS', '']).send('issuerb')
        await blockchain.applyTransaction(Transaction.from({
          expiration: 0, ref_block_num: 0, ref_block_prefix: 0,
          actions: [ expreptoAction(oswaps, 'bob', 'alice', 2, 1, '0.2000 AZURES', 'my memo'),
                     transferAction(token, 'bob', 'oswaps', '0.3000 BURGS', 'yip') ] 
        }))
        //console.log(blockchain.console)
        balances = [ token.tables.accounts([nameToBigInt('oswaps')]).getTableRows(),
             token.tables.accounts([nameToBigInt('alice')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'10.2062 BURGS'}, {balance:'4.8000 AZURES'}], [{balance:'0.2000 AZURES'}] ])
        console.log(`new oswaps input balance ${JSON.stringify(balances[0].filter((e)=>(e.balance.split(' ')[1]=='BURGS')) )}`)
        
        console.log("balancer computation, exact in")
        await oswaps.actions.querypool([[1,2]]).send('bob')
        rvbuf = Buffer.from(blockchain.actionTraces[0].returnValue)
        rv = Serializer.decode({data: rvbuf, type: 'poolStatus', abi: oswaps.abi})
        rvstruct = JSON.parse(JSON.stringify(rv))
        {
          // floating point calculation
          in_token = 2
          out_token = 1
          in_bal_before = parseFloat(rvstruct.status_entries.filter((e)=>e.token_id==in_token)[0]
            .balance.split(' ')[0])
          in_amount = 0.2500
          in_bal_after = in_bal_before + in_amount
          lc = Math.log(in_bal_after/in_bal_before)
          out_weight = parseFloat(rvstruct.status_entries.filter((e)=>e.token_id==out_token)[0]
            .weight.split(' ')[0])
          in_weight = parseFloat(rvstruct.status_entries.filter((e)=>e.token_id==in_token)[0]
            .weight.split(' ')[0])
          lnc = -in_weight/out_weight * lc
          out_bal_before = parseFloat(rvstruct.status_entries.filter((e)=>e.token_id==out_token)[0]
            .balance.split(' ')[0])
          out_bal_after = out_bal_before * Math.exp(lnc)
          computed_amt = out_bal_before - out_bal_after
          console.log(`computed output ${out_bal_before} - ${computed_amt} = ${out_bal_after}`)
        }

        console.log('exchange 2 - exact in')
        await blockchain.applyTransaction(Transaction.from({
          expiration: 0, ref_block_num: 0, ref_block_prefix: 0,
          actions: [ exprepfromAction(oswaps, 'bob', 'alice', 2, 1, '0.2500 BURGS', 'my memo'),
                     transferAction(token, 'bob', 'oswaps', '0.2500 BURGS', 'yip') ] 
        }))
        //console.log(blockchain.console)
        balances = [ token.tables.accounts([nameToBigInt('oswaps')]).getTableRows(),
             token.tables.accounts([nameToBigInt('alice')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'10.4562 BURGS'}, {balance:'4.5732 AZURES'}], [{balance:'0.4268 AZURES'}] ])
        console.log(`new oswaps output balance ${JSON.stringify(balances[0].filter((e)=>(e.balance.split(' ')[1]=='AZURES')) )}`)
        console.log("check for liquidity tokens")
        balances = [ oswaps.tables.accounts([nameToBigInt('issuera')]).getTableRows(),
             oswaps.tables.accounts([nameToBigInt('issuerb')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'5.0000 LIQB'}], [{balance:'10.0000 LIQC'}] ])

    });
})

