import { TouchedFormFields } from 'alt-model/form_fields_hooks';
import { ShieldFormValidationResult, ShieldFormFields } from './shield_form_validation';

function getAmountInputFeedback(result: ShieldFormValidationResult, touched: boolean) {
  if (!touched) return;
  if (result.mustAllowForFee && result.mustAllowForGas) {
    const fee = result.input.feeAmount;
    const cost = fee?.add(result.reservedForL1GasIfTargetAssetIsEth ?? 0n);
    return `Please allow ${cost?.format()} from your L1 balance for paying the transaction fee and covering gas costs.`;
  }
  if (result.mustAllowForGas) {
    const gas = result.input.targetL2OutputAmount?.withBaseUnits(result.reservedForL1GasIfTargetAssetIsEth ?? 0n);
    return `Please allow ${gas?.format()} from your L1 balance for covering gas costs.`;
  }
  if (result.mustAllowForFee) {
    const fee = result.input.feeAmount;
    return `Please allow ${fee?.format()} from your L1 balance for paying the transaction fee.`;
  }
  if (result.beyondTransactionLimit) {
    const { targetL2OutputAmount, transactionLimit } = result.input;
    const txLimitAmount = targetL2OutputAmount?.withBaseUnits(transactionLimit ?? 0n);
    return `Transactions are capped at ${txLimitAmount?.format()}`;
  }
  if (result.insufficientTargetAssetBalance) {
    return `Insufficient funds`;
  }
  if (result.noAmount) {
    return `Amount must be non-zero`;
  }
}

function getWalletAccountFeedback(result: ShieldFormValidationResult) {
  if (result.noWalletConnected) {
    return 'Please connect a wallet';
  } else if (result.wrongNetwork) {
    return 'Wrong network';
  }
}

function getFooterFeedback(result: ShieldFormValidationResult, attemptedLock: boolean) {
  if (!attemptedLock) return;
  if (result.insufficientFeePayingAssetBalance) {
    const fee = result.input.feeAmount;
    const output = result.input.targetL2OutputAmount;
    return `You do not have enough zk${
      fee?.info.symbol
    } to pay the fee for this transaction. Please first shield at least ${fee?.toFloat()} ${
      fee?.info.symbol
    } in a seperate transaction before attempting again to shield any ${output?.info.symbol}.`;
  }
}

export function getShieldFormFeedback(
  result: ShieldFormValidationResult,
  touchedFields: TouchedFormFields<ShieldFormFields>,
  attemptedLock: boolean,
) {
  return {
    amount: getAmountInputFeedback(result, touchedFields.amountStr || attemptedLock),
    walletAccount: getWalletAccountFeedback(result),
    footer: getFooterFeedback(result, attemptedLock),
  };
}

export type ShieldFormFeedback = ReturnType<typeof getShieldFormFeedback>;
