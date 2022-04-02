/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the GTK specific pieces of the display
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtkdatabox.h>
#include <gtkdatabox_points.h>
#include <gtkdatabox_lines.h>
#include <math.h>

#include <gdk-pixbuf/gdk-pixdata.h>

#include <string.h>
#include "xoscope.h"            /* program defaults */
#include "display.h"
#include "func.h"
#include "file.h"
#include "xoscope_gtk.h"

#include "builtins.h"

GtkWidget *glade_window = NULL;
GtkWidget *comedi_options_dialog = NULL;
GtkWidget *alsa_options_dialog = NULL;
GtkWidget *databox = NULL;

int fixing_widgets = 0;

char my_filename[FILENAME_MAX+1] = "";
//GdkFont *font;
char fontname[80] = DEF_FX;
char fonts[] = "xlsfonts";

/* emulate several libsx function in GTK */
int OpenDisplay(int argc, char *argv[])
{
    gtk_init(&argc, &argv);
    return(argc);
}

/* set up some common event handlers */

void delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_main_quit ();
}

gint key_press_event(GtkWidget *widget, GdkEventKey *event)
{
    if (event->keyval == GDK_KEY_Tab) {
        handle_key('\t');
    } else if (event->length == 1) {
        handle_key(event->string[0]);
    }

    return TRUE;
}

/* simple event callback that emulates the user hitting the given key */
void hit_key(GtkWidget *w, gint data)
{
    if (fixing_widgets) return;
    handle_key(data);
}

int is_GtkWidget(GType Type) {
    GType GtkWidgetType = g_type_from_name("GtkWidget");

    while (Type) {
        if (Type == GtkWidgetType) {
            return 1;
        }
        Type = g_type_parent(Type);
    }
    return 0;
}

/* gtk_builder sets the "name" property initially to the class-name, check with: gtk_widget_get_name()*/
/* the name defined by glade can be obtained with : gtk_buildable_get_name() */
/* we use gtk_buildable_set_buildable_property() to set the "name" property to what we defined in glade */
/* Without seting the "name" property, we can't set the styles from the rc-file ! */
/* ps: we could set the widget "name" property in Glade too, see the "Common" tab 
 * for each one.
 */

void set_name_property(GtkWidget *widget, GtkBuilder *builder)
{
    GValue      g_value = G_VALUE_INIT;

    g_value_init (&g_value, G_TYPE_STRING);
    g_value_set_string (&g_value, gtk_buildable_get_name(GTK_BUILDABLE (widget)));
    gtk_buildable_set_buildable_property(GTK_BUILDABLE (widget), builder, "name", &g_value);
}

#define GLADE_HOOKUP_OBJECT(component,widget,name)                      \
    g_object_set_data_full (                                            \
                            G_OBJECT (component),                       \
                            name,                                       \
                            g_object_ref ((GtkWidget*)widget),          \
                            (GDestroyNotify) g_object_unref             \
                                                                )

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name)       \
    g_object_set_data (G_OBJECT (component), name, widget)

void store_reference(GtkWidget* widget)
{
    GtkWidget *parent, *child;

    child = widget;
    while(TRUE){
        parent = (GtkWidget*)gtk_widget_get_parent(child);
        if(parent == NULL){
            parent = child;
            break;
        }
        child = parent;
    }

    if(parent == widget){
        GLADE_HOOKUP_OBJECT_NO_REF(widget, widget, gtk_widget_get_name(widget));
    }
    else{
        GLADE_HOOKUP_OBJECT(parent, widget, gtk_widget_get_name(widget));
    }
}

GtkBuilder * builder;

GtkWidget * lookup_widget(const gchar * widget_name)
{
    GtkWidget *found_widget;

    found_widget = (GtkWidget *) gtk_builder_get_object(builder, widget_name);

    if (!found_widget) {
        g_warning ("Widget not found: %s", widget_name);
    }

    return found_widget;
}

GtkWidget * create_main_window(void)
{
    GError                      *err = NULL;
    GSList                      *gslWidgets, *iterator = NULL;
    static GtkCssProvider *css = NULL;
    /* extern char              gladestring[]; */
    if (css == NULL)
	    css = gtk_css_provider_new ();

    builder = gtk_builder_new ();

    if(0 == gtk_builder_add_from_string (builder, gladestring, -1, &err)){
        fprintf(stderr, "Error adding build from string. Error: %s\n", err->message);
        exit(1);
    }

    glade_window = LU("main_window");
    comedi_options_dialog = LU("comedi_dialog");
    // alsa_options_dialog = LU("alsa_options_dialog"); FIXME: where is it?
    databox = LU("databox");

    /* I don't like the added complexity of having to install rc files
     * (and the related problems if they can't be found), so instead of
     * loading 'xoscope.css', it gets compiled into the program and
     * loaded as a string.
     */
    gtk_css_provider_load_from_data (css, xoscope_css, -1, &err);
    if (err)
        fprintf(stderr, "Error adding CSS from string. Error: %s\n", err->message);
    gtk_style_context_add_provider_for_screen (
		    gtk_window_get_screen (GTK_WINDOW (glade_window)),
		    GTK_STYLE_PROVIDER (css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION); 
#if 1
    /* Run this loop TWICE to make sure all rc settings take. */
    gslWidgets = gtk_builder_get_objects (builder);
    for (iterator = gslWidgets; iterator; iterator = iterator->next) {
        if(is_GtkWidget(G_TYPE_FROM_INSTANCE(iterator->data))){
            set_name_property((GtkWidget*)(iterator->data), builder);
            store_reference((GtkWidget*)(iterator->data));

            /* XXX There's a bug in glib that can deadlock a program during class initialization:
             *
             *    https://bugs.launchpad.net/ubuntu/+source/glib2.0/+bug/1179554
             *
             * In xoscope, it manifests itself when the IBus input method initializes for entry
             * boxes.  Getting the entry's PangoLayout here, before calling gtk_main(), seems to
             * force everything to initialize while we're still single threaded.
             */

            if (GTK_IS_ENTRY((GtkWidget*)(iterator->data))) {
                gtk_entry_get_layout((GtkEntry*)(iterator->data));
            }
        }
        else{
            continue;
        }
    }
    g_slist_free(gslWidgets);
    gslWidgets = gtk_builder_get_objects(builder);
    for (iterator = gslWidgets; iterator; iterator = iterator->next) {
        if(is_GtkWidget(G_TYPE_FROM_INSTANCE(iterator->data))){
            set_name_property((GtkWidget*)(iterator->data), builder);
            store_reference((GtkWidget*)(iterator->data));
        }
        else{
            continue;
        }
    }
    g_slist_free(gslWidgets);
#endif

    gtk_builder_connect_signals(builder, NULL);

    return(glade_window);
}

GtkWidget * create_comedi_dialog (void)
{
    return(comedi_options_dialog);
}

void yes_sel(GtkWidget *w, GtkWidget *win)
{
    gtk_widget_destroy(GTK_WIDGET(win));
}


void savefile_ok_sel()
{
    GtkWidget *window, *label;
    struct stat buff;
    int response;

    if (!stat(my_filename, &buff)) {
        window = gtk_dialog_new_with_buttons (
			"Xoscope - Overwrite!",
			GTK_WINDOW (glade_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			"Yes", GTK_RESPONSE_ACCEPT,
			"No", GTK_RESPONSE_REJECT,
			NULL);
        sprintf(error, "\n  Overwrite existing file %s?  \n", my_filename);
        label = gtk_label_new(error);
        gtk_box_pack_start(GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG(window))),
		       	label, TRUE, TRUE, 0);
        gtk_widget_show(label);
        response = gtk_dialog_run (GTK_DIALOG (window));
	if (response == GTK_RESPONSE_ACCEPT)
    		savefile(my_filename);
	gtk_widget_destroy (window);
    } else
        savefile(my_filename);
}

/* get a file name for load (0) or save (1) */

void LoadSaveFile(int save)
{
    GtkWidget *filew;
    GtkFileChooserAction action;
    int res;
    gchar *fname;

    action = save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN;
    filew = gtk_file_chooser_dialog_new (save ? "Save File": "Load File",
		    GTK_WINDOW (glade_window),
		    action,
		    "Cancel",
		    GTK_RESPONSE_CANCEL,
		    save ? "Save" : "Load",
		    GTK_RESPONSE_ACCEPT,
		    NULL);

    if (my_filename[0] != '\0')
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filew), (gchar *)my_filename);

    res = gtk_dialog_run (GTK_DIALOG (filew));
    if (res == GTK_RESPONSE_ACCEPT) {
      fname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filew));
      strncpy(my_filename, fname, FILENAME_MAX);
      g_free (fname);

      if (save)
	      savefile_ok_sel ();
      else
    	loadfile(my_filename);
    }
    gtk_widget_destroy (filew);
}

void ExternCommand(void)
{
    GtkWidget *window;
    int response;

    if (fixing_widgets) return;

    window = LU("extcom_dialog");
    gtk_widget_show (window);

    response = gtk_dialog_run (GTK_DIALOG (window));
    if (response == GTK_RESPONSE_ACCEPT)
    	startcommand (gtk_entry_get_text (GTK_ENTRY(LU("extcom_entry"))));
    gtk_widget_hide (window);
}

void perl_function_help(GtkWidget *w, GtkEntry *command)
{
    GtkWidget *window;
    GtkWidget *text;
    GtkTextBuffer *textbuffer;
    GtkTextIter iter;

    window = LU("help_window");
    gtk_window_set_title (GTK_WINDOW (window), "Xoscope - Perl function help");
    text = LU("help_text_view");
    textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
    gtk_text_buffer_set_text (textbuffer, "", -1);

    gtk_text_buffer_get_end_iter(textbuffer, &iter);
    gtk_text_buffer_insert(textbuffer, &iter, operl_help_text, strlen(operl_help_text));

    gtk_widget_show (window);
}

void PerlFunction(void)
{
    GtkWidget *window;
    GtkComboBoxText *combo;
    int response;
    static gchar *function = NULL;

    if (fixing_widgets) return;
    window = LU("perl_dialog");
    gtk_widget_show (window);
    combo = GTK_COMBO_BOX_TEXT (LU("perl_combo"));
    response = gtk_dialog_run (GTK_DIALOG (window));
    if (response == GTK_RESPONSE_ACCEPT) {
        if (function != NULL)
	    g_free (function);
	function = gtk_combo_box_text_get_active_text (combo);
    	start_perl_function (function);
    } else if (response == GTK_RESPONSE_HELP)
        perl_function_help (window, NULL);
    gtk_widget_hide (window);
}

/* key bindings callbacks */

void datasource(GtkWidget *widget, gpointer data)
{
    if (fixing_widgets) return;
    datasrc_force_open((DataSrc *) data);
    clear();
}

void datasource_cb_wrapper (GtkWidget *widget, gint data)
{
	char *name = NULL;
	int i;
	if (data == 1) // COMEDI
		name = g_strdup ("COMEDI");
	else if (data == 2) // ALSA
		name = g_strdup ("ALSA");
	for (i = 0; i < ndatasrcs; i++) {
		if (g_strcmp0 (name, datasrcs[i]->name) == 0)
			break;
	}	
	if (i < ndatasrcs)
		datasource (widget, (gpointer) (datasrcs[i]));
	else
		datasource (widget, NULL);
}

void option_dialog(GtkWidget *w, gint data)
{
    if (fixing_widgets) return;
    if (datasrc && datasrc->gtk_options) datasrc->gtk_options();
}

void plotmode(GtkWidget *w, gint data)
{
    if (fixing_widgets) return;
    scope.plot_mode = data;
    update_text();
    show_data();
}

void scrollmode(GtkWidget *w, gint data)
{
    if (fixing_widgets) return;
    scope.scroll_mode = data;
    update_text();
    show_data();
}

void runmode(GtkWidget *w, gint data)
{
    if (fixing_widgets) return;
    scope.run = data;
    clear();
}

void graticule(GtkWidget *w, gint data)
{
    if (fixing_widgets) return;
    if (data < 2)
        scope.behind = data;
    else
        scope.grat = data - 2;
    update_text();
    show_data();
}

void change_trigger(int trigch, int trig, int trige)
{
    /* Triggering change.  Try the new trigger, and if it doesn't work, try the old one's
     * settings again, if they don't work (!) leave the trigger off.
     */
    if (trige == 0) {
        if (datasrc && datasrc->clear_trigger) datasrc->clear_trigger();
        scope.trige = 0;
    } else if (datasrc && datasrc->set_trigger
               && datasrc->set_trigger(trigch, &trig, trige)) {
        scope.trigch = trigch;
        scope.trig = trig;
        scope.trige = trige;
    } else if (datasrc && datasrc->set_trigger && datasrc->clear_trigger
               && !datasrc->set_trigger(scope.trigch, &scope.trig, scope.trige)){
        datasrc->clear_trigger();
        scope.trige = 0;
    }
}

void trigger(GtkWidget *w, gint data)
{
    if (fixing_widgets) return;

    if (data >= 'a' && data <= 'c') {
        change_trigger(scope.trigch, scope.trig, data - 'a');
    }

    if (data >= '1' && data <= '8') {
        change_trigger(data - '1', scope.trig, scope.trige);
    }

    clear();
}

/* Radio buttons cause 2 events: the deselect and the select.  Selecting a built-in after external
 * causes an extraneous command dialog but I can't figure out how to get rid of it.
 */
void mathselect(GtkWidget *w, gint data)
{
    if (fixing_widgets) return;
    if (scope.select > 1) {
        if (data == '$') {
            PerlFunction();
        } else if (data == '!') {
            ExternCommand();
        } else {
            function_bynum_on_channel(data - '0', &ch[scope.select]);
            ch[scope.select].show = 1;
        }
        clear();
    }
}

void setbits(GtkWidget *w, gint data)
{
    if (fixing_widgets) return;
    ch[scope.select].bits = data;
    clear();
}

void setscale(GtkWidget *w, gint data)
{
    int mul[] = {50, 20, 10, 5, 2, 1, 1, 1,  1,  1,  1};
    int div[] = {1,   1,  1, 1, 1, 1, 2, 5, 10, 20, 50};

    ch[scope.select].scale = (double) mul[data] / div[data];
    clear();
}

void set_trigger_level(GtkWidget *w, gint data)
{
    change_trigger(scope.trigch, data, scope.trige);

    clear();
}

void set_trigger_step(GtkWidget *w, gint data)
{
    scope.trigger_step = data;
}

void setposition(GtkWidget *w, gint data)
{
    ch[scope.select].pos = data;

    clear();
}

void set_position_step(GtkWidget *w, gint data)
{
    if (data == 0)
	    scope.position_step = 0.1;
    else
	    scope.position_step = data;
}

void help(GtkWidget *w, gint data)
{
    char c;
    char charbuffer[16];
    int charbuffer_len = 0;
    int running_tag = 0;
    FILE *p;

    GtkWidget *window;
    GtkWidget *text;
    GtkTextBuffer *textbuffer;
    GtkTextIter iter, start;
    GtkTextTag *underline_tag, *bold_tag;

    window = LU("help_window");
    gtk_window_set_title (GTK_WINDOW (window), "Xoscope - help");
    text = LU("help_text_view");
    textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
    gtk_text_buffer_set_text (textbuffer, "", -1);

    bold_tag = gtk_text_buffer_create_tag (textbuffer, NULL,
                                           "weight", PANGO_WEIGHT_BOLD, NULL);
    underline_tag = gtk_text_buffer_create_tag (textbuffer, NULL,
                                                "underline", PANGO_UNDERLINE_SINGLE, NULL);

    /* Now run 'man' and copy its output into the text buffer.  We use an intermediate 'charbuffer'
     * for two reasons: to handle backspaces (for overstrikes or underlines) which get converted
     * into formatting tags, and to ensure that multibyte UTF-8 characters get inserted as a unit;
     * otherwise GTK+ 3 complains.
     *
     * XXX this can hang the program if the 'man' runs slowly
     *
     * XXX we're counting on GNU man to handle the '-Tutf8' flag; other versions of man might not do
     * this.
     *
     * XXX the tags are attached individually to each character; it would be better to attach them
     * to entire words, as this could conceivably affect formating, especially of underlines
     */

    gtk_text_buffer_get_end_iter(textbuffer, &iter);
    if ((p = popen(HELPCOMMAND, "r")) != NULL) {

        while ((c = fgetc(p)) != EOF) {
            if (c == '\b') {
                if (charbuffer[0] == '_') running_tag = 1;
                else running_tag = 2;
                charbuffer_len = 0;
                continue;
            }
            if (c < 0) {
                charbuffer[charbuffer_len ++] = c;
            } else {
                if (charbuffer_len > 0) {
                    gtk_text_buffer_insert(textbuffer, &iter,
                                           charbuffer, charbuffer_len);
                    if (running_tag) {
                        start = iter;
                        gtk_text_iter_backward_char(&start);
                        if (running_tag == 1) {
                            gtk_text_buffer_apply_tag (textbuffer, underline_tag,
                                                       &start, &iter);
                        } else if (running_tag == 2) {
                            gtk_text_buffer_apply_tag (textbuffer, bold_tag,
                                                       &start, &iter);
                        }
                        running_tag = 0;
                    }
                }
                charbuffer[0] = c;
                charbuffer_len = 1;
            }
        }
        pclose(p);
    }

    gtk_widget_show (window);
}

/* Mapping to assign functions and data to each activated GtkMenuItem
 */
#define ESC 27
typedef struct _KeyBindings {
	gchar cmd[20];
	void (*cb)(GtkWidget *, gint);
	guint dt;
} KeyBindings;
static const KeyBindings kb[] = {
	{.cmd = "file_open",	.dt = '@',	.cb = hit_key},
	{.cmd = "file_save",	.dt = '#',	.cb = hit_key},
	{.cmd = "device_none",	.dt = 0,	.cb = datasource_cb_wrapper},
	{.cmd = "device_comedi",.dt = 1,	.cb = datasource_cb_wrapper},
	{.cmd = "device_alsa",	.dt = 2,	.cb = datasource_cb_wrapper},
	{.cmd = "device_options",.dt = 0,	.cb = option_dialog},
	{.cmd = "file_quit",	.dt = ESC,	.cb = hit_key},
	{.cmd = "channel_1",	.dt = '1',	.cb = hit_key},
	{.cmd = "channel_2",	.dt = '2',	.cb = hit_key},
	{.cmd = "channel_3",	.dt = '3',	.cb = hit_key},
	{.cmd = "channel_4",	.dt = '4',	.cb = hit_key},
	{.cmd = "channel_5",	.dt = '5',	.cb = hit_key},
	{.cmd = "channel_6",	.dt = '6',	.cb = hit_key},
	{.cmd = "channel_7",	.dt = '7',	.cb = hit_key},
	{.cmd = "channel_8",	.dt = '8',	.cb = hit_key},
	{.cmd = "channel_show",	.dt = '\t',	.cb = hit_key},
	{.cmd = "scale_up",	.dt = '}',	.cb = hit_key},
	{.cmd = "scale_down",	.dt = '{',	.cb = hit_key},
	{.cmd = "scale_50",	.dt = 0,	.cb = setscale},
	{.cmd = "scale_20",	.dt = 1,	.cb = setscale},
	{.cmd = "scale_10",	.dt = 2,	.cb = setscale},
	{.cmd = "scale_5",	.dt = 3,	.cb = setscale},
	{.cmd = "scale_2",	.dt = 4,	.cb = setscale},
	{.cmd = "scale_1",	.dt = 5,	.cb = setscale},
	{.cmd = "scale_500m",	.dt = 6,	.cb = setscale},
	{.cmd = "scale_200m",	.dt = 7,	.cb = setscale},
	{.cmd = "scale_100m",	.dt = 8,	.cb = setscale},
	{.cmd = "scale_50m",	.dt = 9,	.cb = setscale},
	{.cmd = "scale_20m",	.dt = 10,	.cb = setscale},
	{.cmd = "position_up",	.dt = ']',	.cb = hit_key},
	{.cmd = "position_down",.dt = '[',	.cb = hit_key},
	{.cmd = "position_step_01",.dt = 0,	.cb = set_position_step},
	{.cmd = "position_step_1",.dt = 1,	.cb = set_position_step},
	{.cmd = "position_step_10",.dt = 10,	.cb = set_position_step},
	{.cmd = "position_0",	.dt = 0,	.cb = setposition},
	{.cmd = "bits_analog",	.dt = 0,	.cb = setbits},
	{.cmd = "bits_8",	.dt = 8,	.cb = setbits},
	{.cmd = "bits_10",	.dt = 10,	.cb = setbits},
	{.cmd = "bits_12",	.dt = 12,	.cb = setbits},
	{.cmd = "bits_14",	.dt = 14,	.cb = setbits},
	{.cmd = "bits_16",	.dt = 16,	.cb = setbits},
	/* this math will need hacked if functions are added/changed in func.c */
	{.cmd = "math_prev",	.dt = ':',	.cb = hit_key},
	{.cmd = "math_next",	.dt = ';',	.cb = hit_key},
	{.cmd = "math_inv1",	.dt = 0,	.cb = mathselect},
	{.cmd = "math_inv2",	.dt = 1,	.cb = mathselect},
	{.cmd = "math_inv2",	.dt = 2,	.cb = mathselect},
	{.cmd = "math_delta",	.dt = 3,	.cb = mathselect},
	{.cmd = "math_avg",	.dt = 4,	.cb = mathselect},
	{.cmd = "math_fft1",	.dt = 5,	.cb = mathselect},
	{.cmd = "math_fft2",	.dt = 6,	.cb = mathselect},
	{.cmd = "math_perl",	.dt = '$',	.cb = mathselect},
	{.cmd = "math_external",.dt = '!',	.cb = mathselect},
	{.cmd = "store_a",	.dt = 'A',	.cb = hit_key},
	{.cmd = "store_b",	.dt = 'B',	.cb = hit_key},
	{.cmd = "store_c",	.dt = 'C',	.cb = hit_key},
	{.cmd = "store_d",	.dt = 'D',	.cb = hit_key},
	{.cmd = "store_e",	.dt = 'E',	.cb = hit_key},
	{.cmd = "store_f",	.dt = 'F',	.cb = hit_key},
	{.cmd = "store_g",	.dt = 'G',	.cb = hit_key},
	{.cmd = "store_h",	.dt = 'H',	.cb = hit_key},
	{.cmd = "store_i",	.dt = 'I',	.cb = hit_key},
	{.cmd = "store_j",	.dt = 'J',	.cb = hit_key},
	{.cmd = "store_k",	.dt = 'K',	.cb = hit_key},
	{.cmd = "store_l",	.dt = 'L',	.cb = hit_key},
	{.cmd = "store_m",	.dt = 'M',	.cb = hit_key},
	{.cmd = "store_n",	.dt = 'N',	.cb = hit_key},
	{.cmd = "store_o",	.dt = 'O',	.cb = hit_key},
	{.cmd = "store_p",	.dt = 'P',	.cb = hit_key},
	{.cmd = "store_q",	.dt = 'Q',	.cb = hit_key},
	{.cmd = "store_r",	.dt = 'R',	.cb = hit_key},
	{.cmd = "store_s",	.dt = 'S',	.cb = hit_key},
	{.cmd = "store_t",	.dt = 'T',	.cb = hit_key},
	{.cmd = "store_u",	.dt = 'U',	.cb = hit_key},
	{.cmd = "store_v",	.dt = 'V',	.cb = hit_key},
	{.cmd = "store_w",	.dt = 'W',	.cb = hit_key},
	{.cmd = "store_x",	.dt = 'X',	.cb = hit_key},
	{.cmd = "store_y",	.dt = 'Y',	.cb = hit_key},
	{.cmd = "store_z",	.dt = 'Z',	.cb = hit_key},
	{.cmd = "recall_a",	.dt = 'a',	.cb = hit_key},
	{.cmd = "recall_b",	.dt = 'b',	.cb = hit_key},
	{.cmd = "recall_c",	.dt = 'c',	.cb = hit_key},
	{.cmd = "recall_d",	.dt = 'd',	.cb = hit_key},
	{.cmd = "recall_e",	.dt = 'e',	.cb = hit_key},
	{.cmd = "recall_f",	.dt = 'f',	.cb = hit_key},
	{.cmd = "recall_g",	.dt = 'g',	.cb = hit_key},
	{.cmd = "recall_h",	.dt = 'h',	.cb = hit_key},
	{.cmd = "recall_i",	.dt = 'i',	.cb = hit_key},
	{.cmd = "recall_j",	.dt = 'j',	.cb = hit_key},
	{.cmd = "recall_k",	.dt = 'k',	.cb = hit_key},
	{.cmd = "recall_l",	.dt = 'l',	.cb = hit_key},
	{.cmd = "recall_m",	.dt = 'm',	.cb = hit_key},
	{.cmd = "recall_n",	.dt = 'n',	.cb = hit_key},
	{.cmd = "recall_o",	.dt = 'o',	.cb = hit_key},
	{.cmd = "recall_p",	.dt = 'p',	.cb = hit_key},
	{.cmd = "recall_q",	.dt = 'q',	.cb = hit_key},
	{.cmd = "recall_r",	.dt = 'r',	.cb = hit_key},
	{.cmd = "recall_s",	.dt = 's',	.cb = hit_key},
	{.cmd = "recall_t",	.dt = 't',	.cb = hit_key},
	{.cmd = "recall_u",	.dt = 'u',	.cb = hit_key},
	{.cmd = "recall_v",	.dt = 'v',	.cb = hit_key},
	{.cmd = "recall_w",	.dt = 'w',	.cb = hit_key},
	{.cmd = "recall_x",	.dt = 'x',	.cb = hit_key},
	{.cmd = "recall_y",	.dt = 'y',	.cb = hit_key},
	{.cmd = "recall_z",	.dt = 'z',	.cb = hit_key},
	{.cmd = "trigger_off",	.dt = 'a',	.cb = trigger},
	{.cmd = "trigger_rising",.dt = 'b',	.cb = trigger},
	{.cmd = "trigger_falling",.dt = 'c',	.cb = trigger},
	{.cmd = "trigger_1",	.dt = '1',	.cb = trigger},
	{.cmd = "trigger_2",	.dt = '2',	.cb = trigger},
	{.cmd = "trigger_3",	.dt = '3',	.cb = trigger},
	{.cmd = "trigger_4",	.dt = '4',	.cb = trigger},
	{.cmd = "trigger_5",	.dt = '5',	.cb = trigger},
	{.cmd = "trigger_6",	.dt = '6',	.cb = trigger},
	{.cmd = "trigger_7",	.dt = '7',	.cb = trigger},
	{.cmd = "trigger_8",	.dt = '8',	.cb = trigger},
	{.cmd = "trigger_up",	.dt = '=',	.cb = hit_key},
	{.cmd = "trigger_down",	.dt = '-',	.cb = hit_key},
	{.cmd = "trigger_step_1",.dt = 1,	.cb = set_trigger_step},
	{.cmd = "trigger_step_10",.dt = 10,	.cb = set_trigger_step},
	{.cmd = "trigger_0",	.dt = 0,	.cb = set_trigger_level},
	{.cmd = "scope_run",	.dt = 1,	.cb = runmode},
	{.cmd = "scope_wait",	.dt = 2,	.cb = runmode},
	{.cmd = "scope_stop",	.dt = 0,	.cb = runmode},
	{.cmd = "time_slower",	.dt = '9',	.cb = hit_key},
	{.cmd = "time_faster",	.dt = '0',	.cb = hit_key},
	{.cmd = "sample_slower",.dt = '(',	.cb = hit_key},
	{.cmd = "sample_faster",.dt = ')',	.cb = hit_key},
	{.cmd = "scope_refresh",.dt = '\n',	.cb = hit_key},
	{.cmd = "plot_point",	.dt = 0,	.cb = plotmode},
	{.cmd = "plot_line",	.dt = 1,	.cb = plotmode},
	{.cmd = "plot_step",	.dt = 2,	.cb = plotmode},
	{.cmd = "scroll_sweep",	.dt = 0,	.cb = scrollmode},
	{.cmd = "scroll_accumulate",.dt = 1,	.cb = scrollmode},
	{.cmd = "scroll_strip",	.dt = 2,	.cb = scrollmode},
	{.cmd = "graticule_front",.dt = 0,	.cb = graticule},
	{.cmd = "graticule_behind",.dt = 1,	.cb = graticule},
	{.cmd = "graticule_none",.dt = 2,	.cb = graticule},
	{.cmd = "graticule_minor",.dt = 3,	.cb = graticule},
	{.cmd = "graticule_all",.dt = 4,	.cb = graticule},
	{.cmd = "scope_cursors",.dt = '\'',	.cb = hit_key},
	{.cmd = "help_keys",	.dt = '?',	.cb = hit_key},
	{.cmd = "help_manual",	.dt = 0,	.cb = help}
};
static const int kb_n = sizeof (kb) / sizeof (KeyBindings);

/* Callback to assign procedures and data to each activated GtkMenuItem
 */
void key_bindings (GtkWidget *widget, guint data)
{
	const char *wname = gtk_widget_get_name (widget);
	int i;

	for (i = 0; i < kb_n; i++)
	{
		if (g_strcmp0 (wname, kb[i].cmd) == 0)
		{
			kb[i].cb (widget, kb[i].dt);
			return;
		}
	}
	g_warning ("Binding key not found for menu item: %s", wname);
}

/* Callback to assign procedures and data to each channel button
 */
void on_channel_button_toggled (GtkToggleButton *toggle, gpointer data)
{
	const char *wname;

	if (fixing_widgets) return;
	wname = gtk_widget_get_name (GTK_WIDGET (toggle));
	handle_key (wname[2]);
	handle_key ('\t');
}

void set_fixing_widgets (gboolean on_off)
{
	fixing_widgets = on_off;
}

/* set current state colors, labels, and check marks on widgets */
void fix_widgets(void)
{
    GtkWidget *widget;
    GtkLabel *label;
    char wg_id[40]; 
    char buf[32];
    int i;
    char mem_id;

    fixing_widgets = 1;

    sprintf(wg_id, "channel_%i", scope.select + 1);
    if ((widget = LU(wg_id)))
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);

    if (ch[scope.select].bits == 0) {
        if ((widget = LU("bits_analog")))
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);
    } else {
        sprintf(wg_id, "bits_%i", ch[scope.select].bits);
	if ((widget = LU(wg_id)))
	    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);
    }

    if ((widget = LU("channel_show")))
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), ch[scope.select].show);

    if ((widget = LU("device_options")))
        gtk_widget_set_sensitive (widget, datasrc && datasrc->gtk_options != NULL);

    switch (scope.trige)
    {
	    case 1: widget = LU("trigger_rising"); break;
	    case 2: widget = LU("trigger_falling"); break;
	    default: widget = LU("trigger_off");
    }
    if (widget)
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);

    /* The trigger channels.  There are eight of them defined, but we only show as many
     * as datasrc->nchans() indicate are available. (We assume datasrc->nchans() <= 8).
     * Set their labels to the names in the Signal arrays; make them all insensitive if
     * triggering is turned off, and select the one corresponding to the triggered 
     * channel.
     */
    if ((widget = LU("trigger_1"))) {
        for (i = 1; i <= 8; i++) {
    	    sprintf(wg_id, "trigger_%i", i);
	    widget = LU(wg_id);
	    if (widget == NULL)
		    continue;
            if (!datasrc || i > datasrc->nchans()) {
                gtk_widget_hide(widget);
            } else {
                gtk_widget_show(widget);
                label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (widget)));
                gtk_label_set_text(label, datasrc->chan(i)->name);
                gtk_widget_set_sensitive(widget, scope.trige != 0);
            }
        }

    	sprintf (wg_id, "trigger_%i", scope.trigch + 1);
	if ((widget = LU(wg_id)))
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);
    }

    /* The triggering options should only be sensitive if we have a data source
     * and it defines a set_trigger function.
     */

    if ((widget = LU("trigger_rising")))
        gtk_widget_set_sensitive (widget, datasrc && datasrc->set_trigger != NULL);
    if ((widget = LU("trigger_falling")))
        gtk_widget_set_sensitive (widget, datasrc && datasrc->set_trigger != NULL);

    /* The remaining items on the trigger menu should only be sensitive if triggering 
     * is turned on */
    if ((widget = LU("trigger_position")))
        gtk_widget_set_sensitive (widget, scope.trige != 0);

    switch (scope.run)
    {
	    case 1: widget = LU("scope_run"); break;
	    case 2: widget = LU("scope_wait"); break;
	    default: widget = LU("scope_stop");
    }
    if (widget)
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE); 

    switch (scope.plot_mode)
    {
	    case 1: widget = LU("plot_line"); break;
	    case 2: widget = LU("plot_step"); break;
	    default: widget = LU("plot_point");
    }
    if (widget)
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE); 

    switch (scope.scroll_mode)
    {
	    case 1: widget = LU("scroll_accumulate"); break;
	    case 2: widget = LU("scroll_strip"); break;
	    default: widget = LU("scroll_sweep");
    }
    if (widget)
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE); 

    if (scope.behind)
	    widget = LU("graticule_behind");
    else
	    widget = LU("graticule_front");
    if (widget)
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE); 

    switch (scope.grat)
    {
	    case 0: widget = LU("graticule_none"); break;
	    case 1: widget = LU("graticule_minor"); break;
	    default: widget = LU("graticule_all");
    }
    if (widget)
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE); 

    if ((widget = LU("scope_cursors")))
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), scope.curs); 

    if ((widget = LU("help_keys")))
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), scope.verbose); 

    if ((widget = LU("math_menu")))
            gtk_widget_set_sensitive (widget , scope.select > 1);
    /* XXX add a check to each function's isvalid() test on math_menu itens */

    /* Now the store and recall channels - set the names for memory or data source,
     * and mark the channels active/sensitive if memory is stored there.
     * Store menu only displays options for the memory channels that are actually
     * 'visible', i.e, not occupied by data source channels.
     */
    if ((widget = LU("store_a")))
        for (i = 0; i < 26; i++) {
	    mem_id = 'a' + i;

    	    sprintf (wg_id, "store_%c", mem_id);
	    widget = LU(wg_id);
	    if (widget == NULL)
		    break;
            if (datasrc && i < datasrc->nchans()) {
                gtk_widget_hide (widget);
            } else {
                gtk_widget_show (widget);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), 
                     mem[i].num > 0);
            }

    	    sprintf (wg_id, "recall_%c", mem_id);
	    widget = LU(wg_id);
	    if (widget == NULL)
		    break;
            label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (widget)));
            if (datasrc && i < datasrc->nchans()) {
                gtk_label_set_text(label, datasrc->chan(i)->name);
                gtk_widget_set_sensitive(widget, TRUE);
            } else {
                sprintf(buf, "Mem %c", 'A' + i);
                gtk_label_set_text (label, buf);
                gtk_widget_set_sensitive (widget, mem[i].num > 0);
            }
        }

    fixing_widgets = 0;
}

gboolean on_databox_button_press_event (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    gfloat num, l, x;
    int cursor;
    Channel *p = &ch[scope.select];

    /* XXX duplicates code in draw_data() */
    if (p->signal->rate > 0) {
        num = (gfloat) 1 / p->signal->rate;
    } else {
        num = (gfloat) 1 / 1000;
    }
    l = p->signal->delay * num / 10000;

    if (scope.curs) {
        // XXX what's all this?
#if 0
        GtkDataboxCoord coord;
        GtkDataboxValue value;
        coord.x = event->x;
        coord.y = event->y;
        value = gtk_databox_value_from_coord (GTK_DATABOX(databox), coord);
        x = value.x;
#else
        x = gtk_databox_pixel_to_value_x (GTK_DATABOX(databox), event->x);
#endif
        cursor = rintf((x - l) / num) + 1;
#if 0
        if (event->state & GDK_BUTTON1_MASK) {
            scope.cursa = cursor;
        } else if (event->state & GDK_BUTTON2_MASK) {
            scope.cursb = cursor;
        }
#else
        if (event->button == 1) {
            scope.cursa = cursor;
        } else if (event->button == 3) {
            scope.cursb = cursor;
        }
#endif
        show_data();
    }

    return FALSE;
}

#if 0
/* draggable cursor positioning */
gint motion_event (GtkWidget *widget, GdkEventMotion *event)
{
    static int x, y;
    GdkModifierType state;

    if (event->is_hint)
        gdk_window_get_pointer (event->window, &x, &y, &state);
    else {
        x = event->x;
        y = event->y;
        state = event->state;
    }
    if (state & GDK_BUTTON1_MASK)
        return positioncursor(x, y, 1);
    if (state & GDK_BUTTON2_MASK)
        return positioncursor(x, y, 2);
    return TRUE;
}
#endif

#if 0
/* context sensitive mouse click select, recall and pop-up menus */
gint button_event(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    static int x, y, b;

    x = event->x;
    y = event->y;
    b = event->button;
    if (positioncursor(x, y, b))
        return TRUE;

    x = buttoncol(event->x);    /* convert graphic to text position */
    y = buttonrow(event->y);
    /*    printf("button: %d @ %f,%f -> %d,%d\n", b, event->x, event->y, x, y); */
    if (!y && x > 70)
        handle_key('?');
    else if (y == 28 && x >= 27 && x <= 53) {
        if (b > 1)
            gtk_menu_popup(GTK_MENU(gtk_item_factory_get_widget
                                    (factory, "/Channel/Store")),
                           NULL, NULL, NULL, NULL, event->button, event->time);
        else
            handle_key((x - 27) + 'a');
    } else if (y < 4) {
        if (b == 1) handle_key('-');
        else if (b == 2) handle_key('=');
        else gtk_menu_popup(GTK_MENU(gtk_item_factory_get_widget
                                     (factory, "/Trigger")),
                            NULL, NULL, NULL, NULL, event->button, event->time);
    } else if (y == 4) {
        if (b < 3) handle_key('!');
        else gtk_menu_popup(GTK_MENU(gtk_item_factory_get_widget
                                     (factory, "/Scope/Plot Mode")),
                            NULL, NULL, NULL, NULL, event->button, event->time);
    }  else if (y < 25 && (x < 11 || x > 69)) {
        handle_key((y - 5) / 5 + '1' + (x > 69 ? 4 : 0));
        if (!((y - 1) % 5)) {
            if (b == 1) handle_key('{');
            else if (b == 2) handle_key('}');
            else gtk_menu_popup(GTK_MENU(gtk_item_factory_get_widget
                                         (factory, "/Channel/Scale")),
                                NULL, NULL, NULL, NULL, event->button, event->time);
        } else if (!((y - 2) % 5)) {
            if (x < 4 || (x > 69 && x < 74)) {
                if (b == 1) handle_key('`');
                else if (b == 2) handle_key('~');
                else gtk_menu_popup(GTK_MENU(gtk_item_factory_get_widget
                                             (factory, "/Channel/Bits")),
                                    NULL, NULL, NULL, NULL, event->button, event->time);
            } else {
                if (b == 1) handle_key('[');
                else if (b == 2) handle_key(']');
                else gtk_menu_popup(GTK_MENU(gtk_item_factory_get_widget
                                             (factory, "/Channel/Position")),
                                    NULL, NULL, NULL, NULL, event->button, event->time);
            }
        } else if (b > 1)
            gtk_menu_popup(GTK_MENU(gtk_item_factory_get_widget
                                    (factory, "/Channel")),
                           NULL, NULL, NULL, NULL, event->button, event->time);
    } else if (b > 1)
        gtk_menu_popup(GTK_MENU(gtk_item_factory_get_widget
                                (factory, "/Scope")),
                       NULL, NULL, NULL, NULL, event->button, event->time);
    return TRUE;
}
#endif

GtkWidget * create_main_window(void);

/* initialize all the widgets, called by init_screen in display.c */
void init_widgets(void)
{
    GtkWidget *wg;
    char ver_string[16];
    int i;

    glade_window = create_main_window ();

    sprintf(ver_string, "ver: %s", VERSION);
    gtk_label_set_label(GTK_LABEL(LU("version_help_label")), (const gchar *)ver_string);
    setup_help_text(glade_window, NULL);

    /* Menubar now is constructed in Glade: 
     */
    for (i = 0; i < ndatasrcs; i++) {
        if (g_strcmp0 ("ALSA", datasrcs[i]->name) == 0) {
	    wg = LU("device_alsa");
            gtk_widget_set_sensitive (wg, TRUE);
	    if (datasrc == datasrcs[i]) {
		    set_fixing_widgets (TRUE);
		    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(wg), TRUE);
		    set_fixing_widgets (FALSE);
	    }
	} else if (g_strcmp0 ("COMEDI", datasrcs[i]->name) == 0) {
	    wg = LU("device_comedi");
            gtk_widget_set_sensitive (wg, TRUE);
	    if (datasrc == datasrcs[i]) {
		    set_fixing_widgets (TRUE);
		    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(wg), TRUE);
		    set_fixing_widgets (FALSE);
	    }
	}
    }

    gtk_databox_set_adjustment_x (GTK_DATABOX (databox),
                                  gtk_range_get_adjustment (
					  GTK_RANGE (LU("databox_hscrollbar"))));
    gtk_widget_show(glade_window);

#if 0
    gtk_signal_connect(GTK_OBJECT(drawing_area), "motion_notify_event", GTK_SIGNAL_FUNC(motion_event), NULL);
    gtk_signal_connect(GTK_OBJECT(drawing_area), "button_press_event",
                       GTK_SIGNAL_FUNC(button_event), NULL);
    gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK
                           | GDK_LEAVE_NOTIFY_MASK
                           | GDK_BUTTON_PRESS_MASK
                           | GDK_POINTER_MOTION_MASK
                           | GDK_POINTER_MOTION_HINT_MASK);
#endif

}

static int timeout_tag_valid = 0;
static gint timeout_tag;

/* GTK documentation says to return 0 if you don't want your timeout
 * function to be called again, or 1 if you do.  In our case,
 * animate() will call show_data(), which will call settimeout(),
 * which will remove the old timeout and possibly set a new one, and
 * all before the callback returns.  It's a little unclear to me what
 * the return value should be in this case (or if it matters)...
 */

gboolean timeout_callback(gpointer data)
{
    timeout_tag_valid = 0;
    do_save_pending();
    animate(NULL);
    return FALSE;
}

void settimeout(int ms)
{
    if (timeout_tag_valid) {
        g_source_remove (timeout_tag);
        timeout_tag_valid = 0;
    }

    if (ms == 0) return;

    timeout_tag = g_timeout_add( ms, timeout_callback, NULL);
    timeout_tag_valid = 1;
}

//void inputCallback(gpointer data, gint source, GdkInputCondition condition)
gboolean inputCallback(GIOChannel *source, GIOCondition condition , gpointer data)
{
    do_save_pending();
    animate(NULL);
    return (TRUE); // Continue with the event source
}

static int input_fd = -1;
static int input_tag_valid = 0;
static guint input_tag;

void setinputfd(int fd)
{
    static GIOChannel *gio_ch = NULL;

    if (input_fd != fd) {

        if (input_tag_valid) {
            //gdk_input_remove(input_tag);
	    g_source_remove (input_tag);
	    g_io_channel_unref (gio_ch);
            input_tag_valid = 0;
        }

        if (fd != -1) {
            //input_tag = gdk_input_add(fd, GDK_INPUT_READ, inputCallback, NULL);
	    if (gio_ch != NULL)
	    {
	    	g_io_channel_unref (gio_ch);
		g_io_channel_shutdown (gio_ch, TRUE, NULL);
	    }
	    gio_ch = g_io_channel_unix_new (fd);
            input_tag = g_io_add_watch (gio_ch, G_IO_IN, inputCallback, NULL);
            input_tag_valid = 1;
        }

        input_fd = fd;
    }
}

#ifndef HAVE_LIBCOMEDI
/* 
 * The sole purpose of this 2 functions is
 * to avoid the "Could not find signal handler" warnings
 * from gtk when compiled without comedi support.
 */
void
comedi_on_ok(GtkButton *button, gpointer user_data)
{
    ;
}

void
comedi_on_apply(GtkButton *button, gpointer user_data)
{
    ;
}
#endif

