/*
    This file is part of RIBS (Robust Infrastructure for Backend Systems).
    RIBS is an infrastructure for building great SaaS applications (but not
    limited to).

    Copyright (C) 2011 Adap.tv, Inc.

    RIBS is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, version 2.1 of the License.

    RIBS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with RIBS.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _HTTP_SERVER__H_
#define _HTTP_SERVER__H_

#include "basic_epoll_event.h"
#include "vmbuf.h"
#include "sstr.h"
#include "URI.h"
#include "mime_types.h"
#include "epoll.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>

SSTREXTRN(HTTP_SERVER_VER);
SSTREXTRN(SERVER_NAME);
SSTREXTRN(CONNECTION);
SSTREXTRN(CONNECTION_KEEPALIVE);
SSTREXTRN(CONNECTION_CLOSE);
SSTREXTRN(CONTENT_LENGTH);
SSTREXTRN(COOKIE_VERSION);
SSTREXTRN(HTTP_SET_COOKIE);
SSTREXTRN(HTTP_LOCATION);
SSTREXTRN(CONTENT_ENCODING);
SSTREXTRN(CONTENT_ENCODING_GZIP);

struct http_server : basic_epoll_event
{
    http_server();

    void reset();
    
    struct basic_epoll_event *close();
    
    struct basic_epoll_event *startWrite();
    
    void headerStart(const char *status, const char *content_type);
    void headerStartMinimal(const char *status);
    void headerStartMime(const char *status, const char *file);
    struct basic_epoll_event *headerClose();
    void headerContentLength(size_t len);
    void headerContentLength();
    void setSessionCookie(const char *name, const char *value);
    void setCookie(const char *name, const char *value, uint32_t max_age);
    void setCookie(const char *name, const char *value, uint32_t max_age, const char *domain, const char *path);

    void beginCookie(const char *name);
    void endCookie(uint32_t max_age, const char *domain, const char *path);
    void endCookieOld(time_t expires, const char *domain, const char *path);
    
    struct basic_epoll_event *response(const char *status, const char *content_type);
    struct basic_epoll_event *response(const char *status, const char *content_type, const char *format, ...);

    struct basic_epoll_event *setRedirect(const char *status, const char *content_type, const char *format, ...);
    
    const char *http_method();
    
    void checkPersistent(char *p);

    struct basic_epoll_event *onInit();
    struct basic_epoll_event *onRead();
    struct basic_epoll_event *onWrite();
    struct basic_epoll_event *onWriteDone();
    struct basic_epoll_event *onWriteNext();

    int sendFile(http_server *server);
    
    struct basic_epoll_event *process_request();

    static void set_max_req_size(uint32_t s);

    void suspend_events();
    void resume_events();
    
    vmbuf inbuf;
    vmbuf header;
    vmbuf payload;
    char *URI;
    char *query;
    char *content;
    char *headers;
    size_t content_length;
    size_t eoh;
    http_server *next;
    bool persistent;

    struct basic_epoll_event_method_0args callback;
    static uint32_t max_req_size;
};

inline void http_server::reset()
{
    inbuf.init();
    header.init();
    payload.init();
    content_length = 0;
    eoh = 0;
    next = NULL;
    persistent = false;
}

inline struct basic_epoll_event *http_server::close()
{
    method.set(&http_server::onInit);
    ::close(fd);
    return NULL;
}

inline struct basic_epoll_event *http_server::startWrite()
{
    method.set(&http_server::onWrite);
    int option = 1;
    if (0 > setsockopt(fd, IPPROTO_TCP, TCP_CORK, &option, sizeof(option)))
        perror("TCP_CORK set");
    return this;
}

inline void http_server::headerStart(const char *status, const char *content_type)
{
    header.sprintf("%s %s\r\nServer: %s\r\nContent-Type: %s", HTTP_SERVER_VER, status, SERVER_NAME, content_type);
    header.sprintf("%s%s", CONNECTION, persistent ? CONNECTION_KEEPALIVE : CONNECTION_CLOSE);
}

inline void http_server::headerStartMinimal(const char *status)
{
    header.sprintf("%s %s\r\nServer: %s", HTTP_SERVER_VER, status, SERVER_NAME);
    header.sprintf("%s%s", CONNECTION, persistent ? CONNECTION_KEEPALIVE : CONNECTION_CLOSE);
}


inline void http_server::headerStartMime(const char *status, const char *file)
{
    headerStart(status, mime_types::instance()->mime_type(file));
}

inline void http_server::headerContentLength(size_t len)
{
    header.sprintf("%s%zu", CONTENT_LENGTH, len);
}

inline void http_server::headerContentLength()
{
    size_t total = payload.wlocpos();
    http_server *p = next;
    while (p)
    {
        struct stat *st = (struct stat *)p->inbuf.data();
        total += st->st_size;
        p = p->next;
    }
    headerContentLength(total);
}

inline void http_server::setSessionCookie(const char *name, const char *value)
{
    header.sprintf("%s%s=\"%s\"; %s", HTTP_SET_COOKIE, name, value, COOKIE_VERSION);
}

inline void http_server::setCookie(const char *name, const char *value, uint32_t max_age)
{
    header.sprintf("%s%s=\"%s\"; Max-Age=%u; %s", HTTP_SET_COOKIE, name, value, max_age, COOKIE_VERSION);
}

inline void http_server::setCookie(const char *name, const char *value, uint32_t max_age, const char *domain, const char *path)
{
    header.sprintf("%s%s=\"%s\"; Max-Age=%u; Domain=\"%s\"; Path=\"%s\"; %s", HTTP_SET_COOKIE, name, value, max_age, domain, path, COOKIE_VERSION);
}

inline void http_server::beginCookie(const char *name)
{
    header.sprintf("%s%s=\"", HTTP_SET_COOKIE, name);
}

inline void http_server::endCookie(uint32_t max_age, const char *domain, const char *path)
{
    header.sprintf("\";Max-Age=%u;Domain=\"%s\";Path=\"%s\";%s", max_age, domain, path, COOKIE_VERSION);
}

inline void http_server::endCookieOld(time_t expires, const char *domain, const char *path)
{
    char buf[1024];
    struct tm tm;
    gmtime_r(&expires, &tm);
    strftime(buf, sizeof(buf), "%a, %d-%b-%Y %H:%M:%S %Z", &tm);
    header.sprintf("\";Path=%s;Domain=%s;Expires=%s", path, domain, buf);
}

inline struct basic_epoll_event *http_server::response(const char *status, const char *content_type)
{
    headerStart(status, content_type);
    headerContentLength(payload.wlocpos());
    return headerClose();
}

inline struct basic_epoll_event *http_server::response(const char *status, const char *content_type, const char *format, ...)
{
    header.reset();
    headerStart(status, content_type);
    payload.reset();
    va_list ap;
    va_start(ap, format);
    payload.vsprintf(format, ap);
    va_end(ap);
    headerContentLength(payload.wlocpos());
    return headerClose();
}

inline struct basic_epoll_event *http_server::setRedirect(const char *status, const char *content_type, const char *format, ...)
{
    header.reset();
    headerStart(status, content_type);
    payload.reset();
    header.strcpy(HTTP_LOCATION);
    va_list ap;
    va_start(ap, format);
    header.vsprintf(format, ap);
    va_end(ap);
    headerContentLength(payload.wlocpos());
    return headerClose();
}

inline const char *http_server::http_method()
{
    return inbuf.data();
}

inline struct basic_epoll_event *http_server::process_request()
{
    query = strchrnul(URI, '?');
    if (*query)
    {
        *query = 0;
        ++query;
    }
    const char HTTP[] = "http://";
    if (0 == SSTRNCMP(HTTP, URI)) // is absolute URI?
    {
        URI += SSTRLEN(HTTP);
        URI = strchrnul(URI, '/');
    }
    return callback.invoke(this);
}

/* static */
inline void http_server::set_max_req_size(uint32_t s)
{
    max_req_size = s;
}

inline void http_server::suspend_events()
{
    epoll::del(this);
}

inline void http_server::resume_events()
{
    epoll::add(this, EPOLLET | EPOLLIN | EPOLLOUT);
}


#endif // _HTTP_SERVER__H_
