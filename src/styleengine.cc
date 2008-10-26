/*
 * File: styleengine.cc
 *
 * Copyright 2008 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include "styleengine.hh"

StyleEngine::StyleEngine () {
}

StyleEngine::~StyleEngine () {
}

void
StyleEngine::startElement (int tag, const char *id, const char *klass, const char *style) {
   fprintf(stderr, "===> START %d %s %s %s\n", tag, id, klass, style);
}

void
StyleEngine::endElement (int tag) {
   fprintf(stderr, "===> END %d\n", tag);
}
