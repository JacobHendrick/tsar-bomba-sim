# Footage comparison and accuracy limits

This is a visual reconstruction, not a validated nuclear-effects simulator.
The reference review informs the renderer and presentation, not the weapons model.

## Supplied references

1. [Tsar Bomba real footage!!! — Physics experiments](https://www.youtube.com/watch?v=JPQlf8C4rD4).
   Inspected sampled frames from the accessible 360p video, including a denser selection
   around 2:54–3:36. The roughly four-minute upload is an edited sequence with several
   camera positions, not a continuous elapsed-time record. Around 1:12–1:30, the image
   washes out around the flash. Later aerial views show a broad, pale cap with rounded
   lobes, shaded undersides and strong atmospheric veiling. Ground views show a tall
   dust column and low spreading material. These observations do not establish exact
   dimensions, radiance, descent time or cloud-growth speed. The re-upload's title is
   not independent provenance for every shot.
2. [Castle bravo 15 Mt 1954 — TheCentralnuclear](https://www.youtube.com/watch?v=dYJ5JfrK7KE).
   Inspected frames from the [archived time-lapse derivative](https://commons.wikimedia.org/wiki/File:CastleBravo1.gif),
   whose source explicitly identifies this video. It shows a saturated bright region,
   rolling cellular edges and layered surrounding cloud. The orange cast is not used
   as a temperature measurement. This is **Castle Bravo, not Tsar Bomba**; its surface-burst
   silhouette, environment and time-lapse speed are not transferred to the Tsar scene.

Video timestamps above identify locations in the uploads, not seconds after detonation.
Reference media and temporary frame sheets remain in the ignored build directory;
they are not redistributed with the application.

## What changed

- Cellular volume detail adds rounded lobes alongside the existing turbulent erosion.
  Detail coordinates follow the displayed volume scale instead of sliding through it.
- Local sun-occlusion samples add relief to those lobes; indirect light keeps the
  undersides readable. This is approximate scattering, not a radiative-transfer solution.
- A second atmospheric haze layer softens distant terrain. More precise terrain-ray
  intersection reduces coarse sampling error, and the landscape has gentler relief.
- Exposure recovers at different rates when the scene brightens and darkens, reducing
  distracting brightness pumping. No attempt is made to recreate the original film stock.
- The `F` key / `--fixed-camera` provides a stationary overview so cloud development can
  be compared without the cinematic camera continually moving away.

## What remains approximate

The 24-second aircraft/drop introduction is time-compressed. After detonation, the
default playback uses logarithmic early time and accelerated later time, as indicated
in the window title. It is not real-time playback of the historical event.

The previously added ~8 km fireball diameter and ~64 km high / ~95 km wide mature
cloud are presentation targets, supported by the historical summaries linked in the
[README](../README.md#historical-scale-and-staging). The mature target at 480 simulation
seconds is an artistic staging choice, not an observed historical timestamp. A
render-only scale transform fits the coarse smoke bounds to that target; erosion,
warping and the visible opacity threshold mean the visible edge is not an exact ruler.

The fluid model, ground boundary, dust, atmosphere, early fireball curve and shock
distortion remain simplified. No blast, radiation, fallout, damage or safety prediction
is validated by the footage or the software tests. The terrain is procedural rather
than surveyed Novaya Zemlya geography. The aircraft, casing and parachute are simplified
exterior models. Weather-dependent condensation layers seen in the footage are not
reconstructed as a measured humidity field.

## Repeatable visual checks

```sh
./build/tsar --grid 64 --steps 240 --fixed-camera --shot 30 early.png
./build/tsar --grid 64 --steps 240 --fixed-camera --shot 120 developing.png
./build/tsar --grid 64 --steps 240 --fixed-camera --shot 480 mature.png
```

These are reproducible software snapshots, not calibrated matches to video timestamps.
