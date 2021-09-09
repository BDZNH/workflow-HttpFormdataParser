#include "HttpFormdataParser.h"
#include "workflow/WFHttpServer.h"
#include "workflow/HttpUtil.h"

namespace protocol
{

    size_t *get_next(const char *content, size_t *next, size_t len)
    {
        if (!next)
        {
            fprintf(stderr, "out of memory, can't malloc next array\n");
            return nullptr;
        }
        next[0] = NOT_FOUND;
        size_t j = 0;
        size_t k = NOT_FOUND;
        while (j < len - 1)
        {
            if (k == NOT_FOUND || content[j] == content[k])
            {
                next[++j] = ++k;
            }
            else
            {
                k = next[k];
            }
        }
        return next;
    }

    /**
 * data 主串(可能有非 Ascii 字符), content 需要比较的字符串， size data 的大小, offset 相对于 data 的偏移量，len 比较的数量
 */

    size_t find(const char *data, size_t size, const char *content, size_t offset, size_t len)
    {
        size_t *next = new size_t[strlen(content)];
        next = get_next(content, next, strlen(content));
        if (next == nullptr)
        {
            fprintf(stderr, "out of memory\n");
            return NOT_FOUND;
        }

        size_t i = offset;
        size_t j = 0;

        size_t contentlen = strlen(content);
        while (i <= (offset + len) && i < size && (j < contentlen || j == NOT_FOUND))
        {
            if (j == NOT_FOUND || data[i] == content[j])
            {
                i++;
                j++;
            }
            else
            {
                j = next[j];
            }
        }
        delete[] next;
        if (j == contentlen)
        {
            return i - j;
        }
        else
        {
            return -1;
        }
    }

    HttpFormdataParser::HttpFormdataParser()
    {
        offset = 0;
        next = nullptr;
        part_start = 0;
        part_len = 0;
    }

    HttpFormdataParser::HttpFormdataParser(const protocol::HttpRequest *req)
    {
        parse(req);
    }

    HttpFormdataParser::~HttpFormdataParser()
    {
        if (next != nullptr)
        {
            delete[] next;
        }
    }

    bool HttpFormdataParser::parse_boundary(std::string &content_type)
    {
        if (content_type.find("multipart/form-data") == std::string::npos || content_type.find("boundary=") == std::string::npos)
        {
            //没有找到 multipart/form-data(application/x-www-form-urlencoded 先不做) 或者 boundary=
            return false;
        }

        size_t boundary_header_len = strlen("boundary=");
        size_t boundary_index = content_type.find("boundary=");

        size_t real_boundary_index = boundary_index + boundary_header_len;

        this->boundary = content_type.substr(real_boundary_index, content_type.size());
        boundary = "--" + boundary;

        generate_kmp_next_array();
        return true;
    }

    bool HttpFormdataParser::parse(const protocol::HttpRequest *req)
    {
        // 获取 content-type 和 boundary
        protocol::HttpHeaderCursor cursor(req);

        std::string content_type;
        if (!cursor.find("Content-Type", content_type))
        {
            return false;
        }

        if (!parse_boundary(content_type))
        {
            return false;
        }

        const void *data;
        req->get_parsed_body(&data, &body_size);

        while (!this->at_body_end())
        {
            parse_part(data);
        }
        return true;
    }

    void HttpFormdataParser::generate_kmp_next_array()
    {
        this->next = new int[this->boundary.size()];
        next[0] = -1;
        int j = 0;
        int k = -1;
        int len = boundary.size();
        while (j < len - 1)
        {
            if (k == -1 || boundary.at(j) == boundary.at(k))
            {
                next[++j] = ++k;
            }
            else
            {
                k = next[k];
            }
        }
    }

    bool HttpFormdataParser::at_body_end()
    {
        return (offset + boundary.size() + 2) >= body_size;
    }

    // 这里就是整个 formdata parser 的核心了，从 body 里面找到所有的 boundary ，分成一个一个的 part
    // 找到 name 字段 filename 字段(有的话) 以及这个 part 的内容
    void HttpFormdataParser::parse_part(const void *data)
    {
        part_start = find_next_boundary((const char *)data, offset);
        if (part_start == NOT_FOUND)
        {
            offset = body_size;
            return;
        }

        size_t part_end = find_next_boundary((const char *)data, offset + boundary.size());

        if (part_end == NOT_FOUND)
        {
            offset = body_size;
            return;
        }

        part_len = part_end - part_start;

        HttpFormdata tmpdata;

        // 找到每一个 part 的 name
        size_t name_start = find((const char *)data, body_size, "name=\"", part_start, part_len);
        name_start += strlen("name=\"");
        size_t name_end = find((const char *)data, body_size, "\"", name_start, part_len - (name_start - part_start));
        tmpdata.name = (const char *)data + name_start;
        tmpdata.name_len = name_end - name_start;

        //如果是一个文件
        size_t filename_start = find((const char *)data, body_size, "filename=\"", part_start, part_len);
        if (filename_start != NOT_FOUND)
        {
            filename_start += strlen("filename=\"");
            name_end = find((const char *)data, body_size, "\"", filename_start, part_len - (filename_start - part_start));
            tmpdata.file_name = (const char *)data + filename_start;
            tmpdata.file_name_len = name_end - filename_start;
        }

        // 从当前位置找到连续两个 \r\n ，也就是这个part的内容开始的地方
        name_start = find((char *)data, body_size, "\r\n\r\n", name_start, part_len - (name_start - part_start));
        name_start += 4;         //
        name_end = part_end - 2; // part_end 减去一个 \r\n 一共两字节，也就是这个 part 内容结束的地方

        tmpdata.value = (const char *)data + name_start;
        tmpdata.value_len = name_end - name_start;

        this->formdata.push_back(std::move(tmpdata));
        offset = part_end;
        part_len = 0;
    }

    size_t HttpFormdataParser::find_next_line(const char *data, size_t _offset, size_t len)
    {
        return find(data, body_size, "\r\n", _offset, len);
    }

    size_t HttpFormdataParser::find_next_boundary(const char *data, size_t _offset)
    {
        size_t i = _offset;
        size_t j = 0;
        size_t boundary_len = boundary.size();
        while (i < body_size && (j < boundary_len || j == NOT_FOUND))
        {
            if (j == NOT_FOUND || data[i] == boundary[j])
            {
                i++;
                j++;
            }
            else
            {
                j = next[j];
            }
        }

        if (j == boundary_len)
        {
            return i - j;
        }
        else
        {
            return NOT_FOUND;
        }
    }

    size_t HttpFormdataParser::get_part_size()
    {
        return this->formdata.size();
    }

    HttpFormdataCursor::HttpFormdataCursor(HttpFormdataParser *_parser)
    {
        parser = _parser;
    }

    void HttpFormdataCursor::reset_index()
    {
        index = 0;
    }

    bool HttpFormdataCursor::next(std::string &name)
    {
        if (index >= parser->formdata.size())
        {
            return false;
        }
        const HttpFormdata &part = get(index);
        name = std::string(part.name, part.name_len);
        index++;
        return true;
    }

    bool HttpFormdataCursor::is_file(const std::string &name, std::string &file_name)
    {
        size_t size = parser->get_part_size();
        for (size_t i = 0; i < size; i++)
        {
            const HttpFormdata &part = get(i);
            if (name.size() != part.name_len)
            {
                continue;
            }
            else if ((strncmp(part.name, name.c_str(), part.name_len) == 0) && part.file_name != nullptr)
            {
                file_name = std::string(part.file_name, part.file_name_len);
                return true;
            }
        }
        return false;
    }

    const HttpFormdata &HttpFormdataCursor::get(size_t i)
    {
        return parser->formdata.at(i);
    }

    bool HttpFormdataCursor::get_content(const std::string &name, const void **data, size_t *size)
    {
        size_t len = parser->get_part_size();
        for (size_t i = 0; i < len; i++)
        {
            const HttpFormdata &part = get(i);
            if (name.size() != part.name_len)
            {
                continue;
            }
            else if ((strncmp(part.name, name.c_str(), part.name_len) == 0) && part.value != nullptr)
            {
                *data = part.value;
                *size = part.value_len;
                return true;
            }
        }
        return false;
    }

    bool HttpFormdataCursor::get_string(const std::string &name, std::string &value)
    {
        const void *data;
        size_t size;
        if (this->get_content(name, &data, &size))
        {
            value = std::string((char *)data, size);
            return true;
        }
        return false;
    }

} // namespace protocal
