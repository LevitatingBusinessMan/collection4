#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <dirent.h> /* for PATH_MAX */
#include <assert.h>
#include <math.h>

#include <rrd.h>

#include "common.h"
#include "action_graph.h"
#include "graph_list.h"
#include "utils_params.h"
#include "utils_array.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

struct graph_data_s
{
  str_array_t *args;
  rrd_info_t *info;
  time_t mtime;
  time_t expires;
  long now;
  long begin;
  long end;
};
typedef struct graph_data_s graph_data_t;

static int get_time_args (graph_data_t *data) /* {{{ */
{
  const char *begin_str;
  const char *end_str;
  long now;
  long begin;
  long end;
  char *endptr;
  long tmp;

  begin_str = param ("begin");
  end_str = param ("end");

  now = (long) time (NULL);
  data->now = now;
  data->begin = now - 86400;
  data->end = now;

  if (begin_str != NULL)
  {
    endptr = NULL;
    errno = 0;
    tmp = strtol (begin_str, &endptr, /* base = */ 0);
    if ((endptr == begin_str) || (errno != 0))
      return (-1);
    if (tmp <= 0)
      begin = now + tmp;
    else
      begin = tmp;
  }
  else /* if (begin_str == NULL) */
  {
    begin = now - 86400;
  }

  if (end_str != NULL)
  {
    endptr = NULL;
    errno = 0;
    tmp = strtol (end_str, &endptr, /* base = */ 0);
    if ((endptr == end_str) || (errno != 0))
      return (-1);
    end = tmp;
    if (tmp <= 0)
      end = now + tmp;
    else
      end = tmp;
  }
  else /* if (end_str == NULL) */
  {
    end = now;
  }

  if (begin == end)
    return (-1);

  if (begin > end)
  {
    tmp = begin;
    begin = end;
    end = tmp;
  }

  data->begin = begin;
  data->end = end;

  array_append (data->args, "-s");
  array_append_format (data->args, "%li", begin);
  array_append (data->args, "-e");
  array_append_format (data->args, "%li", end);

  return (0);
} /* }}} int get_time_args */

static void emulate_graph (int argc, char **argv) /* {{{ */
{
  int i;

  printf ("rrdtool \\\n");
  for (i = 0; i < argc; i++)
  {
    if (i < (argc - 1))
      printf ("  \"%s\" \\\n", argv[i]);
    else
      printf ("  \"%s\"\n", argv[i]);
  }
} /* }}} void emulate_graph */

static int ag_info_print (rrd_info_t *info) /* {{{ */
{
  if (info->type == RD_I_VAL)
    printf ("[info] %s = %g;\n", info->key, info->value.u_val);
  else if (info->type == RD_I_CNT)
    printf ("[info] %s = %lu;\n", info->key, info->value.u_cnt);
  else if (info->type == RD_I_STR)
    printf ("[info] %s = %s;\n", info->key, info->value.u_str);
  else if (info->type == RD_I_INT)
    printf ("[info] %s = %i;\n", info->key, info->value.u_int);
  else if (info->type == RD_I_BLO)
    printf ("[info] %s = [blob, %lu bytes];\n", info->key, info->value.u_blo.size);
  else
    printf ("[info] %s = [unknown type %#x];\n", info->key, info->type);

  return (0);
} /* }}} int ag_info_print */

static int output_graph (graph_data_t *data) /* {{{ */
{
  rrd_info_t *img;
  char time_buffer[256];
  time_t expires;
  int status;

  for (img = data->info; img != NULL; img = img->next)
    if ((strcmp ("image", img->key) == 0)
        && (img->type == RD_I_BLO))
      break;

  if (img == NULL)
    return (ENOENT);

  printf ("Content-Type: image/png\n"
      "Content-Length: %lu\n",
      img->value.u_blo.size);
  if (data->mtime > 0)
  {
    int status;
    
    status = time_to_rfc1123 (data->mtime, time_buffer, sizeof (time_buffer));
    if (status == 0)
    {
      printf ("Last-Modified: %s\n", time_buffer);
      printf ("X-Last-Modified: %s\n", time_buffer);
      fprintf (stderr, "Last-Modified: %s (%li)\n", time_buffer, (long) data->mtime);
    }
  }

  /* Print Expires header. */
  if (data->end >= data->now)
  {
    /* The end of the timespan can be seen. */
    long secs_per_pixel;

    /* FIXME: Handle graphs with width != 400. */
    secs_per_pixel = (data->end - data->begin) / 400;

    expires = (time_t) (data->now + secs_per_pixel);
  }
  else /* if (data->end < data->now) */
  {
    expires = (time_t) (data->now + 86400);
  }
  status = time_to_rfc1123 (expires, time_buffer, sizeof (time_buffer));
  if (status == 0)
    printf ("Expires: %s\n", time_buffer);

  printf ("\n");

  fwrite (img->value.u_blo.ptr, img->value.u_blo.size,
      /* nmemb = */ 1, stdout);

  return (0);
} /* }}} int output_graph */

#define OUTPUT_ERROR(...) do {             \
  printf ("Content-Type: text/plain\n\n"); \
  printf (__VA_ARGS__);                    \
  return (0);                              \
} while (0)

int action_graph (void) /* {{{ */
{
  graph_data_t data;
  graph_config_t *cfg;
  graph_instance_t *inst;
  int status;

  cfg = gl_graph_get_selected ();
  if (cfg == NULL)
    OUTPUT_ERROR ("gl_graph_get_selected () failed.\n");

  inst = inst_get_selected (cfg);
  if (inst == NULL)
    OUTPUT_ERROR ("inst_get_selected (%p) failed.\n", (void *) cfg);

  data.args = array_create ();
  if (data.args == NULL)
    return (ENOMEM);

  array_append (data.args, "graph");
  array_append (data.args, "-");
  array_append (data.args, "--imgformat");
  array_append (data.args, "PNG");

  get_time_args (&data);

  status = inst_get_rrdargs (cfg, inst, data.args);
  if (status != 0)
  {
    array_destroy (data.args);
    OUTPUT_ERROR ("inst_get_rrdargs failed with status %i.\n", status);
  }

  rrd_clear_error ();
  data.info = rrd_graph_v (array_argc (data.args), array_argv (data.args));
  if ((data.info == NULL) || rrd_test_error ())
  {
    printf ("Content-Type: text/plain\n\n");
    printf ("rrd_graph_v failed: %s\n", rrd_get_error ());
    emulate_graph (array_argc (data.args), array_argv (data.args));
  }
  else
  {
    int status;

    data.mtime = inst_get_mtime (inst);

    status = output_graph (&data);
    if (status != 0)
    {
      rrd_info_t *ptr;

      printf ("Content-Type: text/plain\n\n");
      printf ("output_graph failed. Maybe the \"image\" info was not found?\n\n");

      for (ptr = data.info; ptr != NULL; ptr = ptr->next)
      {
        ag_info_print (ptr);
      }
    }
  }

  if (data.info != NULL)
    rrd_info_free (data.info);

  array_destroy (data.args);
  data.args = NULL;

  return (0);
} /* }}} int action_graph */

/* vim: set sw=2 sts=2 et fdm=marker : */
