import { readFile } from 'node:fs/promises';

// Explicit store-sandbox use only. A trusted, configured main-process verifier
// must authenticate the receipt; queueing a payment is not purchase completion.
export async function queueVerifiedPurchase(inAppPurchase, productId, verifyReceipt) {
  if (typeof verifyReceipt !== 'function') throw Error('A real receipt verifier is required');
  if (!inAppPurchase.canMakePayments()) throw Error('Payments unavailable');
  const listener = async (_event, transactions) => {
    for (const transaction of transactions) {
      if (transaction.payment.productIdentifier !== productId) continue;
      if (
        transaction.transactionState === 'purchased' ||
        transaction.transactionState === 'restored'
      ) {
        try {
          const receiptPath = inAppPurchase.getReceiptURL();
          const receipt = await readFile(
            receiptPath.startsWith('file:') ? new URL(receiptPath) : receiptPath
          );
          if ((await verifyReceipt({ receipt, productId, transaction })) !== true)
            throw Error('Receipt verification rejected');
          inAppPurchase.finishTransactionByDate(transaction.transactionDate);
          console.log('RECEIPT_VERIFIED');
          inAppPurchase.removeListener('transactions-updated', listener);
        } catch {
          console.error('Receipt verification failed; transaction is not marked complete');
        }
      } else if (transaction.transactionState === 'failed') {
        inAppPurchase.finishTransactionByDate(transaction.transactionDate);
        inAppPurchase.removeListener('transactions-updated', listener);
      }
    }
  };
  inAppPurchase.on('transactions-updated', listener);
  try {
    if (!(await inAppPurchase.purchaseProduct(productId, 1))) throw Error('Store rejected product');
  } catch (error) {
    inAppPurchase.removeListener('transactions-updated', listener);
    throw error;
  }
  return { queued: true };
}
