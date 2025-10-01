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
 * Main extension module that integrates with Evolution's composer window.
 * Provides context menu integration and keyboard shortcuts for AI-powered
 * email generation using OpenAI's GPT models.
 */

#include "evolution-llm-extension.h"
#include "llm-preferences-dialog.h"
#include <gmodule.h>
#include <gdk/gdkkeysyms.h>

G_DEFINE_DYNAMIC_TYPE_EXTENDED(ELLMExtension, e_llm_extension, E_TYPE_EXTENSION, 0,
    G_ADD_PRIVATE_DYNAMIC(ELLMExtension))

typedef struct {
    ELLMExtension *extension;
    gchar *prompt;
    gchar *original_text;
} LLMProcessData;

static void llm_extension_process_prompt(ELLMExtension *extension);
static void llm_extension_setup_composer(ELLMExtension *extension, EMsgComposer *composer);
static void llm_extension_cleanup_composer(ELLMExtension *extension);
static WebKitWebView* find_webkit_web_view_recursive(GtkWidget *widget);

/**
 * Create new LLMProcessData
 *
 * @param extension The LLM extension instance
 */
static LLMProcessData* llm_process_data_new(ELLMExtension *extension) {
    LLMProcessData *data = g_new0(LLMProcessData, 1);
    data->extension = g_object_ref(extension);
    return data;
}

/* Free LLMProcessData */
static void llm_process_data_free(LLMProcessData *data) {
    if (!data) return;
    g_object_unref(data->extension);
    g_free(data->prompt);
    g_free(data->original_text);
    g_free(data);
}

/**
 * Callback when user saves preferences
 * Reinitializes the LLM client with new configuration
 */
static void
on_preferences_saved(PluginConfig *config, gpointer user_data) {
    ELLMExtension *extension = E_LLM_EXTENSION(user_data);

    /* Save to file */
    if (config_save(config)) {
        /* Reinitialize LLM client */
        if (extension->priv->llm_client) {
            llm_client_free(extension->priv->llm_client);
        }
        extension->priv->llm_client = llm_client_new(config);
        g_print("LLM Assistant: Configuration saved successfully\n");
    } else {
        g_warning("LLM Assistant: Failed to save configuration");
    }
}

/* Action callback for context menu */
static void
action_llm_generate_cb(EUIAction *action G_GNUC_UNUSED,
                       GVariant *parameter G_GNUC_UNUSED,
                       gpointer user_data)
{
    ELLMExtension *extension = E_LLM_EXTENSION(user_data);
    g_print("LLM Assistant: Context menu action triggered\n");
    llm_extension_process_prompt(extension);
}

/* Action callback for preferences */
static void
action_llm_preferences_cb(EUIAction *action G_GNUC_UNUSED,
                          GVariant *parameter G_GNUC_UNUSED,
                          gpointer user_data)
{
    ELLMExtension *extension = E_LLM_EXTENSION(user_data);
    g_print("LLM Assistant: Preferences action triggered\n");

    if (!extension->priv->config) {
        g_warning("LLM Assistant: No configuration available");
        return;
    }

    /* Show preferences dialog */
    GtkWindow *parent_window = extension->priv->current_composer ?
                               GTK_WINDOW(extension->priv->current_composer) : NULL;

    llm_preferences_dialog_show(parent_window,
                                extension->priv->config,
                                on_preferences_saved,
                                extension);
}

/* Define the action entries */
static const EUIActionEntry llm_composer_entries[] = {
    { "llm-generate-response",
      NULL,  /* icon name */
      "Generate LLM response",
      "<Shift><Control>G",  /* accelerator - Ctrl+Shift+G */
      "Generate AI response from selected text",
      action_llm_generate_cb, NULL, NULL, NULL },
    { "llm-preferences",
      "preferences-system",  /* icon name */
      "LLM Assistant Preferences...",
      NULL,  /* no accelerator - only accessible via context menu */
      "Configure LLM Assistant",
      action_llm_preferences_cb, NULL, NULL, NULL }
};

/* Cleanup when composer is destroyed */
static void
on_composer_destroyed(GtkWidget *composer G_GNUC_UNUSED, ELLMExtension *extension) {
    llm_extension_cleanup_composer(extension);
}

/* Setup the extension for the given composer */
static void
llm_extension_setup_composer(ELLMExtension *extension, EMsgComposer *composer) {
    if (extension->priv->current_composer == composer) {
        return;
    }

    llm_extension_cleanup_composer(extension);

    extension->priv->current_composer = composer;
    g_object_ref(composer);

    g_signal_connect(composer, "destroy", G_CALLBACK(on_composer_destroyed), extension);

    /* Add our actions to the composer's UI manager */
    EHTMLEditor *html_editor = e_msg_composer_get_editor(composer);
    if (html_editor) {
        EUIManager *ui_manager = e_html_editor_get_ui_manager(html_editor);

        /* EUI definition - add to context menu only, use hotkeys for all actions */
        const gchar *eui_def =
            "<eui>"
              /* Context menu - add both actions */
              "<menu id='context'>"
                "<placeholder id='custom-actions'>"
                  "<item action='llm-generate-response'/>"
                  "<item action='llm-preferences'/>"
                "</placeholder>"
              "</menu>"
              /* Try alternative context menu IDs */
              "<menu id='context-menu'>"
                "<item action='llm-generate-response'/>"
                "<item action='llm-preferences'/>"
              "</menu>"
              "<menu id='mail-composer-context'>"
                "<item action='llm-generate-response'/>"
                "<item action='llm-preferences'/>"
              "</menu>"
            "</eui>";

        g_print("LLM Assistant: Adding actions to UI manager\n");
        e_ui_manager_add_actions_with_eui_data(ui_manager, "composer",
            NULL, /* translation domain */
            llm_composer_entries, G_N_ELEMENTS(llm_composer_entries),
            extension, eui_def);
        g_print("LLM Assistant: Actions registered (context menu + Edit menu + hotkeys)\n");
    }

    g_print("LLM Assistant: Module loaded.\n");
    g_print("  Ctrl+Shift+G - Generate LLM response from selected text\n");
    g_print("  Right-click menu - Access preferences and generation\n");
}

/* Cleanup composer connections */
static void
llm_extension_cleanup_composer(ELLMExtension *extension) {
    if (extension->priv->current_composer) {
        g_signal_handlers_disconnect_by_data(extension->priv->current_composer, extension);
        g_object_unref(extension->priv->current_composer);
        extension->priv->current_composer = NULL;
    }
}

/* Callback when JavaScript to get selection completes */
static void
on_js_selection_result(GObject *source, GAsyncResult *result, gpointer user_data) {
    WebKitWebView *web_view = WEBKIT_WEB_VIEW(source);
    LLMProcessData *data = (LLMProcessData *)user_data;
    GError *error = NULL;

    /* Use new API: webkit_web_view_evaluate_javascript_finish */
    JSCValue *value = webkit_web_view_evaluate_javascript_finish(web_view, result, &error);

    if (error) {
        g_warning("JavaScript error: %s", error->message);
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(data->extension->priv->current_composer),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Failed to get text selection.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_error_free(error);
        llm_process_data_free(data);
        return;
    }

    gchar *selected_text = jsc_value_to_string(value);

    if (!selected_text || strlen(selected_text) == 0 || g_strcmp0(selected_text, "") == 0) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(data->extension->priv->current_composer),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "No text selected. Please select text with your mouse first.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_free(selected_text);
        llm_process_data_free(data);
        return;
    }

    g_print("LLM Assistant: Selected text: %s\n", selected_text);

    /* Create progress dialog */
    GtkWidget *progress_dialog = gtk_message_dialog_new(
        GTK_WINDOW(data->extension->priv->current_composer),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_NONE,
        "Generating response...");
    gtk_widget_show(progress_dialog);

    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    /* Send to OpenAI */
    LLMRequest *request = llm_request_new();
    request->prompt = g_strdup(selected_text);

    g_print("LLM Assistant: Sending request to OpenAI...\n");

    gboolean success = llm_client_generate_response(data->extension->priv->llm_client, request);

    g_print("LLM Assistant: API call result: %s\n", success ? "SUCCESS" : "FAILED");

    gtk_widget_destroy(progress_dialog);

    if (success && request->response) {
        g_print("LLM Assistant: Generated response: %s\n", request->response);

        /* Get content editor to insert response */
        EHTMLEditor *html_editor = e_msg_composer_get_editor(data->extension->priv->current_composer);
        if (html_editor) {
            EContentEditor *content_editor = e_html_editor_get_content_editor(html_editor);
            if (content_editor) {
                /* Insert response - it will replace the selection */
                e_content_editor_insert_content(content_editor, request->response,
                    E_CONTENT_EDITOR_INSERT_TEXT_PLAIN);
                g_print("LLM Assistant: Response inserted\n");
            }
        }
    } else {
        GtkWidget *error_dialog = gtk_message_dialog_new(
            GTK_WINDOW(data->extension->priv->current_composer),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Failed to generate response. Please check your internet connection and API key.");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
    }

    llm_request_free(request);
    llm_process_data_free(data);
}

/* Recursively search for WebKitWebView in the widget tree */
static WebKitWebView*
find_webkit_web_view_recursive(GtkWidget *widget) {
    const gchar *type_name = G_OBJECT_TYPE_NAME(widget);
    g_print("  Checking widget: %s\n", type_name);

    if (WEBKIT_IS_WEB_VIEW(widget)) {
        g_print("  -> FOUND WebKitWebView!\n");
        return WEBKIT_WEB_VIEW(widget);
    }

    if (GTK_IS_CONTAINER(widget)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList *l = children; l != NULL; l = l->next) {
            WebKitWebView *found = find_webkit_web_view_recursive(GTK_WIDGET(l->data));
            if (found) {
                g_list_free(children);
                return found;
            }
        }
        g_list_free(children);
    }

    return NULL;
}

/* Process the selected prompt text */
static void
llm_extension_process_prompt(ELLMExtension *extension) {
    if (!extension->priv->current_composer) {
        g_warning("No active composer found");
        return;
    }

    if (!config_is_valid(extension->priv->config)) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(extension->priv->current_composer),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "LLM Assistant configuration is invalid. Please press Ctrl+Shift+P to configure.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    EHTMLEditor *html_editor = e_msg_composer_get_editor(extension->priv->current_composer);
    if (!E_IS_HTML_EDITOR(html_editor)) {
        g_warning("Could not get HTML editor from composer");
        return;
    }

    EContentEditor *content_editor = e_html_editor_get_content_editor(html_editor);
    if (!content_editor) {
        g_warning("Could not get content editor");
        return;
    }

    /* Recursively search for WebKitWebView in the widget tree */
    GtkWidget *editor_widget = GTK_WIDGET(content_editor);
    WebKitWebView *web_view = find_webkit_web_view_recursive(editor_widget);

    if (!web_view) {
        g_warning("Could not find WebKitWebView in content editor widget tree");
        return;
    }

    g_print("LLM Assistant: Found WebKitWebView, getting selection...\n");

    LLMProcessData *data = llm_process_data_new(extension);

    /* Use new API: webkit_web_view_evaluate_javascript */
    const gchar *js_code = "window.getSelection().toString();";
    webkit_web_view_evaluate_javascript(
        web_view,
        js_code,
        -1, /* length, -1 for null-terminated */
        NULL, /* world_name */
        NULL, /* source_uri */
        NULL, /* GCancellable */
        (GAsyncReadyCallback)on_js_selection_result,
        data);
}

/* GObject lifecycle methods */
static void
llm_extension_constructed(GObject *object) {
    ELLMExtension *extension = E_LLM_EXTENSION(object);
    EExtensible *extensible = e_extension_get_extensible(E_EXTENSION(extension));

    G_OBJECT_CLASS(e_llm_extension_parent_class)->constructed(object);

    if (E_IS_MSG_COMPOSER(extensible)) {
        llm_extension_setup_composer(extension, E_MSG_COMPOSER(extensible));
    }
}

/* GObject dispose method */
static void
llm_extension_dispose(GObject *object) {
    ELLMExtension *extension = E_LLM_EXTENSION(object);

    llm_extension_cleanup_composer(extension);

    if (extension->priv->llm_client) {
        llm_client_free(extension->priv->llm_client);
        extension->priv->llm_client = NULL;
    }

    if (extension->priv->config) {
        config_free(extension->priv->config);
        extension->priv->config = NULL;
    }

    G_OBJECT_CLASS(e_llm_extension_parent_class)->dispose(object);
}

/* Class initialization */

static void
e_llm_extension_class_init(ELLMExtensionClass *class) {
    GObjectClass *object_class = G_OBJECT_CLASS(class);
    EExtensionClass *extension_class = E_EXTENSION_CLASS(class);

    object_class->constructed = llm_extension_constructed;
    object_class->dispose = llm_extension_dispose;

    extension_class->extensible_type = E_TYPE_MSG_COMPOSER;
}

static void
e_llm_extension_class_finalize(ELLMExtensionClass *class G_GNUC_UNUSED) {
}

static void
e_llm_extension_init(ELLMExtension *extension) {
    extension->priv = e_llm_extension_get_instance_private(extension);

    extension->priv->config = config_load();
    if (extension->priv->config) {
        extension->priv->llm_client = llm_client_new(extension->priv->config);
    }
}

void
e_llm_extension_type_register(GTypeModule *type_module) {
    e_llm_extension_register_type(type_module);
}

G_MODULE_EXPORT void
e_module_load(GTypeModule *type_module) {
    e_llm_extension_type_register(type_module);
}

G_MODULE_EXPORT void
e_module_unload(GTypeModule *type_module G_GNUC_UNUSED) {
}

G_MODULE_EXPORT const gchar *
e_module_name(void) {
    return "LLM Assistant";
}

G_MODULE_EXPORT const gchar *
e_module_version(void) {
    return "1.0";
}
