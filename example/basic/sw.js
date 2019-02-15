onmessage = function (data) {
  if (data === 'ping') {
    console.log('SecurityWorker recv: ' + data);
    postMessage('pong');
  }
};