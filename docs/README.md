# Adding documentation

How the docs site works and how to add to it. The site has three parts: this
landing page, the embedded docs (Doxygen), and the python docs (Sphinx).

## Where do I put my document?

| What you are writing | Where it goes |
|---|---|
| A guide (setup, how-to, design notes) | A new `.md` file in this `docs/` folder |
| Notes about a piece of code | A `README.md` next to that code, anywhere under `embedded/` |
| Python package usage or development | A new `.md` file in `python/docs/` |
| API details for a C/C++ function | Doxygen comments in the header (`@brief`, `@param`) |
| API details for a python function | A Google-style docstring in the code |

You do not register anything for C/C++ comments, docstrings, or READMEs under
`embedded/` — they are picked up automatically on the next build.

## Two extra steps that are easy to forget

- **A new guide in `docs/`**: add a `\subpage` line for it in
  `embedded/doxygen/mainpage.md` so it appears in the documentation map
  instead of floating loose, and consider a card in `docs/index.html` if it
  deserves a spot on the landing page.
- **A new python docs page**: you *must* add it to the toctree in
  `python/docs/index.md`. CI builds the python docs with warnings treated as
  errors, and an unlisted page fails the build.

## What to take care of

- **Check it builds before pushing.** Run `./docs.sh` from the repository root
  and open `docs/build/index.html` in a browser. You need `doxygen` and the
  python docs dependencies (`pip install -e '.[docs]'` from `python/`).
- **Link between pages with normal markdown links** to the `.md` file
  (e.g. `[Getting started](getting_started.md)`), not with URLs of the built
  site — the file names change when the site is generated.
- **Avoid `@ref` inside markdown tables**, and avoid `@ref` to pages whose
  file name contains an underscore — both render as plain text instead of a
  link. A plain markdown link always works.
- **Do not document third-party code.** `libtock-c`, `external`, `nanopb`,
  `unity`, and `extras` are excluded on purpose; the site documents only this
  repository's own code and text.
- **Nothing is published by hand.** Every pull request builds the whole site
  in CI, and merging to `main` deploys it. If CI is green, the site is fine.
- Add a line to `CHANGELOG.md` under *Unreleased* for notable additions.

## Layout of the built site

```
/                    docs/index.html (landing page, edited by hand)
/embedded/           Doxygen: firmware API, every README, and the guides
                     from this docs/ folder
/python/             Sphinx: ents package usage, development, API
```
