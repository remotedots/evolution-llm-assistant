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
 * Configuration management implementation. Handles loading, saving, and
 * validating user configuration stored in ~/.config/evolution-llm-assistant/
 */

#include "config.h"
#include <glib.h>
#include <glib/gstdio.h>

static gchar* get_config_dir(void) {
    return g_build_filename(g_get_user_config_dir(), CONFIG_DIR_NAME, NULL);
}

gchar* config_get_file_path(void) {
    gchar *config_dir = get_config_dir();
    gchar *config_path = g_build_filename(config_dir, CONFIG_FILE_NAME, NULL);
    g_free(config_dir);
    return config_path;
}

static void ensure_config_dir_exists(void) {
    gchar *config_dir = get_config_dir();
    if (!g_file_test(config_dir, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents(config_dir, 0755);
    }
    g_free(config_dir);
}

static void create_default_config(const gchar *config_path) {
    GKeyFile *keyfile = g_key_file_new();

    g_key_file_set_comment(keyfile, NULL, NULL,
        "Evolution LLM Assistant Configuration\n"
        "Get your OpenAI API key from: https://platform.openai.com/api-keys", NULL);

    g_key_file_set_string(keyfile, "openai", "api_key", "your_openai_api_key_here");
    g_key_file_set_string(keyfile, "openai", "model", DEFAULT_MODEL);
    g_key_file_set_string(keyfile, "openai", "system_prompt", "You are a helpful email writing assistant.");
    g_key_file_set_string(keyfile, "ui", "hotkey", DEFAULT_HOTKEY);
    g_key_file_set_string(keyfile, "user", "name", "Your Name");
    g_key_file_set_string(keyfile, "user", "email", "your.email@example.com");

    gchar *content = g_key_file_to_data(keyfile, NULL, NULL);
    g_file_set_contents(config_path, content, -1, NULL);

    g_free(content);
    g_key_file_free(keyfile);
}

PluginConfig* config_load(void) {
    ensure_config_dir_exists();

    gchar *config_path = config_get_file_path();

    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        create_default_config(config_path);
        g_warning("Created default config at %s. Please edit it with your OpenAI API key.", config_path);
    }

    GKeyFile *keyfile = g_key_file_new();
    GError *error = NULL;

    if (!g_key_file_load_from_file(keyfile, config_path, G_KEY_FILE_NONE, &error)) {
        g_warning("Failed to load config: %s", error->message);
        g_error_free(error);
        g_key_file_free(keyfile);
        g_free(config_path);
        return NULL;
    }

    PluginConfig *config = g_new0(PluginConfig, 1);

    config->openai_api_key = g_key_file_get_string(keyfile, "openai", "api_key", NULL);
    config->model = g_key_file_get_string(keyfile, "openai", "model", NULL);
    config->system_prompt = g_key_file_get_string(keyfile, "openai", "system_prompt", NULL);
    config->hotkey = g_key_file_get_string(keyfile, "ui", "hotkey", NULL);
    config->user_name = g_key_file_get_string(keyfile, "user", "name", NULL);
    config->user_email = g_key_file_get_string(keyfile, "user", "email", NULL);

    if (!config->model) {
        config->model = g_strdup(DEFAULT_MODEL);
    }

    if (!config->hotkey) {
        config->hotkey = g_strdup(DEFAULT_HOTKEY);
    }

    if (!config->system_prompt) {
        config->system_prompt = g_strdup("You are a helpful email writing assistant.");
    }

    g_key_file_free(keyfile);
    g_free(config_path);

    return config;
}

void config_free(PluginConfig *config) {
    if (!config) return;

    g_free(config->openai_api_key);
    g_free(config->model);
    g_free(config->hotkey);
    g_free(config->user_name);
    g_free(config->user_email);
    g_free(config->system_prompt);
    g_free(config);
}

gboolean config_is_valid(PluginConfig *config) {
    if (!config) return FALSE;

    return (config->openai_api_key &&
            g_strcmp0(config->openai_api_key, "your_openai_api_key_here") != 0 &&
            strlen(config->openai_api_key) > 10);
}

gboolean config_save(PluginConfig *config) {
    if (!config) return FALSE;

    ensure_config_dir_exists();
    gchar *config_path = config_get_file_path();
    GKeyFile *keyfile = g_key_file_new();

    g_key_file_set_comment(keyfile, NULL, NULL,
        "Evolution LLM Assistant Configuration\n"
        "Get your OpenAI API key from: https://platform.openai.com/api-keys", NULL);

    g_key_file_set_string(keyfile, "openai", "api_key",
                          config->openai_api_key ? config->openai_api_key : "");
    g_key_file_set_string(keyfile, "openai", "model",
                          config->model ? config->model : DEFAULT_MODEL);
    g_key_file_set_string(keyfile, "openai", "system_prompt",
                          config->system_prompt ? config->system_prompt : "You are a helpful email writing assistant.");
    g_key_file_set_string(keyfile, "ui", "hotkey",
                          config->hotkey ? config->hotkey : DEFAULT_HOTKEY);
    g_key_file_set_string(keyfile, "user", "name",
                          config->user_name ? config->user_name : "");
    g_key_file_set_string(keyfile, "user", "email",
                          config->user_email ? config->user_email : "");

    gchar *content = g_key_file_to_data(keyfile, NULL, NULL);
    gboolean result = g_file_set_contents(config_path, content, -1, NULL);

    g_free(content);
    g_free(config_path);
    g_key_file_free(keyfile);

    return result;
}