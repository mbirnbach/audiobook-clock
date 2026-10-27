#!/usr/bin/env python3
"""Add a ramoops node at the stock pstore location to the x04g DTS."""
import sys

p = sys.argv[1]
s = open(p).read()
old = """		atf-reserved-memory {
			reg = <0x00 0x54600000 0x00 0x30000>;
			no-map;
		};
"""
new = old + """
		/*
		 * Same location and layout as the stock kernel ramoops, so a
		 * console log left by this kernel can be read back from the
		 * stock (LineageOS) kernel after a watchdog reset.
		 */
		ramoops@54410000 {
			compatible = "ramoops";
			reg = <0x00 0x54410000 0x00 0xe0000>;
			record-size = <0x1000>;
			console-size = <0x40000>;
			ftrace-size = <0x1000>;
			pmsg-size = <0x10000>;
		};
"""
if "ramoops@54410000" in s:
    sys.exit("already present")
assert s.count(old) == 1
open(p, "w").write(s.replace(old, new))
