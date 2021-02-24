import React from 'react';
import styled from 'styled-components';
import { MergeForm, MergeStatus, toBaseUnits } from '../../app';
import { BlockTitle, PaddedBlock, Text } from '../../components';
import { spacings, Theme, themeColours } from '../../styles';
import { MergeProgress } from './merge_progress';
import { MergeTx } from './merge_tx';

const MergeTxRow = styled(MergeTx)`
  padding: ${spacings.xs} 0;
  border-top: 1px solid ${themeColours[Theme.WHITE].border};
  border-bottom: 1px solid ${themeColours[Theme.WHITE].border};
`;

interface MergeProps {
  theme: Theme;
  form: MergeForm;
  onValidate(toMerge: bigint[]): void;
  onGoBack(): void;
  onSubmit(): void;
  onClose(): void;
}

export const Merge: React.FunctionComponent<MergeProps> = ({
  theme,
  form,
  onValidate,
  onGoBack,
  onSubmit,
  onClose,
}) => {
  const { asset, spendableBalance, mergeOptions, fee, status } = form;

  if (status.value !== MergeStatus.NADA) {
    return <MergeProgress theme={theme} form={form} onGoBack={onGoBack} onSubmit={onSubmit} onClose={onClose} />;
  }

  return (
    <div>
      <PaddedBlock>
        <Text size="m">
          <PaddedBlock>
            <Text weight="bold" inline>
              zk.money
            </Text>{' '}
            uses Aztec for cheap private transactions.
          </PaddedBlock>
          <PaddedBlock>
            Aztec represents your balance in an asset with UTXO notes. You can think of these as coins and notes in your
            wallet.
          </PaddedBlock>
          <PaddedBlock>
            Each time you do a transaction, only two coins or notes can be used to pay. The{' '}
            <Text weight="bold" inline>
              two largest notes
            </Text>
            , represent your spendable balance.
          </PaddedBlock>
          <PaddedBlock>
            Overtime your wallet will end up with lots of loose change, and your spendable balance will be smaller than
            your total balance. You can "merge" some of your coins together to increase your spendable balance.
          </PaddedBlock>
        </Text>
      </PaddedBlock>
      {mergeOptions.value.length > 0 && (
        <PaddedBlock>
          <BlockTitle title="Available Merge Transactions" />
          {mergeOptions.value.map((values, i) => (
            <MergeTxRow
              key={i}
              asset={asset.value}
              prevAmount={spendableBalance.value}
              amount={values.reduce((sum, v) => sum + v, 0n) - toBaseUnits(fee.value, asset.value.decimals)}
              fee={fee.value}
              onSubmit={() => onValidate(values)}
            />
          ))}
        </PaddedBlock>
      )}
    </div>
  );
};
