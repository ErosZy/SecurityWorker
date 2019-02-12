## SecurityWorker文档

>SecurityWorker提供完全隐匿且兼容ECMAScript 5.1的类WebWorker的安全可信环境，
帮助保护你的核心Javascript代码不被破解。SecurityWorker不同于普通的Javascript代码混淆，
我们使用 *独立VM* + *高强度混淆* 的方式防止您的代码被开发者工具调试、代码反向以及Node环境运行。

### 0. 特性
* 完整的ECMAScript 5.1标准兼容性
* 极小的SecruityWorker VM文件体积（~160kb）
* 保密性极强，执行逻辑及核心算法完全隐匿不可逆
* 良好的浏览器兼容性，主流浏览器全覆盖
* 易于使用，API兼容WebWorker

### 1. 兼容性
* IE11
* Chrome 20+ 
* Safari 8+ 
* Firefox 4+ 
* Edge 12+ 
* Android 4.4.4+ 
* iOS 8+

### 2. 快速开始
我们以一个Ping-Pong的示例程序讲解SecurityWorker的基本使用，首先我们创建sw.js用于实现SecurityWorker VM的内部业务逻辑:
```javascript
// sw.js
onmessage = function(data) {
  if(data === 'ping'){
    console.log('SecurityWorker recv: ' + data);
    postMessage('pong');
  }
};
```
接着我们在[官网]()上对sw.js进行编译得到保护文件loader.js，然后我们创建index.html加载loader.js并完成调用逻辑:
```html
<html>
<head>
    <!--编译sw.js得到保护文件loader.js-->
    <script src="loader.js"></script>
</head>
<body>
    <!--.....-->
    <script>
        // 等待SecurityWorker初始化完成
        SecurityWorker.ready(function() {
          var sw = new SecurityWorker();
          
          // SecurityWorker正常创建，可被使用
          sw.oncreate = function() {
            console.log('ready');
            sw.postMessage('ping');
          }
          
          // 获取SecurityWorker传递的数据
          sw.onmessage = function(data) {
            console.log('MainThread recv: ' + data);
            sw.terminate();
          }
          
          // SecurityWorker销毁事件
          sw.onterminate = function() {
            console.log('terminate');
          }
        });
    </script>
</body>
</html>
```
最后我们访问index.html即可看到控制台完整打印出如下结果:
```shell
> ready
> SecurityWorker recv: ping
> MainThread recv: pong
> terminate
```
恭喜你已经完全掌握SecurityWorker的使用了，进一步查看[SecurityWorker](https://github.com/qiaozi-tech/SecurityWorker#3-securityworker-api)和[SecurityWorker VM](https://github.com/qiaozi-tech/SecurityWorker#4-securityworker-vm-api)提供的API帮助你更好的运用SecurityWorker。

### 3. SecurityWorker API

#### SecurityWorker(void)
SecurityWorker构造函数，用于创建一个SecurityWorker实例。但请注意，实例的创建需要在Security.ready调用后进行创建，否则将有几率导致SecurityWorker VM内存分配失败。
```javascript
SecurityWorker.ready(function(){
  var sw = new SecurityWorker();
  // ......
});
```

#### SecurityWorker.ready(Function)
SecurityWorker已经初始化完毕，可以安全的进行SecurityWorker实例的创建。
```javascript
SecurityWorker.ready(function(){
  // ......
});
```

#### SecurityWorker.prototype.postMessage(String|Object)
发送消息给内部SecurityWorker对象。
```javascript
var sw = new SecurityWorker();
sw.oncreate = function() {
  sw.postMessage("Hello World!");
  sw.postMessage({
    now: Date.now()
  })
}
```

#### SecurityWorker.prototype.terminate(void)
销毁SecurityWorker实例和SecurityWorker VM对象，此后将无法进行消息发送。
```javascript
var sw = new SecurityWorker();
sw.oncreate = function() {
  sw.terminate();
  sw.postMessage(); // error, can't postMessage after terminate
}
```

#### SecurityWorkerInstance.oncreate
SecurityWorker实例创建成功的回调，此后可以安全的与SecurityWorker VM进行数据通信。
```javascript
var sw = new SecurityWorker();
sw.oncreate = function() {
  console.log('ready')
}
```

#### SecurityWorkerInstance.onmessage
接收到SecurityWorker传递的相关数据的回调。
```javascript
var sw = new SecurityWorker();
sw.onmessage = function(data) {
  if(typeof data == 'string') {
    console.log(data);
  }else if(typeof data == 'object') {
    console.log(JSON.stringify(data));
  }
}
```

#### SecurityWorkerInstance.onterminate
SecurityWorker实例已经成功销毁的回调，此后将不能再进行postMessage的调用。
```javascript
var sw = new SecurityWorker();
sw.onterminate = function() {
  sw.postMessage(); // error, can't postMessage after terminate
}
```

### 4. SecurityWorker VM API
以下所有API只能在SecurityWorker VM中使用，外部环境的SecurityWorker实例及原型并未提供此类API。

#### onmessage
获取外部SecurityWorker实例发送的相关数据
```javascript
onmessage = function(data) {
  if(typeof data == 'string') {
    console.log(data);
  }else if(typeof data == 'object') {
    console.log(JSON.stringify(data);
  }
}

// or use self

self.onmessage = function() {
  // something to do...
}
```

#### postMessage(String|Object)
发送数据给外部的SecurityWorker实例。
```javascript
postMessage('Hello World!');
postMessage({
  now: Date.now()
});
```

#### btoa(String)
对字符串进行Base64编码。
```javascript
var b64 = btoa('Hello World!');
console.log(b64); // SGVsbG8gV29ybGQh
```

#### atob(String)
对用Base64编码过的字符串进行解码。
```javascript
var str = atob('SGVsbG8gV29ybGQh');
console.log(str); // 'Hello World!'
```

#### setTimeout(Function, Number)
延迟执行函数
```javascript
setTimeout(function(){
  console.log('Hello World!');
}, 1000);
```

#### setInterval(Function, Number)
循环执行函数(注意：setInterval内部实现采用setTimeout)
```javascript
setInterval(function(){
  console.log('Hello World!');
}, 1000);
```

#### console相关函数
SecurityWorker VM支持如下的console函数：
* console.log
* console.info
* console.debug
* console.error
* console.time
* console.timeEnd

#### request(Object)
发送Ajax请求，其接收一个对象所含参数为:
* String uri: 请求地址
* String method: 请求方法，可使用GET/POST/DELETE/HEAD/PUT，默认GET
* String body: POST/DELETE/PUT的请求参数，可选
* Object headers: 附加的HTTP Header信息， 可选
* Function success: 请求成功的回调，可选
* Function error: 请求失败的回调，可选
```javascript
// GET请求
request({
  uri: 'http://www.baidu.com',
  method: 'GET',
  headers: {
    'X-AUTH': 'THIS IS YOUR AUTH KEY'
  },
  success: function(data){
    console.log("status: " + data.status);
    console.log("statusText: " + data.statusText);
    console.log("text: " + data.text);
  },
  error: function(err){
    console.log(err);
  }
});

// POST请求
request({
  uri: 'http://www.baidu.com',
  method: 'POST',
  body: JSON.stringify({id: 1}),
  success: function(data){
    console.log("status: " + data.status);
    console.log("statusText: " + data.statusText);
    console.log("text: " + data.text);
  },
  error: function(err){
    console.log(err);
  }
});
```

#### WebSocket

##### WebSocket(String url [, String protocols])
WebSocket构造函数。
```javascript
var ws = new WebSocket('wss://www.baidu.com');
```

##### WebSocket.prototype.send(String|TypeArray)
向服务器发送字符串或二进制数据。
```javascript
var ws = new WebSocket('wss://www.baidu.com');
ws.addEventListener('open', function(){
  ws.send('Hello World!');
  ws.send(new Uint8Array([1,2,3]));
});
```

##### WebSocket.prototype.close(void)
关闭WebSocket连接，并remove所有注册的事件。
```javascript
var ws = new WebSocket('wss://www.baidu.com');
ws.close();
```

##### WebSocketInstance.addEventListener(String eventName, Function handler)
SecurityWorker VM的addEventListener中支持4种标准事件:
* open: 连接打开
* message: 获得服务器发送的数据
* error: 发生相关错误
* close: 连接关闭
```javascript
var ws = new WebSocket('wss://www.baidu.com');
ws.addEventListener('open', function(){
  ws.send('ready');
});

ws.addEventListener('message', function(message){
  if(message === 'close'){
    ws.close();
  }
});

ws.addEventListener('error', function(error){
  console.log(error);
});

ws.addEventListener('close', function(){
  // 不需要进行removeEventListener操作，
  // 当调用close方法后自动解绑所有事件回调
  console.log('ws client open')
});
```

##### WebSocketInstance.removeEventListener(String eventName, Function handler)
SecurityWorker VM的removeEventListener中支持4种标准事件:
* open: 连接打开
* message: 获得服务器发送的数据
* error: 发生相关错误
* close: 连接关闭
```javascript
var ws = new WebSocket('wss://www.baidu.com');
ws.addEventListener('open', function(){
  ws.send('ready');
});

function onmessage(message){
  console.log(message);
}

ws.addEventListener('message', onmessage);

setTimeout(function(){
  ws.removeEventListener('message', onmessage);
}, 1000);
```