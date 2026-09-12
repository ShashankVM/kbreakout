# Camera paddle control

KBreakOut now tracks an ArUco marker attached to the paddle controller and maps
its horizontal position to the on-screen paddle. The tracker starts with the game,
prefers a camera whose name contains `Robo Face2Face K20` or `Face2Face K20`,
and otherwise uses the system's default camera.

## Physical setup

1. Mount the camera above the wooden table with the whole movement area visible.
2. Attach a matte `DICT_4X4_50` marker with ID `0` to the paddle.
   The printed black marker should be 50 mm square with at least 10 mm of white
   margin on every side.
3. Move the paddle left and right on the table. The paddle follows the marker centre with light smoothing
   to reduce jitter. Keyboard and mouse controls continue to work.

If the paddle moves in the opposite direction, launch with:

```sh
KBREAKOUT_CAMERA_MIRROR=1 kbreakout
```

To force a particular camera, set `KBREAKOUT_CAMERA_ID` to the Qt camera device
ID. The selected camera name and ID are printed in KBreakOut's debug log when
tracking starts.

## Detection behaviour

Frames are reduced to 480 pixels wide and accepted at up to 60 frames per
second. ArUco detection runs on a worker thread, and an incoming frame is
dropped whenever the previous frame is still being processed. This keeps old
frames from accumulating and delaying paddle movement. Only marker ID `0` from
the `DICT_4X4_50` dictionary controls the paddle, so shadows, wood grain, and
unrelated dark objects are ignored.

OpenCV 4.7 or newer, including the `objdetect` module, is required at build time.
