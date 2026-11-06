/**
 * In-app purchases main-process reference. Requires a signed Mac App Store sandbox app and
 * configured products. Product lookup is implemented in this reference example;
 * payment/receipt verification must be supplied before enabling purchases. No purchase is run
 * by tests.
 */
import { runExample } from '../shared/run.mjs';

await runExample(import.meta.url, {
  id: 'in-app-purchases',
  requires: ['inAppPurchase.getProducts'],
  earlyRequires: [],
  smoke: false,
  platforms: ['darwin'],
  actions: {
    lookup: async ({ api }, product) => {
      if (typeof product !== 'string' || !product || product.length > 200) {
        throw Error('Expected a configured product ID');
      }
      const products = await api.inAppPurchase.getProducts([product]);
      return products.map((item) => ({
        id: item.productIdentifier,
        title: item.localizedTitle,
        price: item.formattedPrice
      }));
    }
  }
});
