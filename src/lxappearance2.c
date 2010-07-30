/*
 *      lxappearance2.c
 *
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "lxappearance2.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <string.h>

#include "widget-theme.h"
#include "icon-theme.h"
#include "cursor-theme.h"
#include "other.h"

LXAppearance app = {0};

Atom lxsession_atom = 0;
static const char* lxsession_name = NULL;

static void check_lxsession()
{
    lxsession_atom = XInternAtom( GDK_DISPLAY(), "_LXSESSION", True );
    if( lxsession_atom != None )
    {
        XGrabServer( GDK_DISPLAY() );
        if( XGetSelectionOwner( GDK_DISPLAY(), lxsession_atom ) )
        {
            app.use_lxsession = TRUE;
            lxsession_name = g_getenv("DESKTOP_SESSION");
        }
        XUngrabServer( GDK_DISPLAY() );
    }
}

static GOptionEntry option_entries[] =
{
    { NULL }
};

static void save_cursor_theme_name()
{
    char* dir_path;
    if(!app.cursor_theme || !g_strcmp0(app.cursor_theme, "default"))
        return;

    dir_path = g_build_filename(g_get_home_dir(), ".icons/default", NULL);
    if(0 == g_mkdir_with_parents(dir_path, 0700))
    {
        char* index_theme = g_build_filename(dir_path, "index.theme", NULL);
        char* content = g_strdup_printf(
            "# This file is written by LXAppearance. Do not edit."
            "[Icon Theme]\n"
            "Name=Default\n"
            "Comment=Default Cursor Theme\n"
            "Inherits=%s\n", app.cursor_theme);
        g_file_set_contents(index_theme, content, -1, NULL);
        g_free(content);
        g_free(index_theme);
    }
    g_free(dir_path);

    /*
    dir_path = g_build_filename(g_get_home_dir(), ".Xdefaults", NULL);
    Xcursor.theme: name
    Xcursor.size: [size]
    g_file_set_contents(dir_path, "", -1, NULL);
    g_free(dir_path);
    */
}

static void reload_all_programs()
{
    GdkEventClient event;
    event.type = GDK_CLIENT_EVENT;
    event.send_event = TRUE;
    event.window = NULL;

    if( app.use_lxsession )
    {
        event.message_type = gdk_atom_intern_static_string("_LXSESSION");
        event.data.b[0] = 0;    /* LXS_RELOAD */
    }
    else
    {
        /* if( icon_only )
            event.message_type = gdk_atom_intern("_GTK_LOAD_ICONTHEMES", FALSE);
        */
        event.message_type = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);
    }
    event.data_format = 8;
    gdk_event_send_clientmessage_toall((GdkEvent *)&event);
}

static const char* bool2str(gboolean val)
{
    return val ? "TRUE" : "FALSE";
}

static void lxappearance_save_gtkrc()
{
    static const char* tb_styles[]={
        "GTK_TOOLBAR_ICONS",
        "GTK_TOOLBAR_TEXT",
        "GTK_TOOLBAR_BOTH",
        "GTK_TOOLBAR_BOTH_HORIZ"
    };
    static const char* tb_icon_sizes[]={
        "GTK_ICON_SIZE_INVALID",
        "GTK_ICON_SIZE_MENU",
        "GTK_ICON_SIZE_SMALL_TOOLBAR",
        "GTK_ICON_SIZE_LARGE_TOOLBAR",
        "GTK_ICON_SIZE_BUTTON",
        "GTK_ICON_SIZE_DND",
        "GTK_ICON_SIZE_DIALOG"
    };

    char* file_path = g_build_filename(g_get_home_dir(), ".gtkrc-2.0", NULL);
    char* content = g_strdup_printf(
        "# DO NOT EDIT! This file will be overwritten by LXAppearance.\n"
        "# Any customization should be done in ~/.gtkrc-2.0.mine instead.\n\n"
        "gtk-theme-name=\"%s\"\n"
        "gtk-icon-theme-name=\"%s\"\n"
        "gtk-font-name=\"%s\"\n"
        "gtk-toolbar-style=%s\n"
        "gtk-toolbar-icon-size=%s\n"
        "gtk-cursor-theme-name=\"%s\"\n"
        "gtk-cursor-theme-size=%d\n"
#if GTK_CHECK_VERSION(2, 14, 0)
        "gtk-enable-event-sounds=%s\n"
        "gtk-enable-input-feedback-sounds=%s\n"
#endif
        "include \"%s/.gtkrc-2.0.mine\"\n",
        app.widget_theme,
        app.icon_theme,
        app.default_font,
        tb_styles[app.toolbar_style],
        tb_icon_sizes[app.toolbar_icon_size],
        app.cursor_theme,
        app.cursor_theme_size,
#if GTK_CHECK_VERSION(2, 14, 0)
        bool2str(app.enable_event_sound),
        bool2str(app.enable_input_feedback),
#endif
        g_get_home_dir());

    g_file_set_contents(file_path, content, -1, NULL);
    g_free(content);
    g_free(file_path);
}

static void lxappearance_save_lxsession()
{
    char* rel_path = g_strconcat("lxsession/", lxsession_name, "/desktop.conf", NULL);
    char* user_config_file = g_build_filename(g_get_user_config_dir(), rel_path, NULL);
    char* buf;
    int len;
    GKeyFile* kf = g_key_file_new();

    if(!g_key_file_load_from_file(kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        /* the user config file doesn't exist, create its parent dir */
        len = strlen(user_config_file) - strlen("/desktop.conf");
        user_config_file[len] = '\0';
        g_debug("user_config_file = %s", user_config_file);
        g_mkdir_with_parents(user_config_file, 0700);
        user_config_file[len] = '/';

        g_key_file_load_from_dirs(kf, rel_path, g_get_system_config_dirs(), NULL, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
    }

    g_free(rel_path);

    g_key_file_set_string( kf, "GTK", "sNet/ThemeName", app.widget_theme );
    g_key_file_set_string( kf, "GTK", "sGtk/FontName", app.default_font );

    g_key_file_set_string( kf, "GTK", "sNet/IconThemeName", app.icon_theme );

    g_key_file_set_string( kf, "GTK", "sGtk/CursorThemeName", app.cursor_theme );
    g_key_file_set_integer( kf, "GTK", "iGtk/CursorThemeSize", app.cursor_theme_size );
    save_cursor_theme_name();

    g_key_file_set_integer( kf, "GTK", "iGtk/ToolbarStyle", app.toolbar_style );
    g_key_file_set_integer( kf, "GTK", "iGtk/ToolbarIconSize", app.toolbar_icon_size );

#if GTK_CHECK_VERSION(2, 14, 0)
    g_key_file_set_integer( kf, "GTK", "iGtk/ToolbarStyle", app.toolbar_style );
    g_key_file_set_integer( kf, "GTK", "iGtk/ToolbarIconSize", app.toolbar_icon_size );

    /* "Net/SoundThemeName\0"      "gtk-sound-theme-name\0" */
    g_key_file_set_integer( kf, "GTK", "iNet/EnableEventSounds", app.enable_event_sound);
    g_key_file_set_integer( kf, "GTK", "iNet/EnableInputFeedbackSounds", app.enable_input_feedback);
#endif

    buf = g_key_file_to_data( kf, &len, NULL );
    g_key_file_free(kf);

    g_file_set_contents(user_config_file, buf, len, NULL);
    g_free(buf);
    g_free(user_config_file);
}

static void on_dlg_response(GtkDialog* dlg, int res, gpointer user_data)
{
    switch(res)
    {
    case GTK_RESPONSE_APPLY:

        if(app.use_lxsession)
            lxappearance_save_lxsession();
        else
            lxappearance_save_gtkrc();

        reload_all_programs();

        app.changed = FALSE;
        gtk_dialog_set_response_sensitive(app.dlg, GTK_RESPONSE_APPLY, FALSE);
        break;
    case 1: /* about dialog */
        {
            GtkBuilder* b = gtk_builder_new();
            if(gtk_builder_add_from_file(b, PACKAGE_UI_DIR "/about.ui", NULL))
            {
                GtkWidget* dlg = GTK_WIDGET(gtk_builder_get_object(b, "dlg"));
                gtk_dialog_run(dlg);
                gtk_widget_destroy(dlg);
            }
            g_object_unref(b);
        }
        break;
    default:
        gtk_main_quit();
    }
}

static void settings_init()
{
    GtkSettings* settings = gtk_settings_get_default();
    g_object_get(settings,
                "gtk-theme-name", &app.widget_theme,
                "gtk-font-name", &app.default_font,
                "gtk-icon-theme-name", &app.icon_theme,
                "gtk-cursor-theme-name", &app.cursor_theme,
                "gtk-cursor-theme-size", &app.cursor_theme_size,
                "gtk-toolbar-style", &app.toolbar_style,
                "gtk-toolbar-icon-size", &app.toolbar_icon_size,
#if GTK_CHECK_VERSION(2, 14, 0)
                "gtk-enable-event-sounds", &app.enable_event_sound,
                "gtk-enable-input-feedback-sounds", &app.enable_input_feedback,
#endif
                NULL);
    /* try to figure out cursor theme used. */
    if(!app.cursor_theme || g_strcmp0(app.cursor_theme, "default") == 0)
    {
        /* get the real theme name from default. */
        GKeyFile* kf = g_key_file_new();
        char* fpath = g_build_filename(g_get_home_dir(), ".icons/default/index.theme", NULL);
        gboolean ret = g_key_file_load_from_file(kf, fpath, 0, NULL);
        g_free(fpath);

        if(!ret)
            ret = g_key_file_load_from_data_dirs(kf, "icons/default/index.theme", NULL, 0, NULL);

        if(ret)
        {
            g_free(app.cursor_theme);
            app.cursor_theme = g_key_file_get_string(kf, "Icon Theme", "Inherits", NULL);
            g_debug("cursor theme name: %s", app.cursor_theme);
        }
        g_key_file_free(kf);
    }
}

int main(int argc, char** argv)
{
    GError* err = NULL;
    GtkBuilder* b;

    /* gettext support */
#ifdef ENABLE_NLS
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    /* initialize GTK+ and parse the command line arguments */
    if( G_UNLIKELY( ! gtk_init_with_args( &argc, &argv, "", option_entries, GETTEXT_PACKAGE, &err ) ) )
    {
        g_print( "Error: %s\n", err->message );
        return 1;
    }

    /* check if we're under LXSession */
    check_lxsession();

    /* load config values */
    settings_init();

    /* create GUI here */
    b = gtk_builder_new();
    if(!gtk_builder_add_from_file(b, PACKAGE_UI_DIR "/lxappearance.ui", NULL))
        return 1;

    app.dlg = GTK_WIDGET(gtk_builder_get_object(b, "dlg"));

    widget_theme_init(b);
    icon_theme_init(b);
    cursor_theme_init(b);
    other_init(b);

    g_signal_connect(app.dlg, "response", G_CALLBACK(on_dlg_response), NULL);

    gtk_window_present(GTK_WINDOW(app.dlg));
    g_object_unref(b);

    gtk_main();

    return 0;
}

void lxappearance_changed()
{
    if(!app.changed)
    {
        app.changed = TRUE;
        gtk_dialog_set_response_sensitive(app.dlg, GTK_RESPONSE_APPLY, TRUE);
    }
}
