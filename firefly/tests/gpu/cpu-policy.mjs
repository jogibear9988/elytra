/** Validate the pinned compositor's real enabled-feature argv and confinement policy. */
export function hasCpuCompositorPolicy(args) {
  if (!Array.isArray(args) || !args.every((value) => typeof value === 'string')) {
    return false;
  }
  const features = args.filter((value) => value.startsWith('--platform-features='));
  return (
    features.length === 1 &&
    !args.includes('--platform-features') &&
    !features[0].slice('--platform-features='.length).split(',').includes('gpu_painting') &&
    !args.some((value) => value === '--disable-sandbox' || value.startsWith('--disable-sandbox='))
  );
}
