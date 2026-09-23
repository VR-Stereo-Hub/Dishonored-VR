# F10 painted theme assets (VR-206)

Three original generated PNG masters supply the whole skin. They are decorative artwork,
not game assets or screenshots. Text, icons, values, interaction and focus stay in ImGui.

| File | Uses |
| --- | --- |
| backdrop.png | Square atmosphere, skyline, edge distress and corner ornament |
| parchment.png | Section strips, selected tabs/tier buttons and tooltip notes |
| metal.png | Button faces, slider wells and checkbox plates; tinted for primary actions |

Created with the built-in image generation tool. The supplied menu image was a visual
reference, not an image baked into the interactive UI. Final prompt set:

1. **Backdrop:** Square production UI background. Almost-black blue-green charcoal painted
   canvas, fine scratches and antique brass worn border. Misty Victorian industrial gothic
   skyline and bridge concentrated upper right, quiet upper left for live title. Central
   area calm and dark for readable controls; faint original occult compass in lower right.
   Flat orthographic full bleed. No text, letters, buttons, controls or baked panels.
2. **Parchment:** Landscape full-bleed flat aged ivory parchment. Fine cloudy grey-tan fibers,
   very faint rubbed damask, restrained scuffs and edge oxidation. Quiet pale bone center
   for dark labels. Even illumination. No writing, icons, objects, folds, frame or divisions.
   Reusable material sampled for strips, tabs and note panels.
3. **Metal:** Landscape full-bleed dark blue-grey oxidized steel/slate, fine brushed mottling,
   rubbed paint and hairline scratches, low contrast near charcoal teal. Flat and evenly lit,
   no bright highlights, text, bevels, objects, panels, symbols or border. Reusable button
   faces and slider wells, tinted oxblood for primary actions.

PNG files are embedded as RCDATA in ovl_assets.rc, decoded once with WIC, and uploaded to
immutable D3D11 textures. No loose game-directory install is needed. Draw-time sampling
reuses each master at different sizes. Missing artwork falls back to plain theme materials.
Do not replace native hitboxes with baked artwork.
