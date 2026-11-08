/** Empty NativeImage values have no pixel backing; nonempty image codecs remain explicit work. */
export default function createNativeImages(error, unsupported) {
  const alloc = globalThis.Buffer.alloc.bind(globalThis.Buffer);
  const token = Symbol('NativeImage');

  class NativeImage {
    #valid = true;

    #check() {
      return this.#valid;
    }

    constructor(key) {
      if (key !== token) {
        throw error('ERR_ILLEGAL_CONSTRUCTOR', 'Use nativeImage factories');
      }
    }

    isEmpty() {
      this.#check();
      return true;
    }

    getSize() {
      this.#check();
      return { width: 0, height: 0 };
    }

    getAspectRatio() {
      this.#check();
      return 1;
    }

    getScaleFactors() {
      this.#check();
      return [];
    }

    toPNG() {
      this.#check();
      return alloc(0);
    }

    toJPEG() {
      this.#check();
      return alloc(0);
    }

    toBitmap() {
      this.#check();
      return alloc(0);
    }

    getBitmap() {
      this.#check();
      return alloc(0);
    }

    toDataURL() {
      this.#check();
      return 'data:image/png;base64,';
    }

    crop() {
      this.#check();
      return new NativeImage(token);
    }

    resize() {
      this.#check();
      return new NativeImage(token);
    }

    isTemplateImage() {
      this.#check();
      return false;
    }

    setTemplateImage() {
      this.#check();
      // Electron does not attach a template flag to an empty backing image.
    }

    addRepresentation() {
      this.#check();
      unsupported('NativeImage.addRepresentation');
    }

    getNativeHandle() {
      this.#check();
      unsupported('NativeImage.getNativeHandle');
    }
  }

  const createEmpty = () => new NativeImage(token);

  const nativeImage = { createEmpty };
  for (const name of [
    'createFromPath',
    'createFromBuffer',
    'createFromDataURL',
    'createFromBitmap',
    'createThumbnailFromPath',
    'createFromNamedImage'
  ]) {
    nativeImage[name] = () => unsupported(`nativeImage.${name}`);
  }

  nativeImage.createThumbnailFromPath = async () =>
    unsupported('nativeImage.createThumbnailFromPath');

  return { nativeImage, createEmpty };
}
