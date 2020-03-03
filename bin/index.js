#! /usr/bin/env node
const argv = process.argv;
const fs = require("fs");
const request = require("superagent");

function sleep(delay) {
  return new Promise(resolve => {
    setTimeout(resolve, delay);
  });
}

(async () => {
  const filepath = argv[2];
  if (!filepath || !fs.existsSync(filepath)) {
    console.log(`[E] not find filepath: ${filepath}`);
    process.exit(1);
  }

  let data = fs.readFileSync(filepath).toString();
  console.log(`[I] upload codes to SecurityWorker compiler server...`);
  let { status, body } = await request
    .post("http://34.92.175.137/code")
    .send({
      code: data
    })
    .retry(5)
    .timeout({
      response: 15 * 1000,
      deadline: 30 * 1000
    });

  if (status != 200) {
    console.log(`[E] request status error: ${status}`);
    process.exit(0);
  }

  if (body.code != 0) {
    console.log(`[E] request data error: ${JSON.stringify(body)}`);
    process.exit(0);
  }

  const { filename } = body.data;
  console.log(`[I] upload successfuly, compiling...`);
  for (;;) {
    try {
      const { status, body } = await request
        .post("http://34.92.175.137/status")
        .send({ filename })
        .retry(5)
        .timeout({ response: 15 * 1000, deadline: 30 * 1000 });

      if (status != 200) {
        console.log(`[E] request status error: ${status}`);
        continue;
      }

      if (body.code == -1) {
        console.log(`[E] request data error: ${JSON.stringify(body)}`);
        continue;
      }

      if (body.code == 1) {
        await sleep(2000);
        continue;
      }

      if (body.code == 0) {
        console.log(`[I] code write to ./${filename}.js`);
        fs.writeFileSync(`./${filename}.js`, body.data);
        break;
      }
    } catch (e) {
      // nothing to do...
    }
  }
})();
