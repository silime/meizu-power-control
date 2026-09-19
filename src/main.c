// SPDX-License-Identifier: GPL-2.0-only
#include <adwaita.h>
#include <errno.h>
#include <glib/gi18n.h>
#include <locale.h>

#define APP_ID "io.github.chalkin.MeizuPowerControl"
#define HELPER_PATH "/usr/libexec/meizu-power-control-helper"
#define BAT_PATH "/sys/class/power_supply/qcom-battmgr-bat/"
#define USB_PATH "/sys/class/power_supply/qcom-battmgr-usb/"
#define WLS_PATH "/sys/class/power_supply/qcom-battmgr-wls/"
#define WLS_TX_PATH "/sys/class/power_supply/qcom-battmgr-wls-tx/"
#define UCSI_PATH "/sys/class/power_supply/ucsi-source-psy-pmic_glink.ucsi.01/"
#define TYPEC_PATH "/sys/class/typec/port0/"
#define PARTNER_PATH "/sys/class/typec/port0-partner/"
#define USB_ROLE_PATH "/sys/class/usb_role/a600000.usb-role-switch/role"

/* Kernel power_supply status values we know how to translate. */
static const char *const power_supply_statuses[] = {
	N_("Unknown"),
	N_("Charging"),
	N_("Discharging"),
	N_("Not charging"),
	N_("Full"),
	N_("Pending"),
	NULL,
};

typedef struct {
	AdwApplication *application;
	AdwApplicationWindow *window;
	AdwToastOverlay *toast_overlay;
	AdwSwitchRow *tx_switch;
	AdwActionRow *tx_status;
	AdwActionRow *tx_voltage;
	AdwActionRow *tx_current;
	AdwActionRow *rx_online;
	AdwActionRow *rx_voltage;
	AdwActionRow *rx_current;
	AdwActionRow *battery_status;
	AdwActionRow *battery_capacity;
	AdwActionRow *battery_voltage;
	AdwActionRow *battery_current;
	AdwActionRow *battery_power;
	AdwActionRow *battery_temperature;
	AdwActionRow *usb_online;
	AdwActionRow *usb_type;
	AdwActionRow *usb_voltage;
	AdwActionRow *usb_current;
	AdwActionRow *usb_limit;
	AdwActionRow *typec_orientation;
	AdwActionRow *typec_power_role;
	AdwActionRow *typec_data_role;
	AdwActionRow *typec_mode;
	AdwActionRow *typec_usb_role;
	AdwActionRow *typec_pd;
	gboolean updating;
	gboolean operation_pending;
} AppState;

static char *read_value(const char *path)
{
	char *contents = NULL;

	if (!g_file_get_contents(path, &contents, NULL, NULL))
		return g_strdup("—");

	g_strstrip(contents);
	return contents;
}

static char *read_property(const char *base, const char *property)
{
	char *path = g_strconcat(base, property, NULL);
	char *value = read_value(path);

	g_free(path);
	return value;
}

static char *format_property(const char *base, const char *property,
			     double divisor, const char *unit)
{
	char *raw = read_property(base, property);
	char *end;
	gint64 value;
	char *formatted;

	errno = 0;
	value = g_ascii_strtoll(raw, &end, 10);
	if (errno || end == raw || *end) {
		g_free(raw);
		return g_strdup("—");
	}

	formatted = g_strdup_printf("%.2f %s", value / divisor, unit);
	g_free(raw);
	return formatted;
}

static void set_row(AdwActionRow *row, char *value)
{
	adw_action_row_set_subtitle(row, value);
	g_free(value);
}

static char *format_online(const char *base)
{
	char *raw = read_property(base, "online");
	char *formatted;

	if (!strcmp(raw, "1"))
		formatted = g_strdup(_("Connected"));
	else if (!strcmp(raw, "0"))
		formatted = g_strdup(_("Disconnected"));
	else
		formatted = g_strdup("—");
	g_free(raw);
	return formatted;
}

static char *format_status(const char *status)
{
	if (g_strv_contains(power_supply_statuses, status))
		return g_strdup(_(status));

	return g_strdup(status);
}

static char *format_tx_status(const char *online, const char *status)
{
	if (!strcmp(online, "0"))
		return g_strdup(_("Off"));
	if (strcmp(online, "1"))
		return g_strdup("—");

	if (!strcmp(status, "Discharging"))
		return g_strdup(_("Transmitting"));
	if (!strcmp(status, "Not charging"))
		return g_strdup(_("Waiting for a receiver"));
	if (!strcmp(status, "Unknown"))
		return g_strdup(_("On · status unknown"));

	return format_status(status);
}

static gboolean refresh_status(gpointer user_data)
{
	AppState *state = user_data;
	char *online = read_property(WLS_TX_PATH, "online");
	char *tx_status = read_property(WLS_TX_PATH, "status");
	char *capacity = read_property(BAT_PATH, "capacity");
	char *capacity_text;
	char *temperature;
	char *ambient;
	char *temperature_text;
	char *pd;

	state->updating = TRUE;
	adw_switch_row_set_active(state->tx_switch, !strcmp(online, "1"));
	gtk_widget_set_sensitive(GTK_WIDGET(state->tx_switch),
				 !state->operation_pending && strcmp(online, "—"));
	state->updating = FALSE;
	set_row(state->tx_status, format_tx_status(online, tx_status));
	g_free(tx_status);
	g_free(online);
	set_row(state->tx_voltage,
		format_property(WLS_TX_PATH, "voltage_now", 1000000.0, "V"));
	set_row(state->tx_current,
		format_property(WLS_TX_PATH, "current_now", 1000000.0, "A"));

	set_row(state->rx_online, format_online(WLS_PATH));
	set_row(state->rx_voltage,
		format_property(WLS_PATH, "voltage_now", 1000000.0, "V"));
	set_row(state->rx_current,
		format_property(WLS_PATH, "current_now", 1000000.0, "A"));

	set_row(state->battery_status,
		format_status(read_property(BAT_PATH, "status")));
	capacity_text = strcmp(capacity, "—") ? g_strdup_printf("%s %%", capacity)
						 : g_strdup("—");
	set_row(state->battery_capacity, capacity_text);
	g_free(capacity);
	set_row(state->battery_voltage,
		format_property(BAT_PATH, "voltage_now", 1000000.0, "V"));
	set_row(state->battery_current,
		format_property(BAT_PATH, "current_now", 1000000.0, "A"));
	set_row(state->battery_power,
		format_property(BAT_PATH, "power_now", 1000000.0, "W"));
	temperature = format_property(BAT_PATH, "temp", 10.0, "°C");
	ambient = format_property(BAT_PATH, "temp_ambient", 10.0, "°C");
	temperature_text = g_strdup_printf(_("Battery %s · Mainboard %s"),
					   temperature, ambient);
	set_row(state->battery_temperature, temperature_text);
	g_free(temperature);
	g_free(ambient);

	set_row(state->usb_online, format_online(USB_PATH));
	set_row(state->usb_type, read_property(USB_PATH, "usb_type"));
	set_row(state->usb_voltage,
		format_property(USB_PATH, "voltage_now", 1000000.0, "V"));
	set_row(state->usb_current,
		format_property(USB_PATH, "current_now", 1000000.0, "A"));
	set_row(state->usb_limit,
		format_property(USB_PATH, "input_current_limit", 1000000.0, "A"));

	set_row(state->typec_orientation, read_property(TYPEC_PATH, "orientation"));
	set_row(state->typec_power_role, read_property(TYPEC_PATH, "power_role"));
	set_row(state->typec_data_role, read_property(TYPEC_PATH, "data_role"));
	set_row(state->typec_mode,
		read_property(TYPEC_PATH, "power_operation_mode"));
	set_row(state->typec_usb_role, read_value(USB_ROLE_PATH));
	pd = read_property(PARTNER_PATH, "supports_usb_power_delivery");
	if (!strcmp(pd, "yes") || !strcmp(pd, "1")) {
		g_free(pd);
		pd = g_strdup(_("Supported"));
	} else if (!strcmp(pd, "no") || !strcmp(pd, "0")) {
		g_free(pd);
		pd = g_strdup(_("Not supported"));
	}
	set_row(state->typec_pd, pd);

	return G_SOURCE_CONTINUE;
}

static void control_finished(GObject *source, GAsyncResult *result,
			     gpointer user_data)
{
	AppState *state = user_data;
	GError *error = NULL;

	if (!g_subprocess_wait_check_finish(G_SUBPROCESS(source), result, &error)) {
		AdwToast *toast = adw_toast_new(error ? error->message
						      : _("Operation failed"));

		adw_toast_overlay_add_toast(state->toast_overlay, toast);
		g_clear_error(&error);
	}

	state->operation_pending = FALSE;
	refresh_status(state);
}

static void tx_switch_changed(AdwSwitchRow *row, GParamSpec *pspec,
			      gpointer user_data)
{
	AppState *state = user_data;
	GSubprocess *process;
	GError *error = NULL;
	const char *value;

	(void)pspec;

	if (state->updating || state->operation_pending)
		return;

	value = adw_switch_row_get_active(row) ? "1" : "0";
	process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDERR_PIPE, &error,
				   "pkexec", HELPER_PATH, value, NULL);
	if (!process) {
		AdwToast *toast = adw_toast_new(error->message);

		adw_toast_overlay_add_toast(state->toast_overlay, toast);
		g_clear_error(&error);
		refresh_status(state);
		return;
	}

	state->operation_pending = TRUE;
	gtk_widget_set_sensitive(GTK_WIDGET(row), FALSE);
	g_subprocess_wait_check_async(process, NULL, control_finished, state);
	g_object_unref(process);
}

static AdwActionRow *add_row(AdwPreferencesGroup *group, const char *title)
{
	AdwActionRow *row = ADW_ACTION_ROW(adw_action_row_new());

	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
	adw_action_row_set_subtitle(row, "—");
	adw_preferences_group_add(group, GTK_WIDGET(row));
	return row;
}

static AdwPreferencesGroup *add_group(GtkBox *box, const char *title)
{
	AdwPreferencesGroup *group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());

	adw_preferences_group_set_title(group, title);
	gtk_box_append(box, GTK_WIDGET(group));
	return group;
}

static void activate(GtkApplication *application, gpointer user_data)
{
	AppState *state = user_data;
	AdwToolbarView *toolbar_view;
	AdwHeaderBar *header_bar;
	AdwClamp *clamp;
	GtkScrolledWindow *scrolled;
	GtkBox *box;
	AdwPreferencesGroup *group;

	if (state->window) {
		gtk_window_present(GTK_WINDOW(state->window));
		return;
	}

	state->window = ADW_APPLICATION_WINDOW(adw_application_window_new(application));
	gtk_window_set_title(GTK_WINDOW(state->window), _("Meizu Power Control"));
	gtk_window_set_default_size(GTK_WINDOW(state->window), 480, 760);

	state->toast_overlay = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
	adw_application_window_set_content(state->window,
					   GTK_WIDGET(state->toast_overlay));
	toolbar_view = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
	adw_toast_overlay_set_child(state->toast_overlay, GTK_WIDGET(toolbar_view));
	header_bar = ADW_HEADER_BAR(adw_header_bar_new());
	adw_toolbar_view_add_top_bar(toolbar_view, GTK_WIDGET(header_bar));

	scrolled = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
	gtk_scrolled_window_set_policy(scrolled, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	adw_toolbar_view_set_content(toolbar_view, GTK_WIDGET(scrolled));
	clamp = ADW_CLAMP(adw_clamp_new());
	adw_clamp_set_maximum_size(clamp, 640);
	gtk_scrolled_window_set_child(scrolled, GTK_WIDGET(clamp));
	box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_widget_set_margin_top(GTK_WIDGET(box), 18);
	gtk_widget_set_margin_bottom(GTK_WIDGET(box), 24);
	gtk_widget_set_margin_start(GTK_WIDGET(box), 12);
	gtk_widget_set_margin_end(GTK_WIDGET(box), 12);
	adw_clamp_set_child(clamp, GTK_WIDGET(box));

	group = add_group(box, _("Wireless Reverse Charging"));
	state->tx_switch = ADW_SWITCH_ROW(adw_switch_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(state->tx_switch),
				      _("Enable Reverse Charging"));
	adw_preferences_group_add(group, GTK_WIDGET(state->tx_switch));
	g_signal_connect(state->tx_switch, "notify::active",
			 G_CALLBACK(tx_switch_changed), state);
	state->tx_status = add_row(group, _("Transmitter State"));
	state->tx_voltage = add_row(group, _("Transmitter Voltage"));
	state->tx_current = add_row(group, _("Transmitter Current"));

	group = add_group(box, _("Wireless Charging Receiver"));
	state->rx_online = add_row(group, _("Connection Status"));
	state->rx_voltage = add_row(group, _("Receiver Voltage"));
	state->rx_current = add_row(group, _("Receiver Current"));

	group = add_group(box, _("Battery"));
	state->battery_status = add_row(group, _("Charge State"));
	state->battery_capacity = add_row(group, _("Capacity"));
	state->battery_voltage = add_row(group, _("Voltage"));
	state->battery_current = add_row(group, _("Current"));
	state->battery_power = add_row(group, _("Power"));
	state->battery_temperature = add_row(group, _("Temperature"));

	group = add_group(box, _("USB Power"));
	state->usb_online = add_row(group, _("Connection Status"));
	state->usb_type = add_row(group, _("Charger Type"));
	state->usb_voltage = add_row(group, _("Voltage"));
	state->usb_current = add_row(group, _("Current"));
	state->usb_limit = add_row(group, _("Input Current Limit"));

	group = add_group(box, "USB Type-C");
	state->typec_orientation = add_row(group, _("Orientation"));
	state->typec_power_role = add_row(group, _("Power Role"));
	state->typec_data_role = add_row(group, _("Data Role"));
	state->typec_mode = add_row(group, _("Power Operation Mode"));
	state->typec_usb_role = add_row(group, _("USB Role"));
	state->typec_pd = add_row(group, _("USB PD"));

	refresh_status(state);
	g_timeout_add_seconds(2, refresh_status, state);
	gtk_window_present(GTK_WINDOW(state->window));
}

int main(int argc, char **argv)
{
	AppState state = { 0 };
	int status;

	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	state.application = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(state.application, "activate", G_CALLBACK(activate), &state);
	status = g_application_run(G_APPLICATION(state.application), argc, argv);
	g_object_unref(state.application);

	return status;
}
