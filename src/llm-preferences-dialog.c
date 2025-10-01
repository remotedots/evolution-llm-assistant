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
 * Preferences dialog implementation for configuring OpenAI API settings,
 * model selection, and system prompts.
 */

#include "llm-preferences-dialog.h"
#include <string.h>

struct _LLMPreferencesDialog {
    GtkWidget *dialog;
    GtkWidget *api_key_entry;
    GtkWidget *model_combo;
    GtkWidget *system_prompt_text;

    PluginConfig *config;
    LLMPreferencesSaveCallback save_callback;
    gpointer user_data;
};

/* Available OpenAI models */
static const struct {
    const gchar *id;
    const gchar *display_name;
} available_models[] = {
    { "gpt-4o", "GPT-4o (Most capable, expensive)" },
    { "gpt-4o-mini", "GPT-4o Mini (Recommended, balanced)" },
    { "gpt-4-turbo", "GPT-4 Turbo (Fast, capable)" },
    { "gpt-4", "GPT-4 (Capable, slower)" },
    { "gpt-3.5-turbo", "GPT-3.5 Turbo (Fast, affordable)" },
};

/**
 * Free the preferences dialog structure
 */
static void
llm_preferences_dialog_free(LLMPreferencesDialog *prefs) {
    if (!prefs) return;
    g_free(prefs);
}

/**
 * Handle dialog response (OK or Cancel)
 */
static void
on_preferences_response(GtkDialog *dialog, gint response_id, LLMPreferencesDialog *prefs) {
    if (response_id == GTK_RESPONSE_OK) {
        /* Update config with values from UI */
        g_free(prefs->config->openai_api_key);
        g_free(prefs->config->model);
        g_free(prefs->config->system_prompt);

        /* Get API key from entry */
        prefs->config->openai_api_key =
            g_strdup(gtk_entry_get_text(GTK_ENTRY(prefs->api_key_entry)));

        /* Get selected model from combo box */
        gint active_index = gtk_combo_box_get_active(GTK_COMBO_BOX(prefs->model_combo));
        if (active_index >= 0 && active_index < (gint)(sizeof(available_models) / sizeof(available_models[0]))) {
            prefs->config->model = g_strdup(available_models[active_index].id);
        } else {
            prefs->config->model = g_strdup("gpt-4o-mini"); /* fallback */
        }

        /* Get system prompt from text view */
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(prefs->system_prompt_text));
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        prefs->config->system_prompt =
            gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

        /* Call the save callback if provided */
        if (prefs->save_callback) {
            prefs->save_callback(prefs->config, prefs->user_data);
        }
    }

    /* Cleanup */
    gtk_widget_destroy(GTK_WIDGET(dialog));
    llm_preferences_dialog_free(prefs);
}

/**
 * Create and show the preferences dialog
 *
 * @param parent_window Optional parent window for modal dialog
 * @param config Current configuration to display
 * @param save_callback Callback function called when user saves preferences
 * @param user_data User data passed to callback
 */
void
llm_preferences_dialog_show(GtkWindow *parent_window,
                             PluginConfig *config,
                             LLMPreferencesSaveCallback save_callback,
                             gpointer user_data)
{
    if (!config) {
        g_warning("LLM Preferences: Cannot show dialog with NULL config");
        return;
    }

    /* Allocate dialog structure */
    LLMPreferencesDialog *prefs = g_new0(LLMPreferencesDialog, 1);
    prefs->config = config;
    prefs->save_callback = save_callback;
    prefs->user_data = user_data;

    /* Create dialog */
    prefs->dialog = gtk_dialog_new_with_buttons(
        "LLM Assistant Preferences",
        parent_window,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_OK", GTK_RESPONSE_OK,
        NULL);

    gtk_window_set_default_size(GTK_WINDOW(prefs->dialog), 500, 400);

    /* Create content area with grid layout */
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(prefs->dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);

    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 6);

    /* OpenAI API Key field */
    GtkWidget *api_key_label = gtk_label_new("OpenAI API Key:");
    gtk_widget_set_halign(api_key_label, GTK_ALIGN_START);

    prefs->api_key_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(prefs->api_key_entry),
                       config->openai_api_key ? config->openai_api_key : "");
    gtk_entry_set_visibility(GTK_ENTRY(prefs->api_key_entry), FALSE);
    gtk_entry_set_input_purpose(GTK_ENTRY(prefs->api_key_entry), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_widget_set_hexpand(prefs->api_key_entry, TRUE);

    /* API Key hint label */
    GtkWidget *api_key_hint = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(api_key_hint),
        "You can get an API key from <a href=\"https://platform.openai.com/api-keys\">https://platform.openai.com/api-keys</a>");
    gtk_widget_set_margin_bottom(api_key_hint, 6);
    gtk_widget_set_halign(api_key_hint, GTK_ALIGN_START);

    /* Model selection field */
    GtkWidget *model_label = gtk_label_new("Model:");
    gtk_widget_set_halign(model_label, GTK_ALIGN_START);

    prefs->model_combo = gtk_combo_box_text_new();
    gint selected_index = -1;

    /* Try to fetch models from OpenAI API */
    gchar **fetched_models = NULL;
    if (config->openai_api_key && strlen(config->openai_api_key) > 10) {
        g_print("LLM Preferences: Fetching available models from OpenAI...\n");
        fetched_models = llm_client_fetch_available_models(config->openai_api_key);
    }

    if (fetched_models && fetched_models[0]) {
        /* Use fetched models */
        g_print("LLM Preferences: Using %d models from API\n", g_strv_length(fetched_models));
        for (gsize i = 0; fetched_models[i] != NULL; i++) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(prefs->model_combo),
                                      fetched_models[i],
                                      fetched_models[i]);
            /* Check if this is the current model */
            if (config->model && g_strcmp0(config->model, fetched_models[i]) == 0) {
                selected_index = i;
            }
        }
        g_strfreev(fetched_models);
    } else {
        /* Fallback to hardcoded models */
        g_print("LLM Preferences: Using hardcoded models (API fetch failed or no API key)\n");
        for (gsize i = 0; i < sizeof(available_models) / sizeof(available_models[0]); i++) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(prefs->model_combo),
                                      available_models[i].id,
                                      available_models[i].display_name);
            /* Check if this is the current model */
            if (config->model && g_strcmp0(config->model, available_models[i].id) == 0) {
                selected_index = i;
            }
        }
    }

    /* Set default selection if current model not found */
    if (selected_index < 0) {
        selected_index = 1; /* default to second item (gpt-4o-mini in hardcoded list) */
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(prefs->model_combo), selected_index);
    gtk_widget_set_hexpand(prefs->model_combo, TRUE);

    /* Model hint label */
    GtkWidget *model_hint = gtk_label_new("Recommended: gpt-4o-mini (best balance of speed, quality, and cost)");
    gtk_widget_set_margin_bottom(model_hint, 6);
    gtk_widget_set_halign(model_hint, GTK_ALIGN_START);

    /* System Prompt field */
    GtkWidget *system_prompt_label = gtk_label_new("System Prompt:");
    gtk_widget_set_halign(system_prompt_label, GTK_ALIGN_START);
    gtk_widget_set_valign(system_prompt_label, GTK_ALIGN_START);

    prefs->system_prompt_text = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(prefs->system_prompt_text), GTK_WRAP_WORD);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(prefs->system_prompt_text), 6);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(prefs->system_prompt_text), 6);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(prefs->system_prompt_text), 6);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(prefs->system_prompt_text), 6);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(prefs->system_prompt_text));
    gtk_text_buffer_set_text(buffer,
                             config->system_prompt ? config->system_prompt : "", -1);

    /* Wrap text view in scrolled window with border */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 200);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(scrolled), prefs->system_prompt_text);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);

    /* System Prompt hint label */
    GtkWidget *system_prompt_hint = gtk_label_new(
        "The system prompt sets the behavior of the AI assistant.\n"
        "e.g. \'You are a support operator that helps the users finding their answers.\n"
        "Keep the answer short and slightly informal but still professional.\n"
        "Answer in dutch. Use new lines where needed. Sign with The support team\'");
    gtk_widget_set_margin_bottom(system_prompt_hint, 6);
    gtk_widget_set_halign(system_prompt_hint, GTK_ALIGN_START);


    /* Add widgets to grid */
    gtk_grid_attach(GTK_GRID(grid), api_key_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), prefs->api_key_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), api_key_hint, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), model_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), prefs->model_combo, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), model_hint, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), system_prompt_label, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), scrolled, 1, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), system_prompt_hint, 1, 5, 1, 1);

    /* Add grid to dialog content area */
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    /* Connect response signal */
    g_signal_connect(prefs->dialog, "response",
                     G_CALLBACK(on_preferences_response), prefs);

    /* Show dialog */
    gtk_widget_show_all(prefs->dialog);
}