#!/usr/bin/env python3

import sys
import argparse
from typing import Optional, Dict

import mistune
import CppHeaderParser
import semver
import git


parser = argparse.ArgumentParser(description='''Check that the versioning of the repo has been correctly changed.
Intended to run in a PR CI''')
parser.add_argument('branchspec', default='origin/develop', nargs='?',
                    help='Git branch specification for previous file versions (default: %(default)s)')


def die(*args, error_code=1, **kwargs):
    print('ERROR: ', end='', file=sys.stderr)
    print(*args, **kwargs, file=sys.stderr)
    exit(error_code)


class SemVerWithVPrefix(semver.VersionInfo):
    @classmethod
    def parse(cls, version):
        if version[0] != 'V':
            raise ValueError(f"{version}: not a valid semantic version tag. Must start with 'v' or 'V'")
        self = super(SemVerWithVPrefix, cls).parse(version[1:])
        return self

    def __str__(self):
        return 'V' + super(SemVerWithVPrefix, self).__str__()


def get_header_defines(header_contents: str) -> Dict[str, str]:
    defines_detail = CppHeaderParser.CppHeader(header_contents, argType='string').defines_detail
    return dict(d['value'].split(' ', maxsplit=1) for d in defines_detail)


def find_child_text(node: dict) -> Optional[str]:
    if 'children' in node.keys():
        if node['children'] and isinstance(node['children'], list):
            return find_child_text(node['children'][0])
    elif 'text' in node.keys():
        return node['text']
    return None


def get_header_version(version_header_contents: str) -> semver.VersionInfo:
    version_defines = get_header_defines(version_header_contents)
    try:
        version_str = next(v.strip('"') for k, v in version_defines.items() if 'version' in k.lower())
    except StopIteration:
        die(f'Could not find version define in\n{version_header_contents}: {version_defines}')
    try:
        version = SemVerWithVPrefix.parse(version_str)
    except ValueError as e:
        die(f'SemVer error: {e}')

    print(f'Version is {version}')
    return version


def get_changelog_version(changelog_markdown_path: str) -> str:
    markdown = mistune.create_markdown(renderer=mistune.AstRenderer())
    with open(changelog_markdown_path, 'r') as f:
        changelog_lines = f.read()
    changelog_ast = markdown(changelog_lines)

    first_heading_has_list = (changelog_ast[1]['type'] == 'list')
    if not first_heading_has_list:
        die(f'Latest changelog entry does not include a _list_ of what has changed')

    first_heading_str = find_child_text(changelog_ast[0])
    if not first_heading_str:
        die(f'Could not find the latest changelog heading: {changelog_ast[0]}')
    print(f'Latest changelog heading: {first_heading_str}')

    return first_heading_str


def get_previous_header_version(version_header_path: str, refspec: str) -> semver.VersionInfo:
    repo = git.Repo('.')
    previous_header_contents = repo.git.show(f'{refspec}:{version_header_path}')
    print(f'Checking previous header version from {refspec}:{version_header_path}')
    previous_version = get_header_version(previous_header_contents)
    print(f'Read previous header version: {previous_version}')
    return previous_version


def main():
    version_header_path = './Version.h'
    # Get the current version from the header
    print(f'Checking current header version from {version_header_path}')
    with open(version_header_path, 'r') as fp:
        version_header_contents = fp.read()
        header_version = get_header_version(version_header_contents)

    # Get the previous version (using git)
    previous_header_version = get_previous_header_version(version_header_path, args.branchspec)

    # Check that the version has been increased
    if header_version <= previous_header_version:
        die(f'Header version must be greater than the previous version {header_version} <= {previous_header_version}')

    changelog_markdown_path = './Changelog.md'
    # Get a string from the changelog, should have a version in the latest entry
    changelog_version = get_changelog_version(changelog_markdown_path)

    # Check that the header and the changelog match
    if str(header_version).lower() not in changelog_version.lower():
        die(f'Header version ("{header_version}" in {version_header_path}) does \
not match changelog version ("{changelog_version}" in {changelog_markdown_path})')


if __name__ == '__main__':
    args = parser.parse_args()
    main()
    print('Finished version check!')
