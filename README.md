# 这是做什么用的
用于搜狗的 [workflow](https://github.com/sogou/workflow) 框架的 `Http FormData Parser`。

当然了，理论上这个 parser 略加改造就可以用于匹配其他 web 框架，只要你能顺利取到对应的 http 请求的 body 和 body size 以及 boundary。

可以查看 [HttpFormdataParser.cc](https://github.com/BDZNH/workflow-HttpFormdataParser/blob/main/src/HttpFormdataParser.cc) 的 `bool HttpFormdataParser::parse(const protocol::HttpRequest *req)` 和 `bool HttpFormdataParser::parse_boundary(std::string &content_type)` 对应的实现。

# sample

[sample](sample.cc) 是一个使用 [workflow](https://github.com/sogou/workflow) 和 [HttpFormDataParser](https://github.com/BDZNH/workflow-HttpFormdataParser)  实现的支持文件上传和文件下载的 http 服务。

[sample](sample.cc) 内的 `void process_post(WFHttpTask *server_task, const char *root)` 演示了 [HttpFormDataParser](https://github.com/BDZNH/workflow-HttpFormdataParser)  如何使用。

文件下载的部分是从 workflow 框架里面的 tutorial 部分抄过来的。如果有版权争议，以搜狗的版权为准。

# 怎么跑 sample
先下载 [workflow](https://github.com/sogou/workflow) ，下载本仓库，并至于 [workflow](https://github.com/sogou/workflow) 根目录，然后进入本仓库目录执行 `make`。

假设你已经成功的运行了服务端，那么你可以使用这样一条命令来测试功能是否正常

```shell
curl -X POST http://localhost:8888 -F "file=@sample.cc" -F "messaage=hello world"
```

上面的命令，通过 curl 的 -F 参数向 `http://localhost:8888` 以 POST 方式发送了一些数据（文件，或者字符串什么的）。

`sample.cc` 是一个将要上传的文件，你需要使用 curl 的 `-F` 参数，和一个前缀的 `@` 来上传这个文件。

如果程序按照预期那样的工作了，那么你应该可以看到在服务端程序工作的窗口中打印出类似这样的内容
```bash
received file "./tmp/sample.cc" with 6403 bytes
maybe pair "message=hello world"
```

如果你对 http 的 form 表单有所了解的话，你大概知道这个 parser 是做什么用的了。

# FAQ

如果你有兴趣更进一步了解，可以前往 [FAQ](https://github.com/BDZNH/workflow-HttpFormdataParser/issues/1)