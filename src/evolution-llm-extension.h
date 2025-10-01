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

#ifndef EVOLUTION_LLM_EXTENSION_H
#define EVOLUTION_LLM_EXTENSION_H

#include <glib-object.h>
#include <libedataserver/libedataserver.h>
#include <libebook-contacts/libebook-contacts.h>
#include <composer/e-composer-actions.h>
#include <composer/e-msg-composer.h>
#include <e-util/e-util.h>
#include <gtk/gtk.h>

#include "llm_client.h"
#include "../config/config.h"

#define E_TYPE_LLM_EXTENSION \
    (e_llm_extension_get_type())
#define E_LLM_EXTENSION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST \
    ((obj), E_TYPE_LLM_EXTENSION, ELLMExtension))
#define E_LLM_EXTENSION_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST \
    ((cls), E_TYPE_LLM_EXTENSION, ELLMExtensionClass))
#define E_IS_LLM_EXTENSION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE \
    ((obj), E_TYPE_LLM_EXTENSION))
#define E_IS_LLM_EXTENSION_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE \
    ((cls), E_TYPE_LLM_EXTENSION))
#define E_LLM_EXTENSION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS \
    ((obj), E_TYPE_LLM_EXTENSION, ELLMExtensionClass))

typedef struct _ELLMExtension ELLMExtension;
typedef struct _ELLMExtensionClass ELLMExtensionClass;
typedef struct _ELLMExtensionPrivate ELLMExtensionPrivate;

struct _ELLMExtension {
    EExtension parent;
    ELLMExtensionPrivate *priv;
};

struct _ELLMExtensionClass {
    EExtensionClass parent_class;
};

struct _ELLMExtensionPrivate {
    PluginConfig *config;
    LLMClient *llm_client;
    EMsgComposer *current_composer;
};

GType e_llm_extension_get_type(void) G_GNUC_CONST;
void e_llm_extension_type_register(GTypeModule *type_module);

#endif /* EVOLUTION_LLM_EXTENSION_H */