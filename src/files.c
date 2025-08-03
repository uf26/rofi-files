#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <gmodule.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <stdbool.h>
#include <gio/gdesktopappinfo.h>

#include <rofi/mode.h>
#include <rofi/helper.h>
#include <rofi/mode-private.h>
#include <rofi/rofi-icon-fetcher.h>
#include <rofi/rofi-types.h>

#include <stdint.h>

G_MODULE_EXPORT Mode mode;

typedef struct {
    char* name;
    char* icon;
} Entry;

typedef struct {
    Entry* array;
    unsigned int array_length;
    GMutex mutex;
    char* base_dir;
    char* ignore_path;
} MYPLUGINModePrivateData;

extern void rofi_view_reload(void);

bool gtk_launch(const char* app_name, const char* path)
{
    GAppInfo* info = G_APP_INFO(g_desktop_app_info_new(app_name));
    if (!info) {
        g_printerr("%s: error creating app info for '%s'\n", g_get_prgname(), app_name);
        return false;
    }

    GFile *f = g_file_new_for_commandline_arg(path);
    GList *l = g_list_append(NULL, f);

    GError *error = NULL;
    if (!g_app_info_launch(info, l, NULL, &error)) {
        g_printerr("%s: error launching application: %s\n", g_get_prgname(), error->message);
        g_error_free(error);
        g_object_unref(info);
        g_list_free_full(l, (GDestroyNotify)g_object_unref);
        return false;
    }

    g_object_unref(info);
    g_list_free_full(l, (GDestroyNotify)g_object_unref);

    return true;
}

bool launch_with_editor(const char* path)
{
    const char* app_name = getenv("EDITOR");
    if (!app_name || app_name[0] == '\0')
        app_name = "nvim";

    char* desktop_file_name = NULL;
    if (g_str_has_suffix(app_name, ".desktop"))
        desktop_file_name = g_strdup(app_name);
    else 
        desktop_file_name = g_strconcat(app_name, ".desktop", NULL);

    bool success = gtk_launch(desktop_file_name, path);
    g_free(desktop_file_name);
    return success;
}

bool launch_with_default(const char* path)
{
    GError* error = NULL;
    char* uri = g_filename_to_uri(path, NULL, &error);
    if (!uri) {
        g_printerr("Failed to convert path to URI: %s\n", error->message);
        g_error_free(error);
        return false;
    }

    gboolean success = g_app_info_launch_default_for_uri(uri, NULL, &error);
    g_free(uri);

    if (!success) {
        g_printerr("Failed to launch default app for '%s': %s\n", path, error->message);
        g_error_free(error);
        return false;
    }

    return true;
}

char* assure_base_end_with_slash(char* base_dir) {
    size_t len = strlen(base_dir);
    if (base_dir[len - 1] != '/') {
        base_dir = g_realloc(base_dir, len + 2);
        base_dir[len] = '/';
        base_dir[len + 1] = '\0'; 
    }
    return base_dir;
}

void load_config(MYPLUGINModePrivateData* pd) {
    char* temp = NULL;

    if (!find_arg_str("-files-base-dir", &temp)) {
        pd->base_dir = assure_base_end_with_slash(g_strdup(getenv("HOME")));
    } else {
        pd->base_dir = assure_base_end_with_slash(g_strdup(temp));
    }

    if (!find_arg_str("-files-ignore-path", &temp)) {
        pd->ignore_path = NULL;
    } else {
        pd->ignore_path = g_strdup(temp);
    }
}

void remove_newline(char* str) {
    while (*str != '\0' && *str != '\n')
        str++;

    *str = '\0';
}

char* get_icon_name(char* filepath) {
    GFile* file = g_file_new_for_path(filepath);
    if (!file) 
        return NULL;

    GFileInfo* info = g_file_query_info(file, "standard::icon", G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (!info) {
        g_object_unref(file);
        return NULL;
    }

    GIcon* icon = g_file_info_get_icon(info);
    if (!icon) {
        g_object_unref(info);
        g_object_unref(file);
        return NULL;
    }

    char* icon_name = NULL;
    if (G_IS_THEMED_ICON(icon)) {
        GThemedIcon* themed_icon = G_THEMED_ICON(icon);
        const char* const* icon_names = g_themed_icon_get_names(themed_icon);

        while (*icon_names) {
            if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), *icon_names)) {
                icon_name = g_strdup(*icon_names);
                break;
            }
            icon_names++;
        }
    }
    g_object_unref(info);
    g_object_unref(file);

    return icon_name;
}

char* get_full_path(char* sub, char* base) {
    char* full_path = g_malloc(strlen(sub) + strlen(base) + 2);
    strcpy(full_path, base);
    strcat(full_path, sub);
    return full_path;
}

void get_command(char* command, size_t command_size, char* base_dir, char* ignore_path) {
    if (ignore_path == NULL)
        snprintf(command, command_size, "fd --base-directory %s", base_dir);
    else
        snprintf(command, command_size, "fd --base-directory %s --ignore-file %s", base_dir, ignore_path);
}

void load_files(MYPLUGINModePrivateData* pd) {
    char command[1024];
    get_command(command, sizeof(command), pd->base_dir, pd->ignore_path);

    FILE *pipe = popen(command, "r");
    char line[1024];

    while (fgets(line, sizeof(line), pipe) != NULL) {
        char* entry = g_strdup(line);
        remove_newline(entry);
        char* full_path = get_full_path(entry, pd->base_dir);
        char* icon_name = get_icon_name(full_path);
        g_free(full_path);

        g_mutex_lock(&pd->mutex);
        pd->array = g_realloc(pd->array, sizeof(Entry) * (pd->array_length + 1));
        pd->array[pd->array_length].name = entry;
        pd->array[pd->array_length].icon = icon_name;
        pd->array_length++;
        g_mutex_unlock(&pd->mutex);

        rofi_view_reload();
    }
    pclose(pipe);
}

void load_files_async(MYPLUGINModePrivateData* pd) {
    GThread* thread = g_thread_new("load_files", (GThreadFunc)load_files, pd);
    g_thread_unref(thread);
}

static void get_files(Mode* sw) {
    MYPLUGINModePrivateData* pd = (MYPLUGINModePrivateData*)mode_get_private_data(sw);

    load_config(pd);
    gtk_init(0, NULL);

    g_mutex_init(&pd->mutex);
    pd->array_length = 0;
    pd->array = NULL;

    load_files_async(pd);
}

static int files_mode_init(Mode* sw) {
    if(mode_get_private_data(sw) == NULL) {
        MYPLUGINModePrivateData* pd = g_malloc(sizeof(MYPLUGINModePrivateData));
        mode_set_private_data(sw, (void*)pd);
        get_files(sw);
    }
    return TRUE;
}
static unsigned int files_mode_get_num_entries(const Mode *sw)
{
    MYPLUGINModePrivateData *pd = (MYPLUGINModePrivateData *)mode_get_private_data(sw);

    g_mutex_lock(&pd->mutex);
    unsigned int length = pd->array_length;
    g_mutex_unlock(&pd->mutex);

    return length;
}

void action(MYPLUGINModePrivateData *pd, unsigned int selected_line, bool alt) {
    g_mutex_lock(&pd->mutex);
    if (selected_line < pd->array_length) {
        char* full_path = get_full_path(pd->array[selected_line].name, pd->base_dir);

        if (alt)
            launch_with_editor(full_path);
        else
            launch_with_default(full_path);

        g_free(full_path);
    }
    g_mutex_unlock(&pd->mutex);
}

static ModeMode files_mode_result(Mode *sw, int mretv, char **input, unsigned int selected_line)
{
    ModeMode retv = MODE_EXIT;
    MYPLUGINModePrivateData *pd = (MYPLUGINModePrivateData *)mode_get_private_data(sw);

    if(mretv & MENU_NEXT) {
        retv = NEXT_DIALOG;
    } else if(mretv & MENU_PREVIOUS) {
        retv = PREVIOUS_DIALOG;
    } else if(mretv & MENU_QUICK_SWITCH) {
        retv = (mretv & MENU_LOWER_MASK);
    } else if((mretv & MENU_OK)) {
        action(pd, selected_line, false);
    } else if((mretv & MENU_ENTRY_DELETE) == MENU_ENTRY_DELETE) {
        retv = RELOAD_DIALOG;
    } else if(mretv & MENU_CUSTOM_COMMAND){
        action(pd, selected_line, true);
    }

    return retv;
}

static void files_mode_destroy(Mode *sw)
{
    MYPLUGINModePrivateData *pd = (MYPLUGINModePrivateData *)mode_get_private_data(sw);
    if(pd != NULL) {
        g_mutex_lock(&pd->mutex);
        for(unsigned int i = 0; i < pd->array_length; i++) {
            g_free(pd->array[i].icon);
            g_free(pd->array[i].name);
        }

        g_free(pd->array);
        g_free(pd->base_dir);
        g_free(pd->ignore_path);

        g_mutex_unlock(&pd->mutex);
        g_mutex_clear(&pd->mutex);
        g_free(pd);
        mode_set_private_data(sw, NULL);
    }
}

static char *files_get_display_value(const Mode *sw, unsigned int selected_line, 
        G_GNUC_UNUSED int *state, G_GNUC_UNUSED GList **attr_list, int get_entry)
{
    if(!get_entry)
        return NULL;

    MYPLUGINModePrivateData *pd = (MYPLUGINModePrivateData *)mode_get_private_data(sw);

    g_mutex_lock(&pd->mutex);
    if(selected_line < pd->array_length && pd->array[selected_line].name != NULL) {
        char* name = g_strdup(pd->array[selected_line].name);
        g_mutex_unlock(&pd->mutex);
        return name;
    }
    g_mutex_unlock(&pd->mutex);

    return g_strdup("n/a");
}

static cairo_surface_t* file_get_icon(const Mode *sw, unsigned int selected_line, unsigned int size)
{
    MYPLUGINModePrivateData *pd = (MYPLUGINModePrivateData *)mode_get_private_data(sw);

    g_mutex_lock(&pd->mutex);
    if(selected_line < pd->array_length && pd->array[selected_line].icon != NULL) {
        uint32_t id = rofi_icon_fetcher_query(pd->array[selected_line].icon, size);
        g_mutex_unlock(&pd->mutex);

        return rofi_icon_fetcher_get(id);
    }
    g_mutex_unlock(&pd->mutex);

    return NULL;
}

static int files_token_match(const Mode *sw, rofi_int_matcher **tokens, unsigned int index)
{
    MYPLUGINModePrivateData *pd = (MYPLUGINModePrivateData *)mode_get_private_data(sw);

    g_mutex_lock(&pd->mutex);
    int retv = helper_token_match(tokens, pd->array[index].name);
    g_mutex_unlock(&pd->mutex);

    return retv;
}


Mode mode =
{
    .abi_version        = ABI_VERSION,
    .name               = "files",
    .cfg_name_key       = "display-files",
    ._init              = files_mode_init,
    ._get_num_entries   = files_mode_get_num_entries,
    ._result            = files_mode_result,
    ._destroy           = files_mode_destroy,
    ._token_match       = files_token_match,
    ._get_display_value = files_get_display_value,
    ._get_icon          = file_get_icon,
    ._get_message       = NULL,
    ._get_completion    = NULL,
    ._preprocess_input  = NULL,
    .private_data       = NULL,
    .free               = NULL,
};
