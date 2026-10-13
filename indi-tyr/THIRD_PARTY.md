# Licensing and dependencies

The Tyr INDI driver, its documentation and test helpers are copyright
2026 Prometheus Astro and distributed under GNU GPL version 2 or any later
version (SPDX: `GPL-2.0-or-later`). The complete GPLv2 text is in `COPYING`.
The driver is provided without warranty as described in that licence.

This licence is scoped to this driver directory. It does not relicense the
Windows Control Center, ASCOM bridge, mount firmware or unrelated repository
contents.

INDI is an external dependency and retains its upstream licence and notices.
The installed `nlohmann_json` library is an external header-only dependency;
its code is not vendored in this source tree. Its headers are instantiated into
the driver binary, so the following notices accompany binary distributions.

## nlohmann/json — MIT / Expat

Copyright (c) 2013–2025 Niels Lohmann.

Included conversion/serialization portions additionally retain copyright
(c) 2009 Florian Loitsch and (c) 2008–2009 Bjoern Hoehrmann, respectively.
Future dependency versions retain their own updated notices.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to
do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Source and complete dependency notices:
[nlohmann/json](https://github.com/nlohmann/json),
[INDI](https://github.com/indilib/indi).
