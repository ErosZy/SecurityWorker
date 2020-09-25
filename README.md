## SecurityWorker文档（V1.0.1）

>SecurityWorker提供完全隐匿且兼容ECMAScript 5.1的类WebWorker的安全可信环境，帮助保护你的核心Javascript代码不被破解。
SecurityWorker不同于普通的Javascript代码混淆，我们使用 *独立Javascript VM* + *二进制混淆opcode核心执行* 的方式防止您的代码被开发者工具调试、代码反向。



* [特性](https://github.com/qiaozi-tech/SecurityWorker#0-%E7%89%B9%E6%80%A7)
* [兼容性](https://github.com/qiaozi-tech/SecurityWorker#1-%E5%85%BC%E5%AE%B9%E6%80%A7)
* [快速开始](https://github.com/qiaozi-tech/SecurityWorker#2-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)
* [SecurityWorker API](https://github.com/qiaozi-tech/SecurityWorker#3-securityworker-api)
* [SecurityWorker VM API](https://github.com/qiaozi-tech/SecurityWorker#4-securityworker-vm-api)
* [有一定安全风险的API](https://github.com/qiaozi-tech/SecurityWorker#5-%E6%9C%89%E4%B8%80%E5%AE%9A%E5%AE%89%E5%85%A8%E9%A3%8E%E9%99%A9%E7%9A%84api)
* [性能优化建议](https://github.com/qiaozi-tech/SecurityWorker#6-%E6%80%A7%E8%83%BD%E4%BC%98%E5%8C%96%E5%BB%BA%E8%AE%AE)
* [Roadmap](https://github.com/qiaozi-tech/SecurityWorker#7-roadmap)

> ⚠️ SecurityWorker不再维护，你可以选择更好的代替方案 [sablejs - 🏖️ The safer and faster JavaScript interpreter written by JavaScript](https://github.com/ErosZy/sablejs)

### 0. 特性
* 完整的ECMAScript 5.1标准兼容性
* 极小的SecruityWorker VM文件体积（~160kb）
* 保密性极强，执行逻辑及核心算法完全隐匿不可逆
* 可选择支持多种环境，Browser/NodeJS/小程序（默认不允许NodeJS黑盒运行）
* 良好的浏览器兼容性，主流浏览器全覆盖
* 易于使用，API兼容WebWorker（不允许访问DOM/BOM）
* 易于调试，被保护代码不做混淆，报错信息准确

### 1. 兼容性
* IE11
* Chrome 24+ 
* Safari 6.2+ 
* Firefox 16+ 
* Edge 15+
* Android 4.4.2+ 
* iOS Safari 8+
* iOS WebView 9+

### 2. 快速开始
我们以一个[Ping-Pong](https://github.com/qiaozi-tech/SecurityWorker/tree/master/example/basic)的示例程序讲解SecurityWorker的基本使用，首先我们创建sw.js用于实现SecurityWorker VM的内部业务逻辑:
```javascript
// sw.js
onmessage = function(data) {
  if(data === 'ping'){
    console.log('SecurityWorker recv: ' + data);
    postMessage('pong');
  }
};
```
接着我们在使用bin文件对sw.js进行编译得到保护文件loader.js：
```shell
> npm run compile ${youfile}
```
然后我们创建index.html加载loader.js并完成调用逻辑:
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
          };
          
          // 获取SecurityWorker传递的数据
          sw.onmessage = function(data) {
            console.log('MainThread recv: ' + data);
            sw.terminate();
          };
          
          // SecurityWorker销毁事件
          sw.onterminate = function() {
            console.log('terminate');
          };
        });
    </script>
</body>
</html>
```
最后我们访问index.html即可看到控制台完整打印出如下结果:
```shell
> ready
> [LOG] SecurityWorker recv: ping
> MainThread recv: pong
> terminate
```
恭喜你已经完全掌握SecurityWorker的使用了，进一步查看[SecurityWorker](https://github.com/qiaozi-tech/SecurityWorker#3-securityworker-api)和[SecurityWorker VM](https://github.com/qiaozi-tech/SecurityWorker#4-securityworker-vm-api)提供的API帮助你更好的运用SecurityWorker。

### 3. SecurityWorker API

#### SecurityWorker.runMode
设定SecurityWorker VM执行的环境，有3个选择:
* SecurityWorker.AUTO_MODE: 自动选择运行于WebWorker还是浏览器主线程中，推荐的默认值
* SecurityWorker.WORKER_THREAD_MODE: 强制运行于WebWorker环境中，由于某些浏览器的WebWorker环境有一些兼容性问题，因此对于需要兼容性的应用不推荐此选项
* SecurityWorker.MAIN_THREAD_MODE: 强制运行于浏览器主线程中，兼容性很好，但是对于较老的设备可能会导致页面较长时间无响应

#### SecurityWorker(void)
SecurityWorker构造函数，用于创建一个SecurityWorker实例。但请注意，实例的创建需要在SecurityWorker.ready调用后进行创建，否则将有几率导致SecurityWorker VM内存分配失败。
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
发送消息给SecurityWorker VM对象。
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
接收到SecurityWorker VM传递的相关数据的回调。
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
SecurityWorker实例及SecurityWorker VM已经成功销毁的回调，此后将不能再进行postMessage的调用。
```javascript
var sw = new SecurityWorker();
sw.onterminate = function() {
  sw.postMessage(); // error, can't postMessage after terminate
}
```

### 4. SecurityWorker VM API
以下所有API只能在SecurityWorker VM中使用，外部环境的SecurityWorker实例及原型并未提供此类API。

#### onmessage
获取SecurityWorker实例发送的相关数据
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
发送数据给SecurityWorker实例。
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

#### Console相关函数
SecurityWorker VM支持如下的Console函数：
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
关闭WebSocket连接。
```javascript
var ws = new WebSocket('wss://www.baidu.com');
ws.close();
```

##### WebSocketInstance.addEventListener(String eventName, Function handler)
支持4种标准事件:
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
  // 当调用close事件后自动解绑所有事件回调
  console.log('ws client open')
});
```

##### WebSocketInstance.removeEventListener(String eventName, Function handler)
支持4种标准事件:
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

### 5. 有一定安全风险的API

#### $(String|Function) -> String
$函数是SecurityWorker VM内部的类预处理函数，其可以方便的在外部环境执行代码。它不同于提供的postMessage和onmessage方法，它是同步的，在编译阶段你的代码会被编译成属性访问，例如：
```javascript
// sw.js
var location = $('window.location.href');

// 编译后实际代码为
var location = $[0];
```
此预处理函数的出现主要是针对使用postMessage容易暴露行为的场景。假设我们的SecurityWorker VM的代码需要首先判断当前的Domain后再决定是否进行数据请求，当我们不使用$时，我们的代码如下：
```javascript
// sw.js
onmessage = function(data){
  if(data.indexOf('your domain') > -1){
    request({
      uri: 'your url', 
      success: function(data){
        postMessage(data.text);
    }});
  }  
}
```
```javascript
// your index.html
SecurityWorker.ready(function(){
  var sw = new SecurityWorker();
  sw.oncreate = function(){
    sw.postMessage(location.href);
  }
  sw.onmessage = function(data){
    console.log(data);
  }
});
```
这里我们可以看到，攻击者很容易发现我们index.html中有传递location.href值的逻辑。但当我们使用$预处理函数后，我们最终的代码会依靠VM转换为opcode经过LLVM处理并进行高强度混淆后嵌入到编译后的代码之中，增强了隐匿性（但需要注意的是，由于$的整个逻辑涉及到从不隐匿环境（Browser）到隐匿环境（SecurityWorker VM）的数据传递，代码仍然在最终编译后的文件中出现，无法做到完全保密，因此可能带来不安全的风险，请斟酌使用）。
```javascript
onmessage = function(data){
  var location = $(function(){
    // 以下代码在宿主环境中运行
    // SecurityWorker会在编译期进行混淆并嵌入到生成代码中
    // 尽管只是代码的混淆，但是我们再隐藏下真正的数据序列
    var l = location.href;
    return l.split('').map(function(v){
      return v.charCodeAt(0) << 4 + 128;
    }).join(',');
  });

  // 下面的代码已经被编译为SecurityWorker VM的opcode，执行在安全环境
  location = location.split(',').map(function(v){
    return String.fromCharCode((v - 128) >> 4);
  });

  if(location.indexOf('your domain') > -1){
    request({
      uri: 'your url',
      success: function(data){
        postMessage(data.text);
      }
    });
  }
}
```
```javascript
SecurityWorker.ready(function(){
  var sw = new SecurityWorker();
  sw.oncreate = function(){
    sw.postMessage('What Ever You Want');
  }
  sw.onmessage = function(data){
    console.log(data);
  }
});
```

### 6. 性能优化建议
SecurityWorker VM与V8等强调性能的Javascript引擎不同，SecurityWorker VM主要目标是更小的emscripten生成体积以及更少的内存使用。对于SecurityWorker VM来说，我们并没有集成类似V8一样的JIT机制，而是使用通过离线翻译你的Javascript代码为SecurityWorker VM指令，然后在运行时解释执行的方式，因此在性能上会有一定的损失。<br/>
相较于最新版本的V8 JIT优化后的代码，纯CPU计算性能相差7-8倍（执行10000次），I/O任务由于使用了原生环境的功能，性能大体持平。在实际应用中，我们使用SecurityWorker VM的WebSocket每20ms接收10k加密字符串并进行纯Javascript的AES256的解密操作这一任务与原生环境测试结果相比并不大（ Mac Pro 2017, Intel Core i5 2.3GHz 平均占用：2.3% vs 1.8% ）。

#### 尽可能减少指令
由于SecurityWorker VM并没有JIT，因此你所熟悉的一些优化手段可能会在SecurityWorker VM中失效。不要寄希望于SecurityWorker会优化你的代码，他目前并不智能（逃），任何多余的Javascript代码操作都会增加运行时的开销，例如：
```javascript
var i = 1000, x = 0;
while( i-- ) x++;
```
相比于
```javascript
var x = 0;
for( var j = 0; j < 1000; j++ ) x++;
```
在SecurityWorker VM中将会快15%，因为for循环中我们额外的引入了比较操作（j < 1000）。但对于此并不需要感到紧张，我们的建议是仍然按照你的方式编写代码，在需要深度优化的时候再进行考虑，因为在SecurityWorker VM中我们持续运行CPU密集型任务的场景并不多，大部分是等待I/O，这很难成为你代码的性能瓶颈。

#### 浮点数有很高的代价
Javascript的Number类型包含了Int和Float，同时根据ECMA-262标准的要求，我们需要通过一个浮点数指针来实现64-bit IEEE Math操作。但是对于SecurityWorker VM内部，我们考虑到内存占用的问题对Int和Float实际上进行了更细的区分，因此在大部分测试下Int的相关操作相比于Float会更快，占用内存会更少。
```javascript
var a = 1; // 4 bytes
var a = 0.7; // 12 bytes
```

#### 尽可能使用TypedArray
当你数组中的类型明确为Number时，我们强烈建议你使用TypedArray来解决你的问题。对于TypedArray来说，我们可以明确类型同时省去类型包装的花销，并且不需要自动的进行数组的resize操作，因此相比与普通数组来说会更快更省内存。
```javascript
var b = new Array( 2048 ); // 4KB for the array with values
for( var i = 0; i++; i < 2048 ) b[ i ] = i + 0.6; // + 8KB with floats

// Just 4KB allocated
var a = new Float32Array( 2048 );
```

#### 当出现无法解决的性能问题
反复测试并联系我们，帮助我们让SecurityWorker变得更好（笑）。


### 7. Roadmap
* 增加SecurityWorker的onerror回调，返回SecurityWorker VM内部的未被捕获的错误
* 提供小程序的环境支持
* 提供小游戏的环境支持
* 提供NodeJS的环境支持
* 进一步优化生成的opcode大小
