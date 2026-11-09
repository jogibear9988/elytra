/**
 * Main-process permission callbacks receive only native authenticated document
 * metadata. Generations revoke pending callbacks before a handler can be replaced;
 * native ownership remains authoritative for lifetime, grants and request budgets.
 */
export default function createPermissions(
  native,
  isReady,
  isStopped,
  contentsFromId,
  error,
  unsupported
) {
  let epoch = 0;
  let stopped = false;
  let configuring = false;
  let checkHandler = null;
  let requestHandler = null;
  const maximumId = 0x7fffffff;
  const names = new Set(['geolocation', 'notifications', 'media']);

  function id(value) {
    return Number.isInteger(value) && value > 0 && value <= maximumId;
  }

  function active() {
    return !stopped && !isStopped();
  }

  function contents(id) {
    const value = contentsFromId(id);
    return value && !value.isDestroyed() ? value : undefined;
  }

  function details(value, permission) {
    if (
      !value ||
      typeof value !== 'object' ||
      Array.isArray(value) ||
      typeof value.requestingUrl !== 'string' ||
      value.requestingUrl.length > 65536 ||
      typeof value.isMainFrame !== 'boolean'
    ) {
      return undefined;
    }
    const result = { requestingUrl: value.requestingUrl, isMainFrame: value.isMainFrame };
    if (value.embeddingOrigin !== undefined) {
      if (typeof value.embeddingOrigin !== 'string' || value.embeddingOrigin.length > 65536) {
        return undefined;
      }
      result.embeddingOrigin = value.embeddingOrigin;
    }
    if (permission === 'media') {
      // The pinned engine has microphone permissions, never a camera grant.
      if (value.mediaType !== undefined && value.mediaType !== 'audio') {
        return undefined;
      }
      if (
        value.mediaTypes !== undefined &&
        (!Array.isArray(value.mediaTypes) ||
          value.mediaTypes.length !== 1 ||
          value.mediaTypes[0] !== 'audio')
      ) {
        return undefined;
      }
      result.mediaType = 'audio';
      result.mediaTypes = ['audio'];
    }
    return result;
  }

  function configure(nextCheck, nextRequest) {
    if (!active()) {
      throw error('ERR_FIREFLY_SESSION_CLOSED', 'Session permissions are closed');
    }
    if (!isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'Session permissions require app readiness');
    }
    if (!native.permissions?.configure) {
      unsupported('Session permission enforcement');
    }
    if (epoch === maximumId) {
      throw error('ERR_OUT_OF_RANGE', 'Session permission generation exhausted');
    }
    if (configuring) {
      throw error(
        'ERR_FIREFLY_SESSION_REENTRANT',
        'Permission configuration is already in progress'
      );
    }
    // Reserve before entering native code: reentrant stop must revoke even the
    // first registration. Never restore an older generation after a failed call.
    const nextEpoch = ++epoch;
    const check =
      nextCheck === null
        ? null
        : (contentsId, permission, origin, wire) => {
            if (
              !active() ||
              epoch !== nextEpoch ||
              !id(contentsId) ||
              !names.has(permission) ||
              typeof origin !== 'string' ||
              origin.length > 65536 ||
              typeof wire !== 'string' ||
              wire.length > 262144
            ) {
              return false;
            }
            let parsed;
            try {
              parsed = JSON.parse(wire);
            } catch {
              return false;
            }
            const metadata = details(parsed, permission);
            const target = contents(contentsId);
            if (!metadata || !target) {
              return false;
            }
            // Exceptions retain the ordinary callback error policy. The native caller
            // denies before reporting them and revalidates document identity afterwards.
            const allowed = nextCheck(target, permission, origin, metadata);
            return (
              allowed === true && active() && epoch === nextEpoch && contents(contentsId) === target
            );
          };
    configuring = true;
    try {
      native.permissions.configure(nextEpoch, check, nextRequest !== null);
    } catch (failure) {
      checkHandler = null;
      requestHandler = null;
      if (epoch < maximumId) {
        ++epoch;
      } else {
        stopped = true;
      }
      // The native setter is atomic. Keep all old JS closures revoked even if a
      // host rejects admission; a subsequent explicit setter can install policy.
      throw failure;
    } finally {
      configuring = false;
    }
    if (!active()) {
      if (epoch < maximumId) {
        native.permissions.configure(++epoch, null, false);
      }
      throw error('ERR_FIREFLY_SESSION_CLOSED', 'Session closed during permission configuration');
    }
    checkHandler = nextCheck;
    requestHandler = nextRequest;
  }

  function handler(value) {
    if (value !== null && typeof value !== 'function') {
      throw Object.assign(new TypeError('Permission handler must be a function or null'), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }
    return value;
  }

  const api = {
    setPermissionCheckHandler(value) {
      configure(handler(value), requestHandler);
    },

    setPermissionRequestHandler(value) {
      configure(checkHandler, handler(value));
    }
  };

  function dispatch(data) {
    if (!data || typeof data !== 'object' || !id(data.token) || !id(data.epoch)) {
      return;
    }

    const finish = (allowed) => native.permissions.complete(data.token, data.epoch, allowed);

    if (
      !active() ||
      data.epoch !== epoch ||
      !requestHandler ||
      !id(data.contentsId) ||
      !names.has(data.permission)
    ) {
      finish(false);
      return;
    }
    const target = contents(data.contentsId);
    const metadata = details(data, data.permission);
    if (!target || !metadata) {
      finish(false);
      return;
    }
    const current = epoch;
    const callbackHandler = requestHandler;
    let completed = false;

    const callback = (allowed) => {
      if (completed) {
        return;
      }
      completed = true;
      if (
        !active() ||
        epoch !== current ||
        requestHandler !== callbackHandler ||
        contents(data.contentsId) !== target
      ) {
        finish(false);
        return;
      }
      if (typeof allowed !== 'boolean') {
        finish(false);
        throw Object.assign(new TypeError('Permission decision must be a boolean'), {
          code: 'ERR_INVALID_ARG_TYPE'
        });
      }
      if (allowed && data.permission === 'media') {
        finish(false);
        unsupported('Renderer microphone capture grants');
      }
      finish(allowed);
    };

    try {
      callbackHandler(target, data.permission, callback, metadata);
    } catch (failure) {
      if (!completed) {
        completed = true;
        finish(false);
      }
      throw failure;
    }
  }

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    checkHandler = null;
    requestHandler = null;
    // Native teardown also revokes ownership. Reconfigure here while the bridge
    // still exists, so user-retained callbacks cannot authorize during shutdown.
    if (epoch && epoch < maximumId) {
      native.permissions.configure(++epoch, null, false);
    }
  }

  return { api, dispatch, stop };
}
