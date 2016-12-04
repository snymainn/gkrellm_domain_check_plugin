/*  
 * 
 *  Copyright (C) 2016 Tommy Skagemo-Andreassen
 *
 *  Heaily based on gkrellmlaunch by:
 *  Author: Lee Webb leewebb@users.sourceforge.net
 *  Latest versions: http://gkrellmlaunch.sourceforge.net
 *
 *  This program is free software which I release under the GNU General Public
 *  License. You may redistribute and/or modify this program under the terms
 *  of that license as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  Requires GKrellM 2 or better
 */

#include <gkrellm2/gkrellm.h>

/*
 * Make sure we have a compatible version of GKrellM
 * (Version 2+ is required due to the use of GTK2)
 */
#if !defined(GKRELLM_VERSION_MAJOR) || GKRELLM_VERSION_MAJOR < 2
#error This plugin requires GKrellM version >= 2
#endif

#define CONFIG_NAME "DomainCheck"

#define PLUGIN_PLACEMENT (MON_UPTIME | MON_INSERT_AFTER)

#define PLUGIN_CONFIG_KEYWORD "domaincheck"

#define STYLE_NAME "GKrellMDomainCheck"

static gchar *GKrellMDomainCheckInfo[] =
{
  "<b>Usage\n\n",
  "Domain: ",
  "This is the subdomain to check against your global ip.\n",
  "Enabled: ",
  "Check box to allow domains to be disabled if not used.\n\n",
  "Use the \"Add\" button to create a new domain.\n",
  "Use the \"Replace\" button to update changes made for currently selected ",
  "list entry.\n",
  "Use the \"Delete\" button to delete the selected entry.\n",
  "Use the /\\ & \\/  buttons to move selected entry up & down in position.\n",
};

static gchar GKrellMDomainCheckAbout[] = 
  "Gkrell Domain Check Version 0.1\n"\
  "GKrellM plugin to check that your subdomains are pointing at your global IP address provided by ISP.\n\n"\
  "Released under the GNU Public License.\n";

static GkrellmMonitor *monitor;

typedef struct
{
  gint  enabled;
  gchar *domain;

  /* Each domain has its own Panel & Decal */
  GkrellmPanel *panel; 
  GkrellmDecal *decal;
} GDomain;

/*
 * We need a list to hold our series of GDomains.
 */
static GList *domainList;

static gboolean listModified;

static gint style_id;

/* 
 * Create Config tab widgets.
 */ 
static GtkWidget *domainEntry;
static GtkWidget *toggleButton;
static GtkWidget *domainVbox;
/*
 * Listbox widget for the config tab.
 */
static GtkWidget *domainCList;

/*
 * Keep track of selected item in the Config listbox.
 */
static gint selectedRow;

/* 
 * Handle decal button presses
 */ 
static void buttonPress (GkrellmDecalbutton *button)
{
    //TODO: Check domains  
}

static gint panel_expose_event (GtkWidget *widget, GdkEventExpose *ev)
{
  GDomain *domain;
  GList     *list;
  
  /*
   * O.K. This isn't particularly efficient, but in the interests of 
   * maintainability, I'm going to keep this in.
   */ 
  for (list = domainList; list; list = list->next)
  {
    domain = (GDomain *) list->data;
    if (widget == domain->panel->drawing_area)
    {
      gdk_draw_pixmap (widget->window,
                       widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                       domain->panel->pixmap, ev->area.x, ev->area.y, 
                      ev->area.x, ev->area.y, ev->area.width, ev->area.height);
    }
  }  

  return FALSE;
}

/*
 * Func called by create_plugin() & apply_config() to show/hide panels 
 * at startup according to related config item.
 */ 
static void setVisibility ()
{
  GDomain *domain;
  GList     *list;

  for (list = domainList ; list ; list = list->next)
  {
    domain = (GDomain *) list->data;
    if (domain->enabled == 0)
    {
      gkrellm_panel_hide (domain->panel);
    }
    else
    {
      gkrellm_panel_show (domain->panel);
    }  
  }  
}
  
static void update_plugin ()
{
  GDomain *domain;
  GList     *list;

  for (list = domainList; list; list = list->next)
  {
    domain = (GDomain *) list->data;
    gkrellm_draw_panel_layers (domain->panel);
  }  
}

/* 
 * Configuration
 */
static void save_plugin_config (FILE *f)
{
  GDomain *domain;
  GList     *list;
  
  for (list = domainList; list; list = list->next)
  {
    domain = (GDomain *) list->data;

    printf ("%s enabled=%d domain=%s\n", 
             PLUGIN_CONFIG_KEYWORD, domain->enabled, domain->domain);
    fprintf (f, "%s enabled=%d domain=%s\n", 
             PLUGIN_CONFIG_KEYWORD, domain->enabled, domain->domain);
  }
}


static void apply_plugin_config ()
{
  gchar     *string;
  gint      i;
  gint      row;
  GDomain *domain;
  GList     *list;
  GList     *newList;
  GkrellmStyle     *style;
  GkrellmTextstyle *ts;
  GkrellmTextstyle *ts_alt;

  if (listModified)
  {
    /*
     * Create a new list with the applied settings
     * Trash the old list.
     */
    newList = NULL;

    /*
     * Read each row of the listbox & create a new domain.
     * Append each domain to the new list.
     */ 
    for (row = 0; row < (GTK_CLIST (domainCList)->rows); row += 1)
    {
      domain = g_new0 (GDomain, 1);
      newList = g_list_append (newList, domain);

      gtk_clist_set_row_data (GTK_CLIST (domainCList), row, domain);

      /* 
       * Fill the enabled option.
       */ 
      gtk_clist_get_text (GTK_CLIST (domainCList), row, 0, &string);
      domain->enabled = (strcmp (string, "No") ? 1 : 0);
    
      /*
       * Fill the domain option.
       */
      gtk_clist_get_text (GTK_CLIST (domainCList), row, 1, &string);
      gkrellm_dup_string (&domain->domain, string);
      
    }

    /*
     * Wipe out the old list.
     */
    while (domainList)
    {
      domain = (GDomain *) domainList->data;
      gkrellm_panel_destroy (domain->panel);
      domainList = g_list_remove (domainList, domain);
    }

    /*
     * And then update to the new list.
     */
    domainList = newList;
    
    /*
     * Since we've destroyed the old list & the panels/decals with it,
     * we have to recreate those associated panels/decals.
     */ 

    /*
     * First make sure we have the styles set up.
     */
    style = gkrellm_meter_style (style_id);
    ts = gkrellm_meter_textstyle (style_id);
    ts_alt = gkrellm_meter_alt_textstyle (style_id);
     
    for (i = 0, list = domainList; list; i += 1, list = list->next)
    {
      domain = (GDomain *) list->data;
      domain->panel = gkrellm_panel_new0();
      domain->decal = gkrellm_create_decal_text (domain->panel,
                            domain->domain, ts_alt, style, -1, -1, -1);
                            
      /*
       * Configure the panel to the created decal, and create it.
       */
      gkrellm_panel_configure (domain->panel, NULL, style);
      gkrellm_panel_create (domainVbox, monitor, domain->panel);

      /*
       * Panel's been created so convert the decal into a button.
       */
      gkrellm_draw_decal_text (domain->panel, domain->decal,
                               domain->domain, 1);
                               
      gkrellm_put_decal_in_meter_button (domain->panel, domain->decal,
                                         buttonPress,
                                         GINT_TO_POINTER (i), NULL);
      /*
       * Connect our panel to the expose event to allow it to be drawn in 
       * update_plugin().
       */ 
      gtk_signal_connect (GTK_OBJECT (domain->panel->drawing_area),
                          "expose_event", (GtkSignalFunc) panel_expose_event,
                          NULL);
                          
    }
    setVisibility ();

    /*
     * Reset the modification state.
     */
    listModified = FALSE;
  }  
} 

static void load_plugin_config (gchar *arg)
{
  gchar     domain_string[255];
  gchar     enabled[2];
  gint      n;
  GDomain *domain;
  GList     *list;

  n = sscanf (arg, "enabled=%s domain=%[^\n]", enabled, domain_string);
  
  if (n == 2)
  {
    domain = g_new0 (GDomain, 1);
    domain->domain = g_strdup (domain_string);
    domain->enabled = atoi (enabled);
    domainList = g_list_append (domainList, domain);
  }

  for (list = domainList; list; list = list->next)
  {
    domain = (GDomain *) list->data;
  }  
}

static void cbMoveUp (GtkWidget *widget, gpointer drawer)
{
  gint      row;
  GtkWidget *clist;

  clist = domainCList;
  row = selectedRow;
  
  /*
   * Only attempt a move if we're not on the first row.
   */
  if (row > 0)
  {
    /* 
     * Move the selected row up one position. 
     * Note that we have to reselect it afterwards.
     */
    gtk_clist_row_move (GTK_CLIST (clist), row, row - 1);
    gtk_clist_select_row (GTK_CLIST (clist), row - 1, -1);
    
    selectedRow = row - 1;
    listModified = TRUE;
  }
}

static void cbMoveDown (GtkWidget *widget, gpointer drawer)
{
  gint      row;
  GtkWidget *clist;
       
  clist = domainCList;
  row = selectedRow;
  
  /*
   * Only attempt a row if we're not on the last row.
   * Note that we have to reselect it afterwards.
   */
  if ((row >= 0) && (row < (GTK_CLIST (clist)->rows - 1)))
  {
    gtk_clist_row_move (GTK_CLIST (clist), row, row + 1);
    gtk_clist_select_row (GTK_CLIST (clist), row + 1, -1);

    selectedRow = row + 1;
    listModified = TRUE;
  }
}

static void cListSelected (GtkWidget *clist, gint row, gint column, 
                          GdkEventButton *bevent, gpointer data)
{
  gchar *string;

  /*
   * Fill the entry widgets & check box accoring to the selected row's text
   */ 
  gtk_clist_get_text (GTK_CLIST (domainCList), row, 0, &string);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggleButton), 
                                strcmp (string, "No") ? TRUE : FALSE);
    
  gtk_clist_get_text (GTK_CLIST (domainCList), row, 1, &string);
  gtk_entry_set_text (GTK_ENTRY (domainEntry), string);
     
  selectedRow = row;
}

static void cListUnSelected (GtkWidget *clist, gint row, gint column, 
                             GdkEventButton *bevent, gpointer data)
{
  /* 
   * Reset the entry widgets & check box
   */ 
  gtk_entry_set_text (GTK_ENTRY (domainEntry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggleButton), 0);
  
  selectedRow = -1;
}

static void cbAdd (GtkWidget *widget, gpointer data)
{
  gchar *buffer[2];
        
  buffer[0] = (gtk_toggle_button_get_active 
               (GTK_TOGGLE_BUTTON (toggleButton)) == TRUE ? "1" : "0");
  buffer[1] = gkrellm_gtk_entry_get_text (&domainEntry);
  
  printf("cbAdd: %s %s\n", buffer[0], buffer[1]);
   
  /*
   * If either of the Label or Command entries are empty, forget it.
   */ 
  if ((!strlen (buffer[1])))
  {
    return;
  }

  buffer[0] = gtk_toggle_button_get_active 
              (GTK_TOGGLE_BUTTON (toggleButton)) == TRUE ? "Yes" : "No";
  gtk_clist_append (GTK_CLIST (domainCList), buffer);
  listModified = TRUE;

  /* 
   * Reset the entry widgets & check box
   */ 
  gtk_entry_set_text (GTK_ENTRY (domainEntry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggleButton), 0);
}

static void cbReplace (GtkWidget *widget, gpointer data)
{
  gchar *buffer[2];
        
  buffer[0] = (gtk_toggle_button_get_active 
               (GTK_TOGGLE_BUTTON (toggleButton)) == TRUE ? "1" : "0");
  buffer[1] = gkrellm_gtk_entry_get_text (&domainEntry);
   
  /*
   * If either of the Label or Command entries are empty, forget it.
   */ 
  if ((!strlen (buffer[1])))
  {
    return;
  }

  /*
   * If a row is selected, we're modifying an existing entry, 
   */ 
  if (selectedRow >= 0)
  {
  
    gtk_clist_set_text (GTK_CLIST (domainCList), 
                        selectedRow, 1, buffer[1]);
    gtk_clist_set_text (GTK_CLIST (domainCList), 
                        selectedRow, 0, 
                        gtk_toggle_button_get_active 
                        (GTK_TOGGLE_BUTTON (toggleButton)) 
                        == TRUE ? "Yes" : "No");
    gtk_clist_unselect_row (GTK_CLIST (domainCList), selectedRow, 0);
    selectedRow = -1;
    listModified = TRUE;
  }

  /* 
   * Reset the entry widgets & check box
   */ 
  gtk_entry_set_text (GTK_ENTRY (domainEntry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggleButton), 0);

  gtk_clist_unselect_row (GTK_CLIST (domainCList), selectedRow, 0);
}

static void cbDelete (GtkWidget *widget, gpointer data)
{
  /* 
   * Reset the entry widgets & check box
   */ 
  gtk_entry_set_text (GTK_ENTRY (domainEntry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggleButton), 0);

  if (selectedRow >= 0)
  {
    gtk_clist_remove (GTK_CLIST (domainCList), selectedRow);
    selectedRow = -1;
    listModified = TRUE;
  }
}

/*
 * Create a Config tab with:
 * 1. Checkbox to make panel enabled/hidden
 * 2. Text entry widget for command to run
 * 3. Text entry widget for button label
 * 4. Listbox to show items
 * 5. Info tab 
 */ 
static void create_plugin_tab (GtkWidget *tab_vbox)
{
  gchar     *titles[2] = {"Visible", "Domain"};
  gchar     *buffer[2];
  gchar     enabled[5];
  gint      i = 0;
  GDomain *domain;
  GList     *list;
  GtkWidget *tabs;
  GtkWidget *vbox; 
  GtkWidget *hbox;
  GtkWidget *scrolled;
  GtkWidget *text;
  GtkWidget *label;
  GtkWidget *button; 
  GtkWidget *aboutLabel;
  GtkWidget *aboutText;

  /* 
   * Make a couple of tabs.  One for Config and one for info.
   */
  tabs = gtk_notebook_new ();
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (tabs), GTK_POS_TOP);
  gtk_box_pack_start (GTK_BOX (tab_vbox), tabs, TRUE, TRUE, 0);

  /* 
   * Setup tab: give it some scroll bars to keep the config 
   * window size compact.
   */
  vbox = gkrellm_gtk_notebook_page (tabs, "Setup");
  vbox = gkrellm_gtk_scrolled_vbox (vbox, NULL,
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  /*
   * Dynamically create the options on the Config tab according to MAX_BUTTONS.
   */
  
  /* 
   * Create text boxes to put Labels and Commands in 
   */
    
  label = gtk_label_new ("Domain:");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, i);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);

  domainEntry = gtk_entry_new_with_max_length (255) ;
  gtk_entry_set_text (GTK_ENTRY (domainEntry), "") ;
  gtk_entry_set_editable (GTK_ENTRY (domainEntry), TRUE) ;
  gtk_box_pack_start (GTK_BOX (vbox), domainEntry, FALSE, FALSE, 0) ;

  toggleButton = gtk_check_button_new_with_label ("Enabled?");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggleButton), 0);
  gtk_box_pack_start (GTK_BOX (vbox), toggleButton, FALSE, TRUE, 0);
  
  /*
   * Add buttons into their own box 
   * => Add  Replace  Delete    /\     \/  
   */
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
    
  /*
   * Add "Add", "Replace" & "Delete" buttons
   */
  button = gtk_button_new_with_label ("Add");
  gtk_signal_connect (GTK_OBJECT (button), "clicked", 
                      (GtkSignalFunc) cbAdd, NULL);

  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  
  button = gtk_button_new_with_label ("Replace");
  gtk_signal_connect (GTK_OBJECT (button), "clicked", 
                      (GtkSignalFunc) cbReplace, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
    
  button = gtk_button_new_with_label ("Delete");
  gtk_signal_connect (GTK_OBJECT (button), "clicked", 
                      (GtkSignalFunc) cbDelete, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
    
  /*
   * Add reposition buttons
   */
  button = gtk_button_new_with_label ("/\\");
  gtk_signal_connect (GTK_OBJECT (button), "clicked", 
                      (GtkSignalFunc) cbMoveUp, NULL);
    
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  button = gtk_button_new_with_label ("\\/");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                      (GtkSignalFunc) cbMoveDown, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  /*
   * Create listbox to hold each item 
   */ 
  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), 
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0); 
    
  /*
   * Create the CList with 2 titles:
   * Domain, Enabled
   */ 
  domainCList = gtk_clist_new_with_titles (2, titles);
  gtk_clist_set_shadow_type (GTK_CLIST (domainCList), GTK_SHADOW_OUT);

  /* 
   * Set the column widths
   */
  /* Domain */
  gtk_clist_set_column_width (GTK_CLIST (domainCList), 1, 100);
  /* Enabled */
  gtk_clist_set_column_width (GTK_CLIST (domainCList), 2, 200);

  gtk_clist_set_column_justification (GTK_CLIST (domainCList), 
                                      1, GTK_JUSTIFY_LEFT);
  gtk_clist_set_column_justification (GTK_CLIST (domainCList), 
                                      2, GTK_JUSTIFY_LEFT);

  /*
   * Add signals for selecting a row in the CList. 
   */ 
  gtk_signal_connect (GTK_OBJECT (domainCList), "select_row", 
                      (GtkSignalFunc) cListSelected, NULL);
 
  /*
   * Add signal for deselecting a row in the CList. 
   * If not, delselecting & attempting to create a new entry will overwrite
   * the previuosly selected row.
   */ 
  gtk_signal_connect (GTK_OBJECT (domainCList), "unselect_row", 
                      (GtkSignalFunc) cListUnSelected, NULL);

  /* 
   * Add the CList to the scrolling window
   */ 
  gtk_container_add (GTK_CONTAINER (scrolled), domainCList);
    
  /*
   * Fill the CList with our commands etc.
   */ 
  for (i = 0, list = domainList; list; i += 1, list = list->next)
  {  
    domain = (GDomain *) list->data;
    sprintf (enabled, "%s", (domain->enabled == 1 ? "Yes" : "No"));        
             buffer[0] = enabled;
    buffer[1] = domain->domain;
    gtk_clist_append (GTK_CLIST (domainCList), buffer);
    gtk_clist_set_row_data (GTK_CLIST (domainCList), i, domain);
  }

  /* 
   * Info tab
   */
  vbox = gkrellm_gtk_notebook_page (tabs, "Info");
  text = gkrellm_gtk_scrolled_text_view (vbox, NULL, GTK_POLICY_AUTOMATIC,
                                         GTK_POLICY_AUTOMATIC);
  gkrellm_gtk_text_view_append_strings (text, GKrellMDomainCheckInfo,
                                       (sizeof (GKrellMDomainCheckInfo) 
                                       / sizeof (gchar *)));

  /*
   * About tab
   */
  aboutText = gtk_label_new (GKrellMDomainCheckAbout);
  aboutLabel = gtk_label_new ("About");
  gtk_notebook_append_page (GTK_NOTEBOOK (tabs), aboutText, aboutLabel);

}

static void create_plugin (GtkWidget *vbox, gint first_create)
{
  gint      i;
  GDomain *domain;
  GList     *list;
  GkrellmStyle     *style;
  GkrellmTextstyle *ts;
  GkrellmTextstyle *ts_alt;

  domainVbox = vbox;

  if (first_create)
  {
    for (list = domainList; list; list = list->next)
    {
      domain = (GDomain *) list->data;
      domain->panel = gkrellm_panel_new0();
    }
  }  
    
  style = gkrellm_meter_style (style_id);

  /* 
   * Each GkrellmStyle has two text styles.  The theme designer has picked the
   * colors and font sizes, presumably based on knowledge of what you draw
   * on your panel.  You just do the drawing.  You probably could assume
   * the ts font is larger than the ts_alt font, but again you can be
   * overridden by the theme designer.
   */
  ts = gkrellm_meter_textstyle (style_id);
  ts_alt = gkrellm_meter_alt_textstyle (style_id);

  /* 
   * Create a text decal that will be converted to a button.  
   * Make it the entire width of the panel.
   */
  for (i = 0, list = domainList; list; i += 1, list = list->next)
  {
    domain = (GDomain *) list->data;
    domain->decal = gkrellm_create_decal_text (domain->panel,
                            domain->domain, ts_alt, style, -1, -1, -1);
  /*
   * Configure the panel to created decal, and create it.
   */
    gkrellm_panel_configure (domain->panel, NULL, style);
    gkrellm_panel_create (vbox, monitor, domain->panel);
    
  /* 
   * After the panel is created, the decal can be converted into a button.
   * First draw the initial text into the text decal button and then
   * put the text decal into a meter button.  
   */
    gkrellm_draw_decal_text (domain->panel, domain->decal, 
                              domain->domain,1);
    gkrellm_put_decal_in_meter_button (domain->panel, domain->decal, 
                                       buttonPress, 
                                       GINT_TO_POINTER (i), NULL);
  }                                            

  /* 
   * Note: all of the above gkrellm_draw_decal_XXX() calls will not
   * appear on the panel until a gkrellm_draw_panel_layers() call is
   * made.  This will be done in update_plugin(), otherwise we would
   * make the call here and anytime the decals are changed.
   */

  if (first_create)
  {
    for (list = domainList; list; list = list->next)
    {
      domain = (GDomain *) list->data;
      gtk_signal_connect (GTK_OBJECT (domain->panel->drawing_area), 
                  "expose_event", (GtkSignalFunc) panel_expose_event, NULL);
    }                      
    /*
     * Setup the initial enabled status of each panel
     * according to the config item read in.
     */ 
    setVisibility ();
  }
}


/* 
 * The monitor structure tells GKrellM how to call the plugin routines.
 */
static GkrellmMonitor plugin_mon	=
{
  CONFIG_NAME,           /* Name, for config tab.          */
  0,                     /* Id,  0 if a plugin             */
  create_plugin,         /* The create function            */
  update_plugin,         /* The update function            */
  create_plugin_tab,     /* The config tab create function */
  apply_plugin_config,   /* Apply the config function      */
  save_plugin_config,    /* Save user config               */
  load_plugin_config,    /* Load user config               */
  PLUGIN_CONFIG_KEYWORD, /* config keyword                 */

  NULL,        /* Undefined 2 */
  NULL,        /* Undefined 1 */
  NULL,        /* private     */

  PLUGIN_PLACEMENT,    /* Insert plugin before this monitor */

  NULL,               /* Handle if a plugin, filled in by GKrellM */
  NULL                /* path if a plugin, filled in by GKrellM   */
};

/* 
 * All GKrellM plugins must have one global routine named gkrellm_init_plugin()
 * which returns a pointer to a filled in monitor structure.
 */
GkrellmMonitor* gkrellm_init_plugin ()
{
  /*
   * Don't want any row in the Config tab initially selected.
   */
  selectedRow = -1;
  listModified = FALSE;

  style_id = gkrellm_add_meter_style (&plugin_mon, STYLE_NAME);
  monitor = &plugin_mon;
  return &plugin_mon;
}
