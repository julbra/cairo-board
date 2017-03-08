#include <stdlib.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <string.h>

#include "cairo-board.h"

/* config file utilities */
static const char conf_dirname[] = "cairo-board";
static const char conf_filename[] = "cairo-board.conf";
static GKeyFile *config_file;
static char config_full_path[256];

/* config groups */
const gchar *login_group = "Login";
const gchar *login_key = "login";
const gchar *password_key = "password";
const gchar *save_login_key = "save_login";
const gchar *auto_login_key = "auto_login";

/* initialise config file utilities - call this once at start of application */
void init_config(void) {

	/* build config dirname string */
	const char *conf_dir = g_get_user_config_dir();
	snprintf(config_full_path, 256, "%s/%s", conf_dir, conf_dirname);

	/* check that the config directory exists and is a directory
	 * Otherwise create it */
	if (!g_file_test(config_full_path, G_FILE_TEST_IS_DIR)) {
		if (!g_file_test(config_full_path, G_FILE_TEST_EXISTS)) {
			debug("Creating Cairo-Board configuration directory: %s\n", config_full_path);
			int ret = g_mkdir_with_parents(config_full_path, 0700);
			if (ret) {
				perror("Cairo-Board configuration directory didn't exists and could not be created!");
				exit(1);
			}
		}
		else {
			fprintf(stderr, "Cairo-Board configuration directory: '%s'. file exists but is not a directory!", config_full_path);
			exit(1);
		}
	}

	/* build full config path string */
	config_full_path[strlen(config_full_path)] = '/';
	strncat(config_full_path, conf_filename, strlen(conf_filename));

	/* check that the config file exists and is a regular file
	 * Otherwise create it */
	GError *error = NULL;
	if (!g_file_test(config_full_path, G_FILE_TEST_IS_REGULAR)) {
		if (!g_file_test(config_full_path, G_FILE_TEST_EXISTS)) {
			debug("Creating Cairo-Board configuration file: %s\n", config_full_path);
			g_file_set_contents(config_full_path, "[global]\nauthor = Julien Bramary\n", -1, &error);
		}
		else {
			fprintf(stderr, "Cairo-Board configuration file: '%s'. file exists but is not a regular file!", config_full_path);
			exit(1);
		}
		if (chmod(config_full_path, 0600)) {
			perror("Failed to change permissions on Cairo-Board configuration file!");
			exit(1);
		}
	}

	// all is well - now load the file into memory
	GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
	config_file = g_key_file_new();
	if (!g_key_file_load_from_file(config_file, config_full_path, flags, &error)) {
		fprintf (stderr, "Unable to read file: %s\n", error->message);
		g_error_free(error);
		exit(1);
	}
}

gboolean save_config(void) {
	gsize length = 0;
	GError *error = NULL;
	gboolean ret;
	gchar *config_data = g_key_file_to_data(config_file, &length, &error);
	if (config_data == NULL) {
		fprintf (stderr, "Unable to serialise configuration to string: %s\n", error->message);
		return FALSE;
	}
	ret = g_file_set_contents(config_full_path, config_data, length, &error);
	if (!ret) {
		fprintf (stderr, "Unable to save configuration string to configuration file: %s\n", error->message);
		return FALSE;
	}
	debug("Successfully saved configuration to %s\n", config_full_path);
	return TRUE;
}

gchar *get_login(void) {
	GError *error;
	gchar *ret = NULL;
	if (g_key_file_has_key(config_file, login_group, login_key, &error)) {
		ret = g_key_file_get_string(config_file, login_group, login_key, &error);
	}
	return ret;
}

void set_login(const gchar *login) {
	g_key_file_set_string(config_file, login_group, login_key, login);
}

static void hide_string(char* s) {
	int n;
	for (n = 0; n < strlen(s); n++) {
		s[n] ^= 42;
	}
}

gchar *get_password(void) {
	GError *error;
	gchar *pass = NULL;
	if (g_key_file_has_key(config_file, login_group, password_key, &error)) {
		pass = g_key_file_get_string(config_file, login_group, password_key, &error);
	}
	hide_string(pass);
	return pass;
}

void set_password(const gchar *password) {
	char *pass = calloc(strlen(password)+1, sizeof(char));
	strcpy(pass, password);
	hide_string(pass);
	g_key_file_set_string(config_file, login_group, password_key, pass);
}

gboolean get_save_login(void) {
	return g_key_file_get_boolean(config_file, login_group, save_login_key, NULL);
}

void set_save_login(gboolean save) {
	g_key_file_set_boolean(config_file, login_group, save_login_key, save);
}

gboolean get_auto_login(void) {
	return g_key_file_get_boolean(config_file, login_group, auto_login_key, NULL);
}

void set_auto_login(gboolean autolog) {
	g_key_file_set_boolean(config_file, login_group, auto_login_key, autolog);
}
