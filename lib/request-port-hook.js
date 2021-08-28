'use strict';
const Readline = require('readline');
const Util = require('util');

async function defaultRequestPortHook(ports) {
  const rl = Readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });
  const question = Util.promisify(rl.question).bind(rl);

  console.log(`${process.title} wants to connect to a serial port.`);
  console.log();

  if (ports.length === 0) {
    console.log('No compatible devices found.');
  } else {
    for (let i = 0; i < ports.length; i++) {
      console.log(`  ${i + 1}. '${ports[i].name}'`);
    }
  }

  console.log();

  let selection;

  do {
    const answer = await question(
      'Enter an option number or \'c\' to cancel: '
    );

    if (answer.toLowerCase() === 'c') {
      break;
    }

    const index = Number(answer) - 1;

    selection = ports[index];
  } while (!selection);

  rl.close();

  return selection;
}

module.exports = { defaultRequestPortHook };
