#ifndef GRAPH_LIST_H
#define GRAPH_LIST_H 1

#include "utils_array.h"

struct graph_ident_s;
typedef struct graph_ident_s graph_ident_t;

struct graph_instance_s;
typedef struct graph_instance_s graph_instance_t;

struct graph_config_s;
typedef struct graph_config_s graph_config_t;

/*
 * Callback types
 */
typedef int (*gl_cfg_callback) (graph_config_t *cfg,
    void *user_data);

typedef int (*gl_inst_callback) (graph_config_t *cfg,
    graph_instance_t *inst, void *user_data);

/*
 * Functions
 */
char *ident_to_file (const graph_ident_t *ident);

int gl_graph_get_all (gl_cfg_callback callback,
    void *user_data);

graph_config_t *graph_get_selected (void);

int gl_graph_get_title (graph_config_t *cfg,
    char *buffer, size_t buffer_size);

int gl_graph_instance_get_all (graph_config_t *cfg,
    gl_inst_callback callback, void *user_data);

graph_instance_t *inst_get_selected (graph_config_t *cfg);

int gl_instance_get_all (gl_inst_callback callback,
    void *user_data);

int gl_instance_get_params (graph_config_t *cfg, graph_instance_t *inst,
    char *buffer, size_t buffer_size);

int gl_instance_get_rrdargs (graph_config_t *cfg, graph_instance_t *inst,
    str_array_t *args);

struct graph_list_s
{
  char *host;
  char *plugin;
  char *plugin_instance;
  char *type;
  char *type_instance;
};
typedef struct graph_list_s graph_list_t;

typedef int (*gl_callback) (
    const graph_list_t *, void *user_data);

int gl_update (void);
int gl_foreach (gl_callback callback, void *user_data);

#endif /* GRAPH_LIST_H */
/* vim: set sw=2 sts=2 et fdm=marker : */
