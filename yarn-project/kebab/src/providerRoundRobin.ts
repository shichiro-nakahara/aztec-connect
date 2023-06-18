import { EthereumRpc } from '@aztec/barretenberg/blockchain';
import { JsonRpcProvider } from '@aztec/blockchain';
import { createDebugLogger } from '@aztec/barretenberg/log';

export class ProviderRoundRobin {
    private ethereumHosts: string[];
    private providers: JsonRpcProvider[];
    private ethereumRpcs: EthereumRpc[];
    private lastProviderIndex = 0;
    private log = createDebugLogger('ProviderRoundRobin');

    constructor(
        urls: string
    ) {
        this.ethereumHosts = urls.split(',');
        this.providers = this.ethereumHosts.map((ethereumHost) => new JsonRpcProvider(ethereumHost, true));
        this.ethereumRpcs = this.providers.map((provider) => new EthereumRpc(provider));
    }

    public getNextProvider() {
        let nextProviderIndex = ++this.lastProviderIndex;
        if (nextProviderIndex >= this.providers.length) nextProviderIndex = 0;
    
        this.lastProviderIndex = nextProviderIndex;

        this.log(`Using provider: ${this.ethereumHosts[nextProviderIndex]}`);
    
        return this.providers[nextProviderIndex];
    }

    public getNextEthereumRpc() {
        let nextEthereumRpcIndex = ++this.lastProviderIndex;
        if (nextEthereumRpcIndex >= this.ethereumRpcs.length) nextEthereumRpcIndex = 0;

        this.lastProviderIndex = nextEthereumRpcIndex;

        this.log(`Using Ethereum RPC: ${this.ethereumHosts[nextEthereumRpcIndex]}`);

        return this.ethereumRpcs[nextEthereumRpcIndex];
    }
}