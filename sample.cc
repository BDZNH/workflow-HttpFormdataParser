/*
  Copyright (c) 2019 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Author: Xie Han (xiehan@sogou-inc.com;63350856@qq.com)
  Author: BDZNH (https://github.com/BDZNH)
*/

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <utility>
#include <string>
#include <string.h>
#include "workflow/HttpMessage.h"
#include "workflow/WFHttpServer.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/Workflow.h"
#include "workflow/WFFacilities.h"
#include "workflow/HttpUtil.h"
#include "HttpFormdataParser.h"

using namespace protocol;

void pread_callback(WFFileIOTask *task)
{
    FileIOArgs *args = task->get_args();
    long ret = task->get_retval();
    HttpResponse *resp = (HttpResponse *)task->user_data;

    close(args->fd);
    if (task->get_state() != WFT_STATE_SUCCESS || ret < 0)
    {
        resp->set_status_code("503");
        resp->append_output_body("<html>503 Internal Server Error.</html>");
    }
    else /* Use '_nocopy' carefully. */
        resp->append_output_body_nocopy(args->buf, ret);
}

void process_get(WFHttpTask *server_task, const char *root)
{
    HttpRequest *req = server_task->get_req();
    HttpResponse *resp = server_task->get_resp();
    const char *uri = req->get_request_uri();
    const char *p = uri;

    printf("Request-URI: %s\n", uri);
    while (*p && *p != '?')
        p++;

    std::string abs_path(uri, p - uri);
    abs_path = root + abs_path;
    if (abs_path.back() == '/')
        abs_path += "index.html";

    resp->add_header_pair("Server", "Sogou C++ Workflow Server");

    int fd = open(abs_path.c_str(), O_RDONLY);
    if (fd >= 0)
    {
        size_t size = lseek(fd, 0, SEEK_END);
        void *buf = malloc(size); /* As an example, assert(buf != NULL); */
        WFFileIOTask *pread_task;

        pread_task = WFTaskFactory::create_pread_task(fd, buf, size, 0,
                                                      pread_callback);
        /* To implement a more complicated server, please use series' context
         * instead of tasks' user_data to pass/store internal data. */
        pread_task->user_data = resp; /* pass resp pointer to pread task. */
        server_task->user_data = buf; /* to free() in callback() */
        server_task->set_callback([](WFHttpTask *t)
                                  { free(t->user_data); });
        series_of(server_task)->push_back(pread_task);
    }
    else
    {
        resp->set_status_code("404");
        resp->append_output_body("<html>404 Not Found.</html>");
    }
}

void pwrite_callback(WFFileIOTask *task)
{
    long ret = task->get_retval();
    HttpResponse *resp = (HttpResponse *)task->user_data;

    if (task->get_state() != WFT_STATE_SUCCESS || ret < 0)
    {
        resp->set_status_code("503");
        resp->append_output_body("<html>503 Internal Server Error.</html>\r\n");
    }
    else
    {
        resp->set_status_code("200");
        resp->append_output_body("<html>200 success.</html>\r\n");
    }
}

void process_post(WFHttpTask *server_task, const char *root)
{

    HttpFormdataParser parser;
    if (parser.parse(server_task->get_req()))
    {
        HttpFormdataCursor cursor(&parser);
        std::string name;
        std::string filename;
        std::string value;
        while (cursor.next(name))
        {
            if (cursor.is_file(name, filename))
            {
                std::string filepath = root;
                if (filepath.back() != '/')
                    filepath.push_back('/');

                const void *data;
                size_t datasize;
                if (cursor.get_content(name, &data, &datasize))
                {
                    filepath += filename;
                    printf("received file \"%s\" with %zu bytes\n", filepath.c_str(), datasize);
                    WFFileIOTask *pwrite_task;
                    pwrite_task = WFTaskFactory::create_pwrite_task(filepath, data, datasize, 0, pwrite_callback);
                    pwrite_task->user_data = server_task->get_resp();
                    series_of(server_task)->push_back(pwrite_task);
                }
            }
            else if (cursor.get_string(name, value))
            {
                printf("maybe pair \"%s=%s\" \n", name.c_str(), value.c_str());
            }
        }
    }
    else
    {
        server_task->get_resp()->set_status_code("503");
        server_task->get_resp()->append_output_body("<html>503 Internal Server Error.</html>\r\n");
    }
}

void process(WFHttpTask *server_task, const char *root)
{
    if (strcmp("POST", server_task->get_req()->get_method()) == 0)
    {
        process_post(server_task, root);
    }
    else if (strcmp("GET", server_task->get_req()->get_method()) == 0)
    {
        process_get(server_task, root);
    }
    else
    {
        server_task->get_resp()->set_status_code("503");
        server_task->get_resp()->append_output_body("<html>503 internal error</html>\r\n");
    }
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main(int argc, char *argv[])
{
    if (argc != 2 && argc != 3 && argc != 5)
    {
        fprintf(stderr, "%s <port> [root path] [cert file] [key file]\n",
                argv[0]);
        exit(1);
    }

    signal(SIGINT, sig_handler);

    unsigned short port = atoi(argv[1]);

    if (port == 0)
    {
        fprintf(stderr, "invalid listen port %s\n", argv[1]);
        exit(1);
    }

    const char *root = (argc >= 3 ? argv[2] : ".");
    auto &&proc = std::bind(process, std::placeholders::_1, root);
    WFHttpServer server(proc);
    int ret;

    if (argc == 5)
        ret = server.start(port, argv[3], argv[4]); /* https server */
    else
        ret = server.start(port);

    if (ret == 0)
    {
        wait_group.wait();
        server.stop();
    }
    else
    {
        perror("start server");
        exit(1);
    }

    return 0;
}
