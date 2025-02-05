/*
 * SPDX-License-Identifier: ISC
 * SPDX-URL: https://spdx.org/licenses/ISC.html
 *
 * Atheme Module: Add "Access Level" to NickServ INFO (IRCops can set, everyone can see)
 */

#include "atheme-compat.h"

// Define a key for storing the role persistently
#define NETWORK_ROLE_KEY "private:network_role"

// Add the "Access Level" field to NickServ INFO output
static void add_access_level_to_info(hook_user_req_t *req)
{
    if (req == NULL || req->mu == NULL || req->si == NULL)
        return;

    struct metadata *md = metadata_find(req->mu, NETWORK_ROLE_KEY);
    if (md != NULL)
    {
        // Display the custom field "OPerator Rank" in the INFO output
        command_success_nodata(req->si, _("Oper rank  : %s"), md->value);
    }
}

// Allow only IRCops to set the network role
static void ns_cmd_setrole(sourceinfo_t *si, int parc, char *parv[])
{
    if (!has_priv(si, PRIV_USER_AUSPEX)) // Ensure only IRCops can use this command
    {
        command_fail(si, fault_noprivs, _("You do not have the required privilege to set network roles."));
        return;
    }

    if (parc < 2)
    {
        command_fail(si, fault_needmoreparams, _("Usage: SETROLE <account> <role>"));
        return;
    }

    const char *account_name = parv[0];
    const char *role = parv[1];

    struct myuser *mu = myuser_find(account_name);
    if (mu == NULL)
    {
        command_fail(si, fault_nosuch_target, _("Account \2%s\2 does not exist."), account_name);
        return;
    }

    // Sanitize the role to ensure it's safe
    if (strchr(role, '\n') || strchr(role, '\r') || strchr(role, ';'))
    {
        command_fail(si, fault_badparams, _("Invalid role name. Role cannot contain newlines or semicolons."));
        return;
    }

    // Store the sanitized role in metadata
    metadata_add(mu, NETWORK_ROLE_KEY, role);
    logcommand(si, CMDLOG_ADMIN, "SETROLE: \2%s\2 \2%s\2", account_name, role);
    command_success_nodata(si, _("OPer rank \2%s\2 has been set for account \2%s\2."), role, account_name);
}

// Define the command for IRCops to set the role
static struct command ns_setrole = {
    .name           = "SETROLE",
    .desc           = N_("Sets the network role for an account (IRCops only)."),
    .access         = PRIV_USER_AUSPEX, // Restrict to IRCops
    .maxparc        = 2,
    .cmd            = &ns_cmd_setrole,
    .help           = { .path = "nickserv/setrole" },
};

static void mod_init(module_t *const restrict m)
{
    MODULE_TRY_REQUEST_DEPENDENCY(m, "nickserv/main");

    // Hook into the NickServ INFO command to display the role
    hook_add_event("user_info");
    hook_add_user_info(add_access_level_to_info);

    // Bind the SETROLE command
    service_named_bind_command("nickserv", &ns_setrole);
}

static void mod_deinit(const module_unload_intent_t ATHEME_VATTR_UNUSED intent)
{
    // Remove the hook and unbind the SETROLE command
    hook_del_user_info(add_access_level_to_info);
    service_named_unbind_command("nickserv", &ns_setrole);
}

SIMPLE_DECLARE_MODULE_V1("nickserv/ns_roleinfo", MODULE_UNLOAD_CAPABILITY_OK);
