# Sphinx configuration. Build with `pixi run doc`.

project = "Nun4tak SP4T"
author = "Benoit Bovy"
copyright = "Benoit Bovy"

extensions = ["myst_parser"]

# The README sections reproduced in ambisonics.md use HTML <img> tags.
myst_enable_extensions = ["html_image"]
myst_heading_anchors = 3

exclude_patterns = ["_build", "_art", "make_screenshots.py", "README.md"]

html_theme = "furo"
html_title = "Nun4tak SP4T"
html_static_path = ["_static"]
# PNG and not the SVG next to it: Safari still ignores SVG favicons.
html_favicon = "_static/favicon.png"
html_css_files = ["custom.css"]
html_theme_options = {
    "light_css_variables": {"color-brand-primary": "#22a4e6", "color-brand-content": "#22a4e6"},
    "dark_css_variables": {"color-brand-primary": "#59c2ff", "color-brand-content": "#59c2ff"},
}


# Furo builds its left-hand menu from document titles only, so the sections of
# a page never appear in it. The modules all live on one page here, and their
# groups are what a reader navigates by, so the tree is rebuilt with sections
# included -- through Furo's own helper, which is what adds the arrows that
# fold each branch away.
def _navigation_with_sections(app, pagename, templatename, context, doctree):
    if "toctree" not in context:
        return
    from furo.navigation import get_navigation_tree

    context["furo_navigation_tree"] = get_navigation_tree(
        context["toctree"](collapse=False, titles_only=False, maxdepth=-1, includehidden=True)
    )


def setup(app):
    # After Furo's own handler, which runs at the default priority.
    app.connect("html-page-context", _navigation_with_sections, priority=900)
