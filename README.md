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
恭喜你已经完全掌握SecurityWorker的使用了，进一步查看[SecurityWorker]()和[SecurityWorker VM]()提供的API帮助你更好的运用SecurityWorker。

### 3. SecurityWorker API

#### SecurityWorker
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

### SecurityWorker VM API
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
TODO

#### atob(String)
TODO

#### btoa(String)
TODO

#### request(Object)
TODO

#### WebSocket

##### WebSocket(String url [, String protocols])
TODO

##### WebSocket.prototype.send(String|TypeArray)
TODO

##### WebSocket.prototype.close(void)
TODO

##### WebSocketInstance.addEventListener(String eventName, Function handler)
TODO

##### WebSocketInstance.removeEventListener(String eventName, Function handler)
