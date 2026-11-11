"use strict";
/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.setDeviceId = exports.getDeviceId = void 0;
const path = __importStar(require("path"));
const fs = __importStar(require("fs-extra"));
const windowRegistry = process.platform === "win32"
    ? require("../build/Release/windows.node")
    : null;
const deviceIdFileName = "deviceid";
function getDirectory() {
    let folder;
    if (!process.env.HOME) {
        throw new Error("Home directory not found");
    }
    if (process.platform === "darwin") {
        folder = path.join(process.env.HOME, "Library", "Application Support");
    }
    else if (process.platform === "linux" || process.platform === "freebsd") {
        folder =
            process.env.XDG_CACHE_HOME ?? path.join(process.env.HOME, ".cache");
    }
    else {
        throw new Error("Unsupported platform");
    }
    return path.join(folder, "Microsoft", "DeveloperTools");
}
function getDeviceIdFilePath() {
    return path.join(getDirectory(), deviceIdFileName);
}
async function getDeviceId() {
    if (process.platform === "win32") {
        return windowRegistry?.GetDeviceId();
    }
    else {
        if (!(await exists(getDeviceIdFilePath()))) {
            return undefined;
        }
        else {
            const content = await fs.readFile(getDeviceIdFilePath(), "utf8");
            // Strip a leading UTF-8 BOM (U+FEFF) if present. Node's "utf8"
            // decoding does not strip it, and a stray BOM in the device ID
            // breaks downstream consumers that put the value in HTTP headers
            // (see issue #34).
            if (content.charCodeAt(0) === 0xfeff) {
                const stripped = content.replace(/^\uFEFF/, "");
                // Rewrite the file without the BOM so future reads (including
                // from other consumers) get a clean value.
                try {
                    await fs.writeFile(getDeviceIdFilePath(), stripped, "utf8");
                }
                catch {
                    // Best-effort rewrite; ignore failures and still return
                    // the stripped value to the caller.
                }
                return stripped;
            }
            return content;
        }
    }
}
exports.getDeviceId = getDeviceId;
async function exists(path) {
    try {
        await fs.promises.access(path);
        return true;
    }
    catch {
        return false;
    }
}
async function setDeviceId(deviceId) {
    if (process.platform === "win32") {
        windowRegistry?.SetDeviceId(deviceId);
    }
    else {
        await fs.ensureDir(getDirectory());
        await fs.writeFile(getDeviceIdFilePath(), deviceId, "utf8");
    }
}
exports.setDeviceId = setDeviceId;
//# sourceMappingURL=storage.js.map