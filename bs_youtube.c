/*
 * Copyright (c) 2025 Mike "reverse" Chevronnet <HybridIRC Network>
 * Rights to this code are as documented in doc/LICENSE.
 */

 #include "atheme-compat.h"
 #include <curl/curl.h>
 #include <jansson.h>
 #include <string.h>
 
 #define YOUTUBE_API_KEY "AIzaSyCaY3gUxKHk8cr1HCkYsFzw7hcfh9H5v48" // Replace with your YouTube API key
 
 struct memory {
     char *response;
     size_t size;
 };
 
 // Callback function to write response data into memory
 static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
 {
     size_t realsize = size * nmemb;
     struct memory *mem = (struct memory *)userp;
 
     char *ptr = realloc(mem->response, mem->size + realsize + 1);
     if (ptr == NULL)
         return 0; // Out of memory
 
     mem->response = ptr;
     memcpy(&(mem->response[mem->size]), contents, realsize);
     mem->size += realsize;
     mem->response[mem->size] = 0;
 
     return realsize;
 }
 
 // Fetch YouTube metadata using libcurl
 static void fetch_youtube_metadata(const char *video_id, mychan_t *mc, user_t *user)
 {
     CURL *curl;
     CURLcode res;
 
     struct memory chunk = {0};
 
     char api_url[512];
     snprintf(api_url, sizeof(api_url),
              "https://www.googleapis.com/youtube/v3/videos?id=%s&key=%s&part=snippet,statistics",
              video_id, YOUTUBE_API_KEY);
 
     curl = curl_easy_init();
     if (!curl) {
         notice(chansvs.me->nick, user->nick, "Error: Could not initialize HTTP client.");
         return;
     }
 
     curl_easy_setopt(curl, CURLOPT_URL, api_url);
     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
     curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
     curl_easy_setopt(curl, CURLOPT_USERAGENT, "Atheme BotServ Youtuble link Module");
 
     res = curl_easy_perform(curl);
     if (res != CURLE_OK) {
         notice(chansvs.me->nick, user->nick, "Failed to fetch YouTube metadata, API MISSING?: %s", curl_easy_strerror(res));
     } else {
         // Process JSON response
         json_error_t error;
         json_t *root = json_loads(chunk.response, 0, &error);
         if (!root) {
             notice(chansvs.me->nick, user->nick, "Error: Failed to parse YouTube API response: %s", error.text);
         } else {
             json_t *items = json_object_get(root, "items");
             if (!items || !json_array_size(items)) {
                 notice(chansvs.me->nick, user->nick, "No metadata found for the video.");
             } else {
                 json_t *snippet = json_object_get(json_array_get(items, 0), "snippet");
                 json_t *statistics = json_object_get(json_array_get(items, 0), "statistics");
 
                 json_t *title = json_object_get(snippet, "title");
                 json_t *channel_title = json_object_get(snippet, "channelTitle");
                 json_t *view_count = json_object_get(statistics, "viewCount");
 
                 if (title && channel_title && view_count) {
                     char *formatted_prefix = "\x02\x03""01,00You\x03""00,04Tube\x0F\x02 ::";
                     char message[512];
                     snprintf(message, sizeof(message), "%s %s :: par: %s :: avec: %s vues.",
                              formatted_prefix,
                              json_string_value(title),
                              json_string_value(channel_title),
                              json_string_value(view_count));
                     msg(chansvs.me->nick, mc->name, "%s", message);
                 } else {
                     notice(chansvs.me->nick, user->nick, "Incomplete metadata found for the video.");
                 }
             }
             json_decref(root);
         }
     }
 
     curl_easy_cleanup(curl);
     if (chunk.response)
         free(chunk.response);
 }
 
 // Handler for messages containing YouTube links
 static void on_channel_message(hook_cmessage_data_t *data)
 {
     if (data == NULL || data->msg == NULL)
         return;
 
     const char *youtube_prefix = "https://www.youtube.com/watch?v=";
     const char *youtube_short_prefix = "https://youtu.be/";
 
     char *video_id = NULL;
 
     // Find the first occurrence of a YouTube link anywhere in the message
     char *url = strstr(data->msg, youtube_prefix);
     if (!url)
         url = strstr(data->msg, youtube_short_prefix);
 
     if (url) {
         // Extract video ID after the link
         if (strncmp(url, youtube_prefix, strlen(youtube_prefix)) == 0) {
             video_id = url + strlen(youtube_prefix);
         } else if (strncmp(url, youtube_short_prefix, strlen(youtube_short_prefix)) == 0) {
             video_id = url + strlen(youtube_short_prefix);
         }
     }
 
     if (video_id) {
         // Extract the video ID (truncate any extra parameters, if present)
         char video_id_clean[12] = {0};
         strncpy(video_id_clean, video_id, sizeof(video_id_clean) - 1);
         char *ampersand = strchr(video_id_clean, '&');
         if (ampersand)
             *ampersand = '\0';
 
         mychan_t *mc = mychan_from(data->c);
         if (mc)
             fetch_youtube_metadata(video_id_clean, mc, data->u);
     }
 }
 
 static void mod_init(module_t *const restrict m)
 {
     hook_add_event("channel_message");
     hook_add_channel_message(on_channel_message);
 }
 
 static void mod_deinit(const module_unload_intent_t intent)
 {
     hook_del_channel_message(on_channel_message);
 }
 
 VENDOR_DECLARE_MODULE_V1("contrib/bs_youtube", MODULE_UNLOAD_CAPABILITY_OK, CONTRIB_VENDOR_REVERSE)
 