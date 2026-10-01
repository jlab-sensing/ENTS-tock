"""Sphinx configuration for the ents python package.

Build with ./build.sh, or `sphinx-build -b html . build/html` from here.
"""

import sys
import tomllib
from pathlib import Path

HERE = Path(__file__).parent
PYPROJECT = HERE.parent / "pyproject.toml"

# The package uses a src layout, so autodoc cannot see it unless ents is
# installed or src is on the path. Adding it here means the docs build from a
# plain checkout, which is what makes `./build.sh` work on a fresh clone and in
# CI without an editable install step. An installed copy still wins, since
# anything already in sys.modules is not re-imported.
sys.path.insert(0, str((HERE.parent / "src").resolve()))

# -- Project ----------------------------------------------------------------

project = "ents"
author = "jLab, UCSC"
copyright = "2026, jLab, UCSC"  # Sphinx requires this exact name

# Single source of truth. Hardcoding the version here is how it ends up a year
# out of date on the published site.
with PYPROJECT.open("rb") as f:
    release = tomllib.load(f)["project"]["version"]
version = release

# -- Extensions -------------------------------------------------------------

extensions = [
    # Pull documentation out of the docstrings rather than restating it here.
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    # The package uses Google style docstrings ("Args:", "Returns:"), which
    # Sphinx cannot read without this.
    "sphinx.ext.napoleon",
    # Link type annotations like datetime and pandas.DataFrame to their own
    # documentation instead of rendering them as dead text.
    "sphinx.ext.intersphinx",
    # "View source" links.
    "sphinx.ext.viewcode",
    # Lets the pages be written in Markdown, matching every other document in
    # this repository, instead of reStructuredText.
    "myst_parser",
]

autosummary_generate = True

autodoc_default_options = {
    "members": True,
    "member-order": "bysource",
    "undoc-members": False,
    "show-inheritance": True,
}

# Importing ents pulls in pandas, protobuf, matplotlib and friends. They are
# real dependencies and should be installed, but a docs-only environment that
# is missing one should still build rather than failing on an import error.
autodoc_mock_imports = [
    "serial",
    "sklearn",
    "matplotlib",
    "PyQt5",
]

napoleon_google_docstring = True
napoleon_numpy_docstring = False

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "pandas": ("https://pandas.pydata.org/docs", None),
}

# colon_fence gives ::: directives, which read better in Markdown than backtick
# fences. linkify is deliberately absent: it needs linkify-it-py as an extra
# dependency and only auto-links bare URLs, which is not worth it.
myst_enable_extensions = ["colon_fence"]

# Generate anchors for headings down to h3, so one page can link to a section of
# another (usage.md -> development.md#adding-a-sensortype). Off by default,
# which makes those links fail silently as "local id not found".
myst_heading_anchors = 3

# -- HTML -------------------------------------------------------------------

html_theme = "furo"
html_title = f"ents {release}"
html_static_path = []

templates_path = ["_templates"]
exclude_patterns = ["build", "Thumbs.db", ".DS_Store"]
