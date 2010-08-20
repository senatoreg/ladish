/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 * DESCRIPTION:
 *  
 *
 * NOTES:
 *  
 *
 *****************************************************************************/

#include "load.h"
#include "limits.h"
#include "studio.h"
#include "../proxies/jmcore_proxy.h"

void ladish_dump_element_stack(struct ladish_parse_context * context_ptr)
{
  signed int depth;
  const char * descr;

  log_info("depth=%d", context_ptr->depth);

  for (depth = context_ptr->depth; depth >= 0; depth--)
  {
    switch (context_ptr->element[depth])
    {
    case PARSE_CONTEXT_ROOT:
      descr = "root";
      break;
    case PARSE_CONTEXT_STUDIO:
      descr = "studio";
      break;
    case PARSE_CONTEXT_JACK:
      descr = "jack";
      break;
    case PARSE_CONTEXT_CONF:
      descr = "conf";
      break;
    case PARSE_CONTEXT_PARAMETER:
      descr = "parameter";
      break;
    case PARSE_CONTEXT_CLIENTS:
      descr = "clients";
      break;
    case PARSE_CONTEXT_CLIENT:
      descr = "client";
      break;
    case PARSE_CONTEXT_PORTS:
      descr = "ports";
      break;
    case PARSE_CONTEXT_PORT:
      descr = "port";
      break;
    case PARSE_CONTEXT_DICT:
      descr = "dict";
      break;
    case PARSE_CONTEXT_KEY:
      descr = "key";
      break;
    case PARSE_CONTEXT_CONNECTIONS:
      descr = "connections";
      break;
    case PARSE_CONTEXT_CONNECTION:
      descr = "connection";
      break;
    case PARSE_CONTEXT_APPLICATIONS:
      descr = "applications";
      break;
    case PARSE_CONTEXT_APPLICATION:
      descr = "application";
      break;
    case PARSE_CONTEXT_ROOMS:
      descr = "rooms";
      break;
    case PARSE_CONTEXT_ROOM:
      descr = "room";
      break;
    case PARSE_CONTEXT_PROJECT:
      descr = "project";
      break;
    default:
      descr = "?";
    }

    log_info("%d - %u (%s)", depth, context_ptr->element[depth], descr);
  }
}

static const char * get_string_attribute_internal(const char * const * attr, const char * key, bool optional)
{
  while (attr[0] != NULL)
  {
    ASSERT(attr[1] != NULL);
    if (strcmp(attr[0], key) == 0)
    {
      return attr[1];
    }
    attr += 2;
  }

  if (!optional)
  {
    log_error("attribute \"%s\" is missing", key);
  }

  return NULL;
}

const char * ladish_get_string_attribute(const char * const * attr, const char * key)
{
  return get_string_attribute_internal(attr, key, false);
}

const char * ladish_get_uuid_attribute(const char * const * attr, const char * key, uuid_t uuid, bool optional)
{
  const char * value;

  value = get_string_attribute_internal(attr, key, optional);
  if (value == NULL)
  {
    return NULL;
  }

  if (uuid_parse(value, uuid) != 0)
  {
    log_error("cannot parse uuid \"%s\"", value);
    return NULL;
  }

  return value;
}

const char * ladish_get_bool_attribute(const char * const * attr, const char * key, bool * bool_value_ptr)
{
  const char * value_str;

  value_str = ladish_get_string_attribute(attr, key);
  if (value_str == NULL)
  {
    return NULL;
  }

  if (strcmp(value_str, "true") == 0)
  {
    *bool_value_ptr = true;
    return value_str;
  }

  if (strcmp(value_str, "false") == 0)
  {
    *bool_value_ptr = false;
    return value_str;
  }

  log_error("boolean XML attribute has value of \"%s\" but only \"true\" and \"false\" are valid", value_str);
  return NULL;
}

const char * ladish_get_byte_attribute(const char * const * attr, const char * key, uint8_t * byte_value_ptr)
{
  const char * value_str;
  long int li_value;
  char * end_ptr;

  value_str = ladish_get_string_attribute(attr, key);
  if (value_str == NULL)
  {
    return NULL;
  }

  errno = 0;    /* To distinguish success/failure after call */
  li_value = strtol(value_str, &end_ptr, 10);
  if ((errno == ERANGE && (li_value == LONG_MAX || li_value == LONG_MIN)) || (errno != 0 && li_value == 0) || end_ptr == value_str)
  {
    log_error("value '%s' of attribute '%s' is not valid integer.", value_str, key);
    return NULL;
  }

  if (li_value < 0 || li_value > 255)
  {
    log_error("value '%s' of attribute '%s' is not valid uint8.", value_str, key);
    return NULL;
  }

  *byte_value_ptr = (uint8_t)li_value;
  return value_str;
}

bool
ladish_get_name_and_uuid_attributes(
  const char * element_description,
  const char * const * attr,
  const char ** name_str_ptr,
  const char ** uuid_str_ptr,
  uuid_t uuid)
{
  const char * name_str;
  const char * uuid_str;

  name_str = ladish_get_string_attribute(attr, "name");
  if (name_str == NULL)
  {
    log_error("%s \"name\" attribute is not available", element_description);
    return false;
  }

  uuid_str = ladish_get_uuid_attribute(attr, "uuid", uuid, false);
  if (uuid_str == NULL)
  {
    log_error("%s \"uuid\" attribute is not available. name=\"%s\"", element_description, name_str);
    return false;
  }

  *name_str_ptr = name_str;
  *uuid_str_ptr = uuid_str;
  return true;
}

bool
ladish_parse_port_type_and_direction_attributes(
  const char * element_description,
  const char * const * attr,
  uint32_t * type_ptr,
  uint32_t * flags_ptr)
{
  const char * type_str;
  const char * direction_str;

  type_str = ladish_get_string_attribute(attr, "type");
  if (type_str == NULL)
  {
    log_error("%s \"type\" attribute is not available", element_description);
    return false;
  }

  direction_str = ladish_get_string_attribute(attr, "direction");
  if (direction_str == NULL)
  {
    log_error("%s \"direction\" attribute is not available", element_description);
    return false;
  }

  if (strcmp(type_str, "midi") == 0)
  {
    *type_ptr = JACKDBUS_PORT_TYPE_MIDI;
  }
  else if (strcmp(type_str, "audio") == 0)
  {
    *type_ptr = JACKDBUS_PORT_TYPE_AUDIO;
  }
  else
  {
    log_error("%s \"type\" attribute contains unknown value \"%s\"", element_description, type_str);
    return false;
  }

  if (strcmp(direction_str, "playback") == 0)
  {
    *flags_ptr = 0;
    JACKDBUS_PORT_SET_INPUT(*flags_ptr);
  }
  else if (strcmp(direction_str, "capture") == 0)
  {
    *flags_ptr = 0;
    JACKDBUS_PORT_SET_OUTPUT(*flags_ptr);
  }
  else
  {
    log_error("%s \"direction\" attribute contains unknown value \"%s\"", element_description, direction_str);
    return false;
  }

  return true;
}

static
bool
interlink_client(
  void * context,
  ladish_graph_handle graph_handle,
  ladish_client_handle jclient,
  const char * name,
  void ** client_iteration_context_ptr_ptr)
{
  uuid_t uuid;
  ladish_client_handle vclient;
  pid_t pid;

  if (strcmp(name, "system") == 0)
  {
    return true;
  }

  if (ladish_client_get_interlink(jclient, uuid))
  {
    ASSERT_NO_PASS;             /* interlinks are not stored in xml yet */
    return true;
  }

  vclient = ladish_graph_find_client_by_name(context, name);
  if (vclient == NULL)
  {
    /* jmcore clients are running when projects are being loaded */
    pid = ladish_client_get_pid(jclient);
    if (pid != jmcore_proxy_get_pid_cached())
    {
      log_error("JACK client '%s' has no vclient associated", name);
    }
    return true;
  }

  log_info("Interlinking clients of app '%s'", name);
  ladish_client_interlink(jclient, vclient);
  return true;
}

void ladish_interlink_clients(ladish_graph_handle vgraph)
{
  ladish_graph_iterate_nodes(ladish_studio_get_jack_graph(), false, NULL, vgraph, interlink_client, NULL, NULL);
}