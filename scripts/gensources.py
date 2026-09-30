#!/usr/bin/env python
"""Resolve a Mono class library's .sources list to the files it compiles.

A port of the lookup and expansion rules in vendor/mono/mcs/tools/gensources/gensources.cs, which
Mono's own build runs for every library, so each assembly is built from exactly the file list Mono
would use for the chosen profile.

  python scripts/gensources.py <library dir> <library file name> <host platform> <profile>

e.g. `python scripts/gensources.py vendor/mono/mcs/class/System System.dll win32 unreal`. Prints
one absolute path per line.

The first of these that exists is the target, together with its matching .exclude.sources:
  <platform>_<profile>_<library>.sources
  <profile>_<library>.sources
  <platform>_defaultprofile_<library>.sources
  <library>.sources
A line is a path or a glob relative to the file it appears in, optionally followed by
`:A.cs,B.cs` to exclude files from the glob's directory. `#include other.sources` nests.
"""
import glob
import os
import sys


class SourcesFile:
    def __init__(self, path):
        self.path = path
        self.includes = []
        self.sources = []     # patterns, relative to the file's directory
        self.exclusions = []


def parse_file(path, table):
    path = os.path.normpath(path)
    if path in table:
        return table[path]
    result = SourcesFile(path)
    table[path] = result
    directory = os.path.dirname(path)
    with open(path, encoding='utf-8') as f:
        for line in f:
            if line.startswith('#'):
                if line.startswith('#include '):
                    name = line[len('#include '):].strip()
                    included = os.path.join(directory, name)
                    if not os.path.isfile(included):
                        raise SystemExit('include does not exist: ' + included)
                    result.includes.append(parse_file(included, table))
                continue
            line = line.strip()
            if not line:
                continue
            parts = line.split(':')
            if len(parts) > 1:
                pattern_dir = os.path.dirname(parts[0])
                for name in parts[1].split(','):
                    result.exclusions.append(os.path.join(directory, pattern_dir, name))
            result.sources.append(os.path.join(directory, parts[0]))
    return result


def matches(patterns, strict):
    for pattern in patterns:
        full = os.path.normpath(pattern)
        if '*' in os.path.basename(full) or '?' in os.path.basename(full):
            for f in sorted(glob.glob(full)):
                if os.path.isfile(f):
                    yield f
        elif os.path.isfile(full):
            yield full
        elif strict:
            raise SystemExit('file does not exist: ' + full)


def collect(sources_file, excluded, strict):
    """Mirrors GetMatchesFromFile: the excluded set is shared and grows as files are visited."""
    if sources_file is None:
        return
    for m in matches(sources_file.exclusions, False):
        excluded.add(os.path.normcase(m))
    for include in sources_file.includes:
        yield from collect(include, excluded, strict)
    for m in matches(sources_file.sources, strict):
        if os.path.normcase(m) not in excluded:
            yield m


def resolve(library_dir, library, platform, profile):
    prefixes = [
        f'{platform}_{profile}_{library}',
        f'{profile}_{library}',
        f'{platform}_defaultprofile_{library}',
        library,
    ]
    for prefix in prefixes:
        sources = os.path.join(library_dir, prefix + '.sources')
        if not os.path.isfile(sources):
            continue
        sources_table, exclusions_table = {}, {}
        target = parse_file(sources, sources_table)
        exclusions_path = os.path.join(library_dir, prefix + '.exclude.sources')
        exclusions = parse_file(exclusions_path, exclusions_table) if os.path.isfile(exclusions_path) else None

        excluded = set()
        for m in collect(exclusions, set(), False):
            excluded.add(os.path.normcase(m))
        seen = set()
        for m in collect(target, excluded, True):
            key = os.path.normcase(m)
            if key not in seen:
                seen.add(key)
                yield os.path.abspath(m)
        return
    raise SystemExit(f'no .sources list for {library} in {library_dir}')


def main(argv):
    if len(argv) != 5:
        print(__doc__, file=sys.stderr)
        return 2
    for path in resolve(*argv[1:]):
        print(path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
