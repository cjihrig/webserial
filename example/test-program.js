/* global navigator */
'use strict';
const { registerGlobals } = require('..');


async function main() {
  registerGlobals(globalThis);
  await navigator.serial.requestPort({
    filters: [{ usbVendorId: '1155', usbProductId: '14155' }]
  });
  const ports = await navigator.serial.getPorts();
  const board = ports[0];
  await board.open({
    baudRate: 115200,
    dataBits: 8,
    stopBits: 1,
    parity: 'none',
    flowControl: 'none',
    bufferSize: 2048
  });

  /* eslint-disable no-unused-vars */
  async function setSignals() {
    await board.setSignals({ break: true });
  }

  async function getSignals() {
    console.log(await board.getSignals());
  }

  async function read() {
    const reader = board.readable.getReader();
    const { value, done } = await reader.read();
    const buf = Buffer.from(value);

    console.log(buf.toString('ascii'));
    reader.releaseLock();
  }

  async function write() {
    const writer = board.writable.getWriter();
    const encoder = new TextEncoder();

    await writer.write(encoder.encode('PING'));
    await writer.releaseLock();
  }
  /* eslint-enable no-unused-vars */

  // await getSignals();
  // await setSignals();
  await read();
  // await write();

  await board.close();
}

main();
