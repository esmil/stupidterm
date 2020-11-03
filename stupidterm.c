/*
 * This file is part of stupidterm.
 * Copyright (C) 2001,2002 Red Hat, Inc.
 * Copyright (C) 2013-2015 Emil Renner Berthing
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <vte/vte.h>

#ifdef VTE_TYPE_REGEX
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#endif

static int exit_status = EXIT_FAILURE;

static void
screen_changed(GtkWidget *widget, GdkScreen *old_screen, gpointer userdata)
{
	GdkScreen *screen = gtk_widget_get_screen(widget);
	GdkVisual *visual = NULL;

	if (gdk_screen_is_composited(screen))
		visual = gdk_screen_get_rgba_visual(screen);

	if (!visual)
		visual = gdk_screen_get_system_visual(screen);

	gtk_widget_set_visual(widget, visual);
}

static void
window_title_changed(GtkWidget *widget, gpointer window)
{
	gtk_window_set_title(GTK_WINDOW(window),
			vte_terminal_get_window_title(VTE_TERMINAL(widget)));
}

static void
handle_bell(GtkWidget *widget, gpointer window)
{
	gtk_window_set_urgency_hint(GTK_WINDOW(window), TRUE);
}

static int
handle_focus_in(GtkWidget *widget, GdkEvent *event, gpointer window)
{
	gtk_window_set_urgency_hint(GTK_WINDOW(window), FALSE);
	return FALSE;
}

static void
destroy_and_quit(GtkWidget *window)
{
	gtk_widget_destroy(window);
	gtk_main_quit();
}

static void
delete_event(GtkWidget *window, GdkEvent *event, gpointer terminal)
{
	exit_status = EXIT_SUCCESS;
	destroy_and_quit(window);
}

static void
child_exited(GtkWidget *terminal, int status, gpointer window)
{
	exit_status = status;
	destroy_and_quit(GTK_WIDGET(window));
}

static int
button_pressed(GtkWidget *widget, GdkEvent *event, gpointer program)
{
	char *match;
	int tag;

	if (event->button.button != 3)
		return FALSE;

	match = vte_terminal_match_check_event(VTE_TERMINAL(widget), event, &tag);
	if (match != NULL) {
		GError *error = NULL;
		gchar *argv[3] = { program, match, NULL };

		if (!g_spawn_async(NULL, argv, NULL,
					G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL,
					NULL, NULL, NULL, &error)) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
		}
		g_free(match);
	}
	return FALSE;
}

static void
iconify_window(GtkWidget *widget, gpointer window)
{
	gtk_window_iconify(GTK_WINDOW(window));
}

static void
deiconify_window(GtkWidget *widget, gpointer window)
{
	gtk_window_deiconify(GTK_WINDOW(window));
}

static void
raise_window(GtkWidget *widget, gpointer window)
{
	GdkWindow *gdkwin = gtk_widget_get_window(GTK_WIDGET(window));

	if (gdkwin)
		gdk_window_raise(gdkwin);
}

static void
lower_window(GtkWidget *widget, gpointer window)
{
	GdkWindow *gdkwin = gtk_widget_get_window(GTK_WIDGET(window));

	if (gdkwin)
		gdk_window_lower(gdkwin);
}

static void
maximize_window(GtkWidget *widget, gpointer window)
{
	GdkWindow *gdkwin = gtk_widget_get_window(GTK_WIDGET(window));

	if (gdkwin)
		gdk_window_maximize(gdkwin);
}

static void
restore_window(GtkWidget *widget, gpointer window)
{
	GdkWindow *gdkwin = gtk_widget_get_window(GTK_WIDGET(window));

	if (gdkwin)
		gdk_window_unmaximize(gdkwin);
}

static void
refresh_window(GtkWidget *widget, gpointer window)
{
	GdkWindow *gdkwin = gtk_widget_get_window(GTK_WIDGET(window));
	GtkAllocation allocation;
	GdkRectangle rect;

	if (gdkwin) {
		gtk_widget_get_allocation(widget, &allocation);
		rect.x = rect.y = 0;
		rect.width = allocation.width;
		rect.height = allocation.height;
		gdk_window_invalidate_rect(gdkwin, &rect, TRUE);
	}
}

static void
resize_window(GtkWidget *widget, guint width, guint height, gpointer window)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
	glong row_count = vte_terminal_get_row_count(terminal);
	glong column_count = vte_terminal_get_column_count(terminal);
	glong char_width = vte_terminal_get_char_width(terminal);
	glong char_height = vte_terminal_get_char_height(terminal);
	gint owidth;
	gint oheight;
	GtkBorder padding;

	if (width < 2)
		width = 2;
	if (height < 2)
		height = 2;

	gtk_window_get_size(GTK_WINDOW(window), &owidth, &oheight);

	/* Take into account border overhead. */
	gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
			gtk_widget_get_state_flags(widget),
			&padding);

	owidth -= char_width * column_count + padding.left + padding.right;
	oheight -= char_height * row_count + padding.top + padding.bottom;
	gtk_window_resize(GTK_WINDOW(window),
			width + owidth, height + oheight);
}

static void
move_window(GtkWidget *widget, guint x, guint y, gpointer window)
{
	GdkWindow *gdkwin = gtk_widget_get_window(GTK_WIDGET(window));

	if (gdkwin)
		gdk_window_move(gdkwin, x, y);
}

static void
adjust_font_size(GtkWidget *widget, GtkWindow *window, gdouble factor)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
	glong rows = vte_terminal_get_row_count(terminal);
	glong columns = vte_terminal_get_column_count(terminal);
	glong char_width = vte_terminal_get_char_width(terminal);
	glong char_height = vte_terminal_get_char_height(terminal);
	gint owidth;
	gint oheight;
	gdouble scale;

	/* Take into account padding and border overhead. */
	gtk_window_get_size(window, &owidth, &oheight);
	owidth -= char_width * columns;
	oheight -= char_height * rows;

	scale = vte_terminal_get_font_scale(terminal);
	vte_terminal_set_font_scale(terminal, scale * factor);

	/* This above call will have changed the char size! */
	char_width = vte_terminal_get_char_width(terminal);
	char_height = vte_terminal_get_char_height(terminal);

	gtk_window_resize(window,
			columns * char_width + owidth,
			rows * char_height + oheight);
}

static void
increase_font_size(GtkWidget *widget, gpointer window)
{
	adjust_font_size(widget, GTK_WINDOW(window), 1.125);
}

static void
decrease_font_size(GtkWidget *widget, gpointer window)
{
	adjust_font_size(widget, GTK_WINDOW(window), 1. / 1.125);
}

static gboolean
handle_key_press(GtkWidget *widget, GdkEvent *event, gpointer window)
{
	GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();

	g_assert(event->type == GDK_KEY_PRESS);

	if ((event->key.state & modifiers) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
		switch (event->key.hardware_keycode) {
		case 21: /* + on US keyboards */
			increase_font_size(widget, window);
			return TRUE;
		case 20: /* - on US keyboards */
			decrease_font_size(widget, window);
			return TRUE;
		}
		switch (gdk_keyval_to_lower(event->key.keyval)) {
		case GDK_KEY_c:
			vte_terminal_copy_clipboard_format(
					(VteTerminal *)widget,
					VTE_FORMAT_TEXT);
			return TRUE;
		case GDK_KEY_v:
			vte_terminal_paste_clipboard((VteTerminal *)widget);
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
handle_selection_changed(VteTerminal *terminal, gpointer data)
{
	if (vte_terminal_get_has_selection(terminal))
		vte_terminal_copy_clipboard_format(
				terminal,
				VTE_FORMAT_TEXT);
	return TRUE;
}

struct config {
	gchar *config_file;
	gchar *font;
	gint lines;
	gchar *role;
	gboolean scroll_on_output;
	gboolean scroll_on_keystroke;
	gboolean mouse_autohide;
	gboolean sync_clipboard;
	gboolean urgent_on_bell;
	gchar **command_argv;
#ifdef VTE_TYPE_REGEX
	VteRegex *regex;
#else
	GRegex *regex;
#endif
	gchar *program;
	GdkRGBA background;
	GdkRGBA foreground;
	GdkRGBA highlight;
	GdkRGBA highlight_fg;
	GdkRGBA palette[16];
	gsize palette_size;
};

static gboolean
parse_color(GKeyFile *file, const gchar *filename,
		const gchar *key, gboolean required, GdkRGBA *out)
{
	GError *error = NULL;
	gchar *color = g_key_file_get_string(file, "colors", key, &error);
	gboolean ret;

	if (error) {
		if (error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND)
			g_printerr("Error parsing '%s': %s\n",
					filename, error->message);
		else if (required)
			g_printerr("Error parsing '%s': "
					"section [colors] must specify %s\n",
					filename, key);
		g_error_free(error);
		return FALSE;
	}
	ret = gdk_rgba_parse(out, color);
	if (!ret)
		g_printerr("Error parsing '%s': invalid color '%s'\n",
				filename, color);
	g_free(color);
	return ret;
}

static void
parse_colors(GKeyFile *file, const gchar *filename, struct config *conf)
{
	gchar name[8];
	unsigned int i;

	if (!parse_color(file, filename, "background", TRUE, &conf->background))
		return;
	if (!parse_color(file, filename, "foreground", TRUE, &conf->foreground))
		return;
	conf->palette_size = 2;

	parse_color(file, filename, "highlight", FALSE, &conf->highlight);
	parse_color(file, filename, "highlight-foreground", FALSE, &conf->highlight_fg);

	for (i = 0; i < 16; i++) {
		g_snprintf(name, 8, "color%u", i);
		if (!parse_color(file, filename, name, FALSE, &conf->palette[i]))
			break;
		conf->palette_size++;
	}
}

static void
parse_urlmatch(GKeyFile *file, const gchar *filename, struct config *conf)
{
	GError *error = NULL;
	gchar *regex;

	conf->program = g_key_file_get_string(file, "urlmatch", "program", &error);
	if (error) {
		if (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)
			g_printerr("Error parsing '%s': "
					"section [urlmatch] must specify program\n",
					filename);
		else
			g_printerr("Error parsing '%s': %s\n",
					filename, error->message);
		g_error_free(error);
		return;
	}

	regex = g_key_file_get_value(file, "urlmatch", "regex", &error);
	if (error) {
		if (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)
			g_printerr("Error parsing '%s': "
					"section [urlmatch] must specify regex\n",
					filename);
		else
			g_printerr("Error parsing '%s': %s\n",
					filename, error->message);
		g_error_free(error);
		g_free(conf->program);
		conf->program = NULL;
		return;
	}

#ifdef VTE_TYPE_REGEX
	conf->regex = vte_regex_new_for_match(regex, -1, PCRE2_MULTILINE, &error);
#else
	conf->regex = g_regex_new(regex, G_REGEX_MULTILINE, 0, &error);
#endif
	if (error) {
		g_printerr("Error compiling regex '%s': %s\n",
				regex, error->message);
		g_error_free(error);
		g_free(conf->program);
		conf->program = NULL;
	}
	g_free(regex);
}

static void
parse_file(struct config *conf, GOptionEntry *options)
{
	GKeyFile *file = g_key_file_new();
	GError *error = NULL;
	GOptionEntry *entry;
	gboolean option;
	gchar *filename;

	if (conf->config_file)
		filename = conf->config_file;
	else
		filename = g_build_filename(
				g_get_user_config_dir(),
				"stupidterm.ini", NULL);

	g_key_file_load_from_file(file, filename,
				G_KEY_FILE_NONE, &error);
	if (error) {
		switch (error->code) {
		case G_FILE_ERROR_NOENT:
		case G_KEY_FILE_ERROR_NOT_FOUND:
			break;
		default:
			g_printerr("Error opening '%s': %s\n", filename, error->message);
		}
		g_error_free(error);
		g_key_file_free(file);
		g_free(filename);
		return;
	}

	for (entry = options; entry->long_name; entry++) {
		switch (entry->arg) {
		case G_OPTION_ARG_NONE:
			option = g_key_file_get_boolean(
					file, "options",
					entry->long_name,
					&error);
			if (*((gboolean *)entry->arg_data))
				*((gboolean *)entry->arg_data) = !option;
			else
				*((gboolean *)entry->arg_data) = option;
			break;
		case G_OPTION_ARG_INT:
			if (*((gint *)entry->arg_data) == 0)
				*((gint *)entry->arg_data) = g_key_file_get_integer(
						file, "options",
						entry->long_name,
						&error);
			break;
		case G_OPTION_ARG_STRING:
			if (*((gchar **)entry->arg_data) == NULL)
				*((gchar **)entry->arg_data) = g_key_file_get_string(
						file, "options",
						entry->long_name,
						&error);
			break;
		default:
			continue;
		}
		if (error) {
			switch (error->code) {
			case G_KEY_FILE_ERROR_GROUP_NOT_FOUND:
			case G_KEY_FILE_ERROR_KEY_NOT_FOUND:
				break;
			default:
				g_printerr("Error parsing '%s': %s\n",
						filename, error->message);
			}
			g_error_free(error);
			error = NULL;
		}
	}

	if (g_key_file_has_group(file, "colors"))
		parse_colors(file, filename, conf);
	if (g_key_file_has_group(file, "urlmatch"))
		parse_urlmatch(file, filename, conf);

	g_key_file_free(file);
	g_free(filename);
}

static void
spawn_callback(VteTerminal *terminal, GPid pid, GError *error, gpointer data)
{
	GtkWidget *window = data;
	GtkWidget *widget = GTK_WIDGET(terminal);

	if (pid < 0) {
		g_printerr("%s\n", error->message);
		g_error_free(error);
		destroy_and_quit(window);
		return;
	}

	g_signal_connect(widget, "child-exited", G_CALLBACK(child_exited), window);
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), widget);

	gtk_widget_realize(widget);
	gtk_widget_show_all(window);
}

static gboolean
setup(int argc, char *argv[])
{
	struct config conf = {};
	GOptionEntry options[] = {
		{
			.long_name = "config",
			.short_name = 'c',
			.arg = G_OPTION_ARG_STRING,
			.arg_data = &conf.config_file,
			.description = "Specify alternative config file",
			.arg_description = "FILE",
		},
		{
			.long_name = "font",
			.short_name = 'f',
			.arg = G_OPTION_ARG_STRING,
			.arg_data = &conf.font,
			.description = "Specify a font to use",
			.arg_description = "FONT",
		},
		{
			.long_name = "lines",
			.short_name = 'n',
			.arg = G_OPTION_ARG_INT,
			.arg_data = &conf.lines,
			.description = "Specify the number of scrollback lines",
			.arg_description = "LINES",
		},
		{
			.long_name = "role",
			.short_name = 'r',
			.arg = G_OPTION_ARG_STRING,
			.arg_data = &conf.role,
			.description = "Set window role",
			.arg_description = "ROLE",
		},
		{
			.long_name = "scroll-on-output",
			.arg = G_OPTION_ARG_NONE,
			.arg_data = &conf.scroll_on_output,
			.description = "Toggle scroll on output",
		},
		{
			.long_name = "scroll-on-keystroke",
			.arg = G_OPTION_ARG_NONE,
			.arg_data = &conf.scroll_on_keystroke,
			.description = "Toggle scroll on keystroke",
		},
		{
			.long_name = "mouse-autohide",
			.arg = G_OPTION_ARG_NONE,
			.arg_data = &conf.mouse_autohide,
			.description = "Toggle autohiding the mouse cursor",
		},
		{
			.long_name = "sync-clipboard",
			.arg = G_OPTION_ARG_NONE,
			.arg_data = &conf.sync_clipboard,
			.description = "Update both primary and clipboard on selection",
		},
		{
			.long_name = "urgent-on-bell",
			.arg = G_OPTION_ARG_NONE,
			.arg_data = &conf.urgent_on_bell,
			.description = "Set window urgency hint on bell",
		},
		{
			.long_name = G_OPTION_REMAINING,
			.arg = G_OPTION_ARG_STRING_ARRAY,
			.arg_data = &conf.command_argv,
		},
		{} /* terminator */
	};
	GtkWidget *window;
	GtkWidget *widget;
	VteTerminal *terminal;
	GError *error = NULL;

	if (!gtk_init_with_args(&argc, &argv,
				"[-- COMMAND] - stupid terminal",
				options, NULL, &error)) {
		g_printerr("%s\n", error->message);
		g_error_free(error);
		return FALSE;
	}

	parse_file(&conf, options);

	/* Create a window to hold the scrolling shell, and hook its
	 * delete event to the quit function.. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	/* Fix transparency in GNOME/Mutter */
	gtk_widget_set_app_paintable(window, TRUE);

	/* Use rgba colour map if possible */
	screen_changed(window, NULL, NULL);
	g_signal_connect(window, "screen-changed", G_CALLBACK(screen_changed), NULL);

	if (conf.role) {
		gtk_window_set_role(GTK_WINDOW(window), conf.role);
		g_free(conf.role);
	}

	/* Create the terminal widget and add it to the window */
	widget = vte_terminal_new();
	terminal = VTE_TERMINAL(widget);
	gtk_container_add(GTK_CONTAINER(window), widget);

	/* Connect to the "window_title_changed" signal to set the main
	 * window's title. */
	g_signal_connect(widget, "window-title-changed",
			G_CALLBACK(window_title_changed), window);

	/* Connect to the "button-press" event. */
	if (conf.program)
		g_signal_connect(widget, "button-press-event",
				G_CALLBACK(button_pressed), conf.program);

	/* Connect to application request signals. */
	g_signal_connect(widget, "iconify-window",
			G_CALLBACK(iconify_window), window);
	g_signal_connect(widget, "deiconify-window",
			G_CALLBACK(deiconify_window), window);
	g_signal_connect(widget, "raise-window",
			G_CALLBACK(raise_window), window);
	g_signal_connect(widget, "lower-window",
			G_CALLBACK(lower_window), window);
	g_signal_connect(widget, "maximize-window",
			G_CALLBACK(maximize_window), window);
	g_signal_connect(widget, "restore-window",
			G_CALLBACK(restore_window), window);
	g_signal_connect(widget, "refresh-window",
			G_CALLBACK(refresh_window), window);
	g_signal_connect(widget, "resize-window",
			G_CALLBACK(resize_window), window);
	g_signal_connect(widget, "move-window",
			G_CALLBACK(move_window), window);

	/* Connect to font tweakage. */
	g_signal_connect(widget, "increase-font-size",
			G_CALLBACK(increase_font_size), window);
	g_signal_connect(widget, "decrease-font-size",
			G_CALLBACK(decrease_font_size), window);
	g_signal_connect(widget, "key-press-event",
			G_CALLBACK(handle_key_press), window);

	/* Connect to bell signal */
	if (conf.urgent_on_bell) {
		g_signal_connect(widget, "bell",
				G_CALLBACK(handle_bell), window);
		g_signal_connect(widget, "focus-in-event",
				G_CALLBACK(handle_focus_in), window);
	}

	/* Sync clipboard */
	if (conf.sync_clipboard)
		g_signal_connect(widget, "selection-changed",
				G_CALLBACK(handle_selection_changed), NULL);

	/* Set some defaults. */
	vte_terminal_set_scroll_on_output(terminal, conf.scroll_on_output);
	vte_terminal_set_scroll_on_keystroke(terminal, conf.scroll_on_keystroke);
	vte_terminal_set_mouse_autohide(terminal, conf.mouse_autohide);
	vte_terminal_set_cursor_blink_mode(terminal, VTE_CURSOR_BLINK_OFF);
	vte_terminal_set_cursor_shape(terminal, VTE_CURSOR_SHAPE_BLOCK);
	if (conf.lines)
		vte_terminal_set_scrollback_lines(terminal, conf.lines);
	if (conf.palette_size) {
		vte_terminal_set_colors(terminal,
				&conf.foreground,
				&conf.background,
				conf.palette,
				conf.palette_size - 2);
	}
	if (conf.highlight.alpha)
		vte_terminal_set_color_highlight(terminal, &conf.highlight);
	if (conf.highlight_fg.alpha)
		vte_terminal_set_color_highlight_foreground(terminal, &conf.highlight_fg);
	if (conf.font) {
		PangoFontDescription *desc = pango_font_description_from_string(conf.font);

		vte_terminal_set_font(terminal, desc);
		pango_font_description_free(desc);
		g_free(conf.font);
	}
	if (conf.regex) {
#ifdef VTE_TYPE_REGEX
		int id = vte_terminal_match_add_regex(terminal, conf.regex, 0);
		vte_regex_unref(conf.regex);
#else
		int id = vte_terminal_match_add_gregex(terminal, conf.regex, 0);
		g_regex_unref(conf.regex);
#endif
		vte_terminal_match_set_cursor_name(terminal, id, "pointer");
	}

	if (conf.command_argv == NULL || conf.command_argv[0] == NULL) {
		g_strfreev(conf.command_argv);
		conf.command_argv = g_malloc(2*sizeof(gchar *));
		conf.command_argv[0] = vte_get_user_shell();
		conf.command_argv[1] = NULL;

		if (conf.command_argv[0] == NULL || conf.command_argv[0][0] == '\0') {
			const gchar *shell = g_getenv("SHELL");

			if (shell == NULL || shell[0] == '\0')
				shell = "/bin/sh";

			g_free(conf.command_argv[0]);
			conf.command_argv[0] = g_strdup(shell);
		}
	} else {
		gchar *title = g_strjoinv(" ", conf.command_argv);

		gtk_window_set_title(GTK_WINDOW(window), title);
		g_free(title);
	}

	vte_terminal_spawn_async(terminal,
			VTE_PTY_DEFAULT,
			NULL,
			conf.command_argv,
			NULL,
			G_SPAWN_SEARCH_PATH,
			NULL, NULL, NULL,
			-1,
			NULL,
			&spawn_callback, window);
	g_strfreev(conf.command_argv);
	return TRUE;
}

int
main(int argc, char *argv[])
{
	if (setup(argc, argv))
		gtk_main();

	return exit_status;
}
