/*
 * Evolution LLM Assistant - AI-powered email generation for GNOME Evolution
 *
 * Copyright (c) 2025 rf@remotedots.com
 *
 * This file is part of Evolution LLM Assistant.
 *
 * Evolution LLM Assistant is free software: you can redistribute it and/or modify
 * it under the terms of the MIT License as published in the LICENSE file.
 */

#ifndef LLM_CLIENT_H
#define LLM_CLIENT_H

#include <glib.h>
#include "../config/config.h"

#define PROMPT_PREFIX "/aw:"

typedef struct {
    gchar *original_email;
    gchar *sender_name;
    gchar *sender_email;
    gchar *prompt;
    gchar *response;
} LLMRequest;

typedef struct {
    PluginConfig *config;
} LLMClient;

LLMClient* llm_client_new(PluginConfig *config);
void llm_client_free(LLMClient *client);

LLMRequest* llm_request_new(void);
void llm_request_free(LLMRequest *request);

gboolean llm_client_parse_prompt(const gchar *text, gchar **prompt);
gboolean llm_client_generate_response(LLMClient *client, LLMRequest *request);

gchar* llm_client_extract_original_email(const gchar *compose_text);
void llm_client_extract_sender_info(const gchar *email_headers,
                                    gchar **sender_name,
                                    gchar **sender_email);

/**
 * Fetch available models from OpenAI API
 *
 * @param api_key OpenAI API key
 * @return NULL-terminated array of model IDs, or NULL on error. Caller must free with g_strfreev()
 */
gchar** llm_client_fetch_available_models(const gchar *api_key);

#endif /* LLM_CLIENT_H */