# 0060: Bounded cryptographic random integers

Status: implemented and tested locally on macOS ARM64; hosted acceptance pending.

## Decision

Implement node:crypto.randomInt([min,] max[, callback]) in the existing portable
crypto facade, consuming the existing native libuv OS entropy service. No new
dependency, privileged renderer exposure or Ladybird modification is needed.
The public overloads, safe-integer validation, exclusive maximum, range below
2**48, callback return/error shape and CJS/ESM identity follow the
[Node 24.15 implementation contract](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/crypto/random.js).

Interpret six bytes as an unsigned 48-bit value using exact Number arithmetic.
For range r, accept values below 2**48 - (2**48 % r), then map by remainder plus
min. Every output has the same number of accepted preimages. Retry values in the
remaining tail, rather than applying biased modulo to all possible byte values.
All intermediate values are exactly representable. Endpoint validation bounds
the final sum to safe integers even for ranges near either safe-integer endpoint.

Synchronous attempts block for native entropy as randomBytes already does.
Asynchronous attempts use the existing worker/owner-thread completion boundary,
root callbacks through that service and keep pending work alive. A rejected sample
queues another native worker, so repeated rejection does not recurse on the JS
stack. Entropy failures retain the existing native error object.

## Deliberate difference

There is no random-integer entropy cache. Each attempt obtains fresh bytes, as the
existing UUID implementation does. Node can deliver warm-cache calls through
nextTick; Mite's worker completion can occur after the Promise checkpoint instead.
This timing/performance difference is documented, with no throughput claim.
The mathematical accept threshold differs from Node's conservative threshold but
preserves unbiased selection and the same public bounds.

## Evidence and limits

The new CTest suite compares 48 validation cases with Node, checks 448 native
samples across tiny/negative/48-bit/safe-integer-endpoint ranges and 48 overlapping
async calls, export identity, return shape, strict receiver and callback exceptions.
An isolated Node VM evaluates the owned facade with controlled entropy, checking
both tail rejection boundaries, delayed retry, one completion and native error
identity. This adds no shipping entropy override or test-only runtime API.
Sampling is a smoke check, not a statistical proof or entropy-source certification.
All 39 local runtime suites pass. Native hosted Windows/Linux, forced OS failure
and performance acceptance remain separate work.
