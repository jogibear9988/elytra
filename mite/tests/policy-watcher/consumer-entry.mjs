/** Unchanged Code-OSS services, genuine native watcher; unique absent domain, no preference writes. */
const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const timeout = setTimeout(() => {
  console.error('Code-OSS native policy consumer timed out');
  process.exit(1);
}, 15000);
let policies;
let managed;
try {
  await import(process.env.ELYTRA_POLICY_NLS);
  const { NativePolicyService } = await import(process.env.ELYTRA_POLICY_CONSUMER);
  const { NativeManagedSettingsService } = await import(process.env.ELYTRA_MANAGED_POLICY_CONSUMER);
  const domain = process.env.ELYTRA_POLICY_DOMAIN;
  check(
    /^com\.elytra\.policy\.probe\.[a-f0-9]{32}$/.test(domain),
    'owned unique read-only probe domain'
  );
  const logs = { traces: [], errors: [] };
  const log = {
    trace: (...args) => logs.traces.push(args.join(' ')),

    error: (...args) => logs.errors.push(args.join(' '))
  };
  policies = new NativePolicyService(log, domain);
  const changes = [];

  policies.onDidChange((names) => changes.push([...names]));

  const definitions = {
    ElytraProbeString: { type: 'string' },
    ElytraProbeBoolean: { type: 'boolean' },
    ElytraProbeNumber: { type: 'number' }
  };
  const initial = await policies.updatePolicyDefinitions(definitions);
  check(
    Object.keys(initial).length === 0 && changes.length === 1 && changes[0].length === 0,
    'real initial absent-domain snapshot initializes unchanged NativePolicyService'
  );
  for (const name of Object.keys(definitions)) {
    check(
      policies.getPolicyValue(name) === undefined &&
        policies.getPolicyValueSource(name) === undefined,
      'actual absence is not invented policy value/source'
    );
  }
  const serialized = policies.serialize();
  check(
    Object.keys(serialized).length === 3 &&
      serialized.ElytraProbeBoolean.definition.type === 'boolean' &&
      serialized.ElytraProbeBoolean.value === undefined,
    'actual consumer preserves definitions and absent values'
  );
  await policies.updatePolicyDefinitions(definitions);
  check(
    changes.length === 1,
    'unchanged definition identities avoid unnecessary watcher recreation'
  );

  managed = new NativeManagedSettingsService(log, domain);
  const managedChanges = [];

  managed.onDidChangeManagedSettings((values) => managedChanges.push(values));

  const managedInitial = await managed.initialize();
  check(
    Object.keys(managedInitial).length === 0 && managedChanges.length === 0,
    'real initial absent-domain snapshot initializes unchanged managed-settings service'
  );
  const managedAgain = await managed.initialize();
  check(
    Object.keys(managedAgain).length === 0,
    'repeated initialization retains empty managed state'
  );
  const updated = await managed.updatePolicyDefinitions({
    ElytraOwnedPolicy: {
      type: 'boolean',
      managedSettings: {
        'elytra.probe.string': { type: 'string' },
        'elytra.probe.number': { type: 'number' },
        'elytra.probe.boolean': { type: 'boolean' }
      }
    }
  });
  check(
    Object.keys(updated).length === 0 &&
      Object.keys(managed.managedSettings).length === 0 &&
      managedChanges.length === 0,
    'actual definition projection recreates watcher and retains true absence'
  );
  check(
    logs.errors.length === 0,
    'native initialization must not silently recover from provider errors'
  );
  check(
    logs.traces.filter((line) => line.startsWith('Creating watcher for productName ')).length ===
      1 &&
      logs.traces.filter((line) =>
        line.startsWith('Creating native managed-settings watcher for productName ')
      ).length === 2,
    'unchanged consumer exercised actual initial/replacement watcher paths'
  );
  policies.dispose();
  policies = undefined;
  managed.dispose();
  managed = undefined;
  console.log(
    'PASS actual pinned Code-OSS NativePolicyService and NativeManagedSettingsService genuine macOS absent-domain initialization, definition replacement and disposal; no OS writes, injected callbacks, MDM-change or workbench claim'
  );
} catch (failure) {
  console.error(failure);
  process.exitCode = 1;
} finally {
  policies?.dispose();
  managed?.dispose();
  clearTimeout(timeout);
}
