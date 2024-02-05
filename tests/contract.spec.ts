const { Blockchain, nameToBigInt, symbolCodeToBigInt, addInlinePermission,
        expectToThrow } = require("@proton/vert");
const { Asset, TimePoint } = require("@greymass/eosio");
const { assert, expect } = require("chai");
const blockchain = new Blockchain()

// Load contract (use paths relative to the root of the project)
const oswaps = blockchain.createContract('oswaps', 'fyartifacts/oswaps')
const token = blockchain.createContract('token', 'fyartifacts/token')
const token2 = blockchain.createContract('token2', 'fyartifacts/token')
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
    it('did something', async () => {
        console.log('configure')
    	await oswaps.actions.init(['user2', 10000, 'Telos']).send('oswaps@owner')
        const cfg = oswaps.tables.configs(nameToBigInt('oswaps')).getTableRows()
        assert.deepEqual(cfg, [ {chain_id: "4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11",
            last_nonce: 1111, last_token_id: 0, manager: "user2", nonce_life_msec: 10000 } ] )
        console.log('reconfigure')
    	await oswaps.actions.init(['manager', 10000, 'Telos']).send('user2@active')
        const cfg2 = oswaps.tables.configs(nameToBigInt('oswaps')).getTableRows()
        assert.deepEqual(cfg2, [ {chain_id: "4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11",
            last_nonce: 1111, last_token_id: 0, manager: "manager", nonce_life_msec: 10000 } ] )
        console.log('create assets')
        await oswaps.actions.createasseta(['issuera', 'Telos', 'token', 'AZURES', '']).send('issuera@active')
        await oswaps.actions.createasseta(['issuerb', 'Telos', 'token', 'BURGS', '']).send('issuerb@active')
        rows = oswaps.tables.assetsa(nameToBigInt('oswaps')).getTableRows()
        assert.deepEqual(rows, [ 
            { token_id: 1, chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11',
              contract_name: 'token', symbol: 'AZURES', active: true, metadata: '', weight: '0.0000000' },
            { token_id: 2, chain_code: '4667b205c6838ef70ff7988f6e8257e8be0e1284a2f59699054a018f743b1d11',
              contract_name: 'token', symbol: 'BURGS', active: true, metadata: '', weight: '0.0000000' } ] )
        console.log('add AZURES liquidity 1 - prep')
        starttime = new Date()
        starttime.setSeconds(0, 0)
        blockchain.setTime(TimePoint.fromMilliseconds(starttime.valueOf()))
        starttimeString = starttime.toISOString().slice(0, -1);
        await oswaps.actions.addliqprep(['issuera', 1, '10.0000 AZURES', 1.00]).send('issuera@active')
        rvbuf = Buffer.from(blockchain.actionTraces[0].returnValue)
        rv = new Int32Array(rvbuf.buffer, rvbuf.byteOffset, 1)[0]
        //console.log(rv)
        rows = oswaps.tables.adpreps([nameToBigInt('oswaps')]).getTableRows()
        const expires = new Date(starttime.getTime()+10000)
        const expectedExpireString = expires.toISOString().slice(0,-1)
        assert.deepEqual(rows, [ { nonce: 1112, expires: expectedExpireString, account: 'issuera',
            token_id: 1, amount: '10.0000 AZURES', weight: '1.0000000' } ] )
        console.log('add AZURES liquidity 2 - transfer')
        await token.actions.transfer(['issuera','oswaps','10.0000 AZURES',`nonce ${rv}`]).send('issuera')
        balances = [ token.tables.accounts([nameToBigInt('oswaps')]).getTableRows(),
            oswaps.tables.accounts([nameToBigInt('issuera')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'10.0000 AZURES'}], [{balance:'10.0000 LIQB'}] ])
        console.log('withdraw liquidity')
        await oswaps.actions.withdraw(['issuera', 1, '5.0000 AZURES', 0.00]).send('manager')
        balances = [ token.tables.accounts([nameToBigInt('oswaps')]).getTableRows(),
            oswaps.tables.accounts([nameToBigInt('issuera')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'5.0000 AZURES'}], [{balance:'5.0000 LIQB'}] ])
        console.log('add BURGS liquidity 1 - prep')        
        await oswaps.actions.addliqprep(['issuerb', 2, '10.0000 BURGS', 1.00]).send('issuera@active')
        rvbuf = Buffer.from(blockchain.actionTraces[0].returnValue)
        rv = new Int32Array(rvbuf.buffer, rvbuf.byteOffset, 1)[0]
        //console.log(rv)
        rows = oswaps.tables.adpreps([nameToBigInt('oswaps')]).getTableRows()
        assert.deepEqual(rows, [ { nonce: 1113, expires: expectedExpireString, account: 'issuerb',
            token_id: 2, amount: '10.0000 BURGS', weight: '1.0000000' } ] )
        console.log('add BURGS liquidity 2 - transfer')
        await token.actions.transfer(['issuerb','oswaps','10.0000 BURGS',`nonce ${rv}`]).send('issuerb')
        balances = [ token.tables.accounts([nameToBigInt('oswaps')]).getTableRows(),
            oswaps.tables.accounts([nameToBigInt('issuerb')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'10.0000 BURGS'}, {balance:'5.0000 AZURES'}], [{balance:'10.0000 LIQC'}] ])
        console.log('exchange 1 - prep exact out')
        await token.actions.transfer(['issuerb', 'bob', '100.0000 BURGS', '']).send('issuerb')
        await oswaps.actions.exchangeprep(['bob', 2, '0.2500 BURGS',
            'alice', 1, '0.2000 AZURES', '{"exact":"out"}', 'my memo']).send('manager')
        rvbuf = Buffer.from(blockchain.actionTraces[0].returnValue)
        rv = [1,9,17,25].map((x)=>rvbuf.readInt32LE(x));
        //console.log(rv)
        rows = oswaps.tables.expreps(nameToBigInt('oswaps')).getTableRows()
        assert.deepEqual(rows, [{ nonce: 1114, expires: expires.toISOString().slice(0,-1), sender: 'bob',
          in_token_id: 2, in_amount: '0.2500 BURGS', recipient: 'alice', out_token_id: 1,
          out_amount: '0.2000 AZURES', mods: '{"exact":"out"}', memo: 'my memo' }] )
        console.log('exchange 2 - transfer')
        await token.actions.transfer(['bob','oswaps', '0.3000 BURGS', 'ref 1114']).send('bob')
        balances = [ token.tables.accounts([nameToBigInt('oswaps')]).getTableRows(),
             token.tables.accounts([nameToBigInt('alice')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'10.2062 BURGS'}, {balance:'4.8000 AZURES'}], [{balance:'0.2000 AZURES'}] ])
        console.log('exchange 3 - prep exact in')
        await oswaps.actions.exchangeprep([ 'bob', 2, "0.2500 BURGS",
            'alice', 1, "0.2000 AZURES", '{"exact":"in"}', "my memo"]).send('bob')
        rvbuf = Buffer.from(blockchain.actionTraces[0].returnValue)
        rv = [1,9,17,25].map((x)=>rvbuf.readInt32LE(x));
        //console.log(rv)
        rows = oswaps.tables.expreps(nameToBigInt('oswaps')).getTableRows()
        assert.deepEqual(rows, [{ nonce: 1115, expires: expectedExpireString, sender: 'bob',
          in_token_id: 2, in_amount: '0.2500 BURGS', recipient: 'alice', out_token_id: 1,
          out_amount: '0.2000 AZURES', mods: '{"exact":"in"}', memo: 'my memo' }] )
        console.log('exchange 4 - transfer')
        await token.actions.transfer(['bob','oswaps', '0.2500 BURGS', 'ref 1115']).send('bob')
        balances = [ token.tables.accounts([nameToBigInt('oswaps')]).getTableRows(),
             token.tables.accounts([nameToBigInt('alice')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'10.4562 BURGS'}, {balance:'4.5732 AZURES'}], [{balance:'0.4268 AZURES'}] ])
        console.log("check for clean prep tables")
        rows = oswaps.tables.expreps(nameToBigInt('oswaps')).getTableRows()
        assert.deepEqual(rows, [])
        rows = oswaps.tables.adpreps([nameToBigInt('oswaps')]).getTableRows()
        assert.deepEqual(rows, [])
        console.log("check for liquidity tokens")
        balances = [ oswaps.tables.accounts([nameToBigInt('issuera')]).getTableRows(),
             oswaps.tables.accounts([nameToBigInt('issuerb')]).getTableRows() ]
        assert.deepEqual(balances, [ [ {balance:'5.0000 LIQB'}], [{balance:'10.0000 LIQC'}] ])

    });
})

