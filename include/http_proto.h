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
#ifndef _HTTP_PROTO__H_
#define _HTTP_PROTO__H_

#include "basic_epoll_event.h"
#include <unistd.h>
#include "vmbuf.h"
#include "epoll.h"
#include "http_common.h"
#include "compact_hashtable.h"
#include <netinet/in.h>

struct http_proto
{
    static int prepare(tcp_client<http_proto> *client);
    static int read_content(tcp_client<http_proto> *client);
    static void on_error(tcp_client<http_proto> *client);
    static void on_connection_close(tcp_client<http_proto> *client);
    int chunk_size(tcp_client<http_proto> *client);
    static int handle_chunks(tcp_client<http_proto> *client);

    uint32_t eoh;
    uint32_t chunk_start;
    uint32_t chunk_end;
    int chunked;
    
    struct chunk
    {
        uint32_t loc;
        uint32_t size;
        chunk() : loc(0), size(0) {}
    };
    
    static int next_chunk(tcp_client<http_proto> *client, chunk *chunk);
    static char *get_chunk(tcp_client<http_proto> *client, chunk *chunk) { return client->inbuf.data() + chunk->loc; }
    uint32_t get_chunk_size(chunk *chunk) { return chunk->size; }
};


/*static*/
inline int http_proto::prepare(tcp_client<http_proto> *client)
{
    client->proto.eoh = 0;
    client->proto.chunked = -1;
    client->proto.chunk_start = 0;
    return 0;
}

/*static*/
inline void http_proto::on_error(tcp_client<http_proto> *client)
{
    client->proto.eoh = 0; // we use eoh == 0 to detect problems
}

/*static*/
inline void http_proto::on_connection_close(tcp_client<http_proto> *client)
{
    if (0 < http_proto::read_content(client) && client->proto.eoh > 0 && client->proto.chunked >= 0)
        client->proto.eoh = 0; // error or partial response. we use eoh == 0 to detect problems
}

/*static*/
inline int http_proto::read_content(tcp_client<http_proto> *client)
{
    SSTRL(CRLF, "\r\n");
    SSTRL(CRLFCRLF, "\r\n\r\n");
    SSTRL(CONNECTION, "\r\nConnection: ");
    SSTRL(CONNECTION_CLOSE, "close");
    SSTRL(CONTENT_LENGTH, "\r\nContent-Length: ");
    SSTRL(TRANSFER_ENCODING, "\r\nTransfer-Encoding: ");
    
    if (0 == client->proto.eoh) // first time case
    {
        *client->inbuf.wloc() = 0;
        // do we have the header?
        char * eohp = strstr(client->inbuf.data(), CRLFCRLF);
        if (NULL != eohp) // we have the entire header
        {
            client->proto.eoh = eohp - client->inbuf.data() + SSTRLEN(CRLFCRLF);
            client->proto.chunk_start = client->proto.eoh;
            *eohp = 0; // terminate

            char *p = strstr(client->inbuf.data(), CONNECTION);
            if (p != NULL)
            {
                p += SSTRLEN(CONNECTION);
                client->persistent = (0 == SSTRNCMPI(CONNECTION_CLOSE, p) ? 0 : 1);
            }

            // is 204 or 304?
            SSTRL(HTTP, "HTTP/");
            if (0 != SSTRNCMP(HTTP, client->inbuf.data()))
            {
                client->proto.eoh = 0;
                return 0;
            }
            p = strchrnul(client->inbuf.data(), ' ');
            int code = (*p ? atoi(p + 1) : 0);
            if (code == 204 || code == 304)
                return 0;
            if (code == 0)
            {
                client->proto.eoh = 0;
                client->persistent = 0;
                return 0;
            }
            
            char *content_len_str = strstr(client->inbuf.data(), CONTENT_LENGTH);
            if (NULL != content_len_str)
            {
                client->proto.chunked = 0;
                content_len_str += SSTRLEN(CONTENT_LENGTH);
                client->proto.chunk_end = client->proto.chunk_start + atoi(content_len_str);
            } else
            {
                char *transfer_encoding_str = strstr(client->inbuf.data(), TRANSFER_ENCODING);
                if (NULL != transfer_encoding_str &&
                    0 == SSTRNCMP(transfer_encoding_str + SSTRLEN(TRANSFER_ENCODING), "chunked"))
                {
                    client->proto.chunked = 1;
                    client->proto.chunk_end = 0;
                } else
                    client->proto.chunked = -1;
            }
            
            *eohp = CRLF[0]; // restore
        } else
            return 1; // no header yet, yield
    }

    // manage state
    switch (client->proto.chunked)
    {
    case -1:
        return 1; // chunk size unknown, always yield
    case 0:
        if (client->proto.chunk_end <= client->inbuf.wlocpos())
            return 0; // we are done
        break;
    case 1:
        return http_proto::handle_chunks(client);
    }
    return 1; // yield
}

/*static*/
inline int http_proto::handle_chunks(tcp_client<http_proto> *client)
{
    SSTRL(CRLF, "\r\n");
    
    for (;;)
    {
        if (client->proto.chunk_size(client) > 0) // do we have the chunk size?
            break;
        
        if (client->proto.chunk_end <= client->inbuf.wlocpos())
        {
            if (client->proto.chunk_start + SSTRLEN(CRLF) == client->proto.chunk_end) // no more chunks
                return 0; // we are done
            client->proto.chunk_start = client->proto.chunk_end;
            client->proto.chunk_end = 0;
            continue; // next chunk
        }
        break;
    }
    return 1; // not in last chunk yet, yield
}

inline int http_proto::chunk_size(tcp_client<http_proto> *client)
{
    SSTRL(CRLF, "\r\n");
    
    if (0 != chunk_end)
        return 0;  // already reading a chunk
    *client->inbuf.wloc() = 0;
    char *the_size = client->inbuf.data() + chunk_start;
    char *p = strchrnul(the_size, '\r');
    if (*p)
    {
        uint32_t s = strtoul(the_size, NULL, 16);
        chunk_start = p - client->inbuf.data() + SSTRLEN(CRLF);
        chunk_end = chunk_start + s +  SSTRLEN(CRLF);
    } else
        return 1; // wait for more data
    return 0;
}

inline int http_proto::next_chunk(tcp_client<http_proto> *client, chunk *chunk)
{
    SSTRL(CRLF, "\r\n");

    uint32_t s, loc;
    
    switch (client->proto.chunked)
    {
    case -1:
        s = client->inbuf.wlocpos() - client->proto.eoh;
        break;
    case 0:
        s = client->proto.chunk_end - client->proto.chunk_start;
        break;
    case 1:
        if (0 == chunk->loc)
            loc = client->proto.eoh;
        else
            loc = chunk->loc + chunk->size + SSTRLEN(CRLF);
        {
            char *p = client->inbuf.data() + loc;
            char *ep;
            chunk->size = strtoul(p, &ep, 16);
            loc = ep - client->inbuf.data() + SSTRLEN(CRLF);
            chunk->loc = loc;
        }
        return chunk->size == 0 ? -1 : 0;
    default:
        s = 0;
        break;
    }
    if (0 == chunk->loc)
    {
        chunk->loc = client->proto.eoh;
        chunk->size = s;
    } else
        return -1;
    
    return 0;
}

#endif // _HTTP_PROTO__H_
