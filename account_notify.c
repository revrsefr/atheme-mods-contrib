/*
 * Atheme module to notify Django when an account is deleted from NickServ.
 *
 * Hooks into the NickServ DROP and FDROP commands and makes an HTTP request
 * to Django to delete the corresponding user in the database.
 */

#include "atheme-compat.h"    // Use the compatibility header
#include <curl/curl.h>

#define DJANGO_API_URL "/accounts/api/delete_user/"

/* Helper to send a DELETE request to your Django endpoint */
static void
notify_django_user_deleted(const char *username)
{
	if (! username)
		return;

	CURL *curl = curl_easy_init();
	if (! curl)
	{
		slog(LG_ERROR, "ns_delete_notify: Failed to initialize CURL.");
		return;
	}

	/* JSON payload */
	char post_data[256];
	snprintf(post_data, sizeof(post_data), "{\"username\": \"%s\"}", username);

	/* Set up CURL request */
	curl_easy_setopt(curl, CURLOPT_URL, DJANGO_API_URL);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");  // ✅ Django expects POST
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	/* Set JSON header */
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	/* Execute request */
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		slog(LG_ERROR, "ns_delete_notify: CURL request failed: %s", curl_easy_strerror(res));
	}
	else
	{
		slog(LG_INFO, "ns_delete_notify: Django notified about deleted user %s.", username);
	}

	/* Cleanup */
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
}

/* Hook for NickServ DROP */
static void
user_drop_hook(struct myuser *mu)
{
	if (! mu)
		return;

	const char *username = entity(mu)->name;
	slog(LG_INFO, "ns_delete_notify: Account %s has been deleted.", username);

	notify_django_user_deleted(username);
}

/* Hook for NickServ FDROP */
static void
user_fdrop_hook(struct myuser *mu)
{
	if (! mu)
		return;

	const char *username = entity(mu)->name;
	slog(LG_INFO, "ns_delete_notify: Account %s forcibly deleted.", username);

	notify_django_user_deleted(username);
}

/* Module Initialization */
static void
mod_init(struct module *const restrict m)
{
	/* Ensure NickServ is loaded */
	MODULE_TRY_REQUEST_DEPENDENCY(m, "nickserv/main");

	/* Attach directly to user_drop and user_fdrop hooks */
	hook_add_user_drop(user_drop_hook);
	hook_add_user_drop(user_fdrop_hook);

	slog(LG_INFO, "ns_delete_notify: Module loaded successfully.");
}

/* Module Deinitialization */
static void
mod_deinit(const enum module_unload_intent ATHEME_VATTR_UNUSED intent)
{
	/* Remove our hooks */
	hook_del_user_drop(user_drop_hook);
	hook_del_user_drop(user_fdrop_hook);

	slog(LG_INFO, "ns_delete_notify: Module unloaded.");
}

/* Declare module */
SIMPLE_DECLARE_MODULE_V1("nickserv/ns_delete_notify", MODULE_UNLOAD_CAPABILITY_OK);
