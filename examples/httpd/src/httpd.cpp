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
#include "acceptor.h"
#include "class_factory.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "vmbuf.h"
#include "sstr.h"
#include "http_common.h"
#include "http_server.h"
#include "mime_types.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <getopt.h>
#include "logger.h"
#include "daemon.h"
#include "vmpool.h"

#define LISTEN_BACKLOG 32768

static struct epoll_server_event_array events_array;

struct MyServer : http_server
{
    //static __thread vmpool_op<server_epoll_event> pool;
    static int init_per_thread()
    {
        /*
        static __thread vmpool<MyServer> *myserver_pool = NULL;
        myserver_pool = new vmpool<MyServer>;
        pool = myserver_pool->get_op<server_epoll_event>();
        acceptor.init_per_thread(pool);
        */
        acceptor.init_per_thread();
        return 0;
    }

    void handle_accept()
    {
        //printf("accept: %d\n", fd);
    }
    
    struct basic_epoll_event *handle_request()
    {
        /*
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        char addr_str[INET_ADDRSTRLEN];
        const char *addrp = "-";
        if (0 == getpeername(fd, (struct sockaddr*)&addr, &len) &&
            NULL != inet_ntop(AF_INET, &addr.sin_addr, addr_str, INET_ADDRSTRLEN))
        {
            addrp = addr_str;
        }
        LOGGER_INFO("%s %s%s%s", addrp, URI, *query ? "?" : "", query);
        */
        URI::decode(URI);
        const char *file = (URI[1] == 0 ? "." : URI + 1);
        int ffd = open(file, O_RDONLY);
        if (0 > ffd)
            return response(HTTP_STATUS_404, HTTP_CONTENT_TYPE_TEXT_PLAIN);

        /*
        http_server *h = (http_server *)pool.get();
        h->fd = ffd;
        */
        http_server *h = (http_server *)events_array.get(ffd);
        if (0 > h->sendFile(this))
        {
            struct stat st;
            int res = fstat(ffd, &st);
            //pool.put(h);
            ::close(ffd);
            if (res == 0 && S_ISDIR(st.st_mode) && 0 == generateDirList(file))
            {
                return response(HTTP_STATUS_200, HTTP_CONTENT_TYPE_TEXT_HTML);
            } else
                return response(HTTP_STATUS_500, HTTP_CONTENT_TYPE_TEXT_PLAIN);
        }
        headerStartMime(HTTP_STATUS_200, file);
        headerContentLength();
        return headerClose();
    }


    int generateDirList(const char *dir)
    {
        payload.sprintf("<html><head><title>Index of %s</title></head>", dir);
        payload.strcpy("<body>");
        payload.sprintf("<h1>Index of %s</h1><hr>", dir);

        payload.sprintf("<a href=\"..\">../</a><br><br>");
        payload.sprintf("<table width=\"100%\" border=\"0\">");
        DIR *d = opendir(dir);
        int error = 0;
        if (d)
        {
            struct dirent de, *dep;
            while (0 == readdir_r(d, &de, &dep) && dep)
            {
                if (de.d_name[0] == '.')
                    continue;
                struct stat st;
                if (0 > fstatat(dirfd(d), de.d_name, &st, 0))
                {
                    payload.sprintf("<tr><td>ERROR: %s</td><td>N/A</td></tr>", de.d_name);
                    continue;
                }
                const char *slash = (S_ISDIR(st.st_mode) ? "/" : "");
                struct tm t_res, *t;
                t = localtime_r(&st.st_mtime, &t_res);

                payload.sprintf("<tr>");
                payload.sprintf("<td><a href=\"%s%s%s\">%s%s</a></td>", URI, de.d_name, slash, de.d_name, slash);
                payload.sprintf("<td>");
                if (t)
                    payload.strftime("%F %T", t);
                payload.sprintf("</td>");
                payload.sprintf("<td>%zu</td>", st.st_size);
                payload.sprintf("</tr>");
           
            }
            closedir(d);
        }
        payload.strcpy("</table><hr></body>");
        return error;
    }
    
    static struct acceptor acceptor;
};
//__thread vmpool_op<server_epoll_event> MyServer::pool;

struct acceptor MyServer::acceptor;

int init_signals();

static void usage(char *arg0)
{
    printf("\nhttpd version 1.0\nWritten by Lior Amram, Nov 2010\n\n");
    printf("usage: %s [-d|--daemon] [-p|--pidfile <file>]\n", arg0);
    printf("       %*c [-l|--logfile <file or |pipe>]\n", (int)strlen(arg0), ' ');
    printf("       %*c [-P|--port <port #>]\n", (int)strlen(arg0), ' ');
    printf("       %*c [-t|--timeout <# of seconds>]\n", (int)strlen(arg0), ' ');
    printf("       %*c [--help]\n", (int)strlen(arg0), ' ');
    printf("\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"daemon", 0, 0, 'd'},
        {"pidfile", 1, 0, 'p'},
        {"logfile", 1, 0, 'l'},
        {"port", 1, 0, 'P'},
        {"timeout", 1, 0, 't'},
        {"help", 0, 0, 1},
        {0, 0, 0, 0}
    };

    int daemon_mode = 0;
    const char *pidfile = "httpd.pid";
    const char *logfile = "httpd.log";
    int port = 8080;
    int timeout = 30;
    
    while (1)
    {
        int option_index = 0;
        int c = getopt_long(argc, argv, "dp:l:P:t:", long_options, &option_index);
        if (c == -1)
            break;
        switch (c)
        {
        case 'd':
            daemon_mode = 1;
            break;
        case 'p':
            pidfile = optarg;
            break;
        case 'l':
            logfile = optarg;
            break;
        case 'P':
            port = atoi(optarg);
            break;
        case 't':
            timeout = atoi(optarg);
            break;
        case 1:
            usage(argv[0]);
            break;
        default:
            usage(argv[0]);
        }
    }
    logger::log("starting...");
    
    if (daemon_mode)
        daemon::start(NULL, pidfile, logfile);

    epoll::set_per_thread_callback(MyServer::init_per_thread);
    mime_types::instance()->load();
    if (0 > epoll::init(timeout, timeout))
        abort();
    
    events_array.init(class_factory<MyServer>::create);
    if (0 > MyServer::acceptor.init(-1, port, LISTEN_BACKLOG, &events_array))
        abort();
    MyServer::acceptor.callback.set(&MyServer::handle_request);
    MyServer::acceptor.accept_callback.set(&MyServer::handle_accept);
    
    epoll::start();
    daemon::finish();
    return 0;
}
