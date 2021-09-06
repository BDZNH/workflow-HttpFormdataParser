# 这是做什么用的
用于搜狗的 [workflow](https://github.com/sogou/workflow) 框架的 HttpFormDataParser

# sample

[sample](sample.cc) 是一个使用 [workflow](https://github.com/sogou/workflow) 和 HttpFormDataParser 实现的支持文件上传和文件下载的 http 服务。

文件下载的部分是从 workflow 框架里面的 tutorial 部分抄过来的。如果有版权争议，以搜狗的版权为准。

# 怎么跑 sample
先下载 [workflow](https://github.com/sogou/workflow) ，下载本仓库，并至于 [workflow](https://github.com/sogou/workflow) 根目录，然后进入本仓库目录执行 `make`