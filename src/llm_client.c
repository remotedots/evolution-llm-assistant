/*
 * Evolution LLM Assistant - AI-powered email generation for GNOME Evolution
 *
 * Copyright (c) 2025 rf@remotedots.com
 *
 * This file is part of Evolution LLM Assistant.
 *
 * Evolution LLM Assistant is free software: you can redistribute it and/or modify
 * it under the terms of the MIT License as published in the LICENSE file.
 *
 * OpenAI API client implementation. Handles communication with OpenAI's
 * chat completion and model listing endpoints.
 */

#include "llm_client.h"
#include <curl/curl.h>
#include <json-glib/json-glib.h>
#include <string.h>

typedef struct {
    gchar *data;
    size_t size;
} HTTPResponse;

static size_t write_callback(void *contents, size_t size, size_t nmemb, HTTPResponse *response) {
    size_t total_size = size * nmemb;
    response->data = g_realloc(response->data, response->size + total_size + 1);

    if (response->data) {
        memcpy(&(response->data[response->size]), contents, total_size);
        response->size += total_size;
        response->data[response->size] = 0;
    }

    return total_size;
}

LLMClient* llm_client_new(PluginConfig *config) {
    if (!config_is_valid(config)) {
        return NULL;
    }

    LLMClient *client = g_new0(LLMClient, 1);
    client->config = config;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    return client;
}

void llm_client_free(LLMClient *client) {
    if (!client) return;

    curl_global_cleanup();
    g_free(client);
}

LLMRequest* llm_request_new(void) {
    return g_new0(LLMRequest, 1);
}

void llm_request_free(LLMRequest *request) {
    if (!request) return;

    g_free(request->original_email);
    g_free(request->sender_name);
    g_free(request->sender_email);
    g_free(request->prompt);
    g_free(request->response);
    g_free(request);
}

gboolean llm_client_parse_prompt(const gchar *text, gchar **prompt) {
    if (!text || !prompt) return FALSE;

    const gchar *prompt_start = strstr(text, PROMPT_PREFIX);
    if (!prompt_start) return FALSE;

    prompt_start += strlen(PROMPT_PREFIX);

    while (*prompt_start == ' ') prompt_start++;

    const gchar *prompt_end = strchr(prompt_start, '\n');
    if (!prompt_end) {
        prompt_end = prompt_start + strlen(prompt_start);
    }

    *prompt = g_strndup(prompt_start, prompt_end - prompt_start);
    g_strstrip(*prompt);

    return (*prompt && strlen(*prompt) > 0);
}

gchar* llm_client_extract_original_email(const gchar *compose_text) {
    if (!compose_text) return NULL;

    const gchar *quote_start = strstr(compose_text, "On ");
    if (!quote_start) {
        quote_start = strstr(compose_text, "> ");
    }

    if (quote_start) {
        return g_strdup(quote_start);
    }

    return NULL;
}

void llm_client_extract_sender_info(const gchar *email_headers,
                                    gchar **sender_name,
                                    gchar **sender_email) {
    if (!email_headers) return;

    const gchar *from_line = strstr(email_headers, "From:");
    if (!from_line) return;

    from_line += 5;
    while (*from_line == ' ') from_line++;

    const gchar *line_end = strchr(from_line, '\n');
    if (!line_end) line_end = from_line + strlen(from_line);

    gchar *from_value = g_strndup(from_line, line_end - from_line);
    g_strstrip(from_value);

    const gchar *email_start = strchr(from_value, '<');
    if (email_start) {
        const gchar *email_end = strchr(email_start, '>');
        if (email_end) {
            *sender_email = g_strndup(email_start + 1, email_end - email_start - 1);
            *sender_name = g_strndup(from_value, email_start - from_value);
            g_strstrip(*sender_name);
        }
    } else {
        if (strchr(from_value, '@')) {
            *sender_email = g_strdup(from_value);
            *sender_name = g_strdup("Unknown");
        }
    }

    g_free(from_value);
}

static gchar* build_user_prompt(LLMRequest *request, PluginConfig *config G_GNUC_UNUSED) {
    /* Simply return the selected text without any prefixes */
    return g_strdup(request->prompt);
}

/**
 * Fetch available models from OpenAI API
 *
 * @param api_key OpenAI API key
 * @return NULL-terminated array of model IDs, or NULL on error. Caller must free with g_strfreev()
 */
gchar** llm_client_fetch_available_models(const gchar *api_key) {
    if (!api_key || strlen(api_key) < 10) {
        return NULL;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    HTTPResponse response = {0};
    struct curl_slist *headers = NULL;

    gchar *auth_header = g_strdup_printf("Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/models");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    gchar **models = NULL;

    if (res == CURLE_OK && response.data) {
        JsonParser *parser = json_parser_new();
        GError *error = NULL;

        if (json_parser_load_from_data(parser, response.data, -1, &error)) {
            JsonNode *root_node = json_parser_get_root(parser);
            JsonObject *root_obj = json_node_get_object(root_node);

            if (json_object_has_member(root_obj, "data")) {
                JsonArray *data = json_object_get_array_member(root_obj, "data");
                guint length = json_array_get_length(data);

                /* Filter for GPT models only */
                GPtrArray *gpt_models = g_ptr_array_new();

                for (guint i = 0; i < length; i++) {
                    JsonObject *model = json_array_get_object_element(data, i);
                    const gchar *model_id = json_object_get_string_member(model, "id");

                    /* Only include GPT chat models */
                    if (model_id && (g_str_has_prefix(model_id, "gpt-4") ||
                                     g_str_has_prefix(model_id, "gpt-3.5"))) {
                        /* Filter out specific variants we don't want */
                        if (!g_str_has_suffix(model_id, "-vision") &&
                            !g_str_has_suffix(model_id, "-instruct") &&
                            !g_str_has_suffix(model_id, "-audio-preview")) {
                            g_ptr_array_add(gpt_models, g_strdup(model_id));
                        }
                    }
                }

                /* Convert to NULL-terminated array */
                g_ptr_array_add(gpt_models, NULL);
                models = (gchar **)g_ptr_array_free(gpt_models, FALSE);
            }
        } else {
            g_warning("Failed to parse models JSON: %s", error ? error->message : "unknown error");
            if (error) g_error_free(error);
        }

        g_object_unref(parser);
    }

    g_free(response.data);
    g_free(auth_header);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return models;
}

gboolean llm_client_generate_response(LLMClient *client, LLMRequest *request) {
    if (!client || !request || !request->prompt) return FALSE;

    CURL *curl = curl_easy_init();
    if (!curl) return FALSE;

    HTTPResponse response = {0};
    struct curl_slist *headers = NULL;

    gchar *auth_header = g_strdup_printf("Authorization: Bearer %s", client->config->openai_api_key);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    gchar *user_prompt = build_user_prompt(request, client->config);

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "model");
    json_builder_add_string_value(builder, client->config->model);

    json_builder_set_member_name(builder, "messages");
    json_builder_begin_array(builder);

    /* System message */
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "role");
    json_builder_add_string_value(builder, "system");
    json_builder_set_member_name(builder, "content");
    json_builder_add_string_value(builder,
                                   client->config->system_prompt ? client->config->system_prompt : "You are a helpful email writing assistant.");
    json_builder_end_object(builder);

    /* User message */
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "role");
    json_builder_add_string_value(builder, "user");
    json_builder_set_member_name(builder, "content");
    json_builder_add_string_value(builder, user_prompt);
    json_builder_end_object(builder);

    json_builder_end_array(builder);

    json_builder_set_member_name(builder, "max_tokens");
    json_builder_add_int_value(builder, 500);

    json_builder_set_member_name(builder, "temperature");
    json_builder_add_double_value(builder, 0.7);

    json_builder_end_object(builder);

    JsonGenerator *generator = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    gchar *json_data = json_generator_to_data(generator, NULL);

    /* Debug: Print the request being sent */
    g_print("\n=== LLM Request Debug ===\n");
    g_print("Model: %s\n", client->config->model);
    g_print("System Prompt: %s\n", client->config->system_prompt ? client->config->system_prompt : "You are a helpful email writing assistant.");
    g_print("User Prompt: %s\n", user_prompt);
    g_print("Full JSON payload:\n%s\n", json_data);
    g_print("========================\n\n");

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    gboolean success = FALSE;

    if (res == CURLE_OK && response.data) {
        JsonParser *parser = json_parser_new();
        GError *error = NULL;

        if (json_parser_load_from_data(parser, response.data, -1, &error)) {
            JsonNode *root_node = json_parser_get_root(parser);
            JsonObject *root_obj = json_node_get_object(root_node);

            if (json_object_has_member(root_obj, "choices")) {
                JsonArray *choices = json_object_get_array_member(root_obj, "choices");
                if (json_array_get_length(choices) > 0) {
                    JsonObject *choice = json_array_get_object_element(choices, 0);
                    JsonObject *message = json_object_get_object_member(choice, "message");
                    const gchar *content = json_object_get_string_member(message, "content");

                    if (content) {
                        request->response = g_strdup(content);
                        g_strstrip(request->response);
                        success = TRUE;
                    }
                }
            }
        } else {
            g_warning("JSON parse error: %s", error->message);
            g_error_free(error);
        }

        g_object_unref(parser);
    }

    g_free(response.data);
    g_free(auth_header);
    g_free(user_prompt);
    g_free(json_data);
    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return success;
}