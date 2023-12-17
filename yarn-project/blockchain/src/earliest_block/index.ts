export function getEarliestBlock(chainId: number) {
  switch (chainId) {
    case 1:
      return { earliestBlock: 14728000, chunk: 100000, offchainSearchLead: 6 * 60 * 24 };
    case 0xa57ec:
    case 0xdef:
    case 0x57a93:
      return { earliestBlock: 15918000, chunk: 100000, offchainSearchLead: 10 };
    case 0xe2e:
      return { earliestBlock: 15918000, chunk: 10, offchainSearchLead: 10 };
    case 1337:
      return { earliestBlock: 0, chunk: 10, offchainSearchLead: 10 };
    case 137:
      // Roughly 2 seconds per block on Polygon
      return { earliestBlock: 43397697, chunk: 100000, offchainSearchLead: 30 * 60 * 24 }; 
    case 80001:
      // Roughly 2 seconds per block on Polygon
      return { earliestBlock: 0, chunk: 100000, offchainSearchLead: 30 * 60 * 24 };
    default:
      return { earliestBlock: 0, chunk: 100000, offchainSearchLead: 6 * 60 * 24 };
  }
}
