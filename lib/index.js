/* global EventTarget */
'use strict';
const { ReadableStream, WritableStream } = require('stream/web');
const { types } = require('util');
const Binding = require('../build/Release/webserial');
const { defaultRequestPortHook } = require('./request-port-hook');
const kMaxBufferSize = 2 ** 31 - 1;
const kPortName = Symbol('portName'); // Do not export this from this file.
const kStateClosed = 1;
const kStateClosing = 2;
const kStateOpening = 3;
const kStateOpened = 4;
const parityMap = new Map([
  ['none', Binding.kParityNone],
  ['even', Binding.kParityEven],
  ['odd', Binding.kParityOdd]
]);
const flowControlMap = new Map([
  ['none', Binding.kFlowControlNone],
  ['hardware', Binding.kFlowControlHardware]
]);

// TODO(cjihrig): onconnect() and ondisconnect() don't currently do anything.

class Serial extends EventTarget {
  #availablePorts;
  #onConnect;
  #onDisconnect;
  #requestPortHook;

  constructor(options) {
    super();
    this.#availablePorts = new Map();
    this.#onConnect = null;
    this.#onDisconnect = null;
    this.#requestPortHook = typeof options?.requestPortHook === 'function' ?
      options?.requestPortHook : defaultRequestPortHook;
  }

  get onconnect() {
    return this.#onConnect;
  }

  set onconnect(value) {
    if (typeof value !== 'function') {
      return;
    }

    this.#onConnect = value;
  }

  get ondisconnect() {
    return this.#onDisconnect;
  }

  set ondisconnect(value) {
    if (typeof value !== 'function') {
      return;
    }

    this.#onDisconnect = value;
  }

  getPorts() {
    return new Promise((resolve, reject) => {
      resolve(Array.from(this.#availablePorts.values()));
    });
  }

  requestPort(options) {
    // eslint-disable-next-line no-async-promise-executor
    return new Promise(async (resolve, reject) => {
      if (!isObject(options)) {
        options = {};
      }

      const filterMap = new Map();

      if ('filters' in options) {
        const { filters } = options;

        if (!isObject(filters) && typeof filters !== 'function') {
          throw new TypeError(
            'the provided value cannot be converted to a sequence'
          );
        }

        if (typeof filters[Symbol.iterator] !== 'function') {
          throw new TypeError(
            'the object must have a callable @@iterator property'
          );
        }

        const iterator = filters[Symbol.iterator]();

        for (const filter of iterator) {
          if (!isObject(filter)) {
            throw new TypeError('cannot convert signals to dictionary');
          }

          if (!('usbVendorId' in filter)) {
            throw new TypeError('filter must provide a property to filter by');
          }

          filterMap.set(+filter.usbVendorId, filter);
        }
      }

      let ports = Binding.listAllPorts();

      if (filterMap.size > 0) {
        ports = ports.filter(({ vendorId, productId }) => {
          const filter = filterMap.get(+vendorId);

          if (filter === undefined) {
            return false;
          }

          if ('usbProductId' in filter && +filter.usbProductId !== +productId) {
            return false;
          }

          return true;
        });
      }

      const selectedPort = await this.#requestPortHook(ports);

      if (selectedPort === undefined) {
        throwDomException('NotFoundError', 'no port selected');
      }

      let port = this.#availablePorts.get(selectedPort.name);

      if (port === undefined) {
        port = new SerialPort({ // eslint-disable-line no-use-before-define
          [kPortName]: selectedPort.name,
          usbVendorId: selectedPort.vendorId,
          usbProductId: selectedPort.productId,
          parent: this
        });
        this.#availablePorts.set(selectedPort.name, port);
      }

      resolve(port);
    });
  }
}


class SerialPort extends EventTarget {
  #bufferSize;
  #handle;
  #onConnect;
  #onDisconnect;
  #parent;
  #pendingClosePromiseResolve;
  #portName;
  #readable;
  #readFatal;
  #state;
  #usbProductId;
  #usbVendorId;
  #writable;
  #writeFatal;

  constructor(options) {
    super();

    if (options === null || typeof options !== 'object' ||
        !(kPortName in options)) {
      throw new TypeError('illegal constructor');
    }

    const name = options[kPortName];

    this.#bufferSize = undefined;
    this.#handle = Binding.createHandle(name);
    this.#parent = options.parent;
    this.#pendingClosePromiseResolve = null;
    this.#portName = name;
    this.#readable = null;
    this.#readFatal = false;
    this.#state = kStateClosed;
    this.#usbProductId = options.usbProductId;
    this.#usbVendorId = options.usbVendorId;
    this.#writable = null;
    this.#writeFatal = false;
  }

  get onconnect() {
    return this.#onConnect;
  }

  set onconnect(value) {
    if (typeof value !== 'function') {
      return;
    }

    this.#onConnect = value;
  }

  get ondisconnect() {
    return this.#onDisconnect;
  }

  set ondisconnect(value) {
    if (typeof value !== 'function') {
      return;
    }

    this.#onDisconnect = value;
  }

  get readable() {
    if (this.#readable !== null) {
      return this.#readable;
    }

    if (this.#state !== kStateOpened) {
      return null;
    }

    if (this.#readFatal) {
      return null;
    }

    const handle = this.#handle;
    const self = this;

    this.#readable = new ReadableStream({
      type: 'bytes',
      pull(controller) {
        return new Promise((resolve, reject) => {
          try {
            const buffer = Binding.readData(handle, controller.desiredSize);

            if (buffer.bytesRead > 0) {
              controller.enqueue(new Uint8Array(buffer, 0, buffer.bytesRead));
            }
          } catch (err) {
            // TODO(cjihrig): Map the error according to the spec. If the port
            // disconnected, also set readFatal to true.
            controller.error(err);
            self.#closeReadable();
          }

          resolve();
        });
      },
      cancel(reason) {
        return new Promise((resolve, reject) => {
          try {
            Binding.discardRxBuffer(handle);
          } finally {
            self.#closeReadable();
            resolve();
          }
        });
      }
    }, {
      highWaterMark: this.#bufferSize
    });

    return this.#readable;
  }

  set readable(value) { // eslint-disable-line class-methods-use-this
    // Do nothing.
  }

  get writable() {
    if (this.#writable !== null) {
      return this.#writable;
    }

    if (this.#state !== kStateOpened) {
      return null;
    }

    if (this.#writeFatal) {
      return null;
    }

    const handle = this.#handle;
    const self = this;

    this.#writable = new WritableStream({
      write(chunk, controller) {
        return new Promise((resolve, reject) => {
          let bytes;

          if (types.isArrayBufferView(chunk)) {
            const start = chunk.byteOffset;
            const end = start + chunk.byteLength;

            bytes = chunk.buffer.slice(start, end);
          } else if (types.isAnyArrayBuffer(chunk)) {
            bytes = chunk.slice();
          } else {
            throw new TypeError('chunk must be a buffer source');
          }

          try {
            Binding.writeData(handle, bytes);
            resolve();
          } catch (err) {
            // TODO(cjihrig): If the port became disconnected, the error
            // handling here needs to be different according to the spec.
            throwDomException('UnknownError', err.message);
          }
        });
      },
      close() {
        return new Promise((resolve, reject) => {
          try {
            Binding.flushTxBuffer(handle);
          } finally {
            self.#closeWritable();
            resolve();
          }
        });
      },
      abort(reason) {
        return new Promise((resolve, reject) => {
          try {
            Binding.discardTxBuffer(handle);
          } finally {
            self.#closeWritable();
            resolve();
          }
        });
      }
    }, {
      highWaterMark: this.#bufferSize
    });

    return this.#writable;
  }

  set writable(value) { // eslint-disable-line class-methods-use-this
    // Do nothing.
  }

  close() {
    // eslint-disable-next-line no-async-promise-executor
    return new Promise(async (resolve, reject) => {
      const cancelPromise = this.#readable === null ? Promise.resolve() :
        this.#readable.cancel();
      const abortPromise = this.#writable === null ? Promise.resolve() :
        this.#writable.abort();
      let pendingClosePromiseResolve = null;
      const pendingClosePromise = new Promise((resolve, reject) => {
        if (this.#readable === null && this.#writable === null) {
          return resolve();
        }

        pendingClosePromiseResolve = resolve;
      });
      this.#pendingClosePromiseResolve = pendingClosePromiseResolve;
      this.#state = kStateClosing;
      const combinedPromise = Promise.all(
        [cancelPromise, abortPromise, pendingClosePromise]
      );

      try {
        await combinedPromise;
        Binding.closePort(this.#handle);
        this.#state = kStateClosed;
        this.#readFatal = false;
        this.#writeFatal = false;
      } finally {
        this.#pendingClosePromiseResolve = null;
      }

      resolve();
    });
  }

  open(options) {
    return new Promise((resolve, reject) => {
      assertState(this.#state, kStateClosed, 'port is already open');

      if (!isObject(options)) {
        options = {};
      }

      const {
        baudRate,
        dataBits = 8,
        stopBits = 1,
        parity = 'none',
        bufferSize = 255,
        flowControl = 'none'
      } = options;
      const mappedParity = parityMap.get(parity);
      const mappedFlowControl = flowControlMap.get(flowControl);

      if ((baudRate >>> 0) !== baudRate || baudRate === 0) {
        throw new TypeError('baudRate must be a non-zero unsigned integer');
      }

      if (dataBits !== 7 && dataBits !== 8) {
        throw new TypeError('dataBits must be 7 or 8');
      }

      if (stopBits !== 1 && stopBits !== 2) {
        throw new TypeError('stopBits must be 1 or 2');
      }

      if (mappedParity === undefined) {
        throw new TypeError('parity must be none, even, or odd');
      }

      if (mappedFlowControl === undefined) {
        throw new TypeError('flowControl must be none or hardware');
      }

      if (bufferSize <= 0) {
        throw new TypeError('bufferSize must be greater than 0');
      }

      if (bufferSize > kMaxBufferSize) {
        throw new TypeError(
          `bufferSize must be less than ${kMaxBufferSize + 1}`
        );
      }

      this.#state = kStateOpening;

      try {
        Binding.openPort(this.#handle, baudRate, dataBits, stopBits,
          mappedParity, mappedFlowControl);
        this.#state = kStateOpened;
      } catch (err) {
        this.#state = kStateClosed;
        throwDomException('NetworkError', err.message);
      }

      this.#bufferSize = bufferSize;
      resolve();
    });
  }

  getInfo() {
    return {
      usbVendorId: this.#usbVendorId,
      usbProductId: this.#usbProductId
    };
  }

  getSignals() {
    return new Promise((resolve, reject) => {
      assertState(this.#state, kStateOpened, 'port is not open');

      try {
        const signals = Binding.getSignals(this.#handle);

        resolve(signals);
      } catch (err) {
        throwDomException('NetworkError', err.message);
      }
    });
  }

  setSignals(signals) {
    return new Promise((resolve, reject) => {
      assertState(this.#state, kStateOpened, 'port is not open');

      // JavaScript to serial port value mappings:
      // false -> 0 = OFF, true -> 1 = ON, undefined -> -1 = NO CHANGE
      if (!isObject(signals)) {
        throw new TypeError('cannot convert signals to dictionary');
      }

      const { dataTerminalReady, requestToSend, break: brake } = signals;
      const dtr = dataTerminalReady === undefined ? -1 : +!!dataTerminalReady;
      const rts = requestToSend === undefined ? -1 : +!!requestToSend;
      const brk = brake === undefined ? -1 : +!!brake;

      if (dtr === -1 && rts === -1 && brk === -1) {
        throw new TypeError(
          'signals dictionary must contain at least one member'
        );
      }

      try {
        Binding.setSignals(this.#handle, dtr, rts, brk);
        resolve();
      } catch (err) {
        throwDomException('NetworkError', err.message);
      }
    });
  }

  #closeReadable() {
    this.#readable = null;

    if (this.#writable === null && this.#pendingClosePromiseResolve !== null) {
      this.#pendingClosePromiseResolve();
    }
  }

  #closeWritable() {
    this.#writable = null;

    if (this.#readable === null && this.#pendingClosePromiseResolve !== null) {
      this.#pendingClosePromiseResolve();
    }
  }
}


function isObject(value) {
  return typeof value === 'object' && value !== null;
}


function assertState(actual, expected, failMessage) {
  if (actual !== expected) {
    throwDomException('InvalidStateError', failMessage);
  }
}


function throwDomException(name, message) {
  // TODO(cjihrig): Use DOMException once it is available.
  const err = new Error(message);

  err.name = name;
  throw err;
}


function registerGlobals(glblThis) {
  glblThis.Serial ??= Serial;
  glblThis.SerialPort ??= SerialPort;
  glblThis.navigator ??= {};
  glblThis.navigator.serial ??= new Serial();
}


module.exports = { Serial, SerialPort, registerGlobals };
