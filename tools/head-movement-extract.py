"""Compile the production facing handler against host-owned test fixtures."""
from pathlib import Path
import sys
source=Path(sys.argv[1]).read_text()
start=source.index("static std::atomic<bool> g_headMovement")
end=source.index("// The stub: save flags",start)
Path(sys.argv[2]).write_text(source[start:end])
