# webserial

[![Current Version](https://img.shields.io/npm/v/webserial.svg)](https://www.npmjs.org/package/webserial)
![Dependencies](http://img.shields.io/david/cjihrig/webserial.svg)
[![belly-button-style](https://img.shields.io/badge/eslint-bellybutton-4B32C3.svg)](https://github.com/cjihrig/belly-button)

**Experimental**: Requires Node.js 16.5.0 or newer.

[Web Serial API](https://wicg.github.io/serial/) for Node.js.

Using this module:

```javascript
'use strict';
const { Serial, SerialPort, registerGlobals } = require('webserial');

registerGlobals(globalThis);
console.log(globalThis.navigator.serial);
console.log(globalThis.navigator.serial instanceof Serial);
```
