# Sphinx configuration. Build with `pixi run doc`.

project = "Nun4tak SP4T"
author = "Benoit Bovy"
copyright = "Benoit Bovy"

extensions = ["myst_parser"]

myst_enable_extensions = ["html_image"]
myst_heading_anchors = 3

numfig = True

exclude_patterns = ["_build", "_art", "make_screenshots.py", "README.md"]

html_theme = "furo"
html_title = "Nun4tak SP4T"
html_static_path = ["_static"]
html_favicon = "_static/favicon.png"
html_css_files = [
    "https://fonts.googleapis.com/css2?family=Albert+Sans:wght@400;500;600"
    "&family=Quicksand:wght@500;600;700&display=swap",
    "custom.css",
]

# The palette of nun4tak.net: Ayu Dark as the page itself uses it, and an Ayu
# Light counterpart for the light mode the page does not have. Links follow the
# page too, in the accent itself so that they stand out of the text: a faint
# underline at rest that takes the link's own colour on hover, as the stock
# Furo theme does it. Colours live here rather than in custom.css
# because Furo writes them into the document after every stylesheet, so a
# stylesheet cannot override them anyway.
html_theme_options = {
    "light_css_variables": {
        "color-brand-primary": "#A2600D",
        "color-brand-content": "#A2600D",
        "color-brand-visited": "#A2600D",
        "color-background-primary": "#FCFCFC",
        "color-background-secondary": "#F2F3F4",
        "color-background-hover": "#EAECEE",
        "color-background-hover--transparent": "#EAECEE00",
        "color-background-border": "#E7EAED",
        "color-background-item": "#E7EAED",
        "color-foreground-primary": "#4A4F55",
        "color-foreground-secondary": "#5C6166",
        "color-foreground-muted": "#787E85",
        "color-foreground-border": "#D9DDE1",
        # A shade below the accent: the navigation wears it at weight 700,
        # which reads denser than the same colour in running text.
        "color-link": "#8A520B",
        "color-link-underline": "#DFC49D",
        "color-link--hover": "#8A520B",
        "color-link-underline--hover": "#8A520B",
        "color-link--visited": "#8A520B",
        "color-link-underline--visited": "#DFC49D",
        "color-link--visited--hover": "#8A520B",
        "color-link-underline--visited--hover": "#8A520B",
        "color-inline-code-background": "#F2F3F4",
        "color-admonition-background": "#F7F8F9",
        "color-highlight-on-target": "#FBF0D8",
        "color-table-header-background": "#F2F3F4",
        "color-table-border": "#E7EAED",
        "color-sidebar-caption-text": "#787E85",
        "color-sidebar-link-text": "#5C6166",
        "color-sidebar-link-text--top-level": "#4A4F55",
        "color-toc-item-text": "#787E85",
        "color-toc-item-text--hover": "#4A4F55",
        "color-toc-item-text--active": "#A2600D",
    },
    "dark_css_variables": {
        "color-brand-primary": "#E6B450",
        "color-brand-content": "#E6B450",
        "color-brand-visited": "#E6B450",
        "color-background-primary": "#0D1017",
        "color-background-secondary": "#0B0E14",
        "color-background-hover": "#161B26",
        "color-background-hover--transparent": "#161B2600",
        "color-background-border": "#232834",
        "color-background-item": "#232834",
        "color-foreground-primary": "#BFBDB6",
        "color-foreground-secondary": "#A6ACB9",
        # The site writes its text a shade below its headings; custom.css puts
        # the headings back on the brighter foreground.
        "color-content-foreground": "#A6ACB9",
        "color-foreground-muted": "#8A9199",
        "color-foreground-border": "#232834",
        "color-link": "#E6B450",
        "color-link-underline": "#5C4F2F",
        "color-link--hover": "#E6B450",
        "color-link-underline--hover": "#E6B450",
        "color-link--visited": "#E6B450",
        "color-link-underline--visited": "#5C4F2F",
        "color-link--visited--hover": "#E6B450",
        "color-link-underline--visited--hover": "#E6B450",
        "color-inline-code-background": "#161B26",
        "color-admonition-background": "#131721",
        "color-highlight-on-target": "#2A2416",
        "color-table-header-background": "#161B26",
        "color-table-border": "#232834",
        "color-sidebar-background": "#0B0E14",
        "color-sidebar-background-border": "#232834",
        "color-sidebar-brand-text": "#BFBDB6",
        "color-sidebar-caption-text": "#626A78",
        "color-sidebar-link-text": "#A6ACB9",
        "color-sidebar-link-text--top-level": "#BFBDB6",
        "color-sidebar-search-background": "#0D1017",
        "color-sidebar-search-border": "#232834",
        "color-sidebar-search-icon": "#626A78",
        "color-toc-item-text": "#8A9199",
        "color-toc-item-text--hover": "#BFBDB6",
        "color-toc-item-text--active": "#E6B450",
        "color-header-background": "#0B0E14",
        "color-header-border": "#232834",
    },
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
