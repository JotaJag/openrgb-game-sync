"""Generate no-op SDK interfaces for the simulated hardware integration test."""
from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
sdk = root.parent / "openrgb-sequence-fix/OpenRGB"
output = ['#pragma once', '#include "OpenRGBPluginInterface.h"']
for file, name, derived in [
    (sdk / "OpenRGBPluginInterface.h", "OpenRGBPluginAPIInterface", "NullAPI"),
    (sdk / "RGBController/RGBControllerInterface.h", "RGBControllerInterface", "NullController"),
]:
    text = file.read_text().split("class " + name + "\n{", 1)[1].split("\n};", 1)[0]
    output.append("class " + derived + " : public " + name + " { public:")
    for line in text.splitlines():
        match = re.match(r"\s*virtual\s+(.+?)\s*=\s*0;", line)
        if match:
            signature = match.group(1).strip()
            body = "{}" if signature.startswith("void ") else "{ return {}; }"
            output.append(signature + " override " + body)
    output.append("};")
(root / "build/NullInterfaces.h").write_text("\n".join(output))
