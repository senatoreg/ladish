/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2009 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains implementation of the studio functionality
 * related to loading studio from disk
 **************************************************************************
 *
 * LADI Session Handler is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * LADI Session Handler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LADI Session Handler. If not, see <http://www.gnu.org/licenses/>
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "common.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <expat.h>

#include "escape.h"
#include "studio_internal.h"

#define PARSE_CONTEXT_ROOT                0
#define PARSE_CONTEXT_STUDIO              1
#define PARSE_CONTEXT_JACK                2
#define PARSE_CONTEXT_CONF                3
#define PARSE_CONTEXT_PARAMETER           4
#define PARSE_CONTEXT_CLIENTS             5
#define PARSE_CONTEXT_CLIENT              6
#define PARSE_CONTEXT_PORTS               7
#define PARSE_CONTEXT_PORT                8
#define PARSE_CONTEXT_DICT                9
#define PARSE_CONTEXT_KEY                10

#define MAX_STACK_DEPTH       10
#define MAX_DATA_SIZE         1024

struct parse_context
{
  void * call_ptr;
  XML_Bool error;
  unsigned int element[MAX_STACK_DEPTH];
  signed int depth;
  char data[MAX_DATA_SIZE];
  int data_used;
  char * str;
  ladish_client_handle client;
  ladish_port_handle port;
  ladish_dict_handle dict;
};

#define context_ptr ((struct parse_context *)data)

static void callback_chrdata(void * data, const XML_Char * s, int len)
{
  if (context_ptr->error)
  {
    return;
  }

  if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_PARAMETER ||
      context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_KEY)
  {
    if (context_ptr->data_used + len >= sizeof(context_ptr->data))
    {
      lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "xml parse max char data length reached");
      context_ptr->error = XML_TRUE;
      return;
    }

    memcpy(context_ptr->data + context_ptr->data_used, s, len);
    context_ptr->data_used += len;
  }
}

static void callback_elstart(void * data, const char * el, const char ** attr)
{
  uuid_t uuid;

  if (context_ptr->error)
  {
    return;
  }

  if (context_ptr->depth + 1 >= MAX_STACK_DEPTH)
  {
    lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "xml parse max stack depth reached");
    context_ptr->error = XML_TRUE;
    return;
  }

  if (strcmp(el, "studio") == 0)
  {
    //log_info("<studio>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_STUDIO;
    return;
  }

  if (strcmp(el, "jack") == 0)
  {
    //log_info("<jack>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_JACK;
    return;
  }

  if (strcmp(el, "conf") == 0)
  {
    //log_info("<conf>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_CONF;
    return;
  }

  if (strcmp(el, "parameter") == 0)
  {
    //log_info("<parameter>");
    if ((attr[0] == NULL || attr[2] != NULL) || strcmp(attr[0], "path") != 0)
    {
      lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "<parameter> XML element must contain exactly one attribute, named \"path\"");
      context_ptr->error = XML_TRUE;
      return;
    }

    context_ptr->str = strdup(attr[1]);
    if (context_ptr->str == NULL)
    {
      lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "strdup() failed");
      context_ptr->error = XML_TRUE;
      return;
    }

    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_PARAMETER;
    context_ptr->data_used = 0;
    return;
  }

  if (strcmp(el, "clients") == 0)
  {
    //log_info("<clients>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_CLIENTS;
    return;
  }

  if (strcmp(el, "client") == 0)
  {
    //log_info("<client>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_CLIENT;

    if (context_ptr->client != NULL)
    {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "nested clients");
        context_ptr->error = XML_TRUE;
        return;
    }

    if (context_ptr->depth == 3 &&
        context_ptr->element[0] == PARSE_CONTEXT_STUDIO &&
        context_ptr->element[1] == PARSE_CONTEXT_JACK &&
        context_ptr->element[2] == PARSE_CONTEXT_CLIENTS)
    {
      if (attr[0] == NULL ||
          attr[1] == NULL ||
          attr[2] == NULL ||
          attr[3] == NULL ||
          attr[4] != NULL ||
          strcmp(attr[0], "name") != 0 ||
          strcmp(attr[2], "uuid") != 0)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "studio/jack/clients/client XML element must contain exactly two attributes, named \"name\" and \"uuid\", in this order");
        context_ptr->error = XML_TRUE;
        return;
      }

      if (uuid_parse(attr[3], uuid) != 0)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "cannot parse uuid \"%s\"", attr[3]);
        context_ptr->error = XML_TRUE;
        return;
      }

      log_info("jack client \"%s\" with uuid %s", attr[1], attr[3]);

      if (!ladish_client_create(uuid, false, false, false, &context_ptr->client))
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "ladish_client_create() failed.");
        context_ptr->error = XML_TRUE;
        ASSERT(context_ptr->client == NULL);
        return;
      }

      if (!ladish_graph_add_client(g_studio.jack_graph, context_ptr->client, attr[1], true))
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "ladish_graph_add_client() failed to add client '%s' to JACK graph", attr[1]);
        context_ptr->error = XML_TRUE;
        ladish_client_destroy(context_ptr->client);
        context_ptr->client = NULL;
        return;
      }
    }
    else if (context_ptr->depth == 2 &&
             context_ptr->element[0] == PARSE_CONTEXT_STUDIO &&
             context_ptr->element[1] == PARSE_CONTEXT_CLIENTS)
    {
      if (attr[0] == NULL ||
          attr[1] == NULL ||
          attr[2] == NULL ||
          attr[3] == NULL ||
          attr[4] != NULL ||
          strcmp(attr[0], "name") != 0 ||
          strcmp(attr[2], "uuid") != 0)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "studio/clients/client XML element must contain exactly two attributes, named \"name\" and \"uuid\", in this order");
        context_ptr->error = XML_TRUE;
        return;
      }

      if (uuid_parse(attr[3], uuid) != 0)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "cannot parse uuid \"%s\"", attr[3]);
        context_ptr->error = XML_TRUE;
        return;
      }

      log_info("studio client \"%s\" with uuid %s", attr[1], attr[3]);

      if (!ladish_client_create(uuid, true, false, false, &context_ptr->client))
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "ladish_client_create() failed.");
        context_ptr->error = XML_TRUE;
        ASSERT(context_ptr->client == NULL);
        return;
      }

      if (!ladish_graph_add_client(g_studio.studio_graph, context_ptr->client, attr[1], true))
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "ladish_graph_add_client() failed to add client '%s' to studio graph", attr[1]);
        context_ptr->error = XML_TRUE;
        ladish_client_destroy(context_ptr->client);
        context_ptr->client = NULL;
        return;
      }
    }

    return;
  }

  if (strcmp(el, "ports") == 0)
  {
    //log_info("<ports>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_PORTS;
    return;
  }

  if (strcmp(el, "port") == 0)
  {
    //log_info("<port>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_PORT;

    if (context_ptr->port != NULL)
    {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "nested ports");
        context_ptr->error = XML_TRUE;
        return;
    }

    if (context_ptr->client == NULL)
    {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "client-less port");
        context_ptr->error = XML_TRUE;
        return;
    }

    if (context_ptr->depth == 5 &&
        context_ptr->element[0] == PARSE_CONTEXT_STUDIO &&
        context_ptr->element[1] == PARSE_CONTEXT_JACK &&
        context_ptr->element[2] == PARSE_CONTEXT_CLIENTS &&
        context_ptr->element[3] == PARSE_CONTEXT_CLIENT &&
        context_ptr->element[4] == PARSE_CONTEXT_PORTS)
    {
      if (attr[0] == NULL ||
          attr[1] == NULL ||
          attr[2] == NULL ||
          attr[3] == NULL ||
          attr[4] != NULL ||
          strcmp(attr[0], "name") != 0 ||
          strcmp(attr[2], "uuid") != 0)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "studio/jack/clients/client/ports/port XML element must contain exactly two attributes, named \"name\" and \"uuid\", in this order");
        context_ptr->error = XML_TRUE;
        return;
      }

      if (uuid_parse(attr[3], uuid) != 0)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "cannot parse uuid \"%s\"", attr[3]);
        context_ptr->error = XML_TRUE;
        return;
      }

      log_info("jack port \"%s\" with uuid %s", attr[1], attr[3]);

      if (!ladish_port_create(uuid, &context_ptr->port))
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "ladish_port_create() failed.");
        return;
      }

      if (!ladish_graph_add_port(g_studio.jack_graph, context_ptr->client, context_ptr->port, attr[1], 0, 0, true))
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "ladish_graph_add_port() failed.");
        ladish_port_destroy(context_ptr->port);
        context_ptr->port = NULL;
        return;
      }
    }
    else if (context_ptr->depth == 4 &&
             context_ptr->element[0] == PARSE_CONTEXT_STUDIO &&
             context_ptr->element[1] == PARSE_CONTEXT_CLIENTS &&
             context_ptr->element[2] == PARSE_CONTEXT_CLIENT &&
             context_ptr->element[3] == PARSE_CONTEXT_PORTS)
    {
      if (attr[0] == NULL ||
          attr[1] == NULL ||
          attr[2] == NULL ||
          attr[3] == NULL ||
          attr[4] != NULL ||
          strcmp(attr[0], "name") != 0 ||
          strcmp(attr[2], "uuid") != 0)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "studio/clients/client/ports/port XML element must contain exactly two attributes, named \"name\" and \"uuid\", in this order");
        context_ptr->error = XML_TRUE;
        return;
      }

      if (uuid_parse(attr[3], uuid) != 0)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "cannot parse uuid \"%s\"", attr[3]);
        context_ptr->error = XML_TRUE;
        return;
      }

      log_info("studio port \"%s\" with uuid %s", attr[1], attr[3]);

      context_ptr->port = ladish_graph_find_port_by_uuid(g_studio.jack_graph, uuid);
      if (context_ptr->port == NULL)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "studio client with non-jack port %s", attr[3]);
        context_ptr->error = XML_TRUE;
        return;
      }

      if (!ladish_graph_add_port(g_studio.studio_graph, context_ptr->client, context_ptr->port, attr[1], 0, 0, true))
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "ladish_graph_add_port() failed.");
        ladish_port_destroy(context_ptr->port);
        context_ptr->port = NULL;
        return;
      }
    }

    return;
  }

  if (strcmp(el, "dict") == 0)
  {
    //log_info("<dict>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_DICT;

    if (context_ptr->dict != NULL)
    {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "nested dicts");
        context_ptr->error = XML_TRUE;
        return;
    }

    if (context_ptr->depth == 1 &&
        context_ptr->element[0] == PARSE_CONTEXT_STUDIO)
    {
      context_ptr->dict = ladish_graph_get_dict(g_studio.studio_graph);
      ASSERT(context_ptr->dict != NULL);
    }
    else if (context_ptr->depth > 0 &&
             context_ptr->element[context_ptr->depth - 1] == PARSE_CONTEXT_CLIENT)
    {
      ASSERT(context_ptr->client != NULL);
      context_ptr->dict = ladish_client_get_dict(context_ptr->client);
      ASSERT(context_ptr->dict != NULL);
    }
    else if (context_ptr->depth > 0 &&
             context_ptr->element[context_ptr->depth - 1] == PARSE_CONTEXT_PORT)
    {
      ASSERT(context_ptr->port != NULL);
      context_ptr->dict = ladish_port_get_dict(context_ptr->port);
      ASSERT(context_ptr->dict != NULL);
    }
    else
    {
      lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "unexpected dict XML element");
      context_ptr->error = XML_TRUE;
      return;
    }

    return;
  }

  if (strcmp(el, "key") == 0)
  {
    //log_info("<key>");
    context_ptr->element[++context_ptr->depth] = PARSE_CONTEXT_KEY;

    if (context_ptr->dict == NULL)
    {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "dict-less key");
        context_ptr->error = XML_TRUE;
        return;
    }

    if (attr[0] == NULL ||
        attr[1] == NULL ||
        attr[2] != NULL ||
        strcmp(attr[0], "name") != 0)
    {
      lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "dict/key XML element must contain exactly one attributes, named \"name\".");
      context_ptr->error = XML_TRUE;
      return;
    }

    context_ptr->str = strdup(attr[1]);
    if (context_ptr->str == NULL)
    {
      lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "strdup() failed");
      context_ptr->error = XML_TRUE;
      return;
    }

    context_ptr->data_used = 0;

    return;
  }

  lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "unknown element \"%s\"", el);
  context_ptr->error = XML_TRUE;
}

static void callback_elend(void * data, const char * el)
{
  char * src;
  char * dst;
  char * sep;
  size_t src_len;
  size_t dst_len;
  char * address;
  struct jack_parameter_variant parameter;
  bool is_set;

  if (context_ptr->error)
  {
    return;
  }

  //log_info("element end (depth = %d, element = %u)", context_ptr->depth, context_ptr->element[context_ptr->depth]);

  if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_PARAMETER &&
      context_ptr->depth == 3 &&
      context_ptr->element[0] == PARSE_CONTEXT_STUDIO &&
      context_ptr->element[1] == PARSE_CONTEXT_JACK &&
      context_ptr->element[2] == PARSE_CONTEXT_CONF)
  {
    context_ptr->data[context_ptr->data_used] = 0;

    //log_info("'%s' with value '%s'", context_ptr->str, context_ptr->data);

    dst = address = strdup(context_ptr->str);
    src = context_ptr->str + 1;
    while (*src != 0)
    {
      sep = strchr(src, '/');
      if (sep == NULL)
      {
        src_len = strlen(src);
      }
      else
      {
        src_len = sep - src;
      }

      dst_len = unescape(src, src_len, dst);
      dst[dst_len] = 0;
      dst += dst_len + 1;

      src += src_len;
      ASSERT(*src == '/' || *src == 0);
      if (sep != NULL)
      {
        ASSERT(*src == '/');
        src++;                  /* skip separator */
      }
      else
      {
        ASSERT(*src == 0);
      }
    }
    *dst = 0;                   /* ASCIZZ */

    if (!jack_proxy_get_parameter_value(address, &is_set, &parameter))
    {
      lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "jack_proxy_get_parameter_value() failed");
      goto fail_free_address;
    }

    if (parameter.type == jack_string)
    {
      free(parameter.value.string);
    }

    switch (parameter.type)
    {
    case jack_boolean:
      log_info("%s value is %s (boolean)", context_ptr->str, context_ptr->data);
      if (strcmp(context_ptr->data, "true") == 0)
      {
        parameter.value.boolean = true;
      }
      else if (strcmp(context_ptr->data, "false") == 0)
      {
        parameter.value.boolean = false;
      }
      else
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "bad value for a bool jack param");
        goto fail_free_address;
      }
      break;
    case jack_string:
      log_info("%s value is %s (string)", context_ptr->str, context_ptr->data);
      parameter.value.string = context_ptr->data;
      break;
    case jack_byte:
      log_debug("%s value is %u/%c (byte/char)", context_ptr->str, *context_ptr->data, *context_ptr->data);
      if (context_ptr->data[0] == 0 ||
          context_ptr->data[1] != 0)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "bad value for a char jack param");
        goto fail_free_address;
      }
      parameter.value.byte = context_ptr->data[0];
      break;
    case jack_uint32:
      log_info("%s value is %s (uint32)", context_ptr->str, context_ptr->data);
      if (sscanf(context_ptr->data, "%" PRIu32, &parameter.value.uint32) != 1)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "bad value for an uint32 jack param");
        goto fail_free_address;
      }
      break;
    case jack_int32:
      log_info("%s value is %s (int32)", context_ptr->str, context_ptr->data);
      if (sscanf(context_ptr->data, "%" PRIi32, &parameter.value.int32) != 1)
      {
        lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "bad value for an int32 jack param");
        goto fail_free_address;
      }
      break;
    default:
      lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "unknown jack parameter type %d of %s", (int)parameter.type, context_ptr->str);
      goto fail_free_address;
    }

    if (!jack_proxy_set_parameter_value(address, &parameter))
    {
      lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "jack_proxy_set_parameter_value() failed");
      goto fail_free_address;
    }

    free(address);
  }
  else if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_KEY &&
           context_ptr->depth > 0 &&
           context_ptr->element[context_ptr->depth - 1] == PARSE_CONTEXT_DICT)
  {
    ASSERT(context_ptr->dict != NULL);
    context_ptr->data[context_ptr->data_used] = 0;
    log_info("dict key '%s' with value '%s'", context_ptr->str, context_ptr->data);
    if (!ladish_dict_set(context_ptr->dict, context_ptr->str, context_ptr->data))
    {
      lash_dbus_error(context_ptr->call_ptr, LASH_DBUS_ERROR_GENERIC, "ladish_dict_set() failed");
      context_ptr->error = XML_TRUE;
      return;
    }
  }
  else if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_DICT)
  {
    //log_info("</dict>");
    ASSERT(context_ptr->dict != NULL);
    context_ptr->dict = NULL;
  }
  else if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_CLIENT)
  {
    //log_info("</client>");
    ASSERT(context_ptr->client != NULL);
    context_ptr->client = NULL;
  }
  else if (context_ptr->element[context_ptr->depth] == PARSE_CONTEXT_PORT)
  {
    //log_info("</port>");
    ASSERT(context_ptr->port != NULL);
    context_ptr->port = NULL;
  }

  context_ptr->depth--;

  if (context_ptr->str != NULL)
  {
    free(context_ptr->str);
    context_ptr->str = NULL;
  }

  return;

fail_free_address:
  free(address);
  context_ptr->error = XML_TRUE;
  return;
}

#undef context_ptr

bool studio_load(void * call_ptr, const char * studio_name)
{
  char * path;
  struct stat st;
  XML_Parser parser;
  int bytes_read;
  void * buffer;
  int fd;
  enum XML_Status xmls;
  struct parse_context context;

  if (!studio_compose_filename(studio_name, &path, NULL))
  {
    lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "failed to compose path of studio \%s\" file", studio_name);
    return false;
  }

  log_info("Loading studio... ('%s')", path);

  if (stat(path, &st) != 0)
  {
    lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "failed to stat '%s': %d (%s)", path, errno, strerror(errno));
    free(path);
    return false;
  }

  studio_clear();

  g_studio.name = strdup(studio_name);
  if (g_studio.name == NULL)
  {
    lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "strdup(\"%s\") failed", studio_name);
    free(path);
    return false;
  }

  g_studio.filename = path;

  if (!jack_reset_all_params())
  {
    lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "jack_reset_all_params() failed");
    return false;
  }

  fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "failed to open '%s': %d (%s)", path, errno, strerror(errno));
    return false;
  }

  parser = XML_ParserCreate(NULL);
  if (parser == NULL)
  {
    lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "XML_ParserCreate() failed to create parser object.");
    close(fd);
    return false;
  }

  //log_info("conf file size is %llu bytes", (unsigned long long)st.st_size);

  /* we are expecting that conf file has small enough size to fit in memory */

  buffer = XML_GetBuffer(parser, st.st_size);
  if (buffer == NULL)
  {
    lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "XML_GetBuffer() failed.");
    XML_ParserFree(parser);
    close(fd);
    return false;
  }

  bytes_read = read(fd, buffer, st.st_size);
  if (bytes_read != st.st_size)
  {
    lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "read() returned unexpected result.");
    XML_ParserFree(parser);
    close(fd);
    return false;
  }

  context.error = XML_FALSE;
  context.depth = -1;
  context.str = NULL;
  context.client = NULL;
  context.port = NULL;
  context.dict = NULL;
  context.call_ptr = call_ptr;

  XML_SetElementHandler(parser, callback_elstart, callback_elend);
  XML_SetCharacterDataHandler(parser, callback_chrdata);
  XML_SetUserData(parser, &context);

  xmls = XML_ParseBuffer(parser, bytes_read, XML_TRUE);
  if (xmls == XML_STATUS_ERROR)
  {
    if (!context.error)         /* if we have initiated the fail, dbus error is already set to better message */
    {
      lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "XML_ParseBuffer() failed.");
    }
    XML_ParserFree(parser);
    close(fd);
    studio_clear();
    return false;
  }

  XML_ParserFree(parser);
  close(fd);

  if (context.error)
  {
    /* if we have initiated the fail, dbus error is already set to better message */
    studio_clear();
    return false;
  }

  g_studio.persisted = true;
  log_info("Studio loaded. ('%s')", path);

  if (!studio_publish())
  {
    lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "studio_publish() failed.");
    studio_clear();
    return false;
  }

  if (!studio_start())
  {
      lash_dbus_error(call_ptr, LASH_DBUS_ERROR_GENERIC, "Failed to start JACK server.");
      studio_clear();
      return false;
  }

  return true;
}
