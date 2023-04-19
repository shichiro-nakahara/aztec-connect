# Aztec Connect (v2.1) Monorepo

- `aztec-connect-cpp` - C++ proof generators and merkle tree db.
- `blockchain-vks` - Generates verification key smart contracts.
- `contracts` - Solidity smart contracts.
- `yarn-project/account-migrator` - Builds initial data tree with accounts from v2.0.
- `yarn-project/alpha-sdk` - Alpha version of SDK to enable dapp developers to interface with external wallets.
- `yarn-project/aztec-dev-cli` - Development cli tool.
- `yarn-project/barretenberg.js` - Wrapper around barretenberg wasm and assorted low level libs.
- `yarn-project/blockchain` - TypeScript for interacting with smart contracts and the blockchain.
- `yarn-project/end-to-end` - End to end tests. Uses docker to launch a mainnet fork, falafel, and run test suite against them.
- `yarn-project/falafel` - Rollup server.
- `yarn-project/halloumi` - Proof generation server.
- `yarn-project/hummus` - Webpack proof of concept website and terminal using `sdk`.
- `yarn-project/kebab` - Proxy server sitting between falafel and ETH node.
- `yarn-project/sdk` - SDK for interacting with a rollup provider.
- `yarn-project/wasabi` - Load testing tool.

## Bug Bounties and Vulnerability Reporting

We are currently running a Bug Bounty Program. For all information, please visit [immunefi](https://immunefi.com/bounty/aztecnetwork/).

## Audit reports

See audit reports [here](https://github.com/AztecProtocol/aztec-security/tree/main/Audits)

| Title             | Vendor              | Report                                                                                                                                  |
| ----------------- | ------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| Rollup Processor  | Arbitrary Execution | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Aztec_20221014.pdf)                                             |
| Rollup Processor  | Solidified          | [link](<https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20(1).pdf>)                         |
| Aave Bridge       | Solidified          | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20Aave%20Bridge.pdf)                 |
| Compound Bridge   | Solidified          | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20Compound%20Bridge.pdf)             |
| Curve Bridge      | Solidified          | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20Curve%20Bridge.pdf)                |
| DCA Bridge        | Solidified          | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20DCA%20Bridge.pdf)                  |
| Element Bridge    | Solidified          | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20Element%20Bridge.pdf)              |
| Lido Bridge       | Solidified          | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20Lido%20Bridge.pdf)                 |
| Liquity Bridge    | Solidified          | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20Liquity%20Bridge.pdf)              |
| Liquity Bridge #2 | Solidified          | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20Liquity%20Trove%20Bridge%20II.pdf) |
| Set Bridge        | Solidified          | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20Set%20Bridge.pdf)                  |
| Rollup Subsidy    | Solidified          | [link](https://github.com/AztecProtocol/aztec-security/blob/main/Audits/Audit%20Report%20-%20Aztec%20Subsidy%20Contract.pdf)            |

## All Project Dependencies

*Tip: Run scripts in ./scripts/setup folder to install dependencies and allocate swap for Halloumi instances.*

cmake >= 3.2.4
```
$ cmake --version
cmake version 3.26.0
```

Ninja
```
$ sudo dnf list installed | grep ninja-build
ninja-build.x86_64 1.10.2-9.fc37 @fedora
```

clang >= 10 or gcc >= 10
```
$ clang --version
clang version 15.0.7 (Fedora 15.0.7-1.fc37)
Target: x86_64-redhat-linux-gnu
```

clang-format
```
$ clang-format --version
clang-format version 15.0.7 (Fedora 15.0.7-1.fc37)

$ sudo dnf list installed | grep clang-tools-extra
clang-tools-extra.x86_64 15.0.7-1.fc37 @updates
```

libomp (if multithreading required. Multithreading can be disabled using the compiler flag -DMULTITHREADING 0)
```
$ sudo dnf list installed | grep libomp
libomp.x86_64 15.0.7-1.fc37 @updates
```

wasm-opt (part of the Binaryen toolkit)
```
$ sudo dnf list | grep binaryen
binaryen.x86_64 110-1.fc37 @fedora
```

nvm
```
curl https://raw.githubusercontent.com/creationix/nvm/master/install.sh | bash
nvm install v18.8.0

$ nvm --version
0.39.3
$ nvm list
->      v18.8.0

$ node --version
v18.8.0
```

socat
```
$ sudo dnf list | grep socat
socat.x86_64 1.7.4.2-3.fc37 @fedora     
```

yarn
```
npm install yarn -g

$ yarn --version
1.22.19
```

tmux
```
$ sudo dnf list installed | grep tmux
tmux.x86_64 3.3a-1.fc37 @updates
```

sqlite3, sqlite3-devel
```
$ sudo dnf list installed | grep sqlite
sqlite.x86_64 3.40.0-1.fc37 @fedora
sqlite-devel.x86_64 3.40.0-1.fc37 @fedora
```

cast (from foundry)
```
curl -L https://foundry.paradigm.xyz | bash
foundryup

foundryup: installed - forge 0.2.0 (df8ab09 2023-03-14T09:59:20.944687208Z)
foundryup: installed - cast 0.2.0 (df8ab09 2023-03-14T09:59:20.944687208Z)
foundryup: installed - anvil 0.1.0 (df8ab09 2023-03-14T09:59:59.102392673Z)
foundryup: installed - chisel 0.1.1 (df8ab09 2023-03-14T09:59:59.175534662Z)
```