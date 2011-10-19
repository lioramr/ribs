#include "http_server.h"
#include <stdlib.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "epoll.h"
#include "logger.h"

/* static*/
uint32_t http_server::max_req_size = -1;

#define MIN_HTTP_REQ_SIZE (5) // method(3) + space(1) + URI(1) + optional VER...

// 1xx
SSTRE(HTTP_STATUS_100, "100 Continue");

// 2xx
SSTRE(HTTP_STATUS_200, "200 OK");

// 3xx
SSTRE(HTTP_STATUS_300, "300 Multiple Choices");
SSTRE(HTTP_STATUS_301, "301 Moved Permanently");
SSTRE(HTTP_STATUS_302, "302 Found");
SSTRE(HTTP_STATUS_303, "303 See Other");
SSTRE(HTTP_STATUS_304, "304 Not Modified");

// 4xx
SSTRE(HTTP_STATUS_400, "400 Bad Request");
SSTRE(HTTP_STATUS_403, "403 Forbidden");
SSTRE(HTTP_STATUS_404, "404 Not Found");
SSTRE(HTTP_STATUS_411, "411 Length Required");
SSTRE(HTTP_STATUS_413, "413 Request Entity Too Large");

// 5xx
SSTRE(HTTP_STATUS_500, "500 Internal Server Error");
SSTRE(HTTP_STATUS_501, "501 Not Implemented");
SSTRE(HTTP_STATUS_503, "503 Service Unavailable");

// content types
SSTRE(HTTP_CONTENT_TYPE_TEXT_PLAIN, "text/plain");
SSTRE(HTTP_CONTENT_TYPE_TEXT_XML, "text/xml");
SSTRE(HTTP_CONTENT_TYPE_TEXT_HTML, "text/html");
SSTRE(HTTP_CONTENT_TYPE_JSON, "application/json");
SSTRE(HTTP_CONTENT_TYPE_IMAGE_GIF, "image/gif");

// internal use
SSTR(HTTP_SERVER_VER, "HTTP/1.1");
SSTR(SERVER_NAME, "adaptv/1.0");
SSTR(CONNECTION, "\r\nConnection: ");
SSTR(CONNECTION_CLOSE, "close"); 
SSTR(CONNECTION_KEEPALIVE, "Keep-Alive");
SSTR(CONTENT_LENGTH, "\r\nContent-Length: ");
SSTR(COOKIE_VERSION, "Version=\"1\"");
SSTR(HTTP_SET_COOKIE, "\r\nSet-Cookie: ");
SSTR(HTTP_LOCATION, "\r\nLocation: ");
SSTR(CONTENT_ENCODING, "\r\nContent-Encoding: ");
SSTR(CONTENT_ENCODING_GZIP, "gzip");

SSTRL(EXPECT_100, "\r\nExpect: 100");

SSTRL(HEAD, "HEAD " );
SSTRL(GET,  "GET "  );
SSTRL(POST, "POST " );
SSTRL(PUT,  "PUT "  );

SSTRL(CRLFCRLF, "\r\n\r\n");
SSTRL(CRLF, "\r\n");

http_server::http_server()
{
    method.set(&http_server::onInit);
}

struct basic_epoll_event *http_server::headerClose()
{
    header.memcpy(CRLFCRLF, SSTRLEN(CRLFCRLF));
    return startWrite();
}

inline void http_server::checkPersistent(char *p)
{
    char *conn = strstr(p, CONNECTION);
    char *h1_1 = strstr(p, " HTTP/1.1");
    // HTTP/1.1
    if ((NULL != h1_1 &&
         (NULL == conn ||
          0 != SSTRNCMPI(CONNECTION_CLOSE, conn + SSTRLEN(CONNECTION)))) ||
        // HTTP/1.0
        (NULL == h1_1 &&
         NULL != conn &&
         0 == SSTRNCMPI(CONNECTION_KEEPALIVE, conn + SSTRLEN(CONNECTION))))
        persistent = true;
    else
        persistent = false;
}

struct basic_epoll_event *http_server::onInit()
{
    reset();
    method.set(&http_server::onRead);
    return this;
}

struct basic_epoll_event *http_server::onRead()
{
    int res = inbuf.read(fd);
    if (0 >= res)
        return this->close(); // remote side closed or other error occured
    if (inbuf.wlocpos() > max_req_size)
        return response(HTTP_STATUS_413, HTTP_CONTENT_TYPE_TEXT_PLAIN);
    // parse http request
    //
    if (inbuf.wlocpos() > MIN_HTTP_REQ_SIZE)
    {
        if (0 == SSTRNCMP(GET, inbuf.data()) || 0 == SSTRNCMP(HEAD, inbuf.data()))
        {
            if (0 == SSTRNCMP(CRLFCRLF,  inbuf.wloc() - SSTRLEN(CRLFCRLF)))
            {
                // GET or HEAD requests
                // make sure the string is \0 terminated
                // this will overwrite the first CR
                *(inbuf.wloc() - SSTRLEN(CRLFCRLF)) = 0;
                char *p = inbuf.data();
                checkPersistent(p);
                URI = strchrnul(p, ' '); // can't be NULL GET and HEAD constants have space at the end
                *URI = 0;
                ++URI; // skip the space
                p = strchrnul(URI, '\r'); // HTTP/1.0
                headers = p;
                if (0 != *headers) // are headers present?
                    headers += SSTRLEN(CRLF); // skip the new line
                *p = 0;
                p = strchrnul(URI, ' '); // truncate the version part
                *p = 0; // \0 at the end of URI
                content = NULL;
                content_length = 0;
                return process_request();
            }
        } else if (0 == SSTRNCMP(POST, inbuf.data()) || 0 == SSTRNCMP(PUT, inbuf.data()))
        {
            // POST
            if (0 == eoh)
            {
                *inbuf.wloc() = 0;
                content = strstr(inbuf.data(), CRLFCRLF);
                if (NULL != content)
                {
                    eoh = content - inbuf.data() + SSTRLEN(CRLFCRLF);
                    *content = 0; // terminate at the first CR like in GET
                    content += SSTRLEN(CRLFCRLF);
                    if (strstr(inbuf.data(), EXPECT_100))
                    {
                        header.sprintf("%s %s%s", HTTP_SERVER_VER, HTTP_STATUS_100, CRLFCRLF);
                        if (0 > header.write(fd))
                            return this->close();
                        header.reset();
                    }
                    checkPersistent(inbuf.data());
                        
                    // parse the content length
                    char *p = strcasestr(inbuf.data(), CONTENT_LENGTH);
                    if (NULL == p)
                        return response(HTTP_STATUS_411, HTTP_CONTENT_TYPE_TEXT_PLAIN);
                        
                    p += SSTRLEN(CONTENT_LENGTH);
                    content_length = atoi(p);
                } else
                    return epoll::yield(epoll::server_timeout_chain, this); // back to epoll, wait for more data
            } else
                content = inbuf.data() + eoh;
                
            if (content + content_length <= inbuf.wloc())
            {
                // POST or PUT requests
                char *p = inbuf.data();
                URI = strchrnul(p, ' '); // can't be NULL PUT and POST constants have space at the end
                *URI = 0;
                ++URI; // skip the space
                p = strchrnul(URI, '\r'); // HTTP/1.0
                headers = p;
                if (0 != *headers) // are headers present?
                    headers += SSTRLEN(CRLF); // skip the new line
                *p = 0;
                p = strchrnul(URI, ' '); // truncate http version
                *p = 0; // \0 at the end of URI
                *(content + content_length) = 0;
                return process_request();
            }
        } else
        {
            return response(HTTP_STATUS_501, HTTP_CONTENT_TYPE_TEXT_PLAIN);
        }
    }
    // wait for more data
    return epoll::yield(epoll::server_timeout_chain, this);
}


struct basic_epoll_event *http_server::onWrite()
{
    vmbuf *buf = &header;
    do
    {
        if (0 == header.ravail())
            buf = &payload;
        int status = buf->write(fd);
        if (0 < status)
            // continue to payload or exit
            continue; 
        else if (0 == status)
            // EAGAIN
            return epoll::yield(epoll::server_timeout_chain, this); 
        else
        {
            // error
            LOGGER_PERROR_STR("onWrite"); 
            this->close();
            return NULL;
        }
    } while (buf == &header);
    if (NULL != next)
    {
        method.set(&http_server::onWriteNext);
        return this;
    }
    else
        return onWriteDone();
}

struct basic_epoll_event *http_server::onWriteDone()
{
    if (!persistent)
        return this->close();
    else
    {
        // TCP_CORK is needed only when not closing the socket
        int option = 0;
        if (0 > setsockopt(fd, IPPROTO_TCP, TCP_CORK, &option, sizeof(option)))
            LOGGER_PERROR_STR("TCP_CORK release");
        return onInit();
    }
}

struct basic_epoll_event *http_server::onWriteNext()
{
    int error = 0;
    while (next)
    {
        if (!error)
        {
            struct stat *st = (struct stat *)next->inbuf.data();
            off_t *ofs = (off_t *)next->header.data();
            
            ssize_t res = sendfile(this->fd, next->fd, ofs, st->st_size - *ofs);
            if (res < 0)
            {
                if (EAGAIN == errno)
                    return epoll::yield(epoll::server_timeout_chain, this);
                else
                {
                    LOGGER_PERROR_STR("sendfile");
                    error = 1;
                    // must continue here to close socket
                }
            } else if (*ofs < st->st_size)
                return epoll::yield(epoll::server_timeout_chain, this);
        }
        http_server *next_tmp = next->next;
        next->close();
        next = next_tmp;
    }
    if (error)
        return this->close();
    return onWriteDone();
}

int http_server::sendFile(http_server *server)
{
    this->reset();
    struct stat *st = (struct stat *)this->inbuf.data();
    off_t *ofs = (off_t *)this->header.data();
    *ofs = 0;
    if (0 > fstat(this->fd, st) || (errno = 0) || !S_ISREG(st->st_mode))
    {
        if (errno)
            LOGGER_PERROR_STR("HttpServer::sendFile fstat");
        return -1;
    }
    server->next = this;
    return 0;
}
