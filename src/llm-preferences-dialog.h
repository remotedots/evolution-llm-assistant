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

#ifndef LLM_PREFERENCES_DIALOG_H
#define LLM_PREFERENCES_DIALOG_H

#include <gtk/gtk.h>
#include "../config/config.h"
#include "llm_client.h"

G_BEGIN_DECLS

typedef struct _LLMPreferencesDialog LLMPreferencesDialog;

/**
 * Callback function type called when preferences are saved
 *
 * @param config The updated configuration
 * @param user_data User data passed to callback
 */
typedef void (*LLMPreferencesSaveCallback)(PluginConfig *config, gpointer user_data);

/**
 * Create and show the preferences dialog
 *
 * @param parent_window Optional parent window for modal dialog
 * @param config Current configuration to display
 * @param save_callback Callback function called when user saves preferences
 * @param user_data User data passed to callback
 */
void llm_preferences_dialog_show(GtkWindow *parent_window,
                                  PluginConfig *config,
                                  LLMPreferencesSaveCallback save_callback,
                                  gpointer user_data);

G_END_DECLS

#endif /* LLM_PREFERENCES_DIALOG_H */