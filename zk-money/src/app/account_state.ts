import { AccountId, AssetId, Note } from '@aztec/sdk';
import { AccountTx, JoinSplitTx } from './account_txs';
import { Asset, assets } from './assets';

export interface AccountState {
  userId: AccountId;
  alias: string;
  accountTxs: AccountTx[];
  settled: boolean;
}

export interface AssetState {
  asset: Asset;
  price: bigint;
  balance: bigint;
  spendableNotes: Note[];
  spendableBalance: bigint;
  joinSplitTxs: JoinSplitTx[];
}

export const initialAssetState = {
  asset: assets[AssetId.ETH],
  price: 0n,
  balance: 0n,
  spendableNotes: [],
  spendableBalance: 0n,
  joinSplitTxs: [],
};
