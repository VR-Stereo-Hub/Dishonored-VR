// core/util/embedded_assets.h - the data files the mod needs, carried inside d3d9.dll.
//
// Full-arm IK and the Heart's backing need prepared data (assets/vr/ in the repo: the arm rig, the Heart's
// rig, back geometry and material). They are compiled into the proxy as RCDATA (src/CMakeLists.txt) and
// written into the data directory on start, so a player never prepares or copies anything. A file already
// there that this mod wrote is updated when the embedded copy changes; a file the player replaced by hand
// (one this mod did not write) is kept and the log says so.
#pragma once

namespace dvr::assets {
// Call once, after [Paths] DataDir is applied and before anything loads these files. Logs every file.
void install();
}
