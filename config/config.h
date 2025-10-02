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

#ifndef CONFIG_H
#define CONFIG_H

#include <glib.h>

#define CONFIG_DIR_NAME "evolution-llm-assistant"
#define CONFIG_FILE_NAME "config.conf"
#define DEFAULT_MODEL "gpt-4o-mini"
#define DEFAULT_HOTKEY "ctrl+shift+g"

typedef struct {
    gchar *openai_api_key;
    gchar *model;
    gchar *hotkey;
    gchar *system_prompt;
} PluginConfig;

PluginConfig* config_load(void);
void config_free(PluginConfig *config);
gboolean config_is_valid(PluginConfig *config);
gchar* config_get_file_path(void);
gboolean config_save(PluginConfig *config);

#endif /* CONFIG_H */