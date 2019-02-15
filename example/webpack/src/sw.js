const aesjs = require('aes-js');
const pkcs7 = require('pkcs7');
const base64js = require('base64-js');

let ENKEY = 'test1234test1234';
let ENIV = 'test1234test1234';

ENKEY = ENKEY.split('').map(function(v) {
  return v.charCodeAt(0);
});

ENIV = ENIV.split('').map(function(v) {
  return v.charCodeAt(0);
});

ENKEY = new Uint8Array(ENKEY);
ENIV = new Uint8Array(ENIV);

self.onmessage = function(data) {
  const href = $('location.href');
  if (href.indexOf('localhost') == -1) {
    return;
  }

  if (typeof data == 'string') {
    data = data.split('').map(v => {
      return v.charCodeAt(0);
    });

    data = new Uint8Array(data);
    data = pkcs7.pad(data, 16);

    const cbc = new aesjs.ModeOfOperation.cbc(ENKEY, ENIV);
    const encryptBytes = cbc.encrypt(data);

    const body = base64js.fromByteArray(encryptBytes);
    console.log(body);
  }
};
