# F10 painted theme assets (VR-206)

Five original generated PNG masters supply the whole skin. They are decorative artwork,
not game assets or screenshots. Text, icons, values, interaction and focus stay in ImGui.

| File | Uses |
| --- | --- |
| backdrop.png | Square atmosphere, skyline, edge distress and corner ornament |
| parchment.png | Selected tabs and tier buttons |
| metal.png | Very faint grain over slate button/slider gradients |
| header.png | Torn section strip, drawn with fixed-width end caps |
| note.png | Folded-corner paper hover notes |

Created with the built-in image generation tool. The supplied menu image was a visual
reference, not an image baked into the interactive UI. Final prompt set:

1. **Backdrop, revised:** Use the restrained supplied reference. Generate the background
   alone, removing all labels and UI. Almost-black charcoal with muted grey-teal, ghostly
   Victorian skyline confined to the top, quiet dark center, sparse worn brass perimeter.
   Faint grey angular broken-circle emblem in the bottom right. No gold compass, bright
   blue, lit windows, text or controls. Painterly, matte, understated rather than noisy.
2. **Parchment:** Landscape full-bleed flat aged ivory parchment. Fine cloudy grey-tan fibers,
   very faint rubbed damask, restrained scuffs and edge oxidation. Quiet pale bone center
   for dark labels. Even illumination. No writing, icons, objects, folds, frame or divisions.
   Reusable material sampled for strips, tabs and note panels.
3. **Metal:** Landscape full-bleed dark blue-grey oxidized steel/slate, fine brushed mottling,
   rubbed paint and hairline scratches, low contrast near charcoal teal. Flat and evenly lit,
   no bright highlights, text, bevels, objects, panels, symbols or border. Reusable button
   faces and slider wells, tinted oxblood for primary actions.

4. **Header:** Single wide blank ivory-grey parchment strip on alpha transparency. Frayed
   edges and shallow nicks, subtle aged floral marbling, left pale bone fading towards
   smoky grey at right. No writing, icons, border, lines, shadow, curl or perspective.
5. **Note:** Flat blank grey-beige paper on transparency, subtle torn worn edges and one
   small folded upper-right corner. Fine restrained fibers and edge stains, clean center.
   No text, symbols, border, writing, large curls or perspective.

Generated PNGs are copied unchanged. Header/note UV bands trim transparent margins at
draw time; header end caps preserve the edge proportions when the strip grows wider.
Metal grain is 7% opacity over native gradients. All letters and ornament geometry are
drawn by ImGui. Fonts come from Windows, with fallback paths, and are not redistributed.

PNG files are embedded as RCDATA in ovl_assets.rc, decoded once with WIC, and uploaded to
immutable D3D11 textures. No loose game-directory install is needed. Draw-time sampling
reuses each master at different sizes. Missing artwork falls back to plain theme materials.
Do not replace native hitboxes with baked artwork.
