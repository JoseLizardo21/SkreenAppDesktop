---
name: bump-version
description: Bump the SkreenApp Desktop version for both the .deb and .rpm packages at once, and rebuild the committed .deb. Use when the user asks to bump, update or release a new version (e.g. "/bump-version 0.1.2").
---

# Bump version (deb + rpm)

Argument: the new version `X.Y.Z`. If none is given, read the current one from
`CMakeLists.txt` and ask whether to bump patch, minor or major.

## 1. Update every version reference

Replace the old version `OLD` with the new one `NEW` in:

| File | What to change |
|------|----------------|
| `CMakeLists.txt` | `project(SkreenDesktop VERSION NEW)` |
| `packaging/build-deb.sh` | `VERSION="NEW"` |
| `packaging/build-rpm.sh` | `VERSION="NEW"` |
| `packaging/skreenapp.spec` | `Version:        NEW` and `Release: 1%{?dist}` |
| `README.md` | `skreenapp-NEW-1.*.rpm` and `skreenapp_NEW_amd64.deb` |

Add a new entry at the top of `%changelog` in `packaging/skreenapp.spec`
(date in `date '+%a %b %d %Y'` format, English locale):

```
* Thu Oct 15 2026 Lizardo <jose.212002lizardo@gmail.com> - NEW-1
- Release NEW

```

Then check nothing was missed (ignore old `%changelog` entries and the CMake
comment example):

```bash
grep -rn "OLD" --exclude-dir={.git,build,output} .
```

## 2. Rebuild the .deb

The built `.deb` is committed in `packaging/`, so it must be regenerated.
`build-deb.sh` starts with `sudo apt install`, which fails on Fedora. Run a
copy without that step (in the scratchpad), with `PROJ_DIR` pointing at the repo:

```bash
sed -e '/^sudo apt install/,/^    git$/d' \
    -e "s|^PROJ_DIR=.*|PROJ_DIR=\"$PWD\"|" \
    packaging/build-deb.sh > "$SCRATCH/build-deb.sh"
bash "$SCRATCH/build-deb.sh"
dpkg-deb -f packaging/skreenapp_NEW_amd64.deb Version   # must print NEW
git rm -q packaging/skreenapp_OLD_amd64.deb
git add packaging/skreenapp_NEW_amd64.deb
```

The driver sources come from `../skreen_drive` (its `VERSION` file is
independent of the app version; don't change it unless asked).

Warn the user that a `.deb` built on Fedora links against Fedora's libraries
and may not run on older Ubuntu/Debian; building on Ubuntu is safer.

The `.rpm` is not committed. Only build it (`packaging/build-rpm.sh`) if the
user asks.

## 3. Commit (only if the user asked)

One commit, single-line message, no co-author: `Bump version to NEW`.
