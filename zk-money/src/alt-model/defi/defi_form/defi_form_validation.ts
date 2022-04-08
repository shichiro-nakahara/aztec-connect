import type { DefiSettlementTime } from '@aztec/sdk';
import type { AmountFactory } from 'alt-model/assets/amount_factory';
import type { DefiComposerPayload } from './defi_composer';
import type { RemoteAsset } from 'alt-model/types';
import { Amount } from 'alt-model/assets';
import { max, min } from 'app';
import { MAX_MODE, StrOrMax } from 'alt-model/forms/constants';

export interface DefiFormFields {
  amountStrOrMax: StrOrMax;
  speed: DefiSettlementTime;
}

interface DefiFormValidationInput {
  fields: DefiFormFields;
  amountFactory?: AmountFactory;
  depositAsset: RemoteAsset;
  balanceInTargetAsset?: bigint;
  feeAmount?: Amount;
  balanceInFeePayingAsset?: bigint;
  transactionLimit?: bigint;
}

export interface DefiFormValidationResult {
  loading?: boolean;
  unrecognisedTargetAmount?: boolean;
  insufficientTargetAssetBalance?: boolean;
  insufficientFeePayingAssetBalance?: boolean;
  mustAllowForFee?: boolean;
  beyondTransactionLimit?: boolean;
  noAmount?: boolean;
  isValid?: boolean;
  validPayload?: DefiComposerPayload;
  maxOutput?: bigint;
  targetDepositAmount?: Amount;
  input: DefiFormValidationInput;
}

export function validateDefiForm(input: DefiFormValidationInput): DefiFormValidationResult {
  const {
    fields,
    amountFactory,
    balanceInTargetAsset,
    feeAmount,
    balanceInFeePayingAsset,
    transactionLimit,
    depositAsset,
  } = input;
  if (!amountFactory || !feeAmount || balanceInTargetAsset === undefined || balanceInFeePayingAsset === undefined) {
    return { loading: true, input };
  }
  if (transactionLimit === undefined) {
    return { unrecognisedTargetAmount: true, input };
  }

  // If the target asset isn't used for paying the fee, we don't need to reserve funds for it
  const targetAssetIsPayingFee = depositAsset.id === feeAmount.id;
  const feeInTargetAsset = targetAssetIsPayingFee ? feeAmount.baseUnits : 0n;

  const maxOutput = max(min(balanceInTargetAsset - feeInTargetAsset, transactionLimit), 0n);
  const targetDepositAmount =
    fields.amountStrOrMax === MAX_MODE
      ? new Amount(maxOutput, depositAsset)
      : Amount.from(fields.amountStrOrMax, depositAsset);

  const requiredInputInTargetAssetCoveringCosts = targetDepositAmount.baseUnits + feeInTargetAsset;

  const beyondTransactionLimit = targetDepositAmount.baseUnits > transactionLimit;
  const noAmount = targetDepositAmount.baseUnits <= 0n;
  const insufficientTargetAssetBalance = balanceInTargetAsset < requiredInputInTargetAssetCoveringCosts;
  const insufficientFeePayingAssetBalance = balanceInFeePayingAsset < feeAmount.baseUnits;
  const mustAllowForFee = insufficientTargetAssetBalance && balanceInTargetAsset >= targetDepositAmount.baseUnits;

  const isValid =
    !insufficientTargetAssetBalance && !insufficientFeePayingAssetBalance && !beyondTransactionLimit && !noAmount;
  const validPayload = isValid
    ? {
        targetDepositAmount,
        feeAmount,
      }
    : undefined;

  return {
    insufficientTargetAssetBalance,
    insufficientFeePayingAssetBalance,
    mustAllowForFee,
    beyondTransactionLimit,
    noAmount,
    isValid,
    validPayload,
    maxOutput,
    input,
  };
}
