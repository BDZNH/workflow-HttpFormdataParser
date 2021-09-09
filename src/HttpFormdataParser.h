
#ifndef _HTTP_FORMDATA_PARSER_H_
#define _HTTP_FORMDATA_PARSER_H_

#include "workflow/WFHttpServer.h"

#define NOT_FOUND (size_t)(-1)

namespace protocol
{

    struct HttpFormdata
    {
        const char *name = nullptr;
        size_t name_len;
        const char *value = nullptr;
        size_t value_len;
        const char *file_name = nullptr;
        size_t file_name_len;
    };

    class HttpFormdataParser
    {
        friend class HttpFormdataCursor;

    private:
        std::string boundary;
        std::vector<HttpFormdata> formdata;
        size_t body_size;
        size_t offset;
        size_t part_len;
        size_t part_start;
        int *next;

        size_t find_next_line(const char *data, size_t _offset, size_t len);
        size_t find_next_boundary(const char *data, size_t _offset);
        bool parse_boundary(std::string &);
        bool at_body_end();
        bool is_last_boundary(const void *data);
        void generate_kmp_next_array();
        void parse_part(const void *data);

    public:
        HttpFormdataParser();
        HttpFormdataParser(const protocol::HttpRequest *req);
        ~HttpFormdataParser();
        bool parse(const protocol::HttpRequest *);
        size_t get_part_size();
    };

    class HttpFormdataCursor
    {
    private:
        size_t index = 0;
        HttpFormdataParser *parser;

    public:
        HttpFormdataCursor(HttpFormdataParser *_parser);
        bool next(std::string &name);
        bool is_file(const std::string &name, std::string &file_name);
        bool get_content(const std::string &name, const void **data, size_t *size);
        bool get_string(const std::string &name, std::string &value);
        const HttpFormdata &get(size_t i);

        void reset_index();
    };
} // namespace protocol
#endif
