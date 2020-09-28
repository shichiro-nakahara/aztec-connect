import { ethers } from '@nomiclabs/buidler';
import { EthAddress } from 'barretenberg/address';
import { expect, use } from 'chai';
import { solidity } from 'ethereum-waffle';
import { Contract, Signer } from 'ethers';
import {
  createDepositProof,
  createTwoDepositsProof,
  createWithdrawProof,
  numToBuffer,
} from '../fixtures/create_mock_proof';
import { setupRollupProcessor } from '../fixtures/setup_rollup_processor';
import { solidityFormatSignatures } from '../signing/solidity_format_sigs';

use(solidity);

describe('rollup_processor: core', () => {
  let rollupProcessor: Contract;
  let erc20A: Contract;
  let erc20B: Contract;
  let userA: Signer;
  let userB: Signer;
  let userAAddress: EthAddress;
  let userBAddress: EthAddress;

  const mintAmount = 100;
  const userADepositAmount = 60;
  const userBDepositAmount = 15;

  beforeEach(async () => {
    [userA, userB] = await ethers.getSigners();
    userAAddress = EthAddress.fromString(await userA.getAddress());
    userBAddress = EthAddress.fromString(await userB.getAddress());
    ({ erc20: erc20A, rollupProcessor } = await setupRollupProcessor([userA, userB], mintAmount));

    // create second erc20
    const ERC20B = await ethers.getContractFactory('ERC20Mintable');
    erc20B = await ERC20B.deploy();
    await erc20B.mint(userBAddress.toString(), mintAmount);
  });

  it('should initialise state variables', async () => {
    const originalNumSupportedAssets = await rollupProcessor.getNumSupportedAssets();
    expect(originalNumSupportedAssets).to.equal(1);

    const supportedAssetAAddress = await rollupProcessor.getSupportedAssetAddress(0);
    expect(supportedAssetAAddress).to.equal(erc20A.address);

    // set new supported asset
    const tx = await rollupProcessor.setSupportedAsset(erc20B.address);
    const receipt = await tx.wait();

    const assetId = rollupProcessor.interface.parseLog(receipt.logs[receipt.logs.length - 1]).args.assetId;
    const assetAddress = rollupProcessor.interface.parseLog(receipt.logs[receipt.logs.length - 1]).args.assetAddress;
    expect(assetId).to.equal(1);
    expect(assetAddress).to.equal(erc20B.address);

    const supportedAssetBAddress = await rollupProcessor.getSupportedAssetAddress(1);
    expect(supportedAssetBAddress).to.equal(erc20B.address);

    const newNumSupportedAssets = await rollupProcessor.getNumSupportedAssets();
    expect(newNumSupportedAssets).to.equal(2);
  });

  it('should process asset A deposit tx and assetB deposit tx in one rollup', async () => {
    // set new supported asset
    await rollupProcessor.setSupportedAsset(erc20B.address);

    const fourViewingKeys = [Buffer.alloc(32, 1), Buffer.alloc(32, 2), Buffer.alloc(32, 3), Buffer.alloc(32, 4)];

    // deposit funds from userA and userB, from assetA and assetB respectively
    const assetAId = Buffer.alloc(32, 0);
    const assetBId = numToBuffer(1);
    const { proofData, signatures, sigIndexes } = await createTwoDepositsProof(
      userADepositAmount,
      userAAddress,
      userA,
      assetAId,
      userBDepositAmount,
      userBAddress,
      userB,
      assetBId,
    );

    await erc20A.approve(rollupProcessor.address, userADepositAmount);
    await erc20B.connect(userB).approve(rollupProcessor.address, userBDepositAmount);

    await rollupProcessor.processRollup(proofData, solidityFormatSignatures(signatures), sigIndexes, fourViewingKeys);

    const postDepositUserABalance = await erc20A.balanceOf(userAAddress.toString());
    expect(postDepositUserABalance).to.equal(mintAmount - userADepositAmount);

    const postDepositUserBBalance = await erc20B.balanceOf(userBAddress.toString());
    expect(postDepositUserBBalance).to.equal(mintAmount - userBDepositAmount);

    const postDepositContractBalanceA = await erc20A.balanceOf(rollupProcessor.address);
    expect(postDepositContractBalanceA).to.equal(userADepositAmount);

    const postDepositContractBalanceB = await erc20B.balanceOf(rollupProcessor.address);
    expect(postDepositContractBalanceB).to.equal(userBDepositAmount);
  });

  it('should not revert if withdraw() fails due to faulty ERC20 contract', async () => {
    const FaultyERC20 = await ethers.getContractFactory('ERC20FaultyTransfer');
    const faultyERC20 = await FaultyERC20.deploy();
    await faultyERC20.mint(userBAddress.toString(), mintAmount);

    const tx = await rollupProcessor.setSupportedAsset(faultyERC20.address);
    const receipt = await tx.wait();
    const assetBId = Number(rollupProcessor.interface.parseLog(receipt.logs[receipt.logs.length - 1]).args.assetId);

    // deposit funds from assetB
    const fourViewingKeys = [Buffer.alloc(32, 1), Buffer.alloc(32, 2), Buffer.alloc(32, 3), Buffer.alloc(32, 4)];
    const { proofData, signatures, sigIndexes } = await createDepositProof(
      userBDepositAmount,
      userBAddress,
      userB,
      assetBId,
    );

    await faultyERC20.approve(rollupProcessor.address, userBDepositAmount);
    await faultyERC20.connect(userB).approve(rollupProcessor.address, userBDepositAmount);
    await rollupProcessor.processRollup(proofData, solidityFormatSignatures(signatures), sigIndexes, fourViewingKeys);

    // withdraw funds to userB - this is not expected to perform a transfer (as the ERC20 is faulty)
    // so we don't expect the withdraw funds to be transferred, and expect an error event emission
    const withdrawAmount = 5;
    const { proofData: withdrawProofData } = await createWithdrawProof(withdrawAmount, userBAddress, assetBId);
    const withdrawTx = await rollupProcessor.processRollup(withdrawProofData, [], [], fourViewingKeys);

    const rollupReceipt = await withdrawTx.wait();
    expect(receipt.status).to.equal(1);

    const errorReason = rollupProcessor.interface.parseLog(rollupReceipt.logs[rollupReceipt.logs.length - 1]).args
      .errorReason;
    expect(errorReason.length).to.be.greaterThan(0);

    const userBFinalBalance = await faultyERC20.balanceOf(userBAddress.toString());
    // not expecting withdraw to have transferred funds
    expect(userBFinalBalance).to.equal(mintAmount - userBDepositAmount);

    const rollupFinalBalance = await faultyERC20.balanceOf(rollupProcessor.address);
    expect(rollupFinalBalance).to.equal(userBDepositAmount);
  });
});
