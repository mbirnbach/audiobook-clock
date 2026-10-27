#!/usr/bin/env python3
"""x04g: point simple-framebuffer at the framebuffer this unit's LK uses."""
import sys

p = sys.argv[1]
s = open(p).read()
old_node = """		framebuffer0: framebuffer@7f1c0000 {
			compatible = "simple-framebuffer";
			reg = <0x0 0x7f1c0000 0x0 (800 * 480 * 4)>;"""
new_node = """		/*
		 * LK (mico_x04g-451d69f-20210619014509-20211214131701) scans out
		 * from the start of its framebuffer mblock at 0x7f2c0000; the
		 * stock kernel reserves 0x7f2c0000-0x7f9c0000 for it.
		 */
		framebuffer0: framebuffer@7f2c0000 {
			compatible = "simple-framebuffer";
			memory-region = <&lk_framebuffer>;
			reg = <0x0 0x7f2c0000 0x0 (800 * 480 * 4)>;"""
old_rsv = """		ramoops@54410000 {"""
new_rsv = """		lk_framebuffer: framebuffer@7f2c0000 {
			reg = <0x00 0x7f2c0000 0x00 0x700000>;
			no-map;
		};

		ramoops@54410000 {"""
assert s.count(old_node) == 1 and s.count(old_rsv) == 1
s = s.replace(old_node, new_node).replace(old_rsv, new_rsv)
open(p, "w").write(s)
