#!/usr/bin/env python3
"""Convert gen2 wlan cfg80211 key/station ops from net_device to wireless_dev.

For each listed function, the parameter `struct net_device *<name>` that
follows `struct wiphy *wiphy` becomes `struct wireless_dev *wdev`; in the
definition the old name is re-derived from wdev->netdev so the body is
unchanged.
"""
import re
import sys
from pathlib import Path

FUNCS = [
    "mtk_cfg80211_add_key", "mtk_cfg80211_get_key", "mtk_cfg80211_del_key",
    "mtk_cfg80211_set_default_mgmt_key", "mtk_cfg80211_get_station",
    "mtk_cfg80211_change_station", "mtk_cfg80211_add_station",
    "mtk_cfg80211_del_station",
    "mtk_p2p_cfg80211_del_station", "mtk_p2p_cfg80211_get_station",
    "mtk_p2p_cfg80211_add_key", "mtk_p2p_cfg80211_get_key",
    "mtk_p2p_cfg80211_del_key",
]

root = Path(sys.argv[1])
total = 0
for path in sorted(list(root.rglob("*.c")) + list(root.rglob("*.h"))):
    src = path.read_text(errors="surrogateescape")
    orig = src
    for fn in FUNCS:
        pat = re.compile(
            r"(\b" + fn + r"\s*\(\s*struct\s+wiphy\s*\*\s*\w+\s*,\s*)"
            r"struct\s+net_device\s*\*\s*(\w+)"
            r"([^;{]*\))(\s*)([;{])", re.S)

        def repl(m):
            head, name, rest, ws, end = m.groups()
            out = head + "struct wireless_dev *wdev" + rest + ws + end
            if end == "{":
                out += "\n\tstruct net_device *%s = wdev->netdev;\n" % name
            return out

        src, n = pat.subn(repl, src)
        total += n
    # cfg80211_new_sta()/cfg80211_del_sta() take the wireless_dev now
    src, n = re.subn(r"\bcfg80211_(new|del)_sta\(\s*([^,]+?prDevHandler)\s*,",
                     r"cfg80211_\1_sta(\2->ieee80211_ptr,", src)
    total += n
    if src != orig:
        path.write_text(src, errors="surrogateescape")
        print("patched", path.relative_to(root))
print("replacements:", total)
