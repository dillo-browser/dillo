/*
 * File: about.c
 *
 * Copyright (C) 1999-2007 Jorge Arellano Cid <jcid@dillo.org>
 * Copyright (C) 2024-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <config.h>

/**
 * HTML text for startup screen
 */
const char *const AboutSplash=
"<!DOCTYPE HTML>\n"
"<html>\n"
"<head>\n"
"  <title>Quickstart</title>\n"
"  <style>\n"
"    body {\n"
"      background: white;\n"
"      margin: 3em;\n"
"      font-size: 16px;\n"
"      font-family: sans-serif;\n"
"      line-height: 1.4em;\n"
"    }\n"
"    .main { max-width: 40em; }\n"
"    p { margin-top: 1em; }\n"
"    ul { margin-left: 1em; }\n"
"    li { margin-top: 0.5em; }\n"
"    kbd {\n"
"      display: inline-block;\n"
"      border: solid 1px #999;\n"
"      padding: 1px 3px;\n"
"    }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"<div class=\"main\">\n"
"\n"
"<h1>Quickstart</h1>\n"
"\n"
"<p>Welcome to Dillo " VERSION ", a small and fast graphical web browser. To\n"
"access the help click the question mark button <code><kbd>?</kbd></code> in the top\n"
"right corner at any time. Here are some tips to get you started:</p>\n"
"\n"
"<ul>\n"
"  <li>The main configuration file is at <code>~/.dillo/dillorc</code>.</li>\n"
"  <li>Most actions can also be done by using the <a href='about:keys'>keyboard</a>.</li>\n"
"  <li>Cookies are <em>disabled by default</em>.</li>\n"
"  <li>Several Dillo plugins are available.</li>\n"
"</ul>\n"
"\n"
"<p>See more details in the\n"
"<a href=\"https://dillo-browser.github.io/\">Dillo website</a>.</p>\n"
"\n"
"</div>\n"
"</body>\n"
"</html>\n";

