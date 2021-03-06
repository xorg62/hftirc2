/*
 * hftirc2 by Martin Duquesnoy <xorg62@gmail.com>
 * For license, see COPYING
 */

#include <unistd.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <ctype.h>

#include "hftirc.h"
#include "ui.h"
#include "irc.h"
#include "event.h"
#include "util.h"

static void irc_manage_event(struct session *s, int plen);

const static struct
{
     const char cmd[32];
     int len;
     void (*func)(struct session *, int, const char*, const char**, int);
} event_list[] =
{
     { "PING",    4, event_ping },
     // { "QUIT",    4, event_quit },
     { "JOIN",    4, event_join },
     // { "PART",    4, event_part },
     //{ "INVITE",  6, event_invit },
     //{ "TOPIC",   5, event_topic },
     //{ "KICK",    4, event_kick },
     //{ "NICK",    4, event_nick },
     //{ "MODE",    4, event_mode },
     { "PRIVMSG", 7, event_privmsg },
     //{ "NOTICE",  6, event_notice },
     //{ "ERROR",   5, event_error }
};

int
irc_send_raw(struct session *s, const char *format, ...)
{
     char buf[BUFSIZE];
     va_list va_alist;

     if(!s->sock)
          return 1;

     va_start(va_alist, format);
     vsnprintf(buf, sizeof(buf), format, va_alist);
     va_end(va_alist);

     strcat(buf, "\r\n");
     send(s->sock, buf, strlen(buf), 0);

     return 0;
}

int
irc_connect(struct session *s)
{
     struct hostent *hp;
     struct sockaddr_in a;

     if(!(hp = gethostbyname(s->info->server)))
     {
          ui_print_buf(STATUS_BUFFER, "[HFTIrc] Cannot resolve host '%s'", s->info->server);
          return 1;
     }

     a.sin_family = AF_INET;
     a.sin_port   = htons(s->info->port ? s->info->port : 6667);

     memcpy(&a.sin_addr, hp->h_addr_list[0], (size_t)hp->h_length);

     if((s->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
     {
          ui_print_buf(STATUS_BUFFER, "[HFTIrc] Cannot create socket");
          return 1;
     }

     //fcntl(s->sock, F_SETFL, O_NONBLOCK);

     if(connect(s->sock, (const struct sockaddr*)&a, sizeof(a)) < 0)
     {
          ui_print_buf(STATUS_BUFFER, "[HFTIrc] Cannot connect to %s\n", s->info->server);
          return 1;
     }

     /* Identification */
     irc_send_raw(s, "NICK %s", s->info->nick);
     irc_send_raw(s, "USER %s localhost %s :%s", s->info->username, s->info->server, s->info->realname);
     irc_send_raw(s, "PASS %s", s->info->password);

     s->flags |= SESSION_CONNECTED;

     return 0;
}

void
irc_disconnect(struct session *s)
{
     if(s->sock >= 0)
          close(s->sock);

     s->flags &= ~SESSION_CONNECTED;

     ui_print_buf(STATUS_BUFFER, "[HFTIrc] Session '%s' disconnected", s->info->name);
}

int
irc_process(struct session *s, fd_set *inset)
{
     int i, len, offset;
     unsigned int amount;

     if(!(s->flags & SESSION_CONNECTED))
     {
          ui_print_buf(STATUS_BUFFER, "[HFTIrc] Server '%s' disconnected", s->info->server);
          return 1;
     }

     if(FD_ISSET(s->sock, inset))
     {
          amount = (sizeof(s->inbuf) - 1) - s->inoffset;

          if((len = recv(s->sock, s->inbuf + s->inoffset, amount, 0)) <= 0)
          {
               ui_print_buf(STATUS_BUFFER, "[HFTIrc] Session '%s' failed recv(ernno = %d)", s->info->name, errno);
               return 1;
          }

          s->inoffset += len;

          do
          {
               for(i = offset = 0; i < ((int)(s->inoffset) - 1); ++i)
                    if(s->inbuf[i] == '\r' && s->inbuf[i + 1] == '\n')
                    {
                         offset = i + 2;
                         break;
                    }

               irc_manage_event(s, offset - 2);

               if(s->inoffset - offset > 0)
                    memmove(s->inbuf, s->inbuf + offset, s->inoffset - offset);

               s->inoffset -= offset;
          }
          while(offset > 0);

          s->last_response = time(NULL);
     }
     else
     {
          if(time(NULL) - s->last_response >= TIMEOUT_IRC)
          {
               ui_print_buf(STATUS_BUFFER, "[HFTIrc] Session '%s' timed out", s->info->name);
               return 1;
          }
          irc_send_raw(s, "PING %s", s->info->server);
     }

     return 0;
}

void
irc_parse_in(char *buf,
             const char *prefix,
             const char *command,
             const char **params,
             int *code,
             int *paramindex)
{
     char *p = buf;
     char *s = NULL;

     /* Parse prefix */
     if(buf[0] == ':')
     {
          for(; *p && *p != ' '; ++p);
          *p++ = '\0';
          strcpy((char *)prefix, buf + 1);
     }

     /* Parse command */
     if(isdigit((int)p[0])
        && isdigit((int)p[1])
        && isdigit((int)p[2]))
     {
          p[3] = '\0';
          *code = atoi (p);
          p += 4;
     }
     else
     {
          for(s = p; *p && *p != ' '; ++p);
          *p++ = '\0';
          strcpy((char *)command, s);
     }

     /* Parse params */
     for(;*p && *paramindex < 10; *p++ = '\0')
     {
          if(*p == ':')
          {
               params[(*paramindex)++] = p + 1;
               break;
          }

          for(s = p; *p && *p != ' '; ++p);

          params[(*paramindex)++] = s;

          if(*p == '\0')
               break;
     }
}

static void
irc_manage_event(struct session *s, int plen)
{
     char buf[BUFSIZE];
     const char command[BUFSIZE] = { 0 };
     const char prefix[BUFSIZE] = { 0 };
     const char *params[11];
     int i, code = 0, paramindex = 0;
     bool managed_event = false;

     if(plen > sizeof(buf))
          return;

     memcpy(buf, s->inbuf, plen);
     buf[plen] = '\0';
     memset((char*)params, 0, sizeof(params));

     /*
      * \_0< QUACK!
      *~~~~~~~~~~~~~
      *  ~    ~   >0_/
      */
     irc_parse_in(buf, prefix, command, params, &code, &paramindex);

     if(code)
     {
          if(!(s->flags & SESSION_MOTD) && (code == 376 || code == 422))
          {
               s->flags |= SESSION_MOTD;
               //event_connect(s, code, prefix, params, paramindex);
          }

          //event_numeric(s, code, prefix, params, paramindex);
     }

     for(i = 0; i < LEN(event_list); ++i)
     {
          if(!strncmp(event_list[i].cmd, command, event_list[i].len))
          {
               event_list[i].func(s, code, prefix, params, paramindex);
               managed_event = true;
               break;
          }
     }

     if(!managed_event)
          event_dump(s, code, prefix, params, paramindex);
}
