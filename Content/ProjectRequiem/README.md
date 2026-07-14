# Project Requiem Content

This directory is the ownership boundary for all Project Requiem production content.

- `Core` contains shared framework composition, input and foundational data.
- Feature folders own their Blueprints, data and presentation assets.
- `Tests` contains reusable validation fixtures.
- `Prototypes` contains disposable experiments and must never be a dependency of `Core` or production content.
- Epic template content remains outside this directory and is reference-only.
- `World/Maps/Intro` is reserved for the four planned MVP maps; none is implemented in this foundation pass.

The empty folders are intentional planning boundaries. They do not imply that their systems are implemented.
