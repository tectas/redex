# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

from __future__ import absolute_import, division, print_function, unicode_literals

import re


class DexdumpSymbolicator(object):

    CLASS_REGEX = re.compile(r"L(?P<class>[A-Za-z][0-9A-Za-z_$]*\/[0-9A-Za-z_$\/]+);")

    LINE_REGEX = re.compile(r"(?P<prefix>0x[0-9a-f]+ line=)(?P<lineno>\d+)")

    # METHOD_CLS_HDR_REGEX captures both method and field headers in a class
    METHOD_CLS_HDR_REGEX = re.compile(
        r"#\d+\s+:\s+\(in L(?P<class>[A-Za-z][0-9A-Za-z]*\/[0-9A-Za-z_$\/]+);\)"
    )
    # METHOD_REGEX captures both method and field names in a class
    METHOD_REGEX = re.compile(r"name\s+:\s+\'(?P<method>[<A-Za-z][>A-Za-z0-9_$]*)\'")

    CLS_CHUNK_HDR_REGEX = re.compile(r"  [A-Z]")
    CLS_HDR_REGEX = re.compile(r"Class #")

    def __init__(self, symbol_maps, all_line_info=False):
        self.symbol_maps = symbol_maps
        self.reading_methods = False
        self.current_class = None
        self.current_class_name = None
        self.current_method = None
        self.last_lineno = None
        self.prev_line = None
        self.all_line_info = all_line_info

    def class_replacer(self, matchobj):
        m = matchobj.group("class")
        cls = m.replace("/", ".")
        if cls in self.symbol_maps.class_map:
            return "L%s;" % self.symbol_maps.class_map[cls].origin_class.replace(
                ".", "/"
            )
        return "L%s;" % m

    def decode_positions_at(self, idx):
        positions = self.symbol_maps.line_map.get_stack(idx)
        results = []
        for pos in positions:
            if pos.method == "redex.$Position.pattern":
                pattern_id = pos.line
                results.append("pattern %d" % pattern_id)
            elif pos.method == "redex.$Position.switch":
                start = pos.line
                count_positions = self.symbol_maps.line_map.get_stack(start)
                assert len(count_positions) == 1
                assert count_positions[0].method == "redex.$Position.count"
                count = count_positions[0].line
                switch_results = []
                for i in range(count):
                    switch_results.append(
                        "{%s}" % (", ".join(self.decode_positions_at(start + 1 + i)))
                    )
                results.append("switch {%s}" % (", ".join(switch_results)))
            elif pos.method == "redex.$Position.case":
                pattern_id = pos.line
                results.append("case(pattern %d)" % pattern_id)
            else:
                results.append("%s:%d" % (pos.file, pos.line))
        return results

    def line_replacer(self, matchobj):
        lineno = int(matchobj.group("lineno"))
        decoded_positions = self.decode_positions_at(lineno - 1)
        if self.all_line_info:
            line_info = str(lineno) + " (" + ", ".join(decoded_positions) + ")"
        else:
            line_info = ", ".join(decoded_positions)

        return matchobj.group("prefix") + line_info

    def method_replacer(self, matchobj):
        m = matchobj.group(0)
        if self.current_class is not None:
            cls = self.current_class.replace("/", ".")
            if cls in self.symbol_maps.class_map:
                left, _sep, right = m.partition(": ")
                if self.reading_methods:
                    method_name = matchobj.group("method")
                    if method_name in self.symbol_maps.class_map[cls].method_mapping:
                        return (
                            left
                            + _sep
                            + self.symbol_maps.class_map[cls].method_mapping[
                                method_name
                            ]
                        )
                else:
                    field_name = matchobj.group("method")
                    if field_name in self.symbol_maps.class_map[cls].field_mapping:
                        return (
                            left
                            + _sep
                            + self.symbol_maps.class_map[cls].field_mapping[field_name]
                        )
        return m

    def reset_state(self):
        self.current_class = None
        self.current_class_name = None
        self.current_method = None
        self.last_lineno = None
        self.prev_line = None

    def symbolicate(self, line):
        symbolicated_line = self._symbolicate(line)
        self.prev_line = line
        return symbolicated_line

    def _symbolicate(self, line):
        if self.symbol_maps.iodi_metadata:
            match = self.METHOD_CLS_HDR_REGEX.search(line)
            if match:
                self.current_class = match.group("class")
            elif self.current_class:
                match = self.METHOD_REGEX.search(line)
                if match:
                    self.current_method = match.group("method")
                    self.current_class_name = self.current_class.replace("/", ".")
                    line = self.METHOD_REGEX.sub(self.method_replacer, line)
                elif self.current_method:
                    match = self.LINE_REGEX.search(line)
                    if match:
                        lineno = match.group("lineno")
                        mapped_line, _ = self.symbol_maps.iodi_metadata.map_iodi(
                            self.symbol_maps.debug_line_map,
                            self.current_class_name,
                            self.current_method,
                            lineno,
                        )
                        if mapped_line:
                            if self.last_lineno and not self.all_line_info:
                                if self.last_lineno == mapped_line:
                                    # Don't emit duplicate line entries
                                    return None
                            self.last_lineno = mapped_line
                            decoded_positions = self.decode_positions_at(
                                mapped_line - 1
                            )

                            if self.all_line_info:
                                line_info = (
                                    lineno + " (" + ", ".join(decoded_positions) + ")"
                                )
                            else:
                                line_info = ", ".join(decoded_positions)

                            return "        " + match.group("prefix") + line_info + "\n"
                    no_debug_info = (
                        "positions     :" in self.prev_line
                        and "locals        :" in line
                    )
                    if no_debug_info:
                        mappings = self.symbol_maps.iodi_metadata.map_iodi_no_debug_to_mappings(
                            self.symbol_maps.debug_line_map,
                            self.current_class_name,
                            self.current_method,
                        )
                        if mappings is None:
                            return line
                        result = ""
                        for i, (pc, mapped_line) in enumerate(mappings):
                            decoded_positions = self.decode_positions_at(
                                mapped_line - 1
                            )
                            if i == 0:
                                pc = 0
                            result += (
                                "        "
                                + "{0:#0{1}x}".format(pc, 6)
                                + " line="
                                + ", ".join(decoded_positions)
                                + "\n"
                            )
                        return result + line

            if self.CLS_CHUNK_HDR_REGEX.match(line):
                # If we match a header but its the wrong header then ignore the
                # contents of this subsection until we hit a methods subsection
                self.reading_methods = (
                    "Direct methods" in line or "Virtual methods" in line
                )
                if not self.reading_methods:
                    self.reset_state()
            elif self.CLS_HDR_REGEX.match(line):
                self.reading_methods = False
                self.reset_state()

        line = self.CLASS_REGEX.sub(self.class_replacer, line)
        line = self.LINE_REGEX.sub(self.line_replacer, line)
        return line

    @staticmethod
    def is_likely_dexdump(line):
        return re.match(r"^Processing '.*\.dex'", line) or re.search(
            r"Class #\d+", line
        )
