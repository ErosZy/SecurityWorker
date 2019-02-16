## SecurityWorker beta 文档（官网制作中，敬请期待）

>SecurityWorker提供完全隐匿且兼容ECMAScript 5.1的类WebWorker的安全可信环境，帮助保护你的核心Javascript代码不被破解。
SecurityWorker不同于普通的Javascript代码混淆，我们使用 *独立Javascript VM* + *二进制混淆SecurityWorker VM核心执行* 的方式防止您的代码被开发者工具调试、代码反向。

* [特性](https://github.com/qiaozi-tech/SecurityWorker#0-%E7%89%B9%E6%80%A7)
* [兼容性](https://github.com/qiaozi-tech/SecurityWorker#1-%E5%85%BC%E5%AE%B9%E6%80%A7)
* [快速开始](https://github.com/qiaozi-tech/SecurityWorker#2-%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B)
* [SecurityWorker API](https://github.com/qiaozi-tech/SecurityWorker#3-securityworker-api)
* [SecurityWorker VM API](https://github.com/qiaozi-tech/SecurityWorker#4-securityworker-vm-api)
* [有一定安全风险的API](https://github.com/qiaozi-tech/SecurityWorker#5-%E6%9C%89%E4%B8%80%E5%AE%9A%E5%AE%89%E5%85%A8%E9%A3%8E%E9%99%A9%E7%9A%84api)

### 0. 特性
* 完整的ECMAScript 5.1标准兼容性
* 极小的SecruityWorker VM文件体积（~160kb）
* 保密性极强，执行逻辑及核心算法完全隐匿不可逆
* 可选择支持多种环境，Browser/NodeJS/小程序（默认不允许NodeJS黑盒运行）
* 良好的浏览器兼容性，主流浏览器全覆盖
* 易于使用，API兼容WebWorker
* 易于调试，被保护代码不做混淆，报错信息准确

### 1. 兼容性
* IE11
* Chrome 20+ 
* Safari 8+ 
* Firefox 4+ 
* Edge 12+ 
* Android 4.4.4+ 
* iOS 8+
* NodeJS V4+(Doing)
* 小程序(Doing)

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
SecurityWorker实例创建成
