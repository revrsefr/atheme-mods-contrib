/*
 * Enhances NS LOGIN to automatically switch to the account nick and release ghosts.
 * Also forces a guest nickname upon logout.
 *
 * Copyright (c) 2024
 * Licensed under Atheme's licensing terms.
 */

 #include "atheme-compat.h"

 // Function to generate a guest nickname and apply it
 static void apply_guest_nickname(struct user *u)
 {
     char guest_nick[NICKLEN + 1];
     int tries;
 
     // Try generating a unique guest nickname up to 30 times
     for (tries = 0; tries < 30; tries++)
     {
         snprintf(guest_nick, sizeof(guest_nick), "%s%u", nicksvs.enforce_prefix, 1 + atheme_random_uniform(9999));
 
         if (!user_find_named(guest_nick))
             break;
     }
 
     // Force a nickname change
     fnc_sts(nicksvs.me->me, u, guest_nick, FNC_FORCE);
 }
 
 // Hook function: Triggered when a user logs in
 static void ns_login_hook(struct user *u)
 {
     myuser_t *mu;
     struct mynick *mn;
     mowgli_node_t *n;
 
     if (!u || !u->myuser)
         return;
 
     mu = u->myuser;
     mn = mynick_find(entity(mu)->name);
 
     // If the user is logged in but using a different nickname, enforce it IMMEDIATELY
     if (mn && strcasecmp(u->nick, mn->nick))
     {
         notice(nicksvs.nick, u->nick, "You are now logged in as \2%s\2. Changing your nickname immediately.", mn->nick);
         fnc_sts(nicksvs.me->me, u, mn->nick, FNC_FORCE);
     }
 
     // Check and remove ghosted users
     MOWGLI_ITER_FOREACH(n, mu->logins.head)
     {
         user_t *ghost = n->data;
         if (ghost != u)
         {
             notice(nicksvs.nick, ghost->nick, "Your nickname has been reclaimed by %s.", u->nick);
             quit_sts(ghost, "Nickname reclaimed");
             user_delete(ghost, "Nickname reclaimed by owner");
         }
     }
 }
 
 // Hook function: Triggered before a user logs out
 static void ns_logout_hook(struct hook_user_logout_check *data)
 {
     if (!data || !data->u)
         return;
 
     notice(nicksvs.nick, data->u->nick, "You have logged out. Changing your nickname.");
     apply_guest_nickname(data->u);
 }
 
 static void mod_init(module_t *m)
 {
     hook_add_event("user_identify");
     hook_add_user_identify(ns_login_hook);
 
     hook_add_event("user_can_logout");
     hook_add_user_can_logout(ns_logout_hook);
 }
 
 static void mod_deinit(module_unload_intent_t intent)
 {
     hook_del_user_identify(ns_login_hook);
     hook_del_user_can_logout(ns_logout_hook);
 }
 
 SIMPLE_DECLARE_MODULE_V1("contrib/ns_login", MODULE_UNLOAD_CAPABILITY_OK)
 
