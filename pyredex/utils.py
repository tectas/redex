#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.


# pyre-strict


import argparse
import distutils.version
import glob
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import typing
import zipfile
from os.path import basename, dirname, isfile, join

from pyredex.logger import log


IS_WINDOWS: bool = os.name == "nt"
temp_dirs: typing.List[str] = []


def abs_glob(directory: str, pattern: str = "*") -> typing.Generator[str, None, None]:
    """
    Returns all files that match the specified glob inside a directory.
    Returns absolute paths. Does not return files that start with '.'
    """
    for result in glob.glob(join(directory, pattern)):
        yield join(directory, result)


def make_temp_dir(name: str = "", debug: bool = False) -> str:
    """Make a temporary directory which will be automatically deleted"""
    global temp_dirs
    directory = tempfile.mkdtemp(name)
    if not debug:
        temp_dirs.append(directory)
    return directory


def remove_temp_dirs() -> None:
    global temp_dirs
    for directory in temp_dirs:
        shutil.rmtree(directory)


def with_temp_cleanup(
    fn: typing.Callable[[], None], always_clean: bool = False
) -> None:
    success = always_clean
    try:
        fn()
        success = True
    finally:
        if success:
            remove_temp_dirs()


def _find_biggest_build_tools_version(base: str) -> typing.Optional[str]:
    VERSION_REGEXP = r"\d+\.\d+\.\d+$"
    build_tools = join(base, "build-tools")
    version = max(
        (
            "0.0.1",
            *[d for d in os.listdir(build_tools) if re.match(VERSION_REGEXP, d)],
        ),
        key=distutils.version.StrictVersion,
    )
    if version == "0.0.1":
        logging.debug(
            "No version found in %s: %s", build_tools, os.listdir(build_tools)
        )
        return None
    logging.debug("max build tools version: %s", version)
    return join(build_tools, version)


def _filter_none_not_exists_ret_none(
    input: typing.Optional[typing.List[typing.Optional[str]]],
) -> typing.Optional[typing.List[str]]:
    if input is None:
        logging.debug("Filtering an input None to None")
        return None
    filtered = [p for p in input if p and os.path.exists(p)]
    ret_val = filtered if filtered else None
    logging.debug("Filtered %s to %s = %s", input, filtered, ret_val)
    return ret_val


def find_android_path_by_env() -> typing.Optional[typing.List[str]]:
    env_values: typing.List[typing.Optional[str]] = [
        os.environ[key] for key in ["ANDROID_SDK", "ANDROID_HOME"] if key in os.environ
    ]
    logging.debug("Android ENV values = %s", env_values)
    return _filter_none_not_exists_ret_none(env_values)


def find_android_build_tools_by_env() -> typing.Optional[typing.List[str]]:
    base = find_android_path_by_env()
    logging.debug("Android Build Tools base by env = %s", base)
    if not base:
        return None

    return _filter_none_not_exists_ret_none(
        [_find_biggest_build_tools_version(p) for p in base]
    )


# If the script isn't run in a directory that buck recognizes, set this
# to a root dir.
root_dir_for_buck: typing.Optional[str] = None


def _load_android_buckconfig_values() -> typing.Dict[str, typing.Any]:
    cmd = ["buck", "audit", "config", "android", "--json"]
    global root_dir_for_buck
    rdfb = root_dir_for_buck
    cwd = rdfb if rdfb is not None else os.getcwd()
    # Set NO_BUCKD to minimize disruption to any currently running buckd
    env = dict(os.environ)
    env["NO_BUCKD"] = "1"
    raw = subprocess.check_output(cmd, cwd=cwd, stderr=subprocess.DEVNULL, env=env)
    return json.loads(raw)


def find_android_path_by_buck() -> typing.Optional[typing.List[str]]:
    logging.debug("Computing SDK path from buck")
    try:
        buckconfig = _load_android_buckconfig_values()
    except BaseException as e:
        logging.debug("Failed loading buckconfig: %s", e)
        return None
    if "android.sdk_path" not in buckconfig:
        logging.debug("android.sdk_path is not in buckconfig")
        return None
    logging.debug(
        "buckconfig android.sdk_path = %s", buckconfig.get("android.sdk_path")
    )
    return _filter_none_not_exists_ret_none([buckconfig.get("android.sdk_path")])


def find_android_build_tools_by_buck() -> typing.Optional[typing.List[str]]:
    logging.debug("Computing SDK path from buck")
    try:
        buckconfig = _load_android_buckconfig_values()
    except BaseException as e:
        logging.debug("Failed loading buckconfig: %s", e)
        return None
    if "android.sdk_path" not in buckconfig:
        logging.debug("android.sdk_path is not in buckconfig")
        return None
    sdk_path = buckconfig.get("android.sdk_path")
    logging.debug("buckconfig android.sdk_path = %s", sdk_path)

    if "android.build_tools_version" in buckconfig:
        version = buckconfig["android.build_tools_version"]
        logging.debug("buckconfig android.build_tools_version is %s", version)
        assert isinstance(sdk_path, str)
        return _filter_none_not_exists_ret_none(
            [join(sdk_path, "build-tools", version)]
        )
    else:
        logging.debug("No android.build_tools_version in buck-config")
        return _filter_none_not_exists_ret_none(
            [_find_biggest_build_tools_version(sdk_path)]
        )


# This order is not necessarily equivalent to buck's. We prefer environment
# variables as they are a lot cheaper.
_sdk_search_order: typing.List[
    typing.Tuple[
        str,
        typing.Callable[[], typing.Optional[typing.List[str]]],
        typing.Callable[[], typing.Optional[typing.List[str]]],
    ]
] = [
    ("Env", find_android_path_by_env, find_android_build_tools_by_env),
    ("Buck", find_android_path_by_buck, find_android_build_tools_by_buck),
]


def add_android_sdk_path(path: str) -> None:
    global _sdk_search_order
    _sdk_search_order.insert(
        0,
        (
            f"Path:{path}",
            lambda: _filter_none_not_exists_ret_none(
                [
                    # For backwards compatibility
                    *[
                        dirname(dirname(p))
                        for p in [path]
                        if basename(dirname(p)) == "build-tools"
                    ],
                    path,
                ]
            ),
            lambda: _filter_none_not_exists_ret_none(
                [
                    _find_biggest_build_tools_version(path),
                    path,  # For backwards compatibility.
                ]
            ),
        ),
    )


def get_android_sdk_path() -> str:
    attempts = []
    global _sdk_search_order
    for name, base_dir_fn, _ in _sdk_search_order:
        logging.debug("Attempting %s to find SDK path", name)
        candidate = base_dir_fn()
        if candidate:
            return candidate[0]
        attempts.append(name)

    raise RuntimeError(f'Could not find SDK path, searched {", ".join(attempts)}')


def find_android_build_tool(tool: str) -> str:
    def run() -> typing.Tuple[typing.Optional[str], typing.List[str]]:
        attempts: typing.List[str] = []

        def try_find(
            name: str,
            base_dir_fn: typing.Callable[[], typing.Optional[typing.List[str]]],
        ) -> typing.Optional[str]:
            try:
                if base_dir_fn is None:
                    return None
                base_dirs = base_dir_fn()
                if not base_dirs:
                    attempts.append(name + ":<Nothing>")
                    return None
                for base_dir in base_dirs:
                    candidate = join(base_dir, tool)
                    if os.path.exists(candidate):
                        return candidate
                    attempts.append(name + ":" + base_dir)
            except BaseException:
                pass
            return None

        global _sdk_search_order
        for name, _, base_tools_fn in _sdk_search_order:
            logging.debug("Attempting %s to find %s", name, tool)
            candidate = try_find(name, base_tools_fn)
            if candidate:
                return candidate, attempts

        # By `PATH`.
        logging.debug("Attempting PATH to find %s", tool)
        tool_path = shutil.which(tool)
        if tool_path is not None:
            return tool_path, attempts
        attempts.append("PATH")

        return None, attempts

    result, _ = run()
    if result:
        return result

    # Try again with more debug messages (the under the hood call to buck audit
    # seems to not always return a result)
    old_level = logging.getLogger().getEffectiveLevel()
    logging.getLogger().setLevel(logging.DEBUG)
    result, attempts = run()
    if result:
        logging.getLogger().setLevel(old_level)
        return result

    raise RuntimeError(f'Could not find {tool}, searched {", ".join(attempts)}')


def find_apksigner() -> str:
    return find_android_build_tool("apksigner.bat" if IS_WINDOWS else "apksigner")


def remove_signature_files(extracted_apk_dir: str) -> None:
    for f in abs_glob(extracted_apk_dir, "META-INF/*"):
        cert_path = join(extracted_apk_dir, f)
        if isfile(cert_path):
            os.remove(cert_path)


def sign_apk(keystore: str, keypass: str, keyalias: str, apk: str) -> None:
    subprocess.check_call(
        [
            find_apksigner(),
            "sign",
            "--v1-signing-enabled",
            "--v2-signing-enabled",
            "--ks",
            keystore,
            "--ks-pass",
            "pass:" + keypass,
            "--ks-key-alias",
            keyalias,
            apk,
        ],
        stdout=sys.stderr,
    )


def remove_comments_from_line(line: str) -> str:
    (found_backslash, in_quote) = (False, False)
    for idx, c in enumerate(line):
        if c == "\\" and not found_backslash:
            found_backslash = True
        elif c == '"' and not found_backslash:
            found_backslash = False
            in_quote = not in_quote
        elif c == "#" and not in_quote:
            return line[:idx]
        else:
            found_backslash = False
    return line


def remove_comments(lines: typing.Iterable[str]) -> str:
    return "".join([remove_comments_from_line(line) + "\n" for line in lines])


def argparse_yes_no_flag(
    parser: argparse.ArgumentParser,
    flag_name: str,
    on_prefix: str = "",
    off_prefix: str = "no-",
    **kwargs: typing.Any,
) -> None:
    class FlagAction(argparse.Action):
        def __init__(self, option_strings, dest, nargs=None, **kwargs):
            super(FlagAction, self).__init__(option_strings, dest, nargs=0, **kwargs)

        def __call__(self, parser, namespace, values, option_string=None):
            setattr(
                namespace,
                self.dest,
                False if option_string.startswith(f"--{off_prefix}") else True,
            )

    parser.add_argument(
        f"--{on_prefix}{flag_name}",
        f"--{off_prefix}{flag_name}",
        dest=flag_name,
        action=FlagAction,
        default=False,
        **kwargs,
    )


def unzip_apk(apk: str, destination_directory: str) -> None:
    with zipfile.ZipFile(apk) as z:
        z.extractall(destination_directory)


def extract_dex_number(dexfilename: str) -> int:
    m = re.search(r"(classes|.*-)(\d+)", basename(dexfilename))
    if m is None:
        raise Exception("Bad secondary dex name: " + dexfilename)
    return int(m.group(2))


def dex_glob(directory: str) -> typing.List[str]:
    """
    Return the dexes in a given directory, with the primary dex first.
    """
    primary = join(directory, "classes.dex")
    if not isfile(primary):
        raise Exception("No primary dex found")

    secondaries = [
        d for d in glob.glob(join(directory, "*.dex")) if not d.endswith("classes.dex")
    ]
    secondaries.sort(key=extract_dex_number)

    return [primary] + secondaries


def move_dexen_to_directories(
    root: str, dexpaths: typing.Iterable[str]
) -> typing.List[str]:
    """
    Move each dex file to its own directory within root and return a list of the
    new paths. Redex will operate on each dex and put the modified dex into the
    same directory.
    """
    res = []
    for idx, dexpath in enumerate(dexpaths):
        dexname = basename(dexpath)
        dirpath = join(root, "dex" + str(idx))
        os.mkdir(dirpath)
        shutil.move(dexpath, dirpath)
        res.append(join(dirpath, dexname))

    return res


def ensure_libs_dir(libs_dir: str, sub_dir: str) -> str:
    """Ensures the base libs directory and the sub directory exist. Returns top
    most dir that was created.
    """
    if os.path.exists(libs_dir):
        os.mkdir(sub_dir)
        return sub_dir
    else:
        os.mkdir(libs_dir)
        os.mkdir(sub_dir)
        return libs_dir


def get_file_ext(file_name: str) -> str:
    return os.path.splitext(file_name)[1]
