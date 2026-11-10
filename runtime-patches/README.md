# runtime-patches

This game's changes to the submodules, applied by `tools/apply_runtime_patches.sh` on top of N64Recomp-browser's
patches, in order of name. A new change is a new file per submodule, numbered after that submodule's highest, with a
line here.

```
N64ModernRuntime-0001  one AI event per buffer played
N64ModernRuntime-0002  set_vi_rate for frame rates above 60; high-resolution VI timer on Windows
RecompFrontend-0001    built-in texture packs (the hi-res font and title)
RecompFrontend-0002    the Rewind input
RecompFrontend-0003    multiplayer mode: a controller joins as the next player on its first button press, the keyboard
                       stays with player 1, and a new controller's profile copies the single-player bindings
RecompFrontend-0004    a controller that disconnects and comes back is its player's again; the log names each controller
RecompFrontend-0005    a saved controller is matched to the connected one again (loaded with no hash, none ever was), so
                       its profile's bindings come back each session; the log says if a controller is known or new
RecompFrontend-0006    a player whose controller is the same kind as an earlier player's gets a numbered profile of
                       their own (made from the kind's), so two players on one kind of controller don't share bindings
RecompFrontend-0007    set_cursor_always_visible: the cursor stays shown while a controller or the keyboard is used (it
                       still hides while the cursor is locked for mouse aiming)
RecompFrontend-0008    set_cursor_shown_while_moving: the cursor shows while the mouse moves, whatever input was last,
                       and hides after two seconds still; the cursor's state is set every frame, not only on an event
RecompFrontend-0009    HDR output's menus (set_hdr_ui_white): with an scRGB swap chain the menus still draw in 8-bit
                       sRGB and go out in linear light at the given white
plume-0005             a query pool read of only the queries written (Vulkan calls a read with an unwritten one not ready)
plume-0006             a swap chain's color space (RenderSwapChainDesc::colorSpace): scRGB for HDR output, on D3D12 and
                       Vulkan (VK_EXT_swapchain_colorspace)
rt64-0001              viewports with left and right edge origins (split screen); fill-rect fix
rt64-0003              GPU timings for --gpu-profile, split into the parts of each pass (none read on the web)
rt64-0004              one barrier call around all of a frame's buffer uploads, not one per uploader
rt64-0005              every slot of the texture set holds a live texture: the blank tile texture in a new set's slots
                       and an evicted texture's, which eviction marks changed (a tile whose texture missed the cache
                       read slot 0, never written or left on a deleted texture)
rt64-0006              a fill rect's clear clamped to its target (a rect ending on the scissor's edge is a pixel wider)
rt64-0007              the workload queue's size set by the game (WORKLOAD_QUEUE_SIZE on the rt64 target); the page's is 16
rt64-0008              G_CULLDL: a display list whose bounding box lies wholly off one side of the view (as RT64 widens
                       it, and a quarter more each way) or behind the camera ends there, as on an N64; RT64 drew it all
rt64-0009              see-through surfaces (blended, Z compare without Z update) drawn at least the game's number of
                       display lists deep go after the rest of their 3D view, in their own order (See-Through Order),
                       with each call's textures still taken from TMEM as the calls before it in the game's order left it
rt64-0010              those see-through surfaces go in after the last other call as deep, not at the end of the view, so
                       a shallower call drawn after them (an effect, a Ghost Race's arrows) is still drawn after them
rt64-0011              and among themselves picture by picture (a call's triangles that share corners), far to near by
                       each picture's farthest corner, a call drawn as one call per run of its pictures in that order
rt64-0012              bloom (setBloom): before the HUD of each scene that draws the 3D world (the first draw without a
                       depth test after 12 that write depth), the bright parts blurred and added back; adapted from
                       VibinToast's RT64 fork for Jet Force Gemini (MIT); none on a WGSL build
rt64-0013              the player's brightness, contrast and gamma (setPictureAdjust), in the VI pass over the whole
                       picture after the VI's own gamma
rt64-0014              HDR output (setHdrOutput): an scRGB swap chain, read at startup; the VI pass writes linear light
                       at a paper white with highlights lifted towards a peak; the 16-bit target turned on with it
rt64-0015              glow (setGlowTextures): with bloom on, the draws that sample a listed texture are drawn again into
                       a mask over the scene's depth (an ubershader pipeline's variant, less-or-equal, no depth write),
                       and the mask's bright parts are added to the bloom at each texture's gain; setTextureDumpDirectory
                       for --dump-textures
rt64-0016              shadows (setShadows): every vertex as an orthographic sun sees it, from RT64's world position
                       (the modelview's; this game keeps the camera's rotation in the projection), around a point ahead
                       of its view's camera; before each 3D scene's HUD, per view, its opaque draws (indexed, perspective,
                       depth test and write) drawn from the sun into a 4096 shadow map, then again over the scene, each
                       pixel whose depth is the draw's own multiplied by the shadow tint where the map covers it (4x4
                       taps); a debug view
rt64-0017              with shadows on, G_CULLDL (rt64-0008) ends nothing, so what lies off the view (a building behind
                       the camera) still reaches the shadow map; the view clips it as before
rt64-0018              shadows: each of the 16 taps compared against the receiving surface's own plane at that texel
                       (receiver plane bias from its map position's derivatives), not a bias by its screen-space
                       slope, so curved roads stop shadowing themselves in their triangles and flickering with the view
rt64-0019              shadows on the ground: every perspective indexed draw takes shadows (shadowReceiver), not only
                       the casters; a draw that writes depth where the depth there is within 2e-4 of its own (the road's
                       came out a little off, so the exact test missed it), one that writes none where nothing nearer is
rt64-0020              shadows: see-through draws (ZMODE_XLU, such as a tunnel's light panels) cast nothing, though
                       they write depth; they still take shadows
rt64-0021              shadows: the depth pass from the sun clamps rather than clips (pancaking), so a tall building's
                       top, nearer the sun than the map's near end, keeps casting as the box moves; the receive pass's
                       own-depth tolerances grow with fwidth of the depth, so a road seen at a grazing angle keeps its
                       shadow at any view angle
rt64-0022              shadows: a mirrored draw (the water's reflection; modelview times projection flips
                       handedness) neither casts nor takes shadows
```
